# Roskilde Festival Interactive Exhibition

This repository documents the Arduino and PlatformIO work behind an interactive festival
sculpture made for the course _Interactive Design in the Field_. The project was built
around a simple brief of making an interactive exhibition piece that could stand near
the main stage at Roskilde Festival if the final result was reliable enough.

The theme for the course was _Magical Mystery Forest_. Our response was a large mushroom
mounted on a pole roughly four meters tall. Under the cap, eight soft arms hang down
like tentacles. Each arm contains an LED strip and reacts when people move it. The idea
is direct enough, but the wiring and sensor choices turned out to matter more than we
first expected.

## What This Repository Contains

- `platformio/gyro-led`: the PlatformIO firmware project for one mushroom arm
- `platformio/gyro-led/src`: the Arduino source files
- `platformio/gyro-led/include`: shared configuration and header files
- `arduino-holder`: 3D files for the holder that keeps eight Arduino boards in one
  manageable structure

The current firmware is written for an Arduino Uno, a GY-85 accelerometer module, and
WS2812B LED strips driven through FastLED. One firmware instance is meant to run one arm
of the sculpture.

## The Short Version

Each tentacle has its own Arduino, its own accelerometer, and its own LED strip. That
may sound excessive at first. We started with the more obvious plan: one Arduino reading
several sensors and controlling several LED strips. In practice, the GY-85 modules
created an address problem on the I2C bus. The accelerometer chips we had were exposed
at the same address, so several identical modules could not be read cleanly from one
Arduino without extra work.

We tried software I2C as a workaround. It did give us input from more than one device,
but it also made the system feel fragile for the hardware we were using. The Arduino
still had to drive hundreds of LEDs, and memory was already tight. After some testing,
the less elegant solution became the more stable one: give each tentacle a separate
Arduino and keep every sensor on its own small circuit.

That decision solved the I2C problem. It created another one. Eight loose boards inside
a power and cable box would be hard to service, especially during a festival
installation where repairs need to be quick. The 3D printed holder in `arduino-holder`
was made for that reason. It keeps the boards organized and lets us remove one without
disturbing the whole system.

## Firmware Behavior

The firmware reads acceleration from the ADXL345 part of the GY-85 module. It then
compares the current reading with the previous reading and uses that difference to shape
the LED animation.

Movement changes two visible properties:

- brightness increases when the arm moves more
- hue shifts according to changes across the x, y, and z axes

When the arm has been still for a short period, the strip blends into an idle rainbow
wave. The transition is smoothed so the animation does not snap between states. If
sensor reads fail repeatedly, the code tries to recover the sensor instead of stopping
the animation entirely.

The active configuration lives in `platformio/gyro-led/include/config.h`. At the time of
writing, the project is configured for:

- Arduino Uno
- two LED data pins, pins `5` and `6`
- `300` WS2812B LEDs in the shared LED buffer
- serial monitor speed `115200`
- FastLED as the LED library

## Wiring Notes

The firmware comments in `main.cpp` use this wiring for one arm:

```text
                 one tentacle / one controller

              +-----------------------------+
              |         Arduino Uno         |
              |                             |
              |  A4 / SDA  -----------------+------ SDA   GY-85 / ADXL345
              |  A5 / SCL  -----------------+------ SCL   accelerometer
              |  5         -----------------+------ DIN   WS2812B strip A
              |  6         -----------------+------ DIN   WS2812B strip B
              |  GND       -----------------+------ GND   sensor and LEDs
              +-----------------------------+

                         external LED power
              +5V  -------------------------------- +5V   WS2812B strips
              GND  -------------------------------- GND   WS2812B strips
```

Each of the eight arms repeats this basic layout with its own Arduino, sensor, and LED
output. The shared point is the larger power system, not the I2C bus.

````

Power wiring is not fully documented in this repository. Treat that as an important
limitation. A strip with this many LEDs needs a power setup designed for the actual
current draw, wire length, and installation conditions.

## Working With the Code

The Arduino code is kept as a PlatformIO project rather than a single Arduino IDE
sketch. That made it easier to split the code into small files:

- `main.cpp` handles startup, sensor recovery, and the main loop
- `accelerometer.cpp` detects and reads the ADXL345
- `animation.cpp` converts movement into LED output
- `config.h` keeps pins, LED counts, brightness limits, and timing values in one place

From the PlatformIO project directory, the usual workflow is:

```sh
cd platformio/gyro-led
pio run
pio run --target upload
pio device monitor
````

The project uses the `uno` environment in `platformio.ini`. FastLED is declared there as
a dependency.

## Why the Design Looks Like This

This project is partly a sculpture and partly a record of hardware compromise. The
original plan was cleaner on paper. One controller, several sensors, and a shared LED
setup would have reduced the number of boards. But the actual modules we had did not
support that plan well, and forcing the architecture around them would have made the
installation harder to trust.

So the final structure is repetitive by design. One arm has one controller, one sensor,
and one LED output path. If an arm fails, the failure is local. If we need to change
animation values, the same firmware can be uploaded to each board. The result is not the
smallest possible system, but it is easier to reason about under festival conditions
(which matters more than neatness here).

## Repository Status

This repository is a project record, not a complete build manual. The code and 3D holder
files are included, but some physical details remain outside the repo: final power
distribution, mounting details, weather protection, and the full festival installation
setup. Those details depend on the actual sculpture and site conditions.

Still, the central technical lesson is clear enough: when a physical interaction depends
on many repeated parts, local simplicity can beat centralized elegance. For this
mushroom, eight small Arduino systems were easier to build, debug, and repair than one
larger system trying to coordinate every tentacle at once.
