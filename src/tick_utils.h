// Copyright (C) 2024, 2025 Jakub "lenwe" Kramarz
// This file is part of The Tick.
//
// The Tick is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// The Tick is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License

#ifndef TICK_UTILS_H
#define TICK_UTILS_H
#include "tick_default_config.h"

void detachInterrupts(void);
void attachInterrupts(void);
void output_debug_string(String s);

byte hex_to_byte(char in) {
  if (in >= '0' && in <= '9')
    return in - '0';
  else if (in >= 'A' && in <= 'F')
    return in - 'A' + 10;
  else if (in >= 'a' && in <= 'f')
    return in - 'a' + 10;
  else
    return 0;
}

char c2h(unsigned char c) { return "0123456789abcdef"[0xF & c]; }

uint32_t getChipID() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  return chipId;
}

uint32_t readVDCVoltage(void) {
  const float R1 = 12700.0;
  const float R2 = 1000.0; 
  const float CALIBRATION_FACTOR = 1.00;
  int raw_mv = analogReadMilliVolts(PIN_VSENSE);
  float vdc = ((float) raw_mv) * ((R1 + R2) / R2) * CALIBRATION_FACTOR;
  return (uint32_t) vdc;
}

void transmit_id(String sendValue, unsigned long bitcount);
void jamming_enable(void);
void jamming_disable(void);
void append_log(String text);
void output_debug_string(String s);
void card_read_handler(String s);
void clear_config(void);
String dhcp_hostname;

#endif