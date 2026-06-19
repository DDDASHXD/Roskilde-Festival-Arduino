#include "accelerometer.h"

#include <Wire.h>

#include "config.h"

static const uint8_t ADXL345_ADDR_ALT_LOW = 0x53;
static const uint8_t ADXL345_ADDR_ALT_HIGH = 0x1D;
static const uint8_t ADXL_DEVID = 0x00;
static const uint8_t ADXL_EXPECTED_DEVID = 0xE5;
static const uint8_t ADXL_POWER_CTL = 0x2D;
static const uint8_t ADXL_DATA_FORMAT = 0x31;
static const uint8_t ADXL_DATA_X0 = 0x32;

static uint8_t detectedAddress = 0;

static bool readRegister(uint8_t addr, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(addr, (uint8_t)1) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}

static bool findAccelerometer() {
  const uint8_t addresses[] = {
    ADXL345_ADDR_ALT_LOW,
    ADXL345_ADDR_ALT_HIGH
  };

  for (uint8_t i = 0; i < 2; i++) {
    uint8_t deviceId = 0;

    if (readRegister(addresses[i], ADXL_DEVID, deviceId) && deviceId == ADXL_EXPECTED_DEVID) {
      detectedAddress = addresses[i];
      return true;
    }
  }

  detectedAddress = 0;
  return false;
}

static bool writeRegister(uint8_t reg, uint8_t value) {
  if (detectedAddress == 0) {
    return false;
  }

  Wire.beginTransmission(detectedAddress);
  Wire.write(reg);
  Wire.write(value);

  return Wire.endTransmission() == 0;
}

bool accelerometerBegin() {
  Wire.begin();

  if (!findAccelerometer()) {
    return false;
  }

  bool ok = true;

  ok &= writeRegister(ADXL_POWER_CTL, 0x08);
  ok &= writeRegister(ADXL_DATA_FORMAT, 0x0B);

  return ok;
}

bool accelerometerRead(AccelReading &reading) {
  if (detectedAddress == 0) {
    return false;
  }

  uint8_t buf[6];

  Wire.beginTransmission(detectedAddress);
  Wire.write(ADXL_DATA_X0);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(detectedAddress, (uint8_t)6) != 6) {
    return false;
  }

  for (uint8_t i = 0; i < 6; i++) {
    buf[i] = Wire.read();
  }

  reading.x = (int16_t)((buf[1] << 8) | buf[0]);
  reading.y = (int16_t)((buf[3] << 8) | buf[2]);
  reading.z = (int16_t)((buf[5] << 8) | buf[4]);

  return true;
}

bool accelerometerLooksValid(const AccelReading &reading) {
  return reading.x >= ACCEL_VALID_MIN && reading.x <= ACCEL_VALID_MAX &&
         reading.y >= ACCEL_VALID_MIN && reading.y <= ACCEL_VALID_MAX &&
         reading.z >= ACCEL_VALID_MIN && reading.z <= ACCEL_VALID_MAX;
}

uint8_t accelerometerAddress() {
  return detectedAddress;
}

void accelerometerPrintI2cScan(Stream &output) {
  output.println(F("I2C scan:"));

  uint8_t foundCount = 0;
  bool foundAdxl345 = false;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      output.print(F("  found 0x"));

      if (addr < 16) {
        output.print('0');
      }

      output.print(addr, HEX);

      if (addr == 0x53 || addr == 0x1D) {
        output.print(F(" (ADXL345)"));
        foundAdxl345 = true;
      } else if (addr == 0x68) {
        output.print(F(" (ITG3200 gyro, GY-85)"));
      } else if (addr == 0x0D || addr == 0x1E) {
        output.print(F(" (HMC5883 mag, GY-85)"));
      }

      output.println();
      foundCount++;
    }
  }

  if (foundCount == 0) {
    output.println(F("  no I2C devices found"));
    return;
  }

  if (!foundAdxl345) {
    output.println(F("  ADXL345 missing from bus (0x53/0x1D)"));
    output.println(F("  If other GY-85 chips respond, check/replace the module."));
  }
}
