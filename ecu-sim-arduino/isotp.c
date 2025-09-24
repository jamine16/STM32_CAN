#include "isotp.h"
#include "bsp_can.h"
#include <string.h>
#include <Arduino.h>

// Minimal ISO-TP implementation sufficient for VIN (Mode 09 PID 02) response.

typedef enum {
    ISOTP_IDLE = 0,
    ISOTP_WAIT_FC,
    ISOTP_SENDING_CF
} IsotpTxState;

static struct {
    uint32_t request_id;      // 0x7DF (functional) and optionally 0x7E0 (physical)
    uint32_t response_id;     // 0x7E8
    uint32_t tester_id_base;  // 0x7E0 (accept 0x7E0-0x7EF for FC)

    IsotpRequestHandler request_handler;

    // TX state for responses
    IsotpTxState tx_state;
    uint8_t tx_buffer[256];
    uint16_t tx_length;
    uint16_t tx_offset;       // next byte to send in CF (after first 6 bytes used by FF)
    uint8_t next_sn;          // sequence number (1..15)
    uint8_t pending_cf;       // number of CFs left to send
    uint8_t block_size;       // 0 = unlimited
    uint8_t st_min_ms;        // STmin in milliseconds (0..127)
    uint8_t block_counter;
    uint32_t next_allowed_tick;
    uint32_t fc_deadline_tick;
} ctx;

static uint32_t millis(void)
{
    return (uint32_t)::millis();
}

void isotp_init(uint32_t request_id, uint32_t response_id, uint32_t tester_id_base)
{
    memset(&ctx, 0, sizeof(ctx));
    ctx.request_id = request_id & 0x7FF;
    ctx.response_id = response_id & 0x7FF;
    ctx.tester_id_base = tester_id_base & 0x7F0; // accept 0x7E0-0x7EF
    ctx.tx_state = ISOTP_IDLE;
}

void isotp_set_request_handler(IsotpRequestHandler handler)
{
    ctx.request_handler = handler;
}

static void isotp_send_single_frame(const uint8_t *payload, uint16_t length)
{
    uint8_t frame[8] = {0};
    frame[0] = (uint8_t)(length & 0x0F);
    if (length > 7) {
        length = 7;
    }
    memcpy(&frame[1], payload, length);
    CAN_BSP_Send(ctx.response_id, frame, 8);
}

static void isotp_send_first_frame(const uint8_t *payload, uint16_t length)
{
    uint8_t frame[8] = {0};
    frame[0] = 0x10 | ((length >> 8) & 0x0F);
    frame[1] = (uint8_t)(length & 0xFF);
    // First frame carries 6 bytes of payload
    memcpy(&frame[2], payload, 6);
    CAN_BSP_Send(ctx.response_id, frame, 8);

    // Prepare CF state
    ctx.tx_state = ISOTP_WAIT_FC;
    ctx.tx_length = length;
    ctx.tx_offset = 6;
    ctx.next_sn = 1;
    uint16_t remaining = (length > 6) ? (length - 6) : 0;
    ctx.pending_cf = (remaining + 6) / 7; // ceil(remaining/7)
    ctx.block_counter = 0;
    ctx.block_size = 0; // until FC arrives
    ctx.st_min_ms = 0;
    ctx.fc_deadline_tick = millis() + 1000; // 1s to receive FC
}

void isotp_send_response(const uint8_t *payload, uint16_t length)
{
    if (length <= 7) {
        isotp_send_single_frame(payload, length);
        ctx.tx_state = ISOTP_IDLE;
        return;
    }

    if (length > sizeof(ctx.tx_buffer)) {
        length = sizeof(ctx.tx_buffer);
    }
    memcpy(ctx.tx_buffer, payload, length);
    isotp_send_first_frame(ctx.tx_buffer, length);
}

static int is_fc_frame(uint8_t pci)
{
    return ((pci & 0xF0u) == 0x30u);
}

static int is_sf_frame(uint8_t pci)
{
    return ((pci & 0xF0u) == 0x00u);
}

void isotp_on_can_rx(uint32_t std_id, const uint8_t *data, uint8_t len)
{
    if (len < 1) {
        return;
    }

    // Handle FlowControl frames from tester (0x7E0-0x7EF)
    if ((std_id & 0x7F0u) == ctx.tester_id_base) {
        if (ctx.tx_state == ISOTP_WAIT_FC && is_fc_frame(data[0])) {
            uint8_t fs = data[0] & 0x0F; // 0=CTS, 1=WT, 2=OVFLW
            uint8_t bs = (len >= 2) ? data[1] : 0;
            uint8_t st = (len >= 3) ? data[2] : 0;
            if (fs == 0x00) { // CTS
                ctx.block_size = bs;
                ctx.st_min_ms = (st <= 0x7F) ? st : 0; // ignore microsecond encodings for simplicity
                ctx.tx_state = ISOTP_SENDING_CF;
                ctx.next_allowed_tick = millis();
                ctx.block_counter = 0;
            } else if (fs == 0x01) {
                // Wait: extend deadline, do nothing
                ctx.fc_deadline_tick = millis() + 1000;
            } else {
                // Overflow/Abort
                ctx.tx_state = ISOTP_IDLE;
                ctx.pending_cf = 0;
            }
        }
        return;
    }

    // Pass single-frame requests addressed to 0x7DF or 0x7E0 to higher layer
    if (std_id == ctx.request_id || (std_id & 0x7FFu) == 0x7E0u) {
        if (is_sf_frame(data[0])) {
            uint8_t payload_len = data[0] & 0x0F;
            if (payload_len > (len - 1)) {
                payload_len = len - 1;
            }
            if (ctx.request_handler) {
                ctx.request_handler(&data[1], payload_len);
            }
        }
        // Multi-frame request handling is not implemented (not needed for VIN request)
    }
}

void isotp_poll(void)
{
    uint32_t now = millis();

    if (ctx.tx_state == ISOTP_WAIT_FC) {
        if (now >= ctx.fc_deadline_tick) {
            // No FC received; abort
            ctx.tx_state = ISOTP_IDLE;
            ctx.pending_cf = 0;
        }
        return;
    }

    if (ctx.tx_state == ISOTP_SENDING_CF && ctx.pending_cf > 0) {
        if (now < ctx.next_allowed_tick) {
            return;
        }

        uint8_t frame[8] = {0};
        frame[0] = 0x20 | (ctx.next_sn & 0x0F);

        uint8_t bytes_this_frame = 7;
        uint16_t remaining = ctx.tx_length - ctx.tx_offset;
        if (remaining < bytes_this_frame) {
            bytes_this_frame = (uint8_t)remaining;
        }
        memcpy(&frame[1], &ctx.tx_buffer[ctx.tx_offset], bytes_this_frame);
        CAN_BSP_Send(ctx.response_id, frame, 8);

        ctx.tx_offset += bytes_this_frame;
        ctx.next_sn = (uint8_t)((ctx.next_sn + 1) & 0x0F);
        if (ctx.next_sn == 0) {
            ctx.next_sn = 1; // sequence numbers 1..15
        }
        ctx.pending_cf--;
        ctx.block_counter++;

        // Respect STmin
        ctx.next_allowed_tick = now + ctx.st_min_ms;

        if (ctx.pending_cf == 0) {
            ctx.tx_state = ISOTP_IDLE;
        } else if (ctx.block_size != 0 && ctx.block_counter >= ctx.block_size) {
            // In a full implementation, we would wait for another FC here.
            // For simplicity, continue sending without additional FC.
            ctx.block_counter = 0;
        }
    }
}

