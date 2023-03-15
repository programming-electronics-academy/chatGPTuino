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
const char* openAPIendPoint = "https://api.openai.com/v1/chat/completions";
const char* server = "api.openai.com";

const int port = 443;

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
#define MAX_CHAT_LINES 60  // This needs better estimated

// These reference may be useful for understanding tokens and message size
// https://platform.openai.com/docs/api-reference/chat/create#chat/create-max_tokens
// https://help.openai.com/en/articles/4936856-what-are-tokens-and-how-to-count-them
// https://platform.openai.com/docs/guides/chat/introduction

#define MAX_TOKENS 150
#define CHARS_PER_TOKEN 6  // Each token equates to roughly 4 chars, but does not include spaces
#define MAX_MESSAGE_LENGTH (MAX_TOKENS * CHARS_PER_TOKEN)
#define MAX_MESSAGES 10  // Everytime you send a message, it must inlcude all previous messages in order to respond with context

enum roles { sys,  //system
             user,
             assistant };


char roleNames[3][10] = { "system", "user", "assistant" };

// This is a HUGE chunk o' memory we need to allocate for all time
struct message {
  enum roles role;
  // roles role;
  char content[MAX_MESSAGE_LENGTH];
} messages[MAX_MESSAGES];

const int MAX_CHAR_PER_LINE = SCREEN_WIDTH / FONT_WIDTH - 2;  //The minus 2 is a buffer for the right edge of the screen
const int MAX_LINES = SCREEN_HEIGHT / FONT_HEIGHT;
const int CHAT_BUFFER_LENGTH = MAX_CHAR_PER_LINE * MAX_CHAT_LINES;
//const int MAX_TOKENS = CHAT_BUFFER_LENGTH / 5;  //https://platform.openai.com/tokenizer


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
// void printUserInput(int index) {
void printUserInput(int mIdx, int cIdx) {

  u8g2.clearBuffer();
  u8g2.setCursor(0, FONT_HEIGHT);
  int start = (cIdx > MAX_CHAR_PER_LINE) ? cIdx - MAX_CHAR_PER_LINE : 0;

  // Draw input buffer
  for (int i = start; i < MAX_CHAR_PER_LINE + start; i++) {
    u8g2.print(messages[mIdx].content[i]);
  }

  u8g2.sendBuffer();
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

  // Track state
  static byte state = GET_USER_INPUT;

  // Track messageIndex
  static int messageIndex = 0;     //offset from default
  static int messageEndIndex = 0;  //offset from default
  static int numMessages = 0;

  //Track input buffer index
  static int inputIndex = 0;
  static boolean inputBufferFull = false;
  static boolean clearInput = false;
  boolean bufferChange = false;
  static boolean printResponse = false;

  /***************************************************************************/
  /*********** GET USER INPUT ************************************************/
  /***************************************************************************/

  // If input and buffer not full, assign characters to buffer
  if (keyboard.available() && state == GET_USER_INPUT) {

    bufferChange = true;

    unsigned int input = keyboard.read();
    input &= 0xFF;  // Get lowest bits

    // Handle Special Keys and text
    switch (input) {
      case PS2_KEY_ENTER:

        messages[messageEndIndex++].role = user;
        messageEndIndex %= MAX_MESSAGES;
        numMessages++;

        state = GET_REPONSE;
        clearInput = true;
        Serial.println("Enter Key Pressed");
        break;

      case PS2_KEY_BS:  //Backspace Pressed
        inputIndex = inputIndex > 0 ? inputIndex - 1 : 0;
        messages[messageEndIndex].content[inputIndex] = ' ';
        inputBufferFull = false;
        break;

      case PS2_KEY_SPACE:
        if (!inputBufferFull) {
          messages[messageEndIndex].content[inputIndex] = ' ';
          inputIndex++;
        }
        break;

      default:

        if (clearInput) {

          // Clear char array
          messages[messageEndIndex].content[0] = '\0';

          inputIndex = 0;  // Return index to start
          clearInput = false;
          inputBufferFull = false;
        }

        // Add character to current message
        if (!inputBufferFull) {

          messages[messageEndIndex].content[inputIndex] = char(input);
          inputIndex++;
        }
    }

    //If you have reached end of input buffer, display a <
    if (inputIndex >= MAX_MESSAGE_LENGTH && !inputBufferFull) {
      bufferChange = true;
      inputBufferFull = true;

      messages[messageEndIndex].content[MAX_MESSAGE_LENGTH - 1] = '<';
    }
  }
  /***************************************************************************/
  /*********** GET RESPONSE **************************************************/
  /***************************************************************************/
  if (state == GET_REPONSE) {

    Serial.println("----------------------Start GET RESPONSE---------------");

    WiFiClientSecure client;
    client.setCACert(rootCACertificate);

    DynamicJsonDocument doc(12288);
    doc["model"] = "gpt-3.5-turbo";
    doc["max_tokens"] = MAX_TOKENS;

    JsonArray messagesJSON = doc.createNestedArray("messages");

    int indexStart = 0;

    if (numMessages >= MAX_MESSAGES) {
      indexStart = numMessages % MAX_MESSAGES;
    }

    // for (int i = messageIndex; i < messageEndIndex; i++) {
    for (int i = 0; i < numMessages && i < MAX_MESSAGES; i++) {
      messagesJSON[i]["role"] = roleNames[messages[indexStart].role];
      messagesJSON[i]["content"] = messages[indexStart].content;

      indexStart++;

      if (indexStart >= MAX_MESSAGES) {
        indexStart = 0;
      }  // messageIndex %= MAX_MESSAGES;
    }

    Serial.println("--------------------JSON SENT------------------------");
    serializeJsonPretty(doc, Serial);
    serializeJson(doc, Serial);

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
      // Body
      serializeJson(doc, client);
      client.println();

      // Wait for server response
      while (client.available() == 0) {
        ;
      }

      // Read all the lines of the reply from server and print them to Serial
      // while (client.available()) {
      String line = client.readStringUntil('X');
      Serial.print(line);
      // }

      client.find("\r\n\r\n");

      // Filter returning JSON
      StaticJsonDocument<500> filter;
      JsonObject filter_choices_0_message = filter["choices"][0].createNestedObject("message");
      filter_choices_0_message["role"] = true;
      filter_choices_0_message["content"] = true;

      StaticJsonDocument<2000> outputDoc;
      DeserializationError error = deserializeJson(outputDoc, client, DeserializationOption::Filter(filter));

      client.stop();

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }

      messages[messageEndIndex].role = assistant;
      // Clear char array
      messages[messageEndIndex].content[0] = '\0'; // Why does this work?
      // memset(messages[messageEndIndex].content, 0, sizeof messages[messageEndIndex].content);

      strncpy(messages[messageEndIndex].content, outputDoc["choices"][0]["message"]["content"] | "...", MAX_MESSAGE_LENGTH);  // "\n\nArduino is a ...

      responseLength = measureJson(outputDoc["choices"][0]["message"]["content"]);


      Serial.println("-------------------Message Buffer----------------------");


      for (int i = 0; i < MAX_MESSAGES; i++) {
        Serial.print(i);
        Serial.print(" - ");
        Serial.println(messages[i].content);
      }

      Serial.print("\nSize of most recent reponse -> ");
      Serial.println(responseLength);

      messageEndIndex++;
      messageEndIndex %= MAX_MESSAGES;
      numMessages++;

    } else {
      client.stop();
      Serial.println("Connection Failed");
    }

    state = GET_USER_INPUT;
    printResponse = true;
    Serial.println("<-------------------- END Get Reponse ----------------->");
  }

  /***************************************************************************/
  /*********** Print User Input - Only change display for new input **********/
  /***************************************************************************/

  if (bufferChange && state == GET_USER_INPUT) {
    printUserInput(messageEndIndex, inputIndex);
  }

  /***************************************************************************/
  /***** Print Response one word at a time **********************************/
  /***************************************************************************/
  if (printResponse) {

    Serial.println("---------------- printResponse Start----------------");
    Serial.print("messageEndIndex -> ");
    Serial.println(messageEndIndex);

    // Roll over
    int responseIdx = messageEndIndex - 1 < 0 ? MAX_MESSAGES - 1 : messageEndIndex - 1;
    Serial.print("responseIdx -> ");
    Serial.println(responseIdx);

    int chatIndex = 0;
    byte currentLine = 1;
    byte displayLineNum = 2;
    int lineStartIndexes[MAX_CHAT_LINES] = { 0 };

    u8g2.setCursor(0, FONT_HEIGHT * displayLineNum);

    // Remove beginning newlines
    while (messages[responseIdx].content[chatIndex] == '\n') {
      chatIndex++;

      Serial.println("-Removing Newline from response");
    }

    while (chatIndex < responseLength) {

      //Get length of next token
      byte lenOfNextToken = lengthOfToken(chatIndex, responseLength, messages[responseIdx].content);

      if (lenOfNextToken + chatIndex >= MAX_CHAR_PER_LINE * currentLine) {
        lineStartIndexes[currentLine] = chatIndex;
        displayLineNum++;
        currentLine++;
        u8g2.setCursor(0, FONT_HEIGHT * displayLineNum);
      }

      for (int i = chatIndex; i < chatIndex + lenOfNextToken + 1; i++) {

        u8g2.print(messages[responseIdx].content[i]);
      }
      chatIndex += lenOfNextToken + 1;
      u8g2.sendBuffer();

      delay(400);

      if (displayLineNum == 4) {
        displayLineNum = 2;
        u8g2.clearBuffer();
        printUserInput(responseIdx - 1 < 0 ? MAX_MESSAGES - 1 : responseIdx - 1, inputIndex);
        u8g2.setCursor(0, FONT_HEIGHT * displayLineNum);
      }
    }

    printResponse = false;
    Serial.println("---------------- printResponse Stop ----------------");
  }
}
