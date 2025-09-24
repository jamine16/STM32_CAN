#include "bsp_can.h"

CAN_HandleTypeDef hcan;

static void CAN_BSP_ConfigFilters(void)
{
    // Filter 0: accept 0x7DF (functional request) exactly
    CAN_FilterTypeDef filter0;
    filter0.FilterBank = 0;
    filter0.FilterMode = CAN_FILTERMODE_IDMASK;
    filter0.FilterScale = CAN_FILTERSCALE_32BIT;
    filter0.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter0.FilterActivation = ENABLE;
    filter0.SlaveStartFilterBank = 14; // not used on single CAN instance, but set per HAL requirements
    filter0.FilterIdHigh = (0x7DF << 5);
    filter0.FilterIdLow = 0x0000;
    filter0.FilterMaskIdHigh = (0x7FF << 5);
    filter0.FilterMaskIdLow = 0x0000;
    HAL_CAN_ConfigFilter(&hcan, &filter0);

    // Filter 1: accept 0x7E0-0x7EF (tester physical IDs) for FlowControl
    CAN_FilterTypeDef filter1;
    filter1.FilterBank = 1;
    filter1.FilterMode = CAN_FILTERMODE_IDMASK;
    filter1.FilterScale = CAN_FILTERSCALE_32BIT;
    filter1.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter1.FilterActivation = ENABLE;
    filter1.SlaveStartFilterBank = 14;
    // Mask lower 4 bits to accept range 0x7E0-0x7EF
    filter1.FilterIdHigh = (0x7E0 << 5);
    filter1.FilterIdLow = 0x0000;
    filter1.FilterMaskIdHigh = (0x7F0 << 5);
    filter1.FilterMaskIdLow = 0x0000;
    HAL_CAN_ConfigFilter(&hcan, &filter1);
}

void CAN_BSP_Init(void)
{
    __HAL_RCC_CAN1_CLK_ENABLE();

    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 4;           // Example for APB1=36 MHz -> 500 kbit/s
    hcan.Init.Mode = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = DISABLE;
    hcan.Init.AutoWakeUp = DISABLE;
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = ENABLE;

    HAL_CAN_DeInit(&hcan);
    HAL_CAN_Init(&hcan);

    CAN_BSP_ConfigFilters();

    HAL_CAN_Start(&hcan);

    // Enable RX FIFO 0 pending interrupt
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

HAL_StatusTypeDef CAN_BSP_Send(uint32_t std_id, const uint8_t *data, uint8_t len)
{
    CAN_TxHeaderTypeDef header;
    uint8_t payload[8] = {0};
    uint32_t mailbox = 0;

    if (len > 8) {
        return HAL_ERROR;
    }

    header.StdId = std_id & 0x7FF;
    header.ExtId = 0;
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = len;
    header.TransmitGlobalTime = DISABLE;

    for (uint8_t i = 0; i < len; i++) {
        payload[i] = data[i];
    }
    // pad with 0x00 (common expectation)
    for (uint8_t i = len; i < 8; i++) {
        payload[i] = 0x00;
    }

    return HAL_CAN_AddTxMessage(&hcan, &header, payload, &mailbox);
}

void CAN_BSP_PollRx(void)
{
    // Polling RX helper for environments where ISR isn't hooked by the Arduino core.
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];
    while (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) > 0) {
        if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &header, data) == HAL_OK) {
            extern void isotp_on_can_rx(uint32_t std_id, const uint8_t *data, uint8_t len);
            isotp_on_can_rx(header.StdId, data, header.DLC);
        } else {
            break;
        }
    }
}

