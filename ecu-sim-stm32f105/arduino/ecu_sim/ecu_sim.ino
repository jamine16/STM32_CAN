#include "Arduino.h"

extern "C" {
  #include "bsp_can.h"
  #include "isotp.h"
  #include "obd.h"
}

void setup() {
  // Arduino core for STM32 already initialized HAL, just init CAN + stacks.
  CAN_BSP_Init();
  isotp_init(0x7DF, 0x7E8, 0x7E0);
  obd_init();
}

void loop() {
  // Poll RX to ensure messages are pulled even if ISR is not connected
  CAN_BSP_PollRx();
  isotp_poll();
}

