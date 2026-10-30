// Minimal class-compliant USB-MIDI device for the ATmega32U4 Arduino core.
// Plugs into the core's PluggableUSB so the board shows up on the Mac as a
// MIDI port (next to the usual serial port used for uploading).
#pragma once
#include <Arduino.h>
#include "PluggableUSB.h"

class UsbMidi : public PluggableUSBModule {
public:
  UsbMidi();
  void send(uint8_t status, uint8_t data1, uint8_t data2);
  void flush();
  void drain();  // discard anything the host sends us

protected:
  bool setup(USBSetup&) override { return false; }
  int getInterface(uint8_t* interfaceCount) override;
  int getDescriptor(USBSetup&) override { return 0; }

private:
  uint8_t epType[2];
};

extern UsbMidi midi;
