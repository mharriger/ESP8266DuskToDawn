# ESP8266 Dusck to Dawn LED Controller

This project is an ESP8266 LED strip controller built using PlatformIO. It is designed to control an LED light strip, turning it on only when it is dark outside.

## Features

- Automatic time synchronization using NTP
- Calculate sunrise and sunset times based on location
- Turns LEDs on at sunset, off at sunrise
- PWM dimming to prolong LED life by reducing temperature

## Hardware Requirements

- Adafruit ESP8266 HUZZAH module (could be adapted to other ESP modules)
- 12v LED strip
- 12v power source for LED strip
- 5v power source for HUZZAH module

## Getting Started

1. **Clone the repository:**
    ```bash
    git clone <repository-url>
    cd ESP8266DuskToDawn
    ```

2. **Open with PlatformIO:**
    - Use [PlatformIO IDE](https://platformio.org/) or the PlatformIO CLI.

3. **Configure your environment:**
    - Edit `platformio.ini` to match your board and environment.

4. **Build and upload:**
    ```bash
    pio run --target upload
    ```

## Configuration

- Compiling with -DENABLE_BUTTON_OVERRIDE will allow a momentary pushbutton on GPIO 0 to toggle the light on or off, overriding the schedule.
- Edit src/config.h to set location, time zone, wifi credentials.

## License

This project is licensed under the MIT License.
