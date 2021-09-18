// Credentials
#include "./Credentials/Blynk.h"
#include "./Credentials/Wifi.h"

// Configurations
#include "./Configuration/Blynk.h"

// Libraries
#include <BlynkSimpleEsp32.h>  // Part of Blynk by Volodymyr Shymanskyy
#include <WiFi.h>              // Part of WiFi Built-In by Arduino
#include <WiFiClient.h>        // Part of WiFi Built-In by Arduino
#include <math.h>
#include <pthread.h>

// GPIO pins
const unsigned short int rightLightPWM = 19;
const unsigned short int leftLightPWM = 16;
const unsigned short int rightLightEnable = 18;
const unsigned short int leftLightEnable = 17;

// setting PWM properties
const unsigned short int leftLightPwmChannel = 3;
const unsigned short int rightLightPwmChannel = 6;
const unsigned short int lightsPwmFrequency = 40000;  // higher frequency -> less flickering
const unsigned short int lightsPwmResolution = 10;    // 10 Bit = 1024 (2^10) for Duty Cycle (0 to 1023)

// Object State
int leftLightState = 1;
int rightLightState = 1;
int leftLightBrightness = 512;
int rightLightBrightness = 512;

// Connection State
String IpAddress = "";
String MacAddress = "";

// Counters
const short maxWifiReconnctAttempts = 5;
const short maxBlynkReconnectAttempts = 5;

short wifiReconnectCounter = 0;
short blynkReconnectCounter = 0;

int blynkConnectionTimeout = 10000;
int wifiConnectionTimeout = 10000;

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  SetupGpio(leftLightEnable, rightLightEnable, leftLightPWM, rightLightPWM, leftLightPwmChannel, rightLightPwmChannel, lightsPwmFrequency, lightsPwmResolution);
  setInitialStateOfLights();
}

// ----------------------------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------------------------

void loop() {
  ConnectToWifi(WIFI_SSID, WIFI_PW);
  ConnectToBlynk();
  UpdateIpAddressInBlynk();
  UpdateMacAddressInBlynk();
  Blynk.run();
}

// ----------------------------------------------------------------------------
// FUNCTIONS
// ----------------------------------------------------------------------------

// Blynk Functions

BLYNK_CONNECTED() {  // Restore hardware pins according to current UI config
  Blynk.syncAll();
}

BLYNK_WRITE(V1) {  // Both lights state
  int pinValue = param.asInt();
  leftLightState = pinValue;
  rightLightState = pinValue;
  digitalWrite(leftLightEnable, pinValue == 0 ? LOW : HIGH);
  digitalWrite(rightLightEnable, pinValue == 0 ? LOW : HIGH);
  Blynk.virtualWrite(V3, pinValue == 0 ? 0 : 1);
  Blynk.virtualWrite(V5, pinValue == 0 ? 0 : 1);
}

BLYNK_WRITE(V2) {  // Both lights brightness (slider)
  int pinValue = param.asInt();
  leftLightBrightness = pinValue;
  rightLightBrightness = pinValue;
  ledcWrite(leftLightPwmChannel, percentToValue(pinValue, 1023));
  ledcWrite(rightLightPwmChannel, percentToValue(pinValue, 1023));
  Blynk.virtualWrite(V4, pinValue);
  Blynk.virtualWrite(V6, pinValue);
  Blynk.virtualWrite(V7, pinValue);
  Blynk.virtualWrite(V8, pinValue);
  Blynk.virtualWrite(V9, pinValue);
}

BLYNK_WRITE(V7) {  // Both lights brightness (stepper)
  int pinValue = param.asInt();
  leftLightBrightness = pinValue;
  rightLightBrightness = pinValue;
  ledcWrite(leftLightPwmChannel, percentToValue(pinValue, 1023));
  ledcWrite(rightLightPwmChannel, percentToValue(pinValue, 1023));
  Blynk.virtualWrite(V2, pinValue);
  Blynk.virtualWrite(V4, pinValue);
  Blynk.virtualWrite(V6, pinValue);
  Blynk.virtualWrite(V8, pinValue);
  Blynk.virtualWrite(V9, pinValue);
}

BLYNK_WRITE(V3) {  //  Left light state
  int pinValue = param.asInt();
  leftLightState = pinValue;
  digitalWrite(leftLightEnable, pinValue == 0 ? LOW : HIGH);
  Blynk.virtualWrite(V1, rightLightState == 0 ? 0 : 1);
  if (pinValue == 0) Blynk.virtualWrite(V1, 0);
}

BLYNK_WRITE(V4) {  // Left light brightness (slider)
  int pinValue = param.asInt();
  leftLightBrightness = pinValue;
  ledcWrite(leftLightPwmChannel, percentToValue(pinValue, 1023));
  Blynk.virtualWrite(V8, pinValue);
}

BLYNK_WRITE(V8) {  // Left light brightness (stepper)
  int pinValue = param.asInt();
  leftLightState = pinValue;
  ledcWrite(leftLightPwmChannel, percentToValue(pinValue, 1023));
  Blynk.virtualWrite(V4, pinValue);
}

