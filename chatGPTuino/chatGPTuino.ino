#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "credentials.h"
#include <ArduinoJson.h>
#include <PS2KeyAdvanced.h>

// Pins for PS/2 keyboard (through USB)
#define DATAPIN 6  // (USB Data -)  (PS2 pin 1)
#define IRQPIN 5   // (USB Data +)  (PS2 pin 5)
PS2KeyAdvanced keyboard;

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// Open AI endpoint
const char* openAPIendPoint = "https://api.openai.com/v1/completions";
const char* server = "api.openai.com";

// Blinking Interval
#define CURSOR_BLINK_INTERVAL 250

// States
#define GET_USER_INPUT 0
#define GET_REPONSE 1
#define DISPLAY_RESPONSE 2


// Display settings
#define LINE_HEIGHT 10
#define SCREEN_HEIGHT 64
#define SCREEN_WIDTH 128
#define FONT_WIDTH 6
#define FONT_HEIGHT 13
#define CHAT_DISPLAY_POS_START FONT_HEIGHT * 2

const int MAX_CHAR_PER_LINE = SCREEN_WIDTH / FONT_WIDTH;
const int MAX_LINES = SCREEN_HEIGHT / FONT_HEIGHT;
const int INPUT_BUFFER_LENGTH = 40;

// A buffer to hold the user input----------------012345678901234567890
//char chatBuffer[MAX_LINES * 2][MAX_CHAR_PER_LINE + 1] = {};
char inputBuffer[INPUT_BUFFER_LENGTH] = {};
const char* chatBuffer = ">                   <";

// OLED Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE);  // High speed I2C

// // A secure wifi client
// WiFiClientSecure client;

void setup(void) {

  Serial.begin(9600);
  delay(1000);
  Serial.println("ChatGPTuino");
  Serial.println("Setup Started...");

  keyboard.begin(DATAPIN, IRQPIN);
  keyboard.setNoBreak(1);   // No break codes for keys (when key released)
  keyboard.setNoRepeat(1);  // Don't repeat shift ctrl etc

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("WiFi connected to IP address: ");
  Serial.println(WiFi.localIP());

  // Connect securely to Server


  // Set up display
  u8g2.begin();
  //https://github.com/olikraus/u8g2/wiki/fntlist12
  u8g2.setFont(u8g2_font_6x13_tf);  // choose a suitable

  Serial.println("...Setup Ended");
}

