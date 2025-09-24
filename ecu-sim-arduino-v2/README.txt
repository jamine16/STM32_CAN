Arduino STM32F105 OBD-II VIN responder (ISO-TP)

Files:
- ecu_sim.ino: Arduino entry point
- bsp_can.cpp/.h: STM32duino in-core CAN adapter (bxCAN)
- isotp.c/.h: minimal ISO-TP responder
- obd.c/.h: Mode 09 PID 02 (VIN) handler

Requirements:
- Boards Manager: STM32 MCU based boards (STMicroelectronics)
- Library Manager: STM32 CAN (STM32duino CAN)

Hardware:
- TJA1050: PA12->TXD, PA11->RXD; CANH/L to bus; 120Ω termination

Usage:
- Open ecu_sim.ino, select your STM32F105 board, upload
- Send 7DF#02090200000000; reply appears on 7E8 via ISO-TP
