#include <SPI.h>
#include <mcp2515.h>

// Lightweight Arduino shim to reuse ISO-TP/OBD logic
extern "C" {
  #include "../../src/isotp.h"
  #include "../../src/obd.h"
}

// Configure MCP2515 pins (adjust for your board)
// CS pin
static const uint8_t CAN_CS_PIN = 10;
// INT pin from MCP2515 to MCU
static const uint8_t CAN_INT_PIN = 2;

MCP2515 mcp2515(CAN_CS_PIN);

volatile bool can_irq = false;

void canIrqHandler() {
  can_irq = true;
}

static void can_send(uint32_t id, const uint8_t *data, uint8_t len) {
  struct can_frame frame;
  frame.can_id = (id & 0x7FF);
  frame.can_dlc = (len > 8) ? 8 : len;
  for (uint8_t i = 0; i < frame.can_dlc; i++) {
    frame.data[i] = data[i];
  }
  // pad to 8 to match our HAL behavior
  for (uint8_t i = frame.can_dlc; i < 8; i++) {
    frame.data[i] = 0x00;
  }
  mcp2515.sendMessage(&frame);
}

void setup() {
  pinMode(CAN_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), canIrqHandler, FALLING);

  SPI.begin();
  mcp2515.reset();
  // Set to 500 kbps @ 16 MHz crystal; adjust if your MCP2515 has 8 MHz
  mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);
  mcp2515.setNormalMode();

  // Wire CAN BSP send to adapter
  // We cannot change C functions at link-time easily, so we call directly in ISO-TP via a weak symbol if desired.
  // Instead, we implement a small forwarding by wrapping CAN_BSP_Send through a weak alias in C source.

  isotp_init(0x7DF, 0x7E8, 0x7E0);
  obd_init();
}

void loop() {
  // RX handling
  if (can_irq) {
    can_irq = false;
    struct can_frame rx;
    while (mcp2515.readMessage(&rx) == MCP2515::ERROR_OK) {
      if ((rx.can_id & 0x80000000UL) == 0) { // ensure standard frame
        isotp_on_can_rx((uint16_t)(rx.can_id & 0x7FF), rx.data, rx.can_dlc);
      }
    }
  }

  // Periodic tasks
  isotp_poll();
}

// Provide CAN_BSP_Send for ISO-TP to use (C linkage)
extern "C" int CAN_BSP_Send(uint32_t std_id, const uint8_t *data, uint8_t len);
extern "C" int CAN_BSP_Send(uint32_t std_id, const uint8_t *data, uint8_t len) {
  can_send(std_id, data, len);
  return 0;
}

