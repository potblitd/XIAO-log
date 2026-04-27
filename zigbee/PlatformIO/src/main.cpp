#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

// ─── Libraries
// ────────────────────────────────────────────────────────────────
#include "Zigbee.h"
#include <Arduino.h>
#include <BH1750.h>
#include <SensirionI2cSht4x.h>
#include <Wire.h>

// ─── Zigbee endpoints
// ─────────────────────────────────────────────────────────
#define TEMP_SENSOR_ENDPOINT_NUMBER 10
#define ANALOG_DEVICE_ENDPOINT_NUMBER 1

// ─── Timing
// ───────────────────────────────────────────────────────────────────
#define uS_TO_S_FACTOR 1000000ULL // µs → s
#define TIME_TO_SLEEP 300         // deep-sleep duration (seconds)
#define JOIN_TIMEOUT_MS 60000     // max time to join network (ms)
#define POST_JOIN_DELAY_MS 3000   // settle time after joining (ms)
#define REPORT_WAIT_MS 10000      // max wait for report ACKs (ms)
#define KEEPALIVE_EXTRA_S 60      // extra margin on top of sleep (seconds)

// ─── Pins
// ─────────────────────────────────────────────────────────────────────
const uint8_t PIN_LED = LED_BUILTIN;
const uint8_t PIN_BUTTON = BOOT_PIN;
const uint8_t PIN_SENSOR_PWR = D10;

// ─── Zigbee objects
// ───────────────────────────────────────────────────────────
ZigbeeTempSensor zbTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);
ZigbeeAnalog zbAnalogLux(ANALOG_DEVICE_ENDPOINT_NUMBER);

// ─── Sensor objects
// ───────────────────────────────────────────────────────────
SensirionI2cSht4x sht40;
BH1750 bh1750;

// ─── Report-tracking state
// ──────────────────────────────────────────────────── Each successful report
// call decrements the counter; we wait until it reaches 0 (or the
// REPORT_WAIT_MS timeout) before sleeping.
volatile int8_t pendingReports = 0;
volatile bool needResend = false;

uint8_t scanI2CDevices(TwoWire &wire) {
  uint8_t found = 0;
  Serial.println("I2C scan start:");
  for (uint8_t addr = 1; addr < 127; addr++) {
    wire.beginTransmission(addr);
    uint8_t err = wire.endTransmission();
    if (err == 0) {
      Serial.printf("  - device at 0x%02X\r\n", addr);
      found++;
    }
  }
  Serial.printf("I2C scan done, found %u device(s)\r\n", found);
  return found;
}

// ─── Helpers
// ──────────────────────────────────────────────────────────────────
void goToSleep() {
  Serial.printf("Going to sleep for %d seconds.\r\n", TIME_TO_SLEEP);
  Serial.flush();
  esp_deep_sleep_start();
}

// ─── Zigbee response callbacks
// ────────────────────────────────────────────────
void onTempSensorResponse(zb_cmd_type_t command, esp_zb_zcl_status_t status) {
  Serial.printf("[TempSensor] cmd=%d status=%s\r\n", command,
                esp_zb_zcl_status_to_name(status));
  if (command == ZB_CMD_REPORT_ATTRIBUTE) {
    if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
      pendingReports--;
    } else if (status == ESP_ZB_ZCL_STATUS_FAIL) {
      needResend = true;
    }
  }
}

void onAnalogLuxResponse(zb_cmd_type_t command, esp_zb_zcl_status_t status) {
  Serial.printf("[AnalogLux]  cmd=%d status=%s\r\n", command,
                esp_zb_zcl_status_to_name(status));
  if (command == ZB_CMD_REPORT_ATTRIBUTE) {
    if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
      pendingReports--;
    } else if (status == ESP_ZB_ZCL_STATUS_FAIL) {
      needResend = true;
    }
  }
}

// ─── Send readings and wait for ACKs ─────────────────────────────────────────
void sendReports(float temp, float humi, uint16_t lux) {
  zbTempSensor.setTemperature(temp);
  zbTempSensor.setHumidity(humi);
  zbAnalogLux.setAnalogInput((float)lux);

  Serial.print("Temp: ");
  Serial.println(temp);

  Serial.print("Humi: ");
  Serial.println(humi);

  Serial.print("Lux: ");
  Serial.println(lux);

  const int maxRetries = 3;
  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    pendingReports = 2; // expecting one ACK from each endpoint
    needResend = false;

    Serial.printf("Sending reports (attempt %d/%d)...\r\n", attempt,
                  maxRetries);
    zbTempSensor.report();
    zbAnalogLux.reportAnalogInput();

    unsigned long t0 = millis();
    while (pendingReports > 0 && (millis() - t0) < REPORT_WAIT_MS) {
      if (needResend) {
        Serial.println("Coordinator signalled failure – resending.");
        needResend = false;
        pendingReports = 2;
        zbTempSensor.report();
        zbAnalogLux.reportAnalogInput();
        t0 = millis(); // reset timeout after resend
      }
      delay(100);
    }

    if (pendingReports <= 0) {
      Serial.println("All reports acknowledged.");
      return;
    }

    Serial.printf("Report attempt %d timed out (pending=%d).\r\n", attempt,
                  pendingReports);
  }

  Serial.println("All retries exhausted – sleeping anyway.");
}

