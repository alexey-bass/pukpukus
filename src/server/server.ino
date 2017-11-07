// https://github.com/aderhgawen/ESP8266-Webserver

extern "C" {
  #include "user_interface.h"
}

#include "Arduino.h"
#include "SPI.h"
#include "Wire.h"
#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"
#include "Adafruit_GFX.h"
#include "Adafruit_LEDBackpack.h"
#include <SimpleTimer.h>
#include <ArduinoJson.h>

// #define SERIAL_VERBOSE

const char *WLAN_SSID = "Pukpukus";
const char *WLAN_PWD  = "********";
// NETWORK: Static IP details
// @see https://forum.arduino.cc/index.php?topic=460595.0
IPAddress staticIp(      1, 2, 3, 4);
IPAddress defaultGateway(1, 2, 3, 1);
IPAddress netmask(255, 255, 255, 0);

bool CABIN_1_CLOSED = false;
bool CABIN_2_CLOSED = false;
uint32_t CABIN_1_CLOSED_COUNTER = 0;
uint32_t CABIN_2_CLOSED_COUNTER = 0;
bool CABIN_1_WAS_OPENED = true;
bool CABIN_2_WAS_OPENED = true;
uint8_t CABIN_TOTAL_COUNT = 2; // we got total 2 cabins

uint32_t TS_UPTIME_PREVIOUS = 0;
uint32_t TS_UPTIME_OVERLAPS = 0;
uint32_t TS_LAST_UPDATED = 0;
uint32_t TS_LAST_UPDATED_OVERLAPS = 0;

// need this to correct "burn" effect
uint8_t CURRENT_MATRIX_COLOR = 0;

// https://www.adafruit.com/product/902
Adafruit_BicolorMatrix matrix = Adafruit_BicolorMatrix();

ESP8266WebServer server(80);

SimpleTimer timerMatrix;
uint8_t timerMatrixId;
// int matrixLedLocations[64];
// int matrixLedLocationsCurrentIndex;
uint8_t matrixLedOffXIndex = 0;
uint8_t matrixLedOffYIndex = 0;

/**
    APIs:
    /
    /clients
    /update?cabin1=1|0&cabin2=1|0
    /status
 */



void setup() {
  #ifdef SERIAL_VERBOSE
    Serial.begin(115200);
    // Serial.setDebugOutput(true);
    delay(1000);
    Serial.println();
    Serial.println();
  #endif

  #ifdef SERIAL_VERBOSE
    Serial.println("Setting up matrix");
  #endif
  setupMatrix();
  #ifdef SERIAL_VERBOSE
    Serial.println("Matrix test");
  #endif
  matrixSelfTest();
  delay(500);
  #ifdef SERIAL_VERBOSE
    Serial.println("Matrix self test finished");
  #endif

  #ifdef SERIAL_VERBOSE
    Serial.println("Connecting to WIFI...");
  #endif

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.disconnect();
  WiFi.setAutoConnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.config(staticIp, defaultGateway, netmask);
  WiFi.begin(WLAN_SSID, WLAN_PWD);

  // IPAddress myIP = WiFi.softAPIP();
  #ifdef SERIAL_VERBOSE
    Serial.print("Server IP address: ");
    Serial.println(WiFi.softAPIP());
  #endif

  setupServerHandlers();
  server.begin();
  #ifdef SERIAL_VERBOSE
    Serial.println("HTTP server started");
  #endif

  setupTimers();

  #ifdef SERIAL_VERBOSE
    Serial.println();
  #endif

  delay(2000);
}

void setupServerHandlers() {
  server.on("/",        handleDefault);
  server.on("/devices", handleDevices);
  server.on("/update",  handleUpdate);
  server.on("/status",  handleStatus);
  server.onNotFound(handleNotFound);
}

void setupMatrix() {
   matrix.begin(0x70); // 0x70 - 0x77
   matrix.setBrightness(13); // 0 - 15, with 1 too much high freq noice from holtek
}

