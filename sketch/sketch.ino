/*
 * NeoPixel + GY-85 IMU - color from position, brightness from movement
 *
 * Color: RGB mapped from tilt (ax, ay, az)
 * Brightness: scales with movement (change in accel); 0 when still
 *
 * Uses GY-85 library: https://github.com/sqrtmo/GY-85-arduino
 * MakerKit guide: https://learn.hobye.dk/kits/makerkit#h.q7l14j2tdqv
 *
 * NeoPixel wiring:
 *   pwr -> 5V, gnd -> GND, bo -> pin 3, do -> not used
 *
 * GY-85 wiring diagram:
 *
 *   GY-85                    Arduino Uno
 *   -----                    -----------
 *   VCC  ------------------  5V
 *   GND  ------------------  GND
 *   SCL  ------------------  A5
 *   SDA  ------------------  A4
 *
 *   GY-85 pinout (typical, left to right):
 *   [VCC] [GND] [SCL] [SDA] [XDA] [XCL] [AD0] [DRDY]
 *
 * Libraries: Adafruit NeoPixel, GY-85 (sqrtmo)
 */

 #include <Adafruit_NeoPixel.h>
 #include "GY_85.h"
 #include <Wire.h>
 
#define LED_PIN       3
#define LED_COUNT     21
// Keep current draw in a safer range so hard shakes do not brown out the Uno/GY-85.
#define MAX_BRIGHT    96
#define FILL_DURATION_MS 250
#define BRIGHT_SMOOTH 0.02
#define MIN_ACTIVE_BRIGHT 40
#define BRIGHT_HOLD_MS 100
#define NEIGHBOR_BLEND 0.25
#define DEBUG_SERIAL_MS 5000
#define MAX_CHANGE_FOR_BRIGHTNESS 30
#define MAX_RGB_SUM 420
#define ACCEL_VALID_MIN -360
#define ACCEL_VALID_MAX 360
#define MAX_BAD_ACCEL_READS 5

#define LOOP_MS       20
 
 Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
 GY_85 GY85;
 
 // Animation state for the LED fill transition and brightness smoothing.
 float smoothBrightness = 0;
 float fillPhase = 1.0;
 unsigned long fillStartMs = 0;
 unsigned long lastMotionMs = 0;
 unsigned long lastDebugMs = 0;
 int prevR = 128, prevG = 128, prevB = 128;
 int fillR = 128, fillG = 128, fillB = 128;
 int prevAx = 0, prevAy = 0, prevAz = 0;
 int badAccelReads = 0;
 bool firstAccelRead = true;

 int mapAxisToColor(int axis) {
   const int minVal = -180;
   const int maxVal = 180;
   // Convert tilt on a single axis into an RGB channel value.
   return constrain(map(axis, minVal, maxVal, 0, 255), 0, 255);
 }

int blendChannel(int from, int to, float amount) {
  amount = constrain(amount, 0.0, 1.0);
  return from + (int)((to - from) * amount);
}

bool accelLooksValid(int ax, int ay, int az) {
  return ax >= ACCEL_VALID_MIN && ax <= ACCEL_VALID_MAX &&
         ay >= ACCEL_VALID_MIN && ay <= ACCEL_VALID_MAX &&
         az >= ACCEL_VALID_MIN && az <= ACCEL_VALID_MAX;
}

void limitColorPower(int &r, int &g, int &b) {
  long total = (long)r + g + b;
  if (total <= MAX_RGB_SUM || total <= 0) {
    return;
  }

  float scale = (float)MAX_RGB_SUM / total;
  r = (int)(r * scale);
  g = (int)(g * scale);
  b = (int)(b * scale);
}

void recoverImu() {
  GY85.init();
  delay(5);
}

void smoothNeighborColors(
  const uint8_t sourceR[],
  const uint8_t sourceG[],
  const uint8_t sourceB[],
  uint8_t smoothedR[],
  uint8_t smoothedG[],
  uint8_t smoothedB[]
) {
  for (int i = 0; i < LED_COUNT; i++) {
    float totalWeight = 1.0;
    float accumR = sourceR[i];
    float accumG = sourceG[i];
    float accumB = sourceB[i];

    if (i > 0) {
      totalWeight += NEIGHBOR_BLEND;
      accumR += sourceR[i - 1] * NEIGHBOR_BLEND;
      accumG += sourceG[i - 1] * NEIGHBOR_BLEND;
      accumB += sourceB[i - 1] * NEIGHBOR_BLEND;
    }

    if (i < LED_COUNT - 1) {
      totalWeight += NEIGHBOR_BLEND;
      accumR += sourceR[i + 1] * NEIGHBOR_BLEND;
      accumG += sourceG[i + 1] * NEIGHBOR_BLEND;
      accumB += sourceB[i + 1] * NEIGHBOR_BLEND;
    }

    smoothedR[i] = (uint8_t)(accumR / totalWeight);
    smoothedG[i] = (uint8_t)(accumG / totalWeight);
    smoothedB[i] = (uint8_t)(accumB / totalWeight);
  }
}
 
 void setup() {
   Wire.begin();
#ifdef WIRE_HAS_TIMEOUT
   // Prevent a loose I2C connection from blocking the entire sketch indefinitely.
   Wire.setWireTimeout(25000, true);
#endif
   delay(10);
   Serial.begin(115200);
   delay(10);
 
   strip.begin();
   strip.show();
 
   GY85.init();
   delay(10);
 }
 
