#ifndef BSP_CAN_H
#define BSP_CAN_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern CAN_HandleTypeDef hcan;

void CAN_BSP_Init(void);
HAL_StatusTypeDef CAN_BSP_Send(uint32_t std_id, const uint8_t *data, uint8_t len);
void CAN_BSP_PollRx(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_CAN_H

