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
#define CLEAR_INPUT 3

// Display settings
#define LINE_HEIGHT 10
#define SCREEN_HEIGHT 64
#define SCREEN_WIDTH 128
#define FONT_WIDTH 6
#define FONT_HEIGHT 13
#define CHAT_DISPLAY_POS_START FONT_HEIGHT * 2
#define INPUT_BUFFER_LENGTH 40
#define MAX_CHAT_LINES 20

const int MAX_CHAR_PER_LINE = SCREEN_WIDTH / FONT_WIDTH - 2;  //The minus 2 is a buffer for the right edge of the screen
const int MAX_LINES = SCREEN_HEIGHT / FONT_HEIGHT;
const int CHAT_BUFFER_LENGTH = MAX_CHAR_PER_LINE * MAX_CHAT_LINES;
const int MAX_TOKENS = CHAT_BUFFER_LENGTH / 5;  //https://platform.openai.com/tokenizer


/*************************************************************************/
/*******  GLOBALS  ******************************************************/
/*************************************************************************/

// There are
char inputBuffer[INPUT_BUFFER_LENGTH] = {};
char chatBuffer[CHAT_BUFFER_LENGTH] = {};
unsigned int responseLength;

// OLED Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE);  // High speed I2C

/*************************************************************************/
/*******  FUNCTIONS ******************************************************/
/*************************************************************************/

/*
 * Function:  lengthOfToken 
 * -------------------------
 * computes length of 'word/token' in a character array by measuring distance to next space
 *
 *  startIndex: index to start at
 *  stopIndex: index to end at.  Usually length of charArray
 *  charArray: char array to inspect
 *
 *  returns early: length of next word
 *  returns: -1 if no space is found
 */
int lengthOfToken(int startIndex, int stopIndex, char charArray[]) {

  for (int i = 0; i < stopIndex - startIndex; i++) {
    if (charArray[i + startIndex] == ' ') {
      return i;
    }
  }
  return -1;
}

/*
 * Function:  printUserInput 
 * -------------------------
 * Displays characters from inputBuffer to top line of OLED.
 * Clears buffer when starts.
 *
 *  index: index to start at
 */
void printUserInput(int index) {

  u8g2.clearBuffer();
  u8g2.setCursor(0, FONT_HEIGHT);
  byte start = (index > MAX_CHAR_PER_LINE) ? index - MAX_CHAR_PER_LINE : 0;

  // Draw input buffer
  for (int i = start; i < MAX_CHAR_PER_LINE + start; i++) {
    u8g2.print(inputBuffer[i]);
  }
  u8g2.sendBuffer();
}

/*
 * Function:  extractResponse 
 * -------------------------
 * Used to extract chat response from resturned JSON response 
 *
 *  jsonPayload: JSON response
 *
 *  returns: 
 */
DeserializationError extractResponse(String jsonPayload) {

  // Store and deserialize JSON
  DynamicJsonDocument document(1024);
  DeserializationError err = deserializeJson(document, jsonPayload);

  // Save response to buffer
  JsonObject choices_0 = document["choices"][0];
  String tempBuffer = choices_0["text"];  // Response
  tempBuffer.toCharArray(chatBuffer, CHAT_BUFFER_LENGTH);

  // Record repsonse length
  responseLength = tempBuffer.length();

  // print RAW JSON
  Serial.print("responseLength ->");
  Serial.println(responseLength);

  // print RAW JSON
  Serial.print("chatBufferArray[] -> ");
  Serial.println(chatBuffer);

  return err;
}


/*************************************************************************/
/*******   SETUP    ******************************************************/
/*************************************************************************/

void setup(void) {

  Serial.begin(9600);
  delay(1000);
  Serial.println("ChatGPTuino");
  Serial.println("Setup Started...");

  // Keyboard setup
  keyboard.begin(DATAPIN, IRQPIN);
  keyboard.setNoBreak(1);   // No break codes for keys (when key released)
  keyboard.setNoRepeat(1);  // Don't repeat shift ctrl etc

  // WiFi Setup
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("WiFi connected to IP address: ");
  Serial.println(WiFi.localIP());

  // Display Setup
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x13_tf);  //https://github.com/olikraus/u8g2/wiki/fntlist12

  Serial.println("...Setup Ended");
}

/*************************************************************************/
/*******  LOOP      ******************************************************/
/*************************************************************************/

