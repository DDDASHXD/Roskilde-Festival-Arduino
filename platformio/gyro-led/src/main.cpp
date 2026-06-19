/*
 * One WS2812B strip + one GY-85/ADXL345 accelerometer on Arduino Uno.
 *
 * Accelerometer:
 *   SDA: A4
 *   SCL: A5
 *
 * LED strips:
 *   Data: pins 5 and 6
 */

#include <Arduino.h>

#include "accelerometer.h"
#include "animation.h"
#include "config.h"

static AccelReading lastGoodReading = {0, 0, 0};
static bool sensorOk = false;
static uint8_t badAccelReads = 0;
static unsigned long lastDebugMs = 0;

static void recoverSensor() {
  sensorOk = accelerometerBegin();
  delay(5);
}

static bool readFrameInput(AccelReading &reading) {
  bool readOk = accelerometerRead(reading);

  if (readOk && accelerometerLooksValid(reading)) {
    lastGoodReading = reading;
    badAccelReads = 0;
    return true;
  }

  badAccelReads++;
  reading = lastGoodReading;

  if (badAccelReads >= MAX_BAD_ACCEL_READS) {
    recoverSensor();
    badAccelReads = 0;
  }

  return sensorOk;
}

static void printDebug(const AccelReading &reading) {
  unsigned long now = millis();

  if (now - lastDebugMs < DEBUG_SERIAL_MS) {
    return;
  }

  lastDebugMs = now;

  Serial.print(F("ax="));
  Serial.print(reading.x);
  Serial.print(F(" ay="));
  Serial.print(reading.y);
  Serial.print(F(" az="));
  Serial.println(reading.z);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting up...");

  delay(100);

  animationBegin();
  sensorOk = accelerometerBegin();

  Serial.print(F("ADXL345 on hardware I2C, LED pins "));
  Serial.print(LED_PIN_1);
  Serial.print(F(" and "));
  Serial.print(LED_PIN_2);

  if (sensorOk) {
    Serial.print(F(": OK at 0x"));
    Serial.println(accelerometerAddress(), HEX);
  } else {
    Serial.println(F(": FAIL, no ADXL345 found at 0x53 or 0x1D"));
    accelerometerPrintI2cScan(Serial);
  }
}

void loop() {
  if (!sensorOk) {
    recoverSensor();
  }

  AccelReading reading = lastGoodReading;

  if (sensorOk) {
    readFrameInput(reading);
    printDebug(reading);
  }

  drawAnimationFrame(reading);
  showAnimationFrame();
}