BLYNK_WRITE(V5) {  // Right light state
  int pinValue = param.asInt();
  rightLightState = pinValue;
  digitalWrite(rightLightEnable, pinValue == 0 ? LOW : HIGH);
  Blynk.virtualWrite(V1, leftLightState == 0 ? 0 : 1);
  if (pinValue == 0) Blynk.virtualWrite(V1, 0);
}

BLYNK_WRITE(V6) {  // Right light brightness (slider)
  int pinValue = param.asInt();
  rightLightBrightness = pinValue;
  ledcWrite(rightLightPwmChannel, percentToValue(pinValue, 1023));
  Blynk.virtualWrite(V9, pinValue);
}

BLYNK_WRITE(V9) {  // Right light brightness (stepper)
  int pinValue = param.asInt();
  rightLightState = pinValue;
  ledcWrite(rightLightPwmChannel, percentToValue(pinValue, 1023));
  Blynk.virtualWrite(V6, pinValue);
}

// General functions

void WaitForWifi(int cycleDelayInMilliSeconds) {
  while (WiFi.status() != WL_CONNECTED) {
    delay(cycleDelayInMilliSeconds);
  }
}

void WaitForBlynk(int cycleDelayInMilliSeconds) {
  while (!Blynk.connected()) {
    delay(cycleDelayInMilliSeconds);
  }
}

void ConnectToWifi(const char* ssid, const char* pass) {
  if (!WiFi.isConnected()) {
    Serial.printf("Connecting to Wifi: %s\n", ssid);
    try {
      WiFi.begin(ssid, pass);  // initial begin as workaround to some bug
      WiFi.disconnect();
      WiFi.begin(ssid, pass);
      WiFi.setHostname("Desklight (ESP32, Blynk)");
      WaitForWifi(wifiConnectionTimeout);
      if (!WiFi.isConnected()) wifiReconnectCounter++;
      if (wifiReconnectCounter >= maxWifiReconnctAttempts) {
        Serial.printf("%d attempts failed to connect to wifi, rebooting..\n", wifiReconnectCounter);
        ESP.restart();
      }
    } catch (const std::exception e) {
      Serial.printf("Error occured: %s\n", e.what());
    }
    Serial.printf("Connected to Wifi: %s\n", ssid);
    blinkLights(2, 50, 50);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
  }
}

void ConnectToBlynk() {
  if (!Blynk.connected()) {
    if (BLYNK_USE_LOCAL_SERVER)
      Blynk.begin(BLYNK_AUTH, WIFI_SSID, WIFI_PW, BLYNK_SERVER, BLYNK_PORT);
    else
      Blynk.begin(BLYNK_AUTH, WIFI_SSID, WIFI_PW);
    WaitForBlynk(blynkConnectionTimeout);
  }
}

void UpdateIpAddressInBlynk() {
  if (IpAddress != WiFi.localIP().toString()) {
    IpAddress = WiFi.localIP().toString();
    Blynk.virtualWrite(V10, IpAddress);
  }
}

void UpdateMacAddressInBlynk() {
  if (MacAddress != WiFi.macAddress()) {
    MacAddress = WiFi.macAddress();
    Blynk.virtualWrite(V11, MacAddress);
  }
}

void blinkLights(short count, short onTime, short offTime) {
  short counter = 0;
  while (counter <= count) {
    digitalWrite(leftLightEnable, LOW);
    digitalWrite(rightLightEnable, LOW);
    delay(offTime);
    digitalWrite(leftLightEnable, HIGH);
    digitalWrite(rightLightEnable, HIGH);
    delay(counter >= count ? 0 : onTime);
    counter++;
  }
  delay(1000);  // small break, so blinks can be distinguished
}

void SetupGpio(unsigned short int leftLightEnablePin, unsigned short int rightLightEnablePin, unsigned short int leftLightPwmPin, unsigned short int rightLightPwmPin,
               unsigned short int leftLightPwmChannel, unsigned short int rightLightPwmChannel, unsigned short int lightsPwmFrequency, unsigned short int lightsPwmResolution) {
  // GPIO Setup
  ledcSetup(leftLightPwmChannel, lightsPwmFrequency, lightsPwmResolution);
  ledcSetup(rightLightPwmChannel, lightsPwmFrequency, lightsPwmResolution);
  ledcAttachPin(leftLightPwmPin, leftLightPwmChannel);
  ledcAttachPin(rightLightPwmPin, rightLightPwmChannel);
  pinMode(leftLightEnablePin, OUTPUT);
  pinMode(rightLightEnablePin, OUTPUT);
}

void setInitialStateOfLights() {
  // Turn light on initially with 50% brightness
  ledcWrite(leftLightPwmChannel, percentToValue(50, 1023));
  ledcWrite(rightLightPwmChannel, percentToValue(50, 1023));
  digitalWrite(rightLightEnable, HIGH);
  digitalWrite(leftLightEnable, HIGH);
}

int percentToValue(int percent, int maxValue) { return 0 <= percent <= 100 ? round((maxValue / 100) * percent) : 1023; }
