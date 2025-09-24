#include <Arduino.h>
#include "bsp_can.h"
extern "C" {
#include "isotp.h"
}
#include "stm32f1xx.h"

// Minimal bxCAN register driver (no external libs)

static void can_gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    // PA11 RX input floating
    GPIOA->CRH &= ~(GPIO_CRH_MODE11 | GPIO_CRH_CNF11);
    GPIOA->CRH |= (0x01u << GPIO_CRH_CNF11_Pos);
    // PA12 TX AF PP 50MHz
    GPIOA->CRH &= ~(GPIO_CRH_MODE12 | GPIO_CRH_CNF12);
    GPIOA->CRH |= (0x03u << GPIO_CRH_MODE12_Pos) | (0x02u << GPIO_CRH_CNF12_Pos);
}

static void can_filters_init(void)
{
    CAN1->FMR |= CAN_FMR_FINIT;
    // Bank0: exact 0x7DF
    CAN1->FS1R |= (1u << 0); // 32-bit
    CAN1->FM1R &= ~(1u << 0); // mask mode
    CAN1->FFA1R &= ~(1u << 0); // FIFO0
    uint32_t id = (0x7DFu << 5);
    uint32_t mask = (0x7FFu << 5);
    CAN1->sFilterRegister[0].FR1 = id;
    CAN1->sFilterRegister[0].FR2 = mask;
    CAN1->FA1R |= (1u << 0);
    // Bank1: 0x7E0-0x7EF
    CAN1->FS1R |= (1u << 1);
    CAN1->FM1R &= ~(1u << 1);
    CAN1->FFA1R &= ~(1u << 1);
    uint32_t id2 = (0x7E0u << 5);
    uint32_t mask2 = (0x7F0u << 5);
    CAN1->sFilterRegister[1].FR1 = id2;
    CAN1->sFilterRegister[1].FR2 = mask2;
    CAN1->FA1R |= (1u << 1);
    CAN1->FMR &= ~CAN_FMR_FINIT;
}

void CAN_BSP_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
    can_gpio_init();
    // init mode
    CAN1->MCR |= CAN_MCR_INRQ;
    while ((CAN1->MSR & CAN_MSR_INAK) == 0) {}
    // 500k @ APB1=36MHz: SJW=1, BS1=13, BS2=4, BRP=4
    CAN1->BTR = (0u << CAN_BTR_SJW_Pos) | (12u << CAN_BTR_TS1_Pos) | (3u << CAN_BTR_TS2_Pos) | (3u << CAN_BTR_BRP_Pos);
    // normal mode
    CAN1->MCR &= ~CAN_MCR_INRQ;
    while (CAN1->MSR & CAN_MSR_INAK) {}
    can_filters_init();
}

int CAN_BSP_Send(uint32_t std_id, const uint8_t *data, uint8_t len)
{
    if (len > 8) len = 8;
    uint32_t tsr = CAN1->TSR;
    uint8_t mb;
    if (tsr & CAN_TSR_TME0) mb = 0; else if (tsr & CAN_TSR_TME1) mb = 1; else if (tsr & CAN_TSR_TME2) mb = 2; else return -1;
    CAN_TxMailBox_TypeDef *m = &CAN1->sTxMailBox[mb];
    m->TIR = ((std_id & 0x7FFu) << 21);
    m->TDTR = (len & 0x0Fu);
    uint32_t low = 0, high = 0;
    for (uint8_t i = 0; i < 4 && i < len; i++) low |= ((uint32_t)data[i] << (8u * i));
    for (uint8_t i = 0; i < 4 && (i + 4) < len; i++) high |= ((uint32_t)data[i + 4] << (8u * i));
    m->TDLR = low; m->TDHR = high;
    m->TIR |= CAN_TI0R_TXRQ;
    return 0;
}

void CAN_BSP_PollRx(void)
{
    while ((CAN1->RF0R & CAN_RF0R_FMP0_Msk) != 0) {
        CAN_FIFOMailBox_TypeDef *m = &CAN1->sFIFOMailBox[0];
        uint32_t id = (m->RIR >> 21) & 0x7FFu;
        uint8_t len = m->RDTR & 0x0Fu;
        uint8_t data[8];
        uint32_t l = m->RDLR, h = m->RDHR;
        data[0] = (uint8_t)(l & 0xFFu); data[1] = (uint8_t)((l >> 8) & 0xFFu); data[2] = (uint8_t)((l >> 16) & 0xFFu); data[3] = (uint8_t)((l >> 24) & 0xFFu);
        data[4] = (uint8_t)(h & 0xFFu); data[5] = (uint8_t)((h >> 8) & 0xFFu); data[6] = (uint8_t)((h >> 16) & 0xFFu); data[7] = (uint8_t)((h >> 24) & 0xFFu);
        isotp_on_can_rx(id, data, len);
        CAN1->RF0R |= CAN_RF0R_RFOM0;
    }
}

