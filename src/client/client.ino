#include "Arduino.h"
#include "ESP8266WiFi.h"

// #define SERIAL_VERBOSE
// #define SERIAL_PRINT_MASTER_RESPONSE

const uint32_t UPDATE_INTERVAL = 5000;

const int LDR_PIN       = A0; // ADC
const int BUTTON_PIN    =  4; // ?
const int LED_GREEN_PIN = 12; // D6 = GPIO15
const int LED_RED_PIN   = 15; // D8 = GPIO12
const int LED_BLUE_PIN  = 13; // D7 = GPIO13
const int CABIN_1_PIN    =  5; // D1 = GPIO5
const int CABIN_2_PIN    = 14; // D5 = GPIO14

const char* WLAN_SSID = "Pukpukus Master";
const char* WLAN_PWD  = "pukpukpuk";

const char* MASTER_HOST = "192.168.4.1";
    uint8_t MASTER_PORT = 80;

void setup() {
  #ifdef SERIAL_VERBOSE
    Serial.begin(115200);
    // Serial.setDebugOutput(true);
    delay(3000);
    Serial.println();
  #endif

  setupPins();

  // bring red to live
  analogWrite(LED_RED_PIN, 1023);

  #ifdef SERIAL_VERBOSE
    Serial.print("Connecting to WLAN: ");
    Serial.println(WLAN_SSID);
  #endif

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.disconnect();
  WiFi.setAutoConnect(true);
  WiFi.mode(WIFI_STA);

  // https://github.com/esp8266/Arduino/issues/2186
  // WiFi.setOutputPower(0);
  // WiFi.begin(ssid);
  WiFi.begin(WLAN_SSID, WLAN_PWD);

  // https://github.com/esp8266/Arduino/issues/119
  while (WiFi.status() != WL_CONNECTED) {
    blinkRed(true);
    #ifdef SERIAL_VERBOSE
      Serial.print(".");
    #endif
    delay(1000);

    // WiFi.begin(WLAN_SSID);
    // WiFi.begin(WLAN_SSID, WLAN_PWD);

  //  WiFi.printDiag(Serial);
  }
  #ifdef SERIAL_VERBOSE
    Serial.println();
  #endif

  #ifdef SERIAL_VERBOSE
    Serial.println("WiFi connected!");

    Serial.print("We got IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println();
    Serial.println();
  #endif
}

void setupPins() {
  pinMode(LDR_PIN,       INPUT);
  pinMode(BUTTON_PIN,    INPUT);

  pinMode(LED_RED_PIN,   OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN,  OUTPUT);

  pinMode(CABIN_1_PIN,    INPUT_PULLUP);
  pinMode(CABIN_2_PIN,    INPUT_PULLUP);
}



void loop() {
  if (checkWifiStatus() != WL_CONNECTED) {
    delay(2000);
    return;
  }

  #ifdef SERIAL_VERBOSE
    Serial.print("Connecting to ");
    Serial.println(MASTER_HOST);
  #endif

  WiFiClient client;
  if (!client.connect(MASTER_HOST, MASTER_PORT)) {
    #ifdef SERIAL_VERBOSE
      Serial.println("connection failed");
    #endif
    return;
  }

  // #ifdef SERIAL_VERBOSE
  //   Serial.print("LDR: ");
  //   Serial.print(analogRead(LDR_PIN));
  //   Serial.print(", PULLDOWN_BUTTON: ");
  //   Serial.print(digitalRead(BUTTON_PIN));
  //   Serial.println();
  // #endif

  // bool DOOR_IS_CLOSED = false;
  // if (analogRead(LDR_PIN) < 300) {
  //   DOOR_IS_CLOSED = true;
  // }

  // open circuit is internally pulled-up, so we are looking for pulled-down signal
  uint8_t cabin_1_Level = digitalRead(CABIN_1_PIN);
  uint8_t cabin_2_Level = digitalRead(CABIN_2_PIN);
  #ifdef SERIAL_VERBOSE
    Serial.printf("CABIN-1=%d, CABIN-2=%d", cabin_1_Level, cabin_2_Level);
    Serial.println();
  #endif

  // blink how many cabins are closed
  if (cabin_1_Level == LOW) {
    blinkRed(false);
    delay(100);
  }
  if (cabin_2_Level == LOW) {
    blinkRed(false);
  }

  // We now create a URI for the request
  String url = "/update?";
  url += "cabin1=";
  url += (cabin_1_Level == LOW) ? "1" : "0";
  url += "&cabin2=";
  url += (cabin_2_Level == LOW) ? "1" : "0";

  #ifdef SERIAL_VERBOSE
    Serial.print("Requesting URL: ");
    Serial.println(url);
  #endif

  // start of the request
  analogWrite(LED_BLUE_PIN, 1023);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + MASTER_HOST + "\r\n" +
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      #ifdef SERIAL_VERBOSE
        Serial.println(">>> Client Timeout");
      #endif
      client.stop();
      analogWrite(LED_BLUE_PIN, 0);
      return;
    }
  }

  // end of the request
  analogWrite(LED_BLUE_PIN, 0);

  #ifdef SERIAL_PRINT_MASTER_RESPONSE
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    Serial.println();
  #endif

  #ifdef SERIAL_VERBOSE
    Serial.println();
  #endif

  delay(UPDATE_INTERVAL);
}

uint8_t checkWifiStatus() {
  blinkRed(true);
  #ifdef SERIAL_VERBOSE
    Serial.print("WIFI status: ");
  #endif

  uint8_t wifiStatus = WiFi.status();

  // https://www.arduino.cc/en/Reference/WiFiStatus
  switch (wifiStatus) {
    case WL_IDLE_STATUS:
      #ifdef SERIAL_VERBOSE
        Serial.println("WL_IDLE_STATUS");
      #endif
      break;

    case WL_CONNECTED:
      #ifdef SERIAL_VERBOSE
        Serial.println("WL_IDLE_STATUS");
      #endif
      analogWrite(LED_RED_PIN,    0);
      analogWrite(LED_GREEN_PIN, 50);
      break;

    case WL_NO_SSID_AVAIL:
      #ifdef SERIAL_VERBOSE
        Serial.println("WL_NO_SSID_AVAIL");
      #endif
      analogWrite(LED_RED_PIN,   1023);
      analogWrite(LED_GREEN_PIN,    0);
      break;

    case WL_CONNECT_FAILED:
      #ifdef SERIAL_VERBOSE
        Serial.println("WL_CONNECT_FAILED");
      #endif
      analogWrite(LED_RED_PIN,   1023);
      analogWrite(LED_GREEN_PIN,    0);
      break;

    case WL_CONNECTION_LOST:
      #ifdef SERIAL_VERBOSE
        Serial.println("WL_CONNECTION_LOST");
      #endif
      analogWrite(LED_RED_PIN,   1023);
      analogWrite(LED_GREEN_PIN,    0);
      break;

    case WL_DISCONNECTED:
      #ifdef SERIAL_VERBOSE
        Serial.println("WL_DISCONNECTED");
      #endif
      analogWrite(LED_RED_PIN,   1023);
      analogWrite(LED_GREEN_PIN,    0);
      break;
  }

  return wifiStatus;
}

void blinkRed(bool initialHigh) {
  analogWrite(LED_RED_PIN,  initialHigh ? 0 : 1023);
  delay(50);
  analogWrite(LED_RED_PIN, !initialHigh ? 0 : 1023);
}