void loop(void) {

  // static String testBuffer;
  static DynamicJsonDocument doc(1024);

  //Track state
  static byte state = GET_USER_INPUT;

  //Track input buffer index
  static byte inputIndex = 0;


  static boolean inputBufferFull = false;
  static boolean clearInput = false;
  boolean bufferChange = false;
  static boolean printResponse = false;


  /*********** GET USER INPUT ***********************************************/
  // If input and buffer not full, assign characters to buffer
  if (keyboard.available() && state == GET_USER_INPUT) {

    bufferChange = true;

    unsigned int input = keyboard.read();
    input &= 0xFF;  // Get lowest bits

    // Handle Special Keys and text
    switch (input) {
      case PS2_KEY_ENTER:
        state = GET_REPONSE;
        clearInput = true;

        //Clear chat Buffer
        for (int i = 0; i < CHAT_BUFFER_LENGTH; i++) {
          chatBuffer[i] = ' ';
        }

        Serial.println("Enter Key Pressed");
        break;
      case PS2_KEY_BS:  //Backspace Pressed
        inputIndex = inputIndex > 0 ? inputIndex - 1 : 0;
        inputBuffer[inputIndex] = ' ';
        inputBufferFull = false;
        break;
      case PS2_KEY_SPACE:
        inputBuffer[inputIndex] = ' ';
        inputIndex++;
        break;
      default:
        //Clear input when asking next question
        if (clearInput && state == GET_USER_INPUT) {
          for (int i = 0; i < INPUT_BUFFER_LENGTH; i++) {
            inputBuffer[i] = ' ';
          }
          Serial.println("Clear Input");
          inputIndex = 0;  // Return index to start
          clearInput = false;
          inputBufferFull = false;
        }

        if (!inputBufferFull) {
          // inputBuffer[inputIndex] = char(input + 32);  // Make input lower case
          inputBuffer[inputIndex] = char(input);  // Make input lower case
          inputIndex++;
        }
    }

    //If you have reached end of input buffer, display a #
    if (inputIndex >= INPUT_BUFFER_LENGTH && !inputBufferFull) {
      bufferChange = true;
      inputBufferFull = true;

      inputBuffer[INPUT_BUFFER_LENGTH - 1] = '<';
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
        https.addHeader("Authorization", openAI_Private_key);

        Serial.print("[HTTPS] begin...\n");
        if (https.begin(*client, openAPIendPoint)) {  // HTTPS

          Serial.print("[HTTPS] POST...\n");

          // start connection and send HTTP header
          String temp_input = String(inputBuffer);
          
          StaticJsonDocument<192> doc;

          int httpCode = https.POST(String("{\"model\":\"text-davinci-003\",\"prompt\":\"" + temp_input + "\",\"max_tokens\":" + MAX_TOKENS + "}"));

          // httpCode will be negative on error
          if (httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

            // file found at server
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
              String payload = https.getString();

              //Print Raw JSON
              Serial.println(payload);
              DeserializationError err = extractResponse(payload);

              if (err) {
                Serial.print(F("deserializeJson() failed with code "));
                Serial.println(err.f_str());
              }
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
    printResponse = true;
    Serial.println("...End GET RESPONSE");
  }

  /*********** Print User Input - Only change display for new input *************************************/
  if (bufferChange && state == GET_USER_INPUT) {
    printUserInput(inputIndex);
  }

  /*********** Print Response one word at a time *************************************/
  if (printResponse) {

    //Print Response
    int chatIndex = 0;
    byte currentLine = 1;
    byte displayLineNum = 2;
    int lineStartIndexes[MAX_CHAT_LINES] = { 0 };

    u8g2.setCursor(0, FONT_HEIGHT * displayLineNum);


    //adjust chat index for newlines at beginnning of response
    while (chatBuffer[chatIndex] == '\n') {
      chatIndex++;
    }

    while (chatIndex < responseLength) {

      //Get length of next token
      byte lenOfNextToken = lengthOfToken(chatIndex, responseLength, chatBuffer);

      //  if (lenOfNextToken + chatIndex >= MAX_CHAR_PER_LINE * (displayLineNum - 1)) {
      if (lenOfNextToken + chatIndex >= MAX_CHAR_PER_LINE * currentLine) {
        lineStartIndexes[currentLine] = chatIndex;
        displayLineNum++;
        currentLine++;
        u8g2.setCursor(0, FONT_HEIGHT * displayLineNum);
      }

      for (int i = chatIndex; i < chatIndex + lenOfNextToken + 1; i++) {
        u8g2.print(chatBuffer[i]);  // write something to the internal memory
      }
      chatIndex += lenOfNextToken + 1;
      u8g2.sendBuffer();

      delay(500);

      if (displayLineNum == 4) {
        displayLineNum = 2;
        u8g2.clearBuffer();
        printUserInput(inputIndex);
        u8g2.setCursor(0, FONT_HEIGHT * displayLineNum);
      }
    }

    printResponse = false;
  }
}
