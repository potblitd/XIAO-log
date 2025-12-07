#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

// import necessary libraries
#include <Wire.h>
#include <Arduino.h>
#include "Zigbee.h"
#include <SensirionI2cSht4x.h>
#include <BH1750.h>

// zigbee configs from https://docs.espressif.com/projects/arduino-esp32/en/latest/libraries.html#zigbee-apis
#define TEMP_SENSOR_ENDPOINT_NUMBER 10
// using analog device to transfer the lux value
#define ANALOG_DEVICE_ENDPOINT_NUMBER 1
// Conversion factor for micro seconds to seconds
#define uS_TO_S_FACTOR 1000000ULL
// Sleep for 5 minutes
#define TIME_TO_SLEEP 300
// Timeout for response from coordinator in ms
#define REPORT_TIMEOUT 2000 
// Set to 0 to use local callback specified directly for the endpoint.
#define USE_GLOBAL_ON_RESPONSE_CALLBACK 1  
uint8_t dataToSend = 2;
bool resend = false; 

ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);
ZigbeeAnalog zbAnalogLux = ZigbeeAnalog(ANALOG_DEVICE_ENDPOINT_NUMBER);

// define pin connections with XIAO board
uint8_t led = LED_BUILTIN;
uint8_t button = BOOT_PIN;
const int ADC_pin = A1;

// setup temp & hum sensor
SensirionI2cSht4x sht40;
static char errorMessage[64];
static int16_t error;
// setup light sensor
BH1750 bh1750;

/************************ Callbacks *****************************/
#if USE_GLOBAL_ON_RESPONSE_CALLBACK
void onGlobalResponse(zb_cmd_type_t command, esp_zb_zcl_status_t status, uint8_t endpoint, uint16_t cluster) {
  Serial.printf("Global response command: %d, status: %s, endpoint: %d, cluster: 0x%04x\r\n", command, esp_zb_zcl_status_to_name(status), endpoint, cluster);
  if ((command == ZB_CMD_REPORT_ATTRIBUTE) && (endpoint == TEMP_SENSOR_ENDPOINT_NUMBER)) {
    switch (status) {
      case ESP_ZB_ZCL_STATUS_SUCCESS: dataToSend--; break;
      case ESP_ZB_ZCL_STATUS_FAIL:    resend = true; break;
      default:                        break;  // add more statuses like ESP_ZB_ZCL_STATUS_INVALID_VALUE, ESP_ZB_ZCL_STATUS_TIMEOUT etc.
    }
  }
}
#else
void onResponse(zb_cmd_type_t command, esp_zb_zcl_status_t status) {
  Serial.printf("Response command: %d, status: %s\r\n", command, esp_zb_zcl_status_to_name(status));
  if (command == ZB_CMD_REPORT_ATTRIBUTE) {
    switch (status) {
      case ESP_ZB_ZCL_STATUS_SUCCESS: dataToSend--; break;
      case ESP_ZB_ZCL_STATUS_FAIL:    resend = true; break;
      default:                        break;  // add more statuses like ESP_ZB_ZCL_STATUS_INVALID_VALUE, ESP_ZB_ZCL_STATUS_TIMEOUT etc.
    }
  }
}
#endif

