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
#include "Adafruit_SSD1306.h"
#include <SimpleTimer.h>

// #define SERIAL_VERBOSE

// Access point credentials
const char *AP_SSID = "Pukpukus Master";
const char *AP_PWD  = "pukpukpuk";

bool CABIN_1_CLOSED = false;
bool CABIN_2_CLOSED = false;
uint16_t CABIN_1_CLOSED_COUNTER = 0;
uint16_t CABIN_2_CLOSED_COUNTER = 0;
bool CABIN_1_WAS_OPENED = true;
bool CABIN_2_WAS_OPENED = true;
bool ALL_DOORS_CLOSED = false;

// SCL GPIO5
// SDA GPIO4
#define OLED_RESET 255  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);
#if (SSD1306_LCDHEIGHT != 48)
  #error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

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
API:
/
/clients
/update?cabin1=1|0cabin2=1|0
 */



void setup() {
  setupDisplay();

  display.clearDisplay();
  display.setTextSize(1); // 6 rows, 10 cols
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("Pukpukus\n");
  display.print("setup\n");
  display.display();

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
  display.print("- Matrix\n");
  display.display();
  #ifdef SERIAL_VERBOSE
    Serial.println("Matrix test");
  #endif
  matrixSelfTest();
  delay(500);
  #ifdef SERIAL_VERBOSE
    Serial.println("Matrix self test finished");
  #endif

  #ifdef SERIAL_VERBOSE
    Serial.println("Configuring access point...");
  #endif

  // WiFi.softAP(AP_SSID);
  display.print("- AP\n");
  display.display();
  WiFi.softAP(AP_SSID, AP_PWD);

  // IPAddress myIP = WiFi.softAPIP();
  #ifdef SERIAL_VERBOSE
    Serial.print("Server IP address: ");
    Serial.println(WiFi.softAPIP());
  #endif

  setupServerHandlers();
  display.print("- HTTP\n");
  display.display();
  server.begin();
  #ifdef SERIAL_VERBOSE
    Serial.println("HTTP server started");
  #endif

  setupTimers();

  #ifdef SERIAL_VERBOSE
    Serial.println();
  #endif

  display.print("setup done");
  display.display();
  delay(2000);
}

void setupServerHandlers() {
  server.on("/",        handleStatus);
  server.on("/devices", handleDevices);
  server.on("/update",  handleUpdate);
  server.onNotFound(handleNotFound);
}

void setupDisplay() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // 0x3C or 0x3D
  display.clearDisplay();
  display.display();
}

void setupMatrix() {
   matrix.begin(0x70); // 0x70 - 0x77
   matrix.setBrightness(12); // 0 - 15, with 1 too much high freq noice from holtek
}

void matrixSelfTest() {
  matrix.clear();
  matrix.fillRect(0, 0, 8, 8, LED_GREEN);
  matrix.writeDisplay();
  delay(500);
  matrix.fillRect(0, 0, 8, 8, LED_YELLOW);
  matrix.writeDisplay();
  delay(500);
  matrix.fillRect(0, 0, 8, 8, LED_RED);
  matrix.writeDisplay();
  delay(500);
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

  uint8_t cabinsTotal = 2;
  uint8_t cabinsTotalClosed = 0;
  String response;

  if (server.arg("cabin1") == "1") {
    CABIN_1_CLOSED = true;
    cabinsTotalClosed++;
    if (CABIN_1_WAS_OPENED) {
      CABIN_1_WAS_OPENED = false;
      // reset counter as we have limited space to show
      if (CABIN_1_CLOSED_COUNTER == 999) {
        CABIN_1_CLOSED_COUNTER = 0;
      }
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
      if (CABIN_2_CLOSED_COUNTER == 999) {
        CABIN_2_CLOSED_COUNTER = 0;
      }
      CABIN_2_CLOSED_COUNTER++;
    }
  } else {
    CABIN_2_CLOSED = false;
    CABIN_2_WAS_OPENED = true;
  }

  #ifdef SERIAL_VERBOSE
    Serial.printf("CABIN-1: %d", CABIN_1_CLOSED ? 1 : 0);
    Serial.println();
    Serial.printf("CABIN-2: %d", CABIN_2_CLOSED ? 1 : 0);
    Serial.println();
  #endif

  if (cabinsTotal == cabinsTotalClosed) {
    ALL_DOORS_CLOSED = true;
  } else {
    ALL_DOORS_CLOSED = false;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Pukpukus\n");
  display.printf("\nC1: %s %d", (CABIN_1_CLOSED) ? "CL" : "OP", CABIN_1_CLOSED_COUNTER);
  display.printf("\nC2: %s %d", (CABIN_2_CLOSED) ? "CL" : "OP", CABIN_2_CLOSED_COUNTER);
  if (ALL_DOORS_CLOSED) {
    display.printf("\n\n%d occupied", cabinsTotalClosed);
  } else {
    display.printf("\n\n%d free", cabinsTotal - cabinsTotalClosed);
  }
  display.display();

  if (ALL_DOORS_CLOSED) {
    response = "occupied";
    matrixSetColor(LED_RED);
  } else {
    response = "free";
    matrixSetColor(LED_GREEN);
  }

  server.send(200, "text/plain", response);

  timerMatrix.enable(timerMatrixId);
  matrixLedOffXIndex = 0;
  matrixLedOffYIndex = 0;

  // matrixLedLocations = {
  //   11, 12, 13, 14, 15, 16, 17, 18,
  //   21, 22, 23, 24, 25, 26, 27, 28,
  //   31, 32, 33, 34, 35, 36, 37, 38,
  //   41, 42, 43, 44, 45, 46, 47, 48,
  //   51, 52, 53, 54, 55, 56, 57, 58,
  //   61, 62, 63, 64, 65, 66, 67, 68,
  //   71, 72, 73, 74, 75, 76, 77, 78,
  //   81, 82, 83, 84, 85, 86, 87, 88
  // };
  // matrixLedLocationsCurrentIndex = 0;
  // matrixLedLocations = shuffleArray(matrixLedLocations, sizeof(matrixLedLocations));
}

void handleStatus() {
  #ifdef SERIAL_VERBOSE
    Serial.println("handleStatus");
  #endif

  String response = (ALL_DOORS_CLOSED) ? "occupied" : "free";
  char buffer[16];
  sprintf(buffer, "\nC1: %s %d", (CABIN_1_CLOSED) ? "CL" : "OP", CABIN_1_CLOSED_COUNTER);
  response+= buffer;
  sprintf(buffer, "\nC2: %s %d", (CABIN_2_CLOSED) ? "CL" : "OP", CABIN_2_CLOSED_COUNTER);
  response+= buffer;

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

// https://forum.arduino.cc/index.php?topic=345964.0
void shuffleArray(int * array, int size) {
  randomSeed(analogRead(A0));

  int last = 0;
  int temp = array[last];
  for (int i = 0; i < size; i++) {
    int index = random(size);
    array[last] = array[index];
    last = index;
  }
  array[last] = temp;
}

void showOnMatrixSinceUpdated() {
  // String target = (String) matrixLedLocations[matrixLedLocationsCurrentIndex];
  // matrixLedLocationsCurrentIndex++;

  matrix.drawPixel(matrixLedOffXIndex, matrixLedOffYIndex, LED_YELLOW);
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
