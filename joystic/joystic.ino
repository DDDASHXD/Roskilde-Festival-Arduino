/*
 * Joystick + NeoPixel control sketch
 *
 * Connection guide:
 *   Joystick GND  -> Arduino GND
 *   Joystick +5V  -> Arduino 5V
 *   Joystick VRx  -> Arduino A0
 *   Joystick VRy  -> Arduino A1
 *   Joystick SW   -> Arduino D2
 *
 *   NeoPixel GND  -> Arduino GND
 *   NeoPixel 5V   -> Arduino 5V
 *   NeoPixel DIN  -> Arduino D6
 *
 * Behavior:
 *   Left/right changes the NeoPixel hue.
 *   Up/down changes the NeoPixel brightness.
 *   Pressing the joystick button toggles the NeoPixel off/on.
 *
 * Library needed:
 *   Adafruit NeoPixel
 */

#include <Adafruit_NeoPixel.h>

const uint8_t JOYSTICK_X_PIN = A0;
const uint8_t JOYSTICK_Y_PIN = A1;
const uint8_t JOYSTICK_SW_PIN = 2;
const uint8_t NEOPIXEL_PIN = 6;
const uint8_t NEOPIXEL_COUNT = 1;

const int ANALOG_MIN = 0;
const int ANALOG_MAX = 1023;
const unsigned long DEBOUNCE_MS = 150;

Adafruit_NeoPixel pixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

bool isPixelOn = true;
bool lastButtonReading = HIGH;
unsigned long lastToggleTime = 0;

void updatePixel() {
  int xValue = analogRead(JOYSTICK_X_PIN);
  int yValue = analogRead(JOYSTICK_Y_PIN);

  uint16_t hue = map(xValue, ANALOG_MIN, ANALOG_MAX, 0, 65535);
  uint8_t brightness = map(yValue, ANALOG_MIN, ANALOG_MAX, 0, 255);

  // Many joystick modules read lower values when pushed up, so invert so up = brighter.
  brightness = 255 - brightness;

  uint32_t color = isPixelOn
    ? pixel.gamma32(pixel.ColorHSV(hue, 255, brightness))
    : pixel.Color(0, 0, 0);

  pixel.setPixelColor(0, color);
  pixel.show();
}

void handleButton() {
  bool buttonReading = digitalRead(JOYSTICK_SW_PIN);
  unsigned long now = millis();

  if (lastButtonReading == HIGH && buttonReading == LOW && now - lastToggleTime > DEBOUNCE_MS) {
    isPixelOn = !isPixelOn;
    lastToggleTime = now;
  }

  lastButtonReading = buttonReading;
}

void setup() {
  pinMode(JOYSTICK_SW_PIN, INPUT_PULLUP);

  pixel.begin();
  pixel.clear();
  pixel.show();
}

void loop() {
  handleButton();
  updatePixel();
  delay(20);
}
