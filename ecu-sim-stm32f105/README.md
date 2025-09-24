STM32F105 OBD-II ECU Simulator (VIN responder)

Overview
This firmware targets STM32F105 and simulates a simple OBD-II ECU that responds to Mode 09 PID 02 (VIN) over ISO 15765-4 (CAN, 11-bit, 500 kbit/s). It uses STM32 HAL (bxCAN) and a minimal ISO-TP implementation with FlowControl handling (CTS, STmin).

Features
- Responds to functional requests on 0x7DF (Mode 09, PID 02) with a VIN on 0x7E8
- ISO-TP multi-frame response (FirstFrame + ConsecutiveFrames) with FlowControl support
- CAN bitrate configured for 500 kbit/s (ISO 15765-4 11/500)
- Clean separation of layers: CAN BSP, ISO-TP, OBD

Hardware Requirements
- STM32F105 MCU (e.g., STM32F105RCTx or similar)
- CAN transceiver (e.g., TJA1050, SN65HVD230, etc.) connected to the MCU CAN pins
- 120 Ω termination as required by your CAN network

CAN IDs
- Request (functional): 0x7DF (tester -> ECU)
- Response (physical): 0x7E8 (ECU -> tester)
- FlowControl accepted from: 0x7E0-0x7EF (tester physical IDs)

VIN
- Default VIN: 1HGBH41JXMN109186
- You can change it in src/obd.c (VIN_STRING)

Project Structure
- src/bsp_can.c, src/bsp_can.h: HAL-based CAN init and TX utilities
- src/isotp.c, src/isotp.h: Minimal ISO-TP segmentation and FlowControl handling
- src/obd.c, src/obd.h: OBD-II Mode 09 PID 02 handler (VIN)
- src/main.c: Example glue code

Integration (STM32CubeIDE / STM32CubeMX)
1) Create a new STM32CubeIDE project for your exact STM32F105 part using STM32CubeMX initialization.
2) Enable CAN1 (bxCAN) peripheral.
   - Target 500 kbit/s at your APB1 clock. Example (APB1 = 36 MHz): Prescaler=4, BS1=13, BS2=4, SJW=1.
   - Ensure GPIOs for CAN RX/TX are configured per your board.
3) Generate code, then copy the files in src/ into your project (e.g., Core/Src and Core/Inc) and add include paths as needed.
4) Call CAN_BSP_Init() early in main after HAL init and clock config.
5) Ensure HAL CAN RX interrupt/notifications are enabled (done inside CAN_BSP_Init()).
6) Build and flash.

Usage
1) Connect your board to a CAN bus via a transceiver.
2) Use a scan tool set to ISO 15765-4 (11-bit, 500 kbit/s).
3) Send Mode 09, PID 02 request (VIN) to 0x7DF. Example payload (Single Frame): 02 09 02 (padded to 8 bytes).
4) The ECU will reply on 0x7E8 with a multi-frame response containing the VIN.

Linux SocketCAN quick test
- Bring up CAN: `sudo ip link set can0 up type can bitrate 500000`
- Observe bus: `candump can0`
- Send VIN request (ISO-TP SF 0x02, 0x09, 0x02 padded): `cansend can0 7DF#02090200000000`
- You should see responses from 0x7E8 with FF/CF frames carrying the VIN.

Notes
- This code assumes standard (11-bit) CAN IDs.
- If your APB1 clock is not 36 MHz, adjust timing parameters in CAN_BSP_Init().
- This is a minimal example; production ISO-TP/UDS stacks include more state handling, timeouts, and robustness features.

License
MIT

