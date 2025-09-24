#include "stm32f1xx_hal.h"
#include "bsp_can.h"
#include "isotp.h"
#include "obd.h"

static void SystemClock_Config(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    CAN_BSP_Init();
    isotp_init(0x7DF, 0x7E8, 0x7E0);
    obd_init();

    while (1) {
        isotp_poll();
    }
}

// Minimal clock config placeholder; adjust via CubeMX in real project
static void SystemClock_Config(void)
{
    // Assume system is configured by CubeMX. Leave empty here.
}

// HAL CAN RX callback
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_arg)
{
    if (hcan_arg->Instance != CAN1) return;
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];
    if (HAL_CAN_GetRxMessage(hcan_arg, CAN_RX_FIFO0, &header, data) == HAL_OK) {
        isotp_on_can_rx(header.StdId, data, header.DLC);
    }
}