void matrixSelfTest() {
  matrix.clear();
  int8_t color = 0;
  for (int8_t i = 1; i < 4; i++) {
    switch (i) {
      case 1: color = LED_GREEN;  break;
      case 2: color = LED_YELLOW; break;
      case 3: color = LED_RED;    break;
    }
    matrix.fillRect(0, 0, 8, 8, color);
    matrix.writeDisplay();
    delay(500);
  }
  matrix.fillRect(0, 0, 8, 8, 0);
  matrix.writeDisplay();
}

void setupTimers() {
  timerMatrixId = timerMatrix.setInterval(1000L, showOnMatrixSinceUpdated);
  timerMatrix.disable(timerMatrixId);
}

void loop() {
  server.handleClient();
  timerMatrix.run();
}



void handleDevices() {
  #ifdef SERIAL_VERBOSE
    Serial.println("handleDevices");
  #endif

  String response;
  struct station_info *stat_info = wifi_softap_get_station_info();

  while (stat_info != NULL) {
    char   station_mac[18] = {0}; sprintf(station_mac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(stat_info->bssid));
    String station_ip      = IPAddress((&stat_info->ip)->addr).toString();

    response+= station_mac;
    response+= " ";
    response+= station_ip;
    if (station_ip == server.client().remoteIP().toString()) {
      response+= " *";
    }
    response+= "\n";

    stat_info = STAILQ_NEXT(stat_info, next);
  }
  wifi_softap_free_station_info();

  server.send(200, "text/plain", response);
}

void handleUpdate() {
  #ifdef SERIAL_VERBOSE
    Serial.println("handleUpdate");
  #endif

  timerMatrix.disable(timerMatrixId);

  uint8_t cabinsTotalClosed = 0;

  if (server.arg("cabin1") == "1") {
    CABIN_1_CLOSED = true;
    cabinsTotalClosed++;
    if (CABIN_1_WAS_OPENED) {
      CABIN_1_WAS_OPENED = false;
      CABIN_1_CLOSED_COUNTER++;
    }
  } else {
    CABIN_1_CLOSED = false;
    CABIN_1_WAS_OPENED = true;
  }

  if (server.arg("cabin2") == "1") {
    CABIN_2_CLOSED = true;
    cabinsTotalClosed++;
    if (CABIN_2_WAS_OPENED) {
      CABIN_2_WAS_OPENED = false;
      CABIN_2_CLOSED_COUNTER++;
    }
  } else {
    CABIN_2_CLOSED = false;
    CABIN_2_WAS_OPENED = true;
  }

  // empty respose was bad while testing via browser, so just "ok"
  server.send(200, "text/plain", "ok");

  TS_LAST_UPDATED = millisWithOverlap();
  TS_LAST_UPDATED_OVERLAPS = TS_UPTIME_OVERLAPS;

  #ifdef SERIAL_VERBOSE
    Serial.printf("CABIN-1: %d", CABIN_1_CLOSED ? 1 : 0);
    Serial.println();
    Serial.printf("CABIN-2: %d", CABIN_2_CLOSED ? 1 : 0);
    Serial.println();
  #endif

  switch (CABIN_TOTAL_COUNT - cabinsTotalClosed) {
    case 0: // all closed
      CURRENT_MATRIX_COLOR = LED_RED;
      matrixSetColor(CURRENT_MATRIX_COLOR);
      break;

    case 1: // one opened
      CURRENT_MATRIX_COLOR = LED_YELLOW;
      matrixSetColor(CURRENT_MATRIX_COLOR);
      break;

    default: // otherwise
      CURRENT_MATRIX_COLOR = LED_GREEN;
      matrixSetColor(CURRENT_MATRIX_COLOR);
      break;
  }

  timerMatrix.enable(timerMatrixId);
  matrixLedOffXIndex = 0;
  matrixLedOffYIndex = 0;
}

void handleDefault() {
  #ifdef SERIAL_VERBOSE
    Serial.println("handleDefault");
  #endif

  String response;
  char buffer[16];
  sprintf(buffer, "\nC1: %s %d", (CABIN_1_CLOSED) ? "CL" : "OP", CABIN_1_CLOSED_COUNTER);
  response+= buffer;
  sprintf(buffer, "\nC2: %s %d", (CABIN_2_CLOSED) ? "CL" : "OP", CABIN_2_CLOSED_COUNTER);
  response+= buffer;

  server.send(200, "text/plain", response);
}

