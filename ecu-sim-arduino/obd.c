#include "obd.h"
#include "isotp.h"
#include <string.h>

// SAE J1979 Mode 09 PID 02 VIN

static const char VIN_STRING[] = "1HGBH41JXMN109186"; // 17 ASCII

void obd_init(void)
{
    isotp_set_request_handler(obd_on_isotp_request);
}

static void respond_vin(void)
{
    // Positive response: 0x49, PID 0x02, then VIN data records
    // Many scan tools accept a simple concatenated payload. We'll send service + pid + VIN bytes.
    uint8_t payload[3 + sizeof(VIN_STRING) - 1];
    payload[0] = 0x49; // response to 0x09
    payload[1] = 0x02; // PID 02
    payload[2] = 0x01; // Record number 1 (minimal)
    memcpy(&payload[3], VIN_STRING, sizeof(VIN_STRING) - 1);

    isotp_send_response(payload, sizeof(payload));
}

void obd_on_isotp_request(const uint8_t *payload, uint16_t length)
{
    if (length < 2) {
        return;
    }
    uint8_t service = payload[0];
    uint8_t pid = payload[1];

    if (service == 0x09 && pid == 0x02) {
        respond_vin();
    }
}

