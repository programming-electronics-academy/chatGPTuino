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

enum states { GET_USER_INPUT,
              GET_REPONSE,
              DISPLAY_RESPONSE,
              CLEAR_INPUT,
              UPDATE_SYS_MSG };

// Display settings
#define LINE_HEIGHT 10
#define SCREEN_HEIGHT 64
#define SCREEN_WIDTH 128
#define FONT_WIDTH 6
#define FONT_HEIGHT 12
#define CHAT_DISPLAY_POS_START FONT_HEIGHT * 2
#define INPUT_BUFFER_LENGTH 40
#define MAX_CHAT_LINES 60  // This needs better estimated


// These reference may be useful for understanding tokens and message size
// https://platform.openai.com/docs/api-reference/chat/create#chat/create-max_tokens
// https://help.openai.com/en/articles/4936856-what-are-tokens-and-how-to-count-them
// https://platform.openai.com/docs/guides/chat/introduction
//https://platform.openai.com/tokenizer

#define MAX_TOKENS 150
#define CHARS_PER_TOKEN 6  // Each token equates to roughly 4 chars, but does not include spaces
#define MAX_MESSAGE_LENGTH (MAX_TOKENS * CHARS_PER_TOKEN)
#define MAX_MESSAGES 3                   // Everytime you send a message, it must inlcude all previous messages in order to respond with context
#define SERVER_RESPONSE_WAIT_TIME 15000  // How long to wait for a server response

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

message systemMessage = { sys, "Respond as if you were a Roman Soldier and as terse as possible." };
message noConnect = { assistant, "I'm sorry, I seem to be having a brain fart, let me think on that again." };


const int MAX_CHAR_PER_LINE = 20;  //SCREEN_WIDTH / FONT_WIDTH - 2;  //The minus 2 is a buffer for the right edge of the screen
const int MAX_LINES = 5;           //SCREEN_HEIGHT / FONT_HEIGHT;
const int CHAT_BUFFER_LENGTH = MAX_CHAR_PER_LINE * MAX_CHAT_LINES;


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
 *  mIdx: The index of the message in the message array 
 *  cIdx: The index of the messages content - the actual text
 */
// void printUserInput(int mIdx, int cIdx) {

//   u8g2.clearBuffer();

//   int lineNum = 0;
//   u8g2.setCursor(0, (FONT_HEIGHT * lineNum) - 2);
//   int start = (cIdx > MAX_CHAR_PER_LINE * MAX_LINES) ? cIdx - (MAX_CHAR_PER_LINE * MAX_LINES) : 0;

//   for (int i = start; i < cIdx; i++) {

//     if (i % MAX_CHAR_PER_LINE == 0) {
//       lineNum++;
//       u8g2.setCursor(0, FONT_HEIGHT * lineNum);
//     }

//     u8g2.print(messages[mIdx].content[i]);
//   }