// ─── Setup
// ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\r\n=== XIAO HAT Zigbee sensor starting ===");

  // LED off (active-low on XIAO)
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // Boot button → factory-reset Zigbee if held at startup
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  if (digitalRead(PIN_BUTTON) == LOW) {
    Serial.println("Boot button held – performing Zigbee factory reset!");
    Zigbee.factoryReset(); // clears stored network credentials
  }

  // Configure deep-sleep wakeup timer now; used in all exit paths
  esp_sleep_enable_timer_wakeup((uint64_t)TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // ── I2C & sensors ──────────────────────────────────────────────────────────
  // XIAO Logger HAT powers SHT40/BH1750 through D10.
  pinMode(PIN_SENSOR_PWR, OUTPUT);
  digitalWrite(PIN_SENSOR_PWR, HIGH);
  delay(40); // let sensor rail settle

  Wire.begin(SDA, SCL);
  Serial.printf("I2C started on SDA=%u SCL=%u, sensor power pin D10=%u\r\n",
                SDA, SCL, PIN_SENSOR_PWR);
  uint8_t i2cFound = scanI2CDevices(Wire);
  if (i2cFound == 0) {
    // Some board revisions or custom hats can invert enable logic.
    Serial.println("No I2C device found with D10=HIGH, trying D10=LOW...");
    digitalWrite(PIN_SENSOR_PWR, LOW);
    delay(40);
    i2cFound = scanI2CDevices(Wire);
    if (i2cFound > 0) {
      Serial.println("I2C devices found with D10=LOW; keeping LOW as sensor "
                     "power enable.");
    } else {
      Serial.println(
          "Still no I2C devices found. Check soldering/wiring/power.");
    }
  }

  // SHT40 temperature & humidity
  sht40.begin(Wire, SHT40_I2C_ADDR_44);
  sht40.softReset();
  delay(10);

  float temp = 0.0f, humi = 0.0f;
  delay(20);
  int16_t shtError = sht40.measureLowestPrecision(temp, humi);
  if (shtError != 0) {
    // Retry with alternate SHT4x address.
    sht40.begin(Wire, SHT40_I2C_ADDR_45);
    sht40.softReset();
    delay(10);
    shtError = sht40.measureLowestPrecision(temp, humi);
  }
  if (shtError != 0) {
    char errMsg[64];
    errorToString(shtError, errMsg, sizeof(errMsg));
    Serial.printf("SHT40 error: %s – will report 0 values.\r\n", errMsg);
    temp = 0.0f;
    humi = 0.0f;
    // Do NOT return here; continue so the device can still sleep properly
  } else {
    Serial.printf("SHT40 → Temp: %.2f °C  Humi: %.1f %%\r\n", temp, humi);
  }

  // BH1750 illuminance
  bool bhOk = bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);
  if (!bhOk) {
    // Some BH1750 boards are strapped to 0x5C.
    bhOk = bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C, &Wire);
  }

  uint16_t lux = 0;
  if (!bhOk) {
    Serial.println(
        "BH1750 init failed at both 0x23 and 0x5C – will report 0 lux.");
  } else {
    delay(180); // first conversion needs integration time
    float luxRaw = bh1750.readLightLevel();
    if (luxRaw < 0) {
      Serial.printf("BH1750 read error: %.1f – will report 0 lux.\r\n", luxRaw);
    } else {
      lux = (uint16_t)luxRaw;
    }
  }
  Serial.printf("BH1750 → Lux: %u\r\n", lux);

  // ── Zigbee configuration ───────────────────────────────────────────────────
  zbTempSensor.setManufacturerAndModel("Z-XIAO", "HAT 02");
  zbTempSensor.setMinMaxValue(-20, 80);
  zbTempSensor.setTolerance(1);
  zbTempSensor.addHumiditySensor(0, 100, 1);
  zbTempSensor.onDefaultResponse(onTempSensorResponse);

  zbAnalogLux.addAnalogInput();
  zbAnalogLux.setAnalogInputApplication(ESP_ZB_ZCL_AI_COUNT_UNITLESS_OTHER);
  zbAnalogLux.setAnalogInputDescription("Illuminance");
  zbAnalogLux.setAnalogInputResolution(1);
  zbAnalogLux.onDefaultResponse(onAnalogLuxResponse);

  Zigbee.addEndpoint(&zbTempSensor);
  Zigbee.addEndpoint(&zbAnalogLux);

  // Keep-alive must comfortably exceed the sleep period so the coordinator
  // doesn't mark us as offline between wake-ups.
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive =
      (TIME_TO_SLEEP + KEEPALIVE_EXTRA_S) * 1000;

  Zigbee.setTimeout(10000); // 10 s to start Zigbee stack

  Serial.println("Starting Zigbee stack...");
  if (!Zigbee.begin(&zigbeeConfig, false)) {
    Serial.println("Zigbee failed to start – rebooting.");
    ESP.restart();
  }
  Serial.println("Zigbee stack started.");

  // ── Network join ──────────────────────────────────────────────────────────
  Serial.print("Joining network");
  unsigned long joinStart = millis();
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(300);
    if (millis() - joinStart > JOIN_TIMEOUT_MS) {
      Serial.println("\r\nJoin timed out – rebooting.");
      ESP.restart();
    }
  }
  Serial.println("\r\nConnected to Zigbee network.");

  // Give the coordinator a moment to fully register the device
  delay(POST_JOIN_DELAY_MS);

  // ── Report sensor values ──────────────────────────────────────────────────
  sendReports(temp, humi, lux);

  // ── Sleep ─────────────────────────────────────────────────────────────────
  goToSleep();
}

// ─── Loop
// ─────────────────────────────────────────────────────────────────────
void loop() {
  // Everything runs in setup(); the device never reaches loop().
}
