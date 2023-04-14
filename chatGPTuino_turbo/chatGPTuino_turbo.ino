#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
// #include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "credentials.h"
#include "bitmaps.h"
#include <ArduinoJson.h>

#include <PS2KeyAdvanced.h>
// USE THESE KEY DEFINE -> https://github.com/techpaul/PS2KeyAdvanced/blob/master/src/PS2KeyAdvanced.h
#include <PS2KeyMap.h>

// Pins for PS/2 keyboard (through USB)
#define DATAPIN 6  // (USB Data -)  (PS2 pin 1)
#define IRQPIN 5   // (USB Data +)  (PS2 pin 5)

PS2KeyAdvanced keyboard;
PS2KeyMap keymap;

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
              REVIEW_REPONSE,
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
#define CURSOR_START_X_AXIS 0
#define CURSOR_START_Y_AXIS -2

#define MAX_CHAR_PER_OLED_ROW 20  //Update to calculate for different screen sizes based on font size
#define MAX_OLED_ROWS 5           //Update to calculate for different screen sizes based on font size
#define MAX_CHARS_ON_SCREEN (MAX_CHAR_PER_OLED_ROW * MAX_OLED_ROWS)

// These reference may be useful for understanding tokens and message size
// https://platform.openai.com/docs/api-reference/chat/create#chat/create-max_tokens
// https://help.openai.com/en/articles/4936856-what-are-tokens-and-how-to-count-them
// https://platform.openai.com/docs/guides/chat/introduction
//https://platform.openai.com/tokenizer

#define MAX_TOKENS 50      // THE ALL IMPORTANT NUMBER!
#define CHARS_PER_TOKEN 6  // Each token equates to roughly 4 chars, but does not include spaces
#define MAX_MESSAGE_LENGTH (MAX_TOKENS * CHARS_PER_TOKEN)
#define MAX_MESSAGES 5                   // Everytime you send a message, it must inlcude all previous messages in order to respond with context
#define SERVER_RESPONSE_WAIT_TIME 15000  // How long to wait for a server response

const int CHAT_BUFFER_LENGTH = MAX_CHAR_PER_OLED_ROW * MAX_CHAT_LINES;

enum roles { sys,  //system
             user,
             assistant };


char roleNames[3][10] = { "system", "user", "assistant" };

// This is a HUGE chunk o' memory we need to allocate for all time
struct message {
  enum roles role;
  char content[MAX_MESSAGE_LENGTH];
} messages[MAX_MESSAGES];

message systemMessage = { sys, "Respond as if you were a pirate." };
message noConnect = { assistant, "I'm sorry, I seem to be having a brain fart, let me think on that again." };



#define ALERT_MSG_LENGTH 70
char SYSTEM_MSG_UPDATE_INITIATE_ALERT[ALERT_MSG_LENGTH] = "Enter new system message.";
char SYSTEM_MSG_UPDATE_SUCCESS_ALERT[ALERT_MSG_LENGTH] = "System message updated! Start typing to ask next question.";
char WELCOME_INSTRUCTIONS_ALERT[ALERT_MSG_LENGTH] = "Start typing to begin chat.";


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
 * Function:  displayMsg 
 * -------------------------
 * Displays contents of char array to OLED.
 * Clears OLED when starts.
 */
void displayMsg(char msg[], int endIdx, int startIdx = 0, bool setDelay = false) {

  u8g2.clearBuffer();
  u8g2.setCursor(0, 0);

  int lineNum = 0;

  int i, count;
  bool firstTime = true;

  for (i = startIdx, count = 1; i < endIdx; i++, count++) {

    if (i % MAX_CHAR_PER_OLED_ROW == 0) {
      lineNum++;
      u8g2.setCursor(0, FONT_HEIGHT * lineNum);
    }

    u8g2.print(msg[i]);

    // Delay at spaces
    if ((msg[i] == ' ' && setDelay)) {

      if (firstTime || lineNum == 5) {
        delay(300);
        u8g2.sendBuffer();
      }
    }

    if ((count != 0) && ((count % MAX_CHARS_ON_SCREEN) == 0)) {

      u8g2.clearBuffer();

      i -= MAX_CHARS_ON_SCREEN - MAX_CHAR_PER_OLED_ROW;  // Move back 4 lines
      lineNum = 0;
      firstTime = false;
    }
  }

  u8g2.sendBuffer();
}

// void displayMsg(char msg[], int endIdx, int startIdx = 0, bool setDelay = false) {

