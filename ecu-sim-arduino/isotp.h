#ifndef ISOTP_H
#define ISOTP_H

#include <stdint.h>

typedef void (*IsotpRequestHandler)(const uint8_t *data, uint16_t len);

void isotp_init(uint32_t request_id, uint32_t response_id, uint32_t tester_id_base);
void isotp_set_request_handler(IsotpRequestHandler handler);
void isotp_on_can_rx(uint32_t std_id, const uint8_t *data, uint8_t len);
void isotp_poll(void);

// Helper to send a response payload (will choose SF or FF/CF based on length)
void isotp_send_response(const uint8_t *payload, uint16_t length);

#endif // ISOTP_H

