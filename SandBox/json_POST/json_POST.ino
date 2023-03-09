#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "credentials.h"

WiFiClientSecure client;

// Open AI endpoint
const char* openAPIendPoint = "https://api.openai.com/v1/completions";
const char* server = "api.openai.com";
const int port = 443;

void setup() {

  Serial.begin(9600);
  delay(1000);
  Serial.println("Start Setup");

  WiFi.mode(WIFI_STA);  //The WiFi is in station mode. The    other is the softAP mode
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected to: ");
  Serial.println(ssid);
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  client.setCACert(rootCACertificate);  //Only communicate with the server if the CA certificates match
  delay(2000);

  Serial.println("End Setup");
}

void loop() {

  StaticJsonDocument<96> doc;

  doc["model"] = "gpt-3.5-turbo";
  JsonObject messages_0 = doc["messages"].createNestedObject();
  messages_0["role"] = "user";
  messages_0["content"] = "Are you alive?";

  Serial.println(server);
  int conn = client.connect(server, port);

  if (conn == 1) {
    Serial.println();
    Serial.println("Sending Parameters...");
    //Request
    client.println("POST https://api.openai.com/v1/chat/completions HTTP/1.1");
    //Headers
    client.print("Host: ");
    client.println(server);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(measureJson(doc));
    client.print("Authorization: ");
    client.println(openAI_Private_key);
    client.println("Connection: Close");
    client.println();
    //Body
    serializeJson(doc, client);
    client.println();

    //Wait for server response
    while (client.available() == 0)
      ;

    // Stream& input;
    client.find("\r\n\r\n");
    StaticJsonDocument<80> filter;

    JsonObject filter_choices_0_message = filter["choices"][0].createNestedObject("message");
    filter_choices_0_message["role"] = true;
    filter_choices_0_message["content"] = true;

    StaticJsonDocument<1400> outputDoc;
    DeserializationError error = deserializeJson(outputDoc, client, DeserializationOption::Filter(filter));

    client.stop();

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    const char* choices_0_message_role = outputDoc["choices"][0]["message"]["role"];        // "assistant"
    const char* choices_0_message_content = outputDoc["choices"][0]["message"]["content"];  // "\n\nArduino is a ...

    Serial.println(choices_0_message_role);
    Serial.println(choices_0_message_content);

  } else {
    client.stop();
    Serial.println("Connection Failed");
  }


  delay(50000);
}
