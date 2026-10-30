#include "UsbMidi.h"

UsbMidi midi;

UsbMidi::UsbMidi() : PluggableUSBModule(2, 2, epType) {
  epType[0] = EP_TYPE_BULK_OUT;
  epType[1] = EP_TYPE_BULK_IN;
  PluggableUSB().plug(this);
}

int UsbMidi::getInterface(uint8_t* interfaceCount) {
  *interfaceCount += 2;
  const uint8_t ac = pluggedInterface, ms = pluggedInterface + 1;
  const uint8_t epOut = pluggedEndpoint, epIn = pluggedEndpoint + 1;
  const uint8_t d[] = {
    8, 11, ac, 2, 1, 1, 0, 0,                // interface association: audio
    9, 4, ac, 0, 0, 1, 1, 0, 0,              // AudioControl interface
    9, 0x24, 1, 0x00, 0x01, 9, 0, 1, ms,     // AC header, owns the streaming interface
    9, 4, ms, 0, 2, 1, 3, 0, 0,              // MIDIStreaming interface
    7, 0x24, 1, 0x00, 0x01, 65, 0,           // MS header (65 bytes of class descriptors)
    6, 0x24, 2, 1, 1, 0,                     // embedded IN jack 1
    6, 0x24, 2, 2, 2, 0,                     // external IN jack 2
    9, 0x24, 3, 1, 3, 1, 2, 1, 0,            // embedded OUT jack 3 <- jack 2
    9, 0x24, 3, 2, 4, 1, 1, 1, 0,            // external OUT jack 4 <- jack 1
    9, 5, epOut, 2, 64, 0, 0, 0, 0,          // bulk OUT endpoint (host -> device)
    5, 0x25, 1, 1, 1,                        //   feeds embedded IN jack 1
    9, 5, uint8_t(0x80 | epIn), 2, 64, 0, 0, 0, 0,  // bulk IN endpoint (device -> host)
    5, 0x25, 1, 1, 3,                        //   fed by embedded OUT jack 3
  };
  return USB_SendControl(0, d, sizeof(d));
}

void UsbMidi::send(uint8_t status, uint8_t data1, uint8_t data2) {
  const uint8_t packet[4] = { uint8_t(status >> 4), status, data1, data2 };
  USB_Send(pluggedEndpoint + 1, packet, sizeof(packet));
}

void UsbMidi::flush() {
  USB_Flush(pluggedEndpoint + 1);
}

void UsbMidi::drain() {
  while (USB_Available(pluggedEndpoint)) USB_Recv(pluggedEndpoint);
}
