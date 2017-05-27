// http://www.esp8266.com/viewtopic.php?f=32&t=11614

#include <Arduino.h>

const int LED_RED_PIN   = 15;
const int LED_GREEN_PIN = 12;
const int LED_BLUE_PIN  = 13;

void setup()
{
  pinMode(LED_GREEN_PIN, OUTPUT);

  Serial.begin(115200);
  Serial.println("setup");

  pinMode(D1, INPUT_PULLUP);
  pinMode(D5, INPUT_PULLUP);

}

void loop()
{
  Serial.print(millis());
  Serial.print(": D1=");
  Serial.print(digitalRead(D1));
  Serial.print(" D5=");
  Serial.print(digitalRead(D5));
  Serial.println();

  digitalWrite(LED_GREEN_PIN, HIGH);
  delay(500);
  digitalWrite(LED_GREEN_PIN, LOW);
  delay(2500);
}
