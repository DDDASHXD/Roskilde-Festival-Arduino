#pragma once

#include <Arduino.h>

struct AccelReading {
  int16_t x;
  int16_t y;
  int16_t z;
};

bool accelerometerBegin();
bool accelerometerRead(AccelReading &reading);
bool accelerometerLooksValid(const AccelReading &reading);
uint8_t accelerometerAddress();
void accelerometerPrintI2cScan(Stream &output);