/********************* Arduino functions **************************/
void setup() {
  
  Serial.begin(115200);
  Serial.println("Zigbee device start");

  // Configure builtin LED and turn it OFF (HIGH)
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);
  // setup ADC battery voltage pin
  pinMode(ADC_pin, INPUT);

  // initialize I2C communication
  Wire.begin();

  // init SHT40 sensor
  sht40.begin(Wire, SHT40_I2C_ADDR_44);
  sht40.softReset();
  delay(10);
  uint32_t serialNumber = 0;
  error = sht40.serialNumber(serialNumber);
  if (error != 0) {
      Serial.print("Error trying to execute serialNumber(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }
  Serial.print("serialNumber: ");
  Serial.print(serialNumber);
  Serial.println();

  // get sht40 measurements
  float temp = 0.0;
  float humi = 0.0;

  delay(20);
  error = sht40.measureLowestPrecision(temp, humi);
  if (error != 0) {
      Serial.print("Error trying to execute measureLowestPrecision(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.print("\t");
  Serial.print("Humidity: ");
  Serial.print(humi);
  Serial.println();

  // initialize BH1750 sensor
  bh1750.begin();

  // Get luminosity from bh1750 sensor
  uint16_t lux = bh1750.readLightLevel();
  // print results
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println();

  // Get battery voltage
  uint32_t Vbat = 0;
  for(int i = 0; i < 10; i++) {
    Vbat = Vbat + analogReadMilliVolts(ADC_pin);
    delay(20);
  }
  float Vbatf = 2 * Vbat / 10 / 1000.0;
  float Vbatp = (Vbatf - 3) * (100) / (1.2);
  if (Vbatp < 0) { Vbatp = 0;
  } else if (Vbatp > 100) { Vbatp = 100;}
  // print results
  Serial.print("Battery voltage: ");
  Serial.print(Vbatf, 3);
  Serial.print(" V");
  Serial.println("\t");
  Serial.print("Battery percentage: ");
  Serial.print(Vbatp, 1);
  Serial.println(" %");

  // Configure the wake up source and set to wake up every 30 minutes
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  //set tempsensor zigbee settings
  zbTempSensor.setManufacturerAndModel("Z-XIAO", "HAT");
  zbTempSensor.setMinMaxValue(-20, 80);
  zbTempSensor.setTolerance(1);
  zbTempSensor.addHumiditySensor(0, 100, 1);
  // Set up analog input for lux value
  zbAnalogLux.addAnalogInput();
  zbAnalogLux.setAnalogInputApplication(ESP_ZB_ZCL_AI_COUNT_UNITLESS_OTHER);
  zbAnalogLux.setAnalogInputDescription("Illuminance");
  zbAnalogLux.setAnalogInputResolution(1);

#if USE_GLOBAL_ON_RESPONSE_CALLBACK
  // Global callback for all endpoints with more params to determine the endpoint and cluster in the callback function.
  Zigbee.onGlobalDefaultResponse(onGlobalResponse);
#else
  // Callback specified for endpoint
  zbTempSensor.onDefaultResponse(onResponse);
#endif

  //Add endpoints to Zigbee Core
  Zigbee.addEndpoint(&zbTempSensor);
  Zigbee.addEndpoint(&zbAnalogLux);
  // Create a default Zigbee configuration for End Device
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 10000;
  // Set timeout for Zigbee Begin to 10s (default is 30s)
  Zigbee.setTimeout(10000);  
  Serial.println("Starting Zigbee...");

  // When all EPs are registered, start Zigbee. By default acts as ZIGBEE_END_DEVICE
  if (!Zigbee.begin(&zigbeeConfig, false)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  } else {
    Serial.println("Zigbee started successfully!");
  }

  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.println("Successfully connected to Zigbee network");
  // Delay approx 1s (may be adjusted) to allow establishing proper connection with coordinator, needed for sleepy devices
  delay(1000);

  // Update temperature and humidity values in Temperature sensor EP
  zbTempSensor.setTemperature(temp);
  zbTempSensor.setHumidity(humi);
  //zbTempSensor.setBatteryPercentage(Vbatp);
  //zbTempSensor.setBatteryVoltage(Vbatf*10);
  zbAnalogLux.setAnalogInput(lux);
  // Report values
  zbTempSensor.report();
  delay(100);
  //zbTempSensor.reportBatteryPercentage();
  zbAnalogLux.reportAnalogInput();
  
  unsigned long startTime = millis();
  const unsigned long timeout = REPORT_TIMEOUT;

  Serial.printf("Waiting for data report to be confirmed \r\n");
  // Wait until data was successfully sent
  int tries = 0;
  const int maxTries = 5;
  while (dataToSend != 0 && tries < maxTries) {
    if (resend) {
      Serial.println("Resending data on failure!");
      resend = false;
      dataToSend = 2;
      zbTempSensor.report();  // report again
      zbAnalogLux.reportAnalogInput();
    }
    if (millis() - startTime >= timeout) {
      Serial.println("\nReport timeout! Report Again");
      dataToSend = 2;
      zbTempSensor.report();  // report again
      zbAnalogLux.reportAnalogInput();
      startTime = millis();
      tries++;
    }
    Serial.printf(".");
    delay(100);  // 50ms delay to avoid busy-waiting
  }

  // Put device to deep sleep
  Serial.printf("Going to sleep for %d seconds\r\n", TIME_TO_SLEEP);
  esp_deep_sleep_start();
}

void loop() {
  // nothing here
}