void handleStatus() {
  #ifdef SERIAL_VERBOSE
    Serial.println("handleStatus");
  #endif

  String response;

  // https://bblanchon.github.io/ArduinoJson/assistant/
  StaticJsonBuffer<400> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  JsonArray& states = root.createNestedArray("states");

  JsonObject& c1 = states.createNestedObject();
  c1["name"] = "c1";
  c1["is-open"] = !CABIN_1_CLOSED;
  c1["counter-closed"] = CABIN_1_CLOSED_COUNTER;

  JsonObject& c2 = states.createNestedObject();
  c2["name"] = "c2";
  c2["is-open"] = !CABIN_2_CLOSED;
  c2["counter-closed"] = CABIN_2_CLOSED_COUNTER;

  root["uptime"]          = millisWithOverlap();
  root["uptime-overlaps"] = TS_UPTIME_OVERLAPS;

  root["last-updated"]          = TS_LAST_UPDATED;
  root["last-updated-overlaps"] = TS_LAST_UPDATED_OVERLAPS;

  root.printTo(response);
  server.send(200, "text/plain", response);
}

void handleNotFound() {
  #ifdef SERIAL_VERBOSE
    Serial.println("handleNotFound");
  #endif

  String message = "404 - Not Found\n";

  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nURI: ";
  message += server.uri();

  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

void matrixDisplayTest() {
  matrix.clear();
  for (int8_t y = 0; y < 8; y++) {
    for (int8_t x = 7; x > -1; x--) {
      matrix.drawPixel(y, x, LED_GREEN);
      matrix.writeDisplay();
      delay(20);
    }
  }
  for (int8_t y = 0; y < 8; y++) {
    for (int8_t x = 7; x > -1; x--) {
      matrix.drawPixel(y, x, LED_YELLOW);
      matrix.writeDisplay();
      delay(20);
    }
  }
  for (int8_t y = 0; y < 8; y++) {
    for (int8_t x = 7; x > -1; x--) {
      matrix.drawPixel(y, x, LED_RED);
      matrix.writeDisplay();
      delay(20);
    }
  }
  for (int8_t y = 0; y < 8; y++) {
    for (int8_t x = 7; x > -1; x--) {
      matrix.drawPixel(y, x, 0); // LED_OFF not working, but 0 yes
      matrix.writeDisplay();
      delay(20);
    }
  }
}

void matrixSetColor(uint8_t color) {
  for (int8_t y = 0; y < 8; y++) {
   for (int8_t x = 7; x > -1; x--) {
     matrix.drawPixel(y, x, color);
     matrix.writeDisplay();
     delay(5);
   }
  }
}

void showOnMatrixSinceUpdated() {
  // String target = (String) matrixLedLocations[matrixLedLocationsCurrentIndex];
  // matrixLedLocationsCurrentIndex++;

  matrix.drawPixel(matrixLedOffXIndex, matrixLedOffYIndex, CURRENT_MATRIX_COLOR == LED_YELLOW ? LED_RED : LED_YELLOW);
  matrix.writeDisplay();
  delay(15);
  matrix.drawPixel(matrixLedOffXIndex, matrixLedOffYIndex, 0);
  matrix.writeDisplay();

  matrixLedOffXIndex++;
  if (matrixLedOffXIndex > 7) {
    matrixLedOffXIndex = 0;
    matrixLedOffYIndex++;
  }

  if (matrixLedOffYIndex > 7) {
    timerMatrix.disable(timerMatrixId);
  }
}

uint32_t millisWithOverlap() {
  uint32_t uptime = millis();
  if (TS_UPTIME_PREVIOUS > uptime) {
    TS_LAST_UPDATED_OVERLAPS++;
  }
  TS_UPTIME_PREVIOUS = uptime;
  return uptime;
}