void loop(void) {


  static DynamicJsonDocument doc(1024);

  //Track state
  static byte state = GET_USER_INPUT;

  //Track input buffer index
  static byte inputIndex = 0;
  static boolean inputBufferFull = false;
  boolean bufferChange = false;

  /*********** GET USER INPUT ***********************************************/
  // If input and buffer not full, assign characters to buffer
  // if (Serial.available() && state == GET_USER_INPUT) {
  if (keyboard.available() && state == GET_USER_INPUT) {

    byte input = keyboard.read();
    bufferChange = true;

    if (input == 'E') {
      state = GET_REPONSE;
    }
    // TESTING -> If delete key entered, clear last char and adjust index
    else if (input == 'D') {

      inputBuffer[inputIndex] = ' ';
      inputIndex = inputIndex > 0 ? inputIndex - 1 : 0;
      // inputIndex--;
      inputBufferFull = false;

      Serial.print("DELETE-> ");
      Serial.println(inputIndex);
    }
    // Add chars to inputBuffer if not full
    else if (!inputBufferFull) {
      inputBuffer[inputIndex] = input;
      inputIndex++;

      Serial.print("CREATE-> ");
      Serial.println(inputIndex);
    }

    //If you have reached end of input buffer, display a #
    if (inputIndex >= INPUT_BUFFER_LENGTH && !inputBufferFull) {
      bufferChange = true;
      inputBufferFull = true;

      inputBuffer[INPUT_BUFFER_LENGTH - 1] = '<';

      Serial.print("EXCEEDS-> ");
      Serial.println(inputIndex);
    }
  }

  /*********** GET RESPONSE ***********************************************/
  if (state == GET_REPONSE) {

    Serial.println("Start GET RESPONSE...");

    WiFiClientSecure* client = new WiFiClientSecure;

    if (client) {
      client->setCACert(rootCACertificate);
      {
        // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
        HTTPClient https;
        https.addHeader("Content-Type", "application/json");
        https.addHeader("Authorization", "Bearer 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");

        Serial.print("[HTTPS] begin...\n");
        if (https.begin(*client, openAPIendPoint)) {  // HTTPS
          Serial.print("[HTTPS] POST...\n");
          // start connection and send HTTP header
          // int httpCode = https.POST("{\"model\":\"text-davinci-003\",\"prompt\":\"Answer this question as a pirate - Who goes there?\",\"max_tokens\":88}");
          String temp_input = String(inputBuffer);
          int httpCode = https.POST(String("{\"model\":\"text-davinci-003\",\"prompt\":\"" + temp_input + "\",\"max_tokens\":88}"));

          // httpCode will be negative on error
          if (httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

            // file found at server
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
              String payload = https.getString();
              Serial.println(payload);


              DeserializationError err = deserializeJson(doc, payload);
              if (err) {
                Serial.print(F("deserializeJson() failed with code "));
                Serial.println(err.f_str());
              }

              JsonObject choices_0 = doc["choices"][0];
              chatBuffer = choices_0["text"];  // "\n\nAvast there! Who be callin' out on me?"
              Serial.println(chatBuffer);
              // Serial.println(choices_0_text);
            }
          } else {
            Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
          }

          https.end();
        } else {
          Serial.printf("[HTTPS] Unable to connect\n");
        }

      }  // End extra scoping block

      delete client;
    } else {
      Serial.println("Unable to create client");
    }

    state = GET_USER_INPUT;
    Serial.println("...End GET RESPONSE");
  }

  //Only change display for new input
  if (bufferChange) {
    u8g2.firstPage();
    do {
      u8g2.setCursor(0, FONT_HEIGHT);
      byte start = (inputIndex > MAX_CHAR_PER_LINE) ? inputIndex - MAX_CHAR_PER_LINE : 0;
      Serial.print("Display inputIndex-> ");
      Serial.print(inputIndex);
      Serial.print("  Display start-> ");
      Serial.print(start);
      Serial.print("  input buf ->");
      Serial.println(inputBuffer);

      // Draw input buffer
      for (int i = start; i < MAX_CHAR_PER_LINE + start; i++) {
        u8g2.print(inputBuffer[i]);  // write something to the internal memory
      }

      //Draw Chat buffer
      u8g2.setCursor(0, FONT_HEIGHT * 2);
      // Draw chat buffer
      for (int i = 0; i < MAX_CHAR_PER_LINE; i++) {
        u8g2.print(chatBuffer[i]);  // write something to the internal memory
      }

    } while (u8g2.nextPage());
  }
}


// static byte row = 0;
// static byte column = 0;
// // If input and buffer not full, assign characters to buffer
// if (Serial.available() && !bufferFull) {
//   bufferChange = true;

//   chatBuffer[row][column] = Serial.read();
//   column++;
//   if (column > MAX_CHAR_PER_LINE) {
//     column = 0;
//     row++;
//   }

//   if (row > MAX_LINES) {
//     bufferFull == true;
//   }
// }

//Only change display for new input
// if (bufferChange) {
//   u8g2.firstPage();
//   do {
//     // Draw chat buffer
//     for (int i = 0; i < MAX_LINES; i++) {
//       u8g2.drawStr(0, CHAT_DISPLAY_POS_START + i * FONT_HEIGHT, chatBuffer[i]);  // write something to the internal memory
//     }
//   } while (u8g2.nextPage());
// }