//   u8g2.sendBuffer();
// }
void printUserInput(int mIdx, int cIdx) {

  u8g2.clearBuffer();

  int lineNum = 0;
  u8g2.setCursor(0, (FONT_HEIGHT * lineNum) - 2);
  int start = (cIdx > MAX_CHAR_PER_LINE * MAX_LINES) ? cIdx - (MAX_CHAR_PER_LINE * MAX_LINES) : 0;

  for (int i = start; i < cIdx; i++) {

    if (i % MAX_CHAR_PER_LINE == 0) {
      lineNum++;
      u8g2.setCursor(0, FONT_HEIGHT * lineNum);
    }

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
  // static byte state = GET_USER_INPUT;
  static states state = GET_USER_INPUT;

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
        Serial.println("Key Pressed ->Enter<-");
        messages[messageEndIndex++].role = user;
        messageEndIndex %= MAX_MESSAGES;
        numMessages++;

        state = GET_REPONSE;
        clearInput = true;
        break;

      case PS2_KEY_BS:  //Backspace Pressed
        Serial.println("Key Pressed ->Backspace<-");
        inputIndex = inputIndex > 0 ? inputIndex - 1 : 0;
        messages[messageEndIndex].content[inputIndex] = ' ';
        inputBufferFull = false;
        break;

      case PS2_KEY_SPACE:
        Serial.println("Key Pressed ->Space Bar<-");
        if (!inputBufferFull) {
          messages[messageEndIndex].content[inputIndex] = ' ';
          inputIndex++;
        }
        break;

      case PS2_KEY_ESC:
        Serial.println("Key Pressed ->Esc<-");
        state = UPDATE_SYS_MSG;
        break;

      default:

        if (clearInput) {

          Serial.print("Input Cleared at messages[");
          Serial.print(messageEndIndex);
          Serial.println("]");

          // Clear char array
          memset(messages[messageEndIndex].content, 0, sizeof messages[messageEndIndex].content);

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
  /*********** GET RESPONSE FROM OPEN_AI *************************************/
  /***************************************************************************/
  if (state == GET_REPONSE) {

    Serial.println("----------------------Start GET RESPONSE---------------");

    // Create a secure wifi client
    WiFiClientSecure client;
    client.setCACert(rootCACertificate);

    // Generate the JSON document that will be sent to OpenAI
    DynamicJsonDocument doc(12288);

    // Add static parameters that get sent with all messages https://platform.openai.com/docs/api-reference/chat/create
    doc["model"] = "gpt-3.5-turbo";
    doc["max_tokens"] = MAX_TOKENS;

    // Create nested array that will hold all the system, user, and assistant messages
    JsonArray messagesJSON = doc.createNestedArray("messages");


    /* 
      Our array messages[] is used like a circular buffer.  
      If the size of messages[] is 10, and we add an 11th message, 
      then messages[0] is replaced with the 11th message. 
      
      This means that messages[0] may hold a message that is newer 
      (more recent chronologically) than messages[1].
      
      When we send the messages to Open AI, the messages need to be in order
      from oldest to newest.  So messagesJSON[0], DOES NOT always map to
      messages[0].  In the case above, messagesJSON[0] would equal messages[1]
      since messages[1] was the oldest message sent.

      To maintain this chronological mapping from messages[] to messagesJSON[]
      we introduce an new index. 
    */
    int oldestMsgIdx = 0;

    if (numMessages >= MAX_MESSAGES) {
      oldestMsgIdx = numMessages % MAX_MESSAGES;
    }

    for (int i = 0; i < numMessages && i < MAX_MESSAGES; i++) {

      // Inject system message before
      if (i == numMessages - 1 || i == MAX_MESSAGES - 1) {
        messagesJSON[i]["role"] = roleNames[systemMessage.role];
        messagesJSON[i]["content"] = systemMessage.content;
        i++;
      }

      messagesJSON[i]["role"] = roleNames[messages[oldestMsgIdx].role];
      messagesJSON[i]["content"] = messages[oldestMsgIdx].content;

      oldestMsgIdx++;
      oldestMsgIdx %= MAX_MESSAGES;
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

      // Troubelshoot server reponse
      String line = client.readStringUntil('X');
      Serial.print(line);

      bool responseSuccess = true;
      bool oncer = false;
      long startWaitTime = millis();

      // Wait for server response
      while (client.available() == 0) {

        if (millis() - startWaitTime > SERVER_RESPONSE_WAIT_TIME) {
          Serial.println("-> SERVER_RESPONSE_WAIT_TIME exceeded.");
          responseSuccess = false;
          break;
        }

        if (!oncer) {
          u8g2.clearBuffer();
          u8g2.setCursor(0, FONT_HEIGHT * 2);
          u8g2.print("Thinking...");
          u8g2.sendBuffer();
          oncer = true;
        }
      }

      if (responseSuccess) {

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
        // messages[messageEndIndex].content[0] = '\0';
        memset(messages[messageEndIndex].content, 0, sizeof messages[messageEndIndex].content);

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

        state = GET_USER_INPUT;
        printResponse = true;

      } else {
        u8g2.clearBuffer();
        u8g2.setCursor(0, FONT_HEIGHT * 2);
        u8g2.print("Brain freeze, 1 sec");
        u8g2.sendBuffer();

        state = GET_REPONSE;
      }

    } else {
      client.stop();
      Serial.println("Connection Failed");
    }

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

    // Roll over
    int responseIdx = messageEndIndex - 1 < 0 ? MAX_MESSAGES - 1 : messageEndIndex - 1;

    for (int i = 0; i < responseLength; i++) {

      printUserInput(responseIdx, i);

      if (messages[responseIdx].content[i] == ' ') {
        delay(100);
      }
    }

    printResponse = false;
    Serial.println("---------------- printResponse Stop ----------------");
  }
}