//   u8g2.clearBuffer();
//   u8g2.setCursor(CURSOR_START_X_AXIS, CURSOR_START_Y_AXIS);

//   int lineNum = 0;
//   // int start = (endIdx > MAX_CHARS_ON_SCREEN)
//   //               ? endIdx - MAX_CHARS_ON_SCREEN
//   //               : 0;

//   int i, count;
//   bool firstTime = true;

//   for (i = startIdx, count = 1; i < endIdx; i++, count++) {

//     if (i % MAX_CHAR_PER_OLED_ROW == 0) {
//       lineNum++;
//       u8g2.setCursor(0, FONT_HEIGHT * lineNum);
//     }

//     u8g2.print(msg[i]);

//     // Delay at spaces
//     if ((msg[i] == ' ' && setDelay) /*&& (firstTime || lineNum == 5)*/) {
//       delay(100);
//       u8g2.sendBuffer();
//     }


//     // If text length exceeds printable space on OLED,
//     // Reprint
//     if((count != 0) && ((count % MAX_CHARS_ON_SCREEN) == 0)) {

//       u8g2.clearBuffer();
//       u8g2.sendBuffer();

//       i -= MAX_CHARS_ON_SCREEN - MAX_CHAR_PER_OLED_ROW; // Move back 4 lines
//       lineNum = 0;
//       firstTime = false;
//       Serial.print("i-> ");
//       Serial.print(i);
//       Serial.print("  Count-> ");
//       Serial.println(count);
//     }
//   }

//   u8g2.sendBuffer();
// }

// void displayMsg(char msg[], int cIdx) {

//   u8g2.clearBuffer();
//   u8g2.setCursor(CURSOR_START_X_AXIS, CURSOR_START_Y_AXIS);

//   int lineNum = 0;
//   int start = (cIdx > MAX_CHARS_ON_SCREEN)
//                 ? cIdx - MAX_CHARS_ON_SCREEN
//                 : 0;

//   for (int i = start; i < cIdx; i++) {

//     if (i % MAX_CHAR_PER_OLED_ROW == 0) {
//       lineNum++;
//       u8g2.setCursor(0, FONT_HEIGHT * lineNum);
//     }

//     u8g2.print(msg[i]);
//   }

//   u8g2.sendBuffer();
// }

/*
 * Function:  displayFace
 * -------------------------
 * Displays face and message on screen.
 */
void displayFace(int iterations, char displayMessage[]) {

  int bitmapIndex;

  for (int i = 0; i <= iterations; i++) {

    randomSeed(analogRead(0));
    int randomBlink = random(1, iterations);
    if (i % randomBlink == 0) {
      bitmapIndex = eyes_closed;
    } else if (i % 2 == 0) {
      bitmapIndex = eyes_open_2;
    } else {
      bitmapIndex = eyes_open_1;
    }

    u8g2.clearBuffer();
    u8g2.drawXBM(0, -10, SCREEN_WIDTH, SCREEN_HEIGHT, bitmaps[bitmapIndex]);

    u8g2.setCursor(0, SCREEN_HEIGHT - 5);
    u8g2.print(displayMessage);

    u8g2.sendBuffer();

    delay(100);
  }
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
  keymap.selectMap((char*)"US");

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

  char welcomeMessage[] = "Hi. I'm chatGPTuino.";
  displayFace(10, welcomeMessage);
  displayMsg(WELCOME_INSTRUCTIONS_ALERT, ALERT_MSG_LENGTH);

  Serial.println("...Setup Ended");
}

/*************************************************************************/
/*******  LOOP      ******************************************************/
/*************************************************************************/

