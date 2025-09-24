#include <Arduino.h>
#include <STM32_CAN.h>
#include "bsp_can.h"
extern "C" {
#include "isotp.h"
}

// Use CAN1 by default (constructor without SYSTEM_CLOCK for newer STM32duino CAN)
STM32_CAN Can(CAN1);

void CAN_BSP_Init(void)
{
    // 500 kbps typical for ISO15765-4
    Can.begin(500000);
    // Filters: accept 0x7DF and 0x7E0-0x7EF
    Can.setFilter(0, 0x7DF, 0x7FF);
    // Mask lower 4 bits to accept range
    Can.setFilter(1, 0x7E0, 0x7F0);
}

int CAN_BSP_Send(uint32_t std_id, const uint8_t *data, uint8_t len)
{
    CAN_message_t msg;
    msg.id = std_id & 0x7FF;
    msg.len = (len > 8) ? 8 : len;
    for (uint8_t i = 0; i < msg.len; i++) {
        msg.data[i] = data[i];
    }
    for (uint8_t i = msg.len; i < 8; i++) {
        msg.data[i] = 0x00;
    }
    return Can.write(msg) ? 0 : -1;
}

void CAN_BSP_PollRx(void)
{
    CAN_message_t rx;
    while (Can.read(rx)) {
        isotp_on_can_rx(rx.id & 0x7FF, rx.data, rx.len);
    }
}