void loop() {
  uint8_t frameR[LED_COUNT];
  uint8_t frameG[LED_COUNT];
  uint8_t frameB[LED_COUNT];
  uint8_t smoothedR[LED_COUNT];
  uint8_t smoothedG[LED_COUNT];
  uint8_t smoothedB[LED_COUNT];

  // Read accelerometer data and use orientation as the current target color.
  int* accel = GY85.readFromAccelerometer();
   int ax = GY85.accelerometer_x(accel);
   int ay = GY85.accelerometer_y(accel);
   int az = GY85.accelerometer_z(accel);
   unsigned long now = millis();

#ifdef WIRE_HAS_TIMEOUT
   if (Wire.getWireTimeoutFlag()) {
     Wire.clearWireTimeoutFlag();
     recoverImu();
   }
#endif


   if (!accelLooksValid(ax, ay, az)) {
     badAccelReads++;
     ax = prevAx;
     ay = prevAy;
     az = prevAz;

     if (badAccelReads >= MAX_BAD_ACCEL_READS) {
       recoverImu();
       badAccelReads = 0;
     }
   } else {
     badAccelReads = 0;
   }
 
   // Seed the previous reading so the first frame does not look like a sudden movement spike.
   if (firstAccelRead) {
     prevAx = ax;
     prevAy = ay;
     prevAz = az;
     lastMotionMs = now;
     firstAccelRead = false;
   }
 
   // Movement is based on how much the accelerometer changed since the last loop.
   int changeMag = abs(ax - prevAx) + abs(ay - prevAy) + abs(az - prevAz);
   prevAx = ax;
   prevAy = ay;
   prevAz = az;
 
   int r = mapAxisToColor(ax);
   int g = mapAxisToColor(ay);
   int b = mapAxisToColor(az);
   limitColorPower(r, g, b);

   if (now - lastDebugMs >= DEBUG_SERIAL_MS) {
     lastDebugMs = now;
     Serial.print(F("Accel ax="));
     Serial.print(ax);
     Serial.print(F(" ay="));
     Serial.print(ay);
     Serial.print(F(" az="));
     Serial.print(az);
     Serial.print(F(" change="));
     Serial.print(changeMag);
     Serial.print(F(" rgb="));
     Serial.print(r);
     Serial.print(',');
     Serial.print(g);
     Serial.print(',');
     Serial.println(b);
   }

   if (changeMag > 20) {
     lastMotionMs = now;
   }
 
   // Start a left-to-right refill animation when the color target changes.
   if (r != prevR || g != prevG || b != prevB) {
     if (fillPhase >= 1.0) {
       fillPhase = 0;
       fillStartMs = now;
       fillR = r;
       fillG = g;
       fillB = b;
     }
   }
 
  if (fillPhase < 1.0) {
    fillPhase = (float)(now - fillStartMs) / FILL_DURATION_MS;
    if (fillPhase > 1.0) fillPhase = 1.0;
    float fillFront = fillPhase * LED_COUNT;

    // Build the raw refill frame first, then smooth across adjacent LEDs.
    for (int i = 0; i < strip.numPixels(); i++) {
      float amount = fillFront - i;
      if (amount >= 1.0) {
        frameR[i] = fillR;
        frameG[i] = fillG;
        frameB[i] = fillB;
      } else if (amount > 0.0) {
        frameR[i] = blendChannel(prevR, fillR, amount);
        frameG[i] = blendChannel(prevG, fillG, amount);
        frameB[i] = blendChannel(prevB, fillB, amount);
      } else {
        frameR[i] = prevR;
        frameG[i] = prevG;
        frameB[i] = prevB;
      }
    }
    if (fillPhase >= 1.0) {
      prevR = fillR;
      prevG = fillG;
      prevB = fillB;
    }
  } else {
    for (int i = 0; i < strip.numPixels(); i++) {
      frameR[i] = r;
      frameG[i] = g;
      frameB[i] = b;
    }
    prevR = r;
    prevG = g;
    prevB = b;
  }

  smoothNeighborColors(frameR, frameG, frameB, smoothedR, smoothedG, smoothedB);
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(smoothedR[i], smoothedG[i], smoothedB[i]));
  }
 
  // Brightness reacts to movement, then eases toward the new value for smoother output.
   int targetBright = constrain(
     map(changeMag, 20, MAX_CHANGE_FOR_BRIGHTNESS, 0, MAX_BRIGHT),
     0,
     MAX_BRIGHT
   );
   if (fillPhase < 1.0 || now - lastMotionMs < BRIGHT_HOLD_MS) {
     targetBright = max(targetBright, MIN_ACTIVE_BRIGHT);
   }
   smoothBrightness += (targetBright - smoothBrightness) * BRIGHT_SMOOTH;
   strip.setBrightness((int)smoothBrightness);
 
   strip.show();
 
   delay(LOOP_MS);
}
 