void loop(void) {

  // Track state
  static states state = GET_USER_INPUT;

  // Track messageIndex
  static int messageIndex = 0;
  static int messageEndIndex = 0;
  static int numMessages = 0;

  //Track input buffer index
  static int inputIndex = 0;
  static boolean inputBufferFull = false;
  static boolean clearInput = false;
  static boolean printResponse = false;
  boolean bufferChange = false;

  //Display Response Se
  static int displayOffset = 0;

  // Users can write User messages, or Systems messages
  struct message* msgPtr = (state == GET_USER_INPUT)
                             ? &messages[numMessages % MAX_MESSAGES]
                             : &systemMessage;

  /***************************************************************************/
  /*********** GET USER INPUT ************************************************/
  /***************************************************************************/

  // If input and buffer not full, assign characters to buffer
  if (keyboard.available() && (state == GET_USER_INPUT || state == UPDATE_SYS_MSG)) {


    Serial.println("Start: User Input.");

    int key = keyboard.read();
    byte base = key & 0xff;
    byte remappedKey = keymap.remapKey(key);

    /*
    Serial.print("Key -> ");
    Serial.println(key);
    Serial.print("Base -> ");
    Serial.println(base);
    Serial.print("remappedKey -> ");
    Serial.println(remappedKey);
*/

    // Printable and Command Keys
    if (remappedKey > 0) {

      bufferChange = true;

      switch (base) {

        case PS2_KEY_ENTER:

          Serial.println("");
          Serial.println("Enter Pressed");

          if (state == GET_USER_INPUT) {
            msgPtr->role = user;
            numMessages++;
            state = GET_REPONSE;
            displayOffset = 0;

            Serial.println("  User Message Submitted.");
            Serial.println("-------------------Message Buffer----------------------");

            for (int i = 0; i < MAX_MESSAGES; i++) {
              Serial.print(i);
              Serial.print(" - ");
              Serial.println(messages[i].content);
            }
          } else {

            msgPtr->role = sys;  // New system message has been added, message update role
            state = GET_USER_INPUT;
            displayMsg(SYSTEM_MSG_UPDATE_SUCCESS_ALERT, ALERT_MSG_LENGTH);
            bufferChange = false;  // Do not update display
            Serial.println("  System Message Updated.");
          }

          clearInput = true;
          break;

        case PS2_KEY_DELETE:
        case PS2_KEY_BS:  //Backspace Pressed
          Serial.println("");
          Serial.println("Backspace pressed");

          inputIndex = inputIndex > 0 ? inputIndex - 1 : 0;
          msgPtr->content[inputIndex] = ' ';
          inputBufferFull = false;
          break;

        case PS2_KEY_TAB:
        case PS2_KEY_SPACE:
          Serial.print(" ");

          if (!inputBufferFull) {
            msgPtr->content[inputIndex] = ' ';
            inputIndex++;
          }
          break;

        case PS2_KEY_ESC:
          Serial.println("");
          Serial.println("Esc pressed");
          Serial.println("  System Message Update...");

          displayMsg(SYSTEM_MSG_UPDATE_INITIATE_ALERT, ALERT_MSG_LENGTH);
          state = UPDATE_SYS_MSG;
          bufferChange = false;  // Do not update display

          break;


        default:

          if (clearInput) {

            Serial.print("  Message cleared.");

            memset(msgPtr->content, 0, sizeof msgPtr->content);

            inputIndex = 0;           // Return index to start
            inputBufferFull = false;  // Handles sitution where
            clearInput = false;
          }

          // Add character to current message
          if (!inputBufferFull) {

            Serial.print(char(remappedKey));

            msgPtr->content[inputIndex] = char(remappedKey);
            inputIndex++;
          }

          //If you have reached end of input buffer, display a <
          if (inputIndex >= MAX_MESSAGE_LENGTH && !inputBufferFull) {
            bufferChange = true;
            inputBufferFull = true;

            msgPtr->content[MAX_MESSAGE_LENGTH - 1] = '<';
          }
      }
    } else {

      switch (base) {
        case PS2_KEY_UP_ARROW:
          Serial.println("");
          Serial.println("Up Arrow pressed");

          displayOffset--;

          state = REVIEW_REPONSE;

          Serial.print("displayOffset -> ");
          Serial.println(displayOffset);

          break;

        case PS2_KEY_DN_ARROW:

          Serial.println("");
          Serial.println("Down Arrow pressed");

          // Move the display index up/back one line
          displayOffset++;

          if (displayOffset > 0) {
            displayOffset = 0;
          }

          // printResponse = true;
          state = REVIEW_REPONSE;

          Serial.print("displayOffset -> ");
          Serial.println(displayOffset);

          break;
      }
    }

    Serial.println("End: User Input.");
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
      Serial.print("numMessages ->");
      Serial.println(numMessages);

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

    Serial.println("");
    Serial.println("<-------------------JSON TO BE SENT--------------------->");
    serializeJsonPretty(doc, Serial);
    Serial.println("");

    int conn = client.connect(server, port);

    delay(1000);

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
      // String line = client.readStringUntil('X'); //NOTE -> this will make wait for server response below execute - neesds chanded...
      // Serial.print(line);

      bool responseSuccess = true;
      // bool oncer = false;
      long startWaitTime = millis();
      char thinkingMsg[] = "Thinking...";
      // Wait for server response
      while (client.available() == 0) {

        if (millis() - startWaitTime > SERVER_RESPONSE_WAIT_TIME) {
          Serial.println("!!---------SERVER_RESPONSE_WAIT_TIME exceeded -----");

          responseSuccess = false;
          break;
        }

        displayFace(5, thinkingMsg);
        // if (!oncer) {
        //   u8g2.clearBuffer();
        //   u8g2.setCursor(0, FONT_HEIGHT * 2);
        //   u8g2.print("Thinking...");
        //   u8g2.sendBuffer();
        //   oncer = true;
        // }
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

        messages[numMessages % MAX_MESSAGES].role = assistant;

        // Clear char array
        memset(messages[numMessages % MAX_MESSAGES].content, 0, sizeof messages[numMessages % MAX_MESSAGES].content);

        strncpy(messages[numMessages % MAX_MESSAGES].content, outputDoc["choices"][0]["message"]["content"] | "...", MAX_MESSAGE_LENGTH);

        responseLength = measureJson(outputDoc["choices"][0]["message"]["content"]);

        Serial.println("-------------------Message Buffer--------------------");

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

        //state = GET_USER_INPUT;
        state = DISPLAY_RESPONSE;
        //printResponse = true;

      } else {
        u8g2.clearBuffer();
        u8g2.setCursor(0, FONT_HEIGHT * 2);
        u8g2.print("Brain freeze, 1 sec");
        u8g2.sendBuffer();

        state = GET_REPONSE;
      }

    } else {
      client.stop();
      Serial.println("");
      Serial.println("Connection Failed");
    }

    Serial.println("<-------------------- END Get Reponse ----------------->");
  }


  /***************************************************************************/
  /*********** Print User Input - Only change display for new input **********/
  /***************************************************************************/
  if (bufferChange && (state == GET_USER_INPUT || state == UPDATE_SYS_MSG)) {
    Serial.println("Print User Input");
    displayMsg(msgPtr->content, inputIndex);
  }


  /***************************************************************************/
  /***** Print Response one word at a time **********************************/
  /***************************************************************************/
  // if (printResponse) {
  if (state == DISPLAY_RESPONSE || state == REVIEW_REPONSE) {

    Serial.println("---------------- printResponse Start----------------");


    // Roll over
    int responseIdx = (numMessages % MAX_MESSAGES) - 1 < 0
                        ? MAX_MESSAGES - 1
                        : numMessages % MAX_MESSAGES - 1;



    int lastRowStartIdx = (responseLength / MAX_CHAR_PER_OLED_ROW) * MAX_CHAR_PER_OLED_ROW;

    int firstVisibleRowStartIdx = lastRowStartIdx - MAX_CHARS_ON_SCREEN + MAX_CHAR_PER_OLED_ROW;

    // Reset display offset every time a new message is revieced
    if (state == DISPLAY_RESPONSE) {
      displayOffset = 0;
      firstVisibleRowStartIdx = 0;
    }

    int startDisplayIdx = firstVisibleRowStartIdx + (MAX_CHAR_PER_OLED_ROW * displayOffset);

    Serial.print("   BEFORE startDisplayIdx -> ");
    Serial.print(startDisplayIdx);
    Serial.print("   BEFORE endIDX -> ");
    Serial.println(endIndex);

    if (startDisplayIdx < 0) {
      startDisplayIdx = 0;
    }

    // Display entire message first time assistant responds
    int endIndex = (state == DISPLAY_RESPONSE)
                     ? responseLength
                     : MAX_CHARS_ON_SCREEN;


    Serial.print("responseLength -> ");
    Serial.print(responseLength);
    Serial.print("   lastRowStartIdx -> ");
    Serial.print(lastRowStartIdx);
    Serial.print("   firstVisibleRowStartIdx -> ");
    Serial.print(firstVisibleRowStartIdx);
    Serial.print("   startDisplayIdx -> ");
    Serial.print(startDisplayIdx);
    Serial.print("   endIDX -> ");
    Serial.print(endIndex);
    Serial.print("   state -> ");
    Serial.println(state);

    displayMsg(messages[responseIdx].content, endIndex, startDisplayIdx, state == DISPLAY_RESPONSE ? true : false);

    state = GET_USER_INPUT;

    Serial.println("---------------- printResponse Stop ----------------");
  }
}
