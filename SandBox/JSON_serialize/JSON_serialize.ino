#include <ArduinoJson.h>

#define BUFFER_LENGTH 400

void setup() {

  Serial.begin(9600);
  delay(1000);
  Serial.println("Start Setup");

  StaticJsonDocument<192> doc;

  doc["model"] = "text-davinci-003";
  doc["prompt"] = "Say this is a test";
  doc["temperature"] = 0;
  doc["max_tokens"] = 7;

  char output[BUFFER_LENGTH] = {};

  serializeJson(doc, output);
  // serializeJson(doc, output, BUFFER_LENGTH);

  Serial.print("output array -> ");
  Serial.println(output);

  Serial.print("JSON output -> ");
  serializeJson(doc, Serial);
  Serial.println(" ");

  Serial.println("End Setup");
}

void loop() {
}
