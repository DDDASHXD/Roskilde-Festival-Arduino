#include "animation.h"

#include "config.h"

static CRGB leds[LED_COUNT];

static AccelReading previousReading = {0, 0, 0};
static bool firstReading = true;

static uint8_t hue = 128;
static uint8_t brightness = IDLE_BRIGHT;
static uint8_t wavePhase = 0;
static uint8_t idleBlend = 0;
static unsigned long lastMovementMs = 0;

static uint8_t movementToHue(int dx, int dy, int dz) {
  int x = constrain(map(dx, -MAX_CHANGE_FOR_HUE, MAX_CHANGE_FOR_HUE, 0, 255), 0, 255);
  int y = constrain(map(dy, -MAX_CHANGE_FOR_HUE, MAX_CHANGE_FOR_HUE, 0, 255), 0, 255);
  int z = constrain(map(dz, -MAX_CHANGE_FOR_HUE, MAX_CHANGE_FOR_HUE, 0, 255), 0, 255);

  return (uint8_t)((x + (2 * y) + (3 * z)) / 6);
}

static CRGB reactivePixel(uint16_t index, uint8_t currentHue, uint8_t currentBrightness) {
  uint8_t wave = sin8(wavePhase + (index * WAVE_SPACING));
  uint8_t amount = WAVE_BASE + scale8(wave, 255 - WAVE_BASE);

  return CHSV(currentHue, 255, scale8(currentBrightness, amount));
}

static CRGB idlePixel(uint16_t index, uint8_t currentBrightness) {
  uint8_t wave = sin8(wavePhase + (index * WAVE_SPACING));
  uint8_t amount = WAVE_BASE + scale8(wave, 255 - WAVE_BASE);
  uint8_t ledHue = wavePhase + (index * RAINBOW_SPACING);

  return CHSV(ledHue, 255, scale8(currentBrightness, amount));
}

void animationBegin() {
  FastLED.addLeds<LED_TYPE, LED_PIN_1, COLOR_ORDER>(leds, LED_COUNT);
  FastLED.addLeds<LED_TYPE, LED_PIN_2, COLOR_ORDER>(leds, LED_COUNT);
  FastLED.setBrightness(255);
  FastLED.clear(true);
  lastMovementMs = millis();
}

void drawAnimationFrame(const AccelReading &reading) {
  if (firstReading) {
    previousReading = reading;
    firstReading = false;
  }

  int dx = reading.x - previousReading.x;
  int dy = reading.y - previousReading.y;
  int dz = reading.z - previousReading.z;
  int changeMag = abs(dx) + abs(dy) + abs(dz);

  previousReading = reading;

  unsigned long now = millis();

  if (changeMag >= MIN_MOVEMENT_FOR_ACTIVE) {
    lastMovementMs = now;
  }

  uint8_t targetBlend = (now - lastMovementMs) >= IDLE_TIMEOUT_MS ? 255 : 0;
  idleBlend += (int16_t)(targetBlend - idleBlend) >> IDLE_BLEND_SMOOTH_SHIFT;

  uint8_t targetHue = changeMag > 0 ? movementToHue(dx, dy, dz) : hue;
  uint8_t targetBrightness = constrain(
    map(changeMag, 0, MAX_CHANGE_FOR_BRIGHTNESS, IDLE_BRIGHT, MAX_BRIGHT),
    IDLE_BRIGHT,
    MAX_BRIGHT
  );

  hue += (int8_t)(targetHue - hue) >> HUE_SMOOTH_SHIFT;
  brightness += (int16_t)(targetBrightness - brightness) >> BRIGHT_SMOOTH_SHIFT;

  uint8_t reactiveSpeed = constrain(
    map(changeMag, 0, MAX_CHANGE_FOR_BRIGHTNESS, WAVE_MIN_SPEED, WAVE_MAX_SPEED),
    WAVE_MIN_SPEED,
    WAVE_MAX_SPEED
  );
  uint8_t waveSpeed = scale8(reactiveSpeed, 255 - idleBlend) + scale8(IDLE_WAVE_SPEED, idleBlend);
  wavePhase += waveSpeed;

  uint8_t idleBrightness = IDLE_ANIM_BRIGHT;

  for (uint16_t i = 0; i < LED_COUNT; i++) {
    CRGB reactive = reactivePixel(i, hue, brightness);
    CRGB idle = idlePixel(i, idleBrightness);
    leds[i] = blend(reactive, idle, idleBlend);
  }
}

void showAnimationFrame() {
  FastLED.show();
}
