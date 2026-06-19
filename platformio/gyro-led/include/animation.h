#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "accelerometer.h"

void animationBegin();
void drawAnimationFrame(const AccelReading &reading);
void showAnimationFrame();
