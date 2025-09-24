#ifndef OBD_H
#define OBD_H

#include <stdint.h>

void obd_init(void);
void obd_on_isotp_request(const uint8_t *payload, uint16_t length);

#endif // OBD_H

