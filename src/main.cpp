/*
  Dusk to Dawn - ESP8266 (Huzzah)

  This code controls LED lighting using an ESP8266 (Huzzah board).
  It automatically turns the LEDs on at 75% brightness (using PWM and a MOSFET) when it is dark,
  and off during daylight, based on calculated sunrise and sunset times for a configurable location.

  Be sure to set the parameters in config.h

  Features:
    - NTP time synchronization and timezone/DST handling
    - Calculates daily sunrise and sunset using the SunSet library
    - Recalculates times at 5am each day
    - Uses GPIO12 (D6) to drive a MOSFET for LED control
    - PWM dimming to 75% when dark, off when daylight

  Hardware:
    - Adafruit Huzzah ESP8266
    - External MOSFET to switch 12V LED supply (gate driven by GPIO12)
    - LEDs powered by 12V supply

  Wiring Notes:
  - Connect the MOSFET source to ground.
  - Connect the MOSFET drain to the negative side of the LED strip.
  - Connect the positive side of the LED strip to 12V (+).
  - Gate is connected to ESP8266 GPIO12 (D6), and through a small resistor (e.g., 220Ω) to ground.

*/

#define LED_MOSFET_PIN 12 // GPIO12 (D6 on Huzzah ESP8266), not used by default
#define BUTTON_PIN 0 // GPIO0 (button)
#ifdef ENABLE_BUTTON_OVERRIDE
volatile bool led_override = false;
volatile int led_state = 0;
#endif
#define LED_PWM_DUTY 192 // 75% brightness
#include <Arduino.h>

#include <ESP8266WiFi.h>

#include <time.h>
#include <sunset.h>

#include "config.h" // Configurable parameters

const char* TZ_STR = TIMEZONE;
SunSet sun;

time_t sunset_time;
time_t sunrise_time;

int current_pwm_duty = 0;

boolean attemptConnect() {
  int i;

  //Wait for WiFi to connect to AP
  Serial.println("Waiting for WiFi");
  for (i=50; i && (WiFi.status() != WL_CONNECTED); --i) {
    delay(500);
    Serial.print(".");
  }
  return (i != 0);	// return truth of "we did NOT time out"
}

void fadeToBrightness(int targetBrightness, int stepDelay=20); // Forward declaration

#ifdef ENABLE_BUTTON_OVERRIDE
// Interrupt Service Routine for button press
void IRAM_ATTR handleButtonPress() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200) { // debounce 200ms
    led_override = !led_override;
  }
  last_interrupt_time = interrupt_time;
}
#endif

void setup() {

#ifdef ENABLE_BUTTON_OVERRIDE
  // Attach interrupt for button (active low)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);
#endif

  pinMode(LED_MOSFET_PIN, OUTPUT);
  analogWrite(LED_MOSFET_PIN, 0); // Start with LEDs off
#ifdef ENABLE_BUTTON_OVERRIDE
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Button, active low
#endif
  Serial.begin(115200);

  // Wifi credentials from config.h
  const char* ssid = WIFI_SSID;
  const char* password = WIFI_PASSWORD;

  WiFi.mode(WIFI_STA);
  delay(500);
  WiFi.begin(ssid, password);

  if (!attemptConnect()) {
    Serial.println("WiFi connect failed. Restarting...");
    ESP.restart();
  } else {
    Serial.println("WiFi Connected.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }

  configTime(TZ_STR, "pool.ntp.org");
}

bool isDark () {
  time_t tnow;
  tnow = time(nullptr);

  if (tnow >= sunrise_time && tnow < sunset_time) {
    return false;
  } else {
    return true;
  }
}

void fadeToBrightness(int targetBrightness, int stepDelay) {
  if (current_pwm_duty < targetBrightness) {
    Serial.printf("Fading up\n");
    while (current_pwm_duty <= targetBrightness) {
      analogWrite(LED_MOSFET_PIN, current_pwm_duty);
      delay(stepDelay);
      current_pwm_duty++;
    }
  } else if (current_pwm_duty > targetBrightness) {
    Serial.printf("Fading down\n");
    while (current_pwm_duty >= targetBrightness) {
      analogWrite(LED_MOSFET_PIN, current_pwm_duty);
      delay(stepDelay);
      current_pwm_duty--;
    }
  }
}

/*
 * Use the SunSet library to calculate sunrise and sunset times
 * Calculates "official" sunset, when the center of the sun is
 * 0.833 degrees below the horizon
 */
void calcSunriseSunset() {
  time_t tnow;
  tnow = time(nullptr);
  struct tm *t = localtime(&tnow);

  sun.setPosition(LATITUDE, LONGITUDE, t->tm_isdst ? DST_OFFSET : TZ_OFFSET);
  Serial.printf("Calculating sunrise/sunset for date %04d-%02d-%02d\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
  sun.setCurrentDate(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

  double sunrise_minutes = sun.calcSunrise();
  double sunset_minutes = sun.calcSunset();

  Serial.printf("Sunrise at %.2f minutes, Sunset at %.2f minutes\n", sunrise_minutes, sunset_minutes);

  // Convert those into time_t for today
  struct tm sunriseTm = *t;
  sunriseTm.tm_hour = sunrise_minutes / 60;
  sunriseTm.tm_min  = (int) sunrise_minutes % 60;
  sunriseTm.tm_sec  = 0;
  sunrise_time = mktime(&sunriseTm);

  struct tm sunsetTm = *t;
  sunsetTm.tm_hour = sunset_minutes / 60;
  sunsetTm.tm_min  = (int) sunset_minutes % 60;
  sunsetTm.tm_sec  = 0;
  sunset_time = mktime(&sunsetTm);
}

void loop() {
#ifdef ENABLE_BUTTON_OVERRIDE
  Serial.printf("Override: %s, State: %s\n", led_override ? "ON" : "OFF", led_state ? "ON" : "OFF");
  if (led_override) {
    // Print override state for debug
    if (!isDark() && !led_state) {
      Serial.println("LEDs ON (override)");
      fadeToBrightness(LED_PWM_DUTY, 20); // Fade to 75% brightness
      led_state = true;
    } else if (isDark() && led_state) {
      Serial.println("LEDs OFF (override)");
      fadeToBrightness(0, 20); // Fade to 0% brightness
      led_state = false;
    }
  }
#endif
  // Wait for NTP time to be set before first calculation
  static bool time_initialized = false;
  time_t tnow = time(nullptr);
  struct tm *t = localtime(&tnow);

  if (!time_initialized) {
    // Wait until year is at least 2020
    if (t->tm_year + 1900 < 2020) {
      Serial.println("Waiting for NTP time...");
      delay(1000);
      return;
    }
    Serial.println("Initial NTP sync succeeded");
  }

  calcSunriseSunset();

  // Print current, sunrise, and sunset times
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
  Serial.print("Current time: ");
  Serial.println(buf);

  struct tm *sr = localtime(&sunrise_time);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", sr);
  Serial.print("Sunrise: ");
  Serial.println(buf);

  struct tm *ss = localtime(&sunset_time);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ss);
  Serial.print("Sunset: ");
  Serial.println(buf);

  // Compare
  bool is_dark = isDark();
  if (is_dark) {
    Serial.println("It is dark");
    fadeToBrightness(LED_PWM_DUTY, 20); // Fade to 75% brightness
  } else {
    Serial.println("It is daylight");
    fadeToBrightness(0, 20); // Fade to 0% brightness
  }

  delay(60000); // update once per minute
}