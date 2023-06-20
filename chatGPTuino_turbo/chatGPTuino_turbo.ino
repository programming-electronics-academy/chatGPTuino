#include <Arduino.h>
#include <U8g2lib.h>  // OLED

#include <WiFi.h>              // ESP32
#include <WiFiClientSecure.h>  // ESP32
#include <ArduinoJson.h>       // Handle JSON formatting for API calls

#include <PS2KeyAdvanced.h>  // Keyboard input
#include <PS2KeyMap.h>       // Keyboard input mapping
// If you want to add more special key functionality, use these key constants -> https://github.com/techpaul/PS2KeyAdvanced/blob/master/src/PS2KeyAdvanced.h

#include "secrets.h"  // Network name, password, and private API key
// #include "credentials.h"  // Network name, password, and private API key
#include "bitmaps.h"  // Images shown on screen

#define DEBUG
//#define DEBUG_SERVER_RESPONSE_BREAKING

// Pins for PS/2 keyboard (through USB)
#define DATAPIN 6  // (USB Data -)  (PS2 pin 1)
#define IRQPIN 5   // (USB Data +)  (PS2 pin 5)

/*************** Open AI endpoint and connection details ****************/
const char* openAPIendPoint = "https://api.openai.com/v1/chat/completions";
const char* server = "api.openai.com";
#define PORT 443                               // The port you'll connect to on the server - this is standard.
#define SERVER_RESPONSE_WAIT_TIME (15 * 1000)  // How long to wait for a server response (seconds * 1000)


// OpenAI API endpoint root certificate used to ensure response is actually from OpenAPI
const char* rootCACertificate =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ\n"
  "RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD\n"
  "VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX\n"
  "DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y\n"
  "ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy\n"
  "VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr\n"
  "mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr\n"
  "IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK\n"
  "mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu\n"
  "XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy\n"
  "dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye\n"
  "jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1\n"
  "BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3\n"
  "DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92\n"
  "9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx\n"
  "jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0\n"
  "Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz\n"
  "ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS\n"
  "R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp\n"
  "-----END CERTIFICATE-----\n";

/*************** Display settings ****************/
#define SCREEN_HEIGHT 64
#define SCREEN_WIDTH 128

/* Font selection is made in setup().  
   For the most part, FONT_WIDTH and FONT_HEIGHT can match the designated font size.
   However, these can be tuned to "scrunch in" more viewable lines for your given OLED size. */
#define FONT_WIDTH 6
#define FONT_HEIGHT 13

/* These constants are used extensively in the displaying of text input and response.
   Adjust at your own peril. */
#define MAX_CHAR_PER_OLED_ROW (SCREEN_WIDTH / FONT_WIDTH)
#define MAX_OLED_ROWS (SCREEN_HEIGHT / FONT_HEIGHT)
#define MAX_CHARS_ON_SCREEN (MAX_CHAR_PER_OLED_ROW * MAX_OLED_ROWS)

/*************** Alert Messages ***************
  Messages dispayed to user for informational purposes. */
#define ALERT_MSG_LENGTH 70
const char SystemMsgUpdateInitiateAlert[ALERT_MSG_LENGTH] = "Enter new system message.";
const char SystemMsgUpdateSuccessAlert[ALERT_MSG_LENGTH] = "System message updated! Start typing to ask next question.";
const char WelcomeInstructionsAlert[ALERT_MSG_LENGTH] = "Start typing to chat.";

/*************** Animation Messages ***************
  Messages dispayed below face animation. */
#define ANIMATION_MSG_LENGTH (MAX_CHAR_PER_OLED_ROW + 1)
const char BootScreenMsg[ANIMATION_MSG_LENGTH] = "Hi. I'm chatGPTuino.";
const char WaitingForApiResponseMsg[ANIMATION_MSG_LENGTH] = "Thinking...";
const char ApiResponseFailMsg[ANIMATION_MSG_LENGTH] = "Brain freeze, 1 sec";
const char ServerConnectionFailMsg[ANIMATION_MSG_LENGTH] = "Contemplating...";
const char DeserializeFailMsg[ANIMATION_MSG_LENGTH] = "I'm a bit scrambled.";

/* These correspond to the messages above, and define the milliseconds each will show. */
#define BOOT_ALERT_INTERVAL (3 * 1000)
#define WAITING_FOR_API_RESPONSE_INTERVAL (1 * 1000)  // This length needs to stay low, as it could slow down acknowledging the response
#define API_RESPONSE_FAIL_INTERVAL (2 * 1000)
#define SERVER_CONNECTION_FAIL_INTERVAL (2 * 1000)
#define DESERIALIZE_FAIL_INTERVAL (2 * 1000)

/*************** System States **************
  The different states the program can be in. */
enum States { GET_USER_INPUT,    // When a user is typing, diplay input on OLED
              GET_RESPONSE,      // Send a POST call to the Open AI API with user input
              DISPLAY_RESPONSE,  // Display the assistant response on the OLED
              REVIEW_RESPONSE,   // Scroll up and down the assistant response
              UPDATE_SYS_MSG };  // User input to change the system message


/*************** States Variables **************
  This struct encapusulates state variables that manage
  the input and display. */
struct StateVars {

  // The state will determine the flow of the program
  States state;

  /*  This will count the total number of messages between the user and the agent.
  This does not include the system message. We will use this message count 
  to determine our index inside the messages array. */
  int msgCount;

  /* Every time a user types a character, we increment inputIdx. 
  If user presses backspace, we decrement inputIdx. */
  int inputIdx;

  // A flag to clear the display buffer and reset inputIdx to 0
  bool clearInput;

  /* When a user presses up and down arrows, this adjusts the index  
   of the message content to show */
  int displayOffset;

  /* A flag to used to ensure user input is displayed only when the 
  display buffer has been meaningfully changed */
  bool bufferChange;

  /* The msgPtr is used for assigning keyboard input text to either:
    -> the System Message or,
    -> a User Message in the messages array
  It is assigned based on the current state.
  The msgPtr is also used for displaying the user keyboard input text. */
  struct message* msgPtr;
};


/*************** Roles **************
  The current chatGTP API format has 3 distinct role types for each message.

  "system" role is a message that can be used to "steer" the response of the model
  "user" role is assigned to messages sent from the user to the model
  "assistant" role is assigned to messages sent from the model to the user

  These roles are sent as a character string in the API call. */
enum roles { sys,  //system
             user,
             assistant };

const char roleNames[3][10] = { "system", "user", "assistant" };


/****** Tokens *******
  A token in the OpenAI API is roughly equivalent to 3/4 of a word.  Tokens are extremely important, because they are used to measure billing.
  You will be billed for the number of token you receive from Open AI *AND* the number you send.

  When you make an API call to Open AI, the number of tokens you request is part of the request.
  Each values below can dramatically modulate the cost of communication as well as the storage space used to store each message. 
  
  These reference may be useful for understanding tokens and message size.
  https://platform.openai.com/docs/api-reference/chat/create#chat/create-max_tokens
  https://help.openai.com/en/articles/4936856-what-are-tokens-and-how-to-count-them
  https://platform.openai.com/docs/guides/chat/introduction
  https://platform.openai.com/tokenizer */
#define MAX_TOKENS 50      // Each token is roughly 3/4 of a word.  The longer this bigger this number, the longer the potenial response.
#define CHARS_PER_TOKEN 6  // Each token equates to roughly 4 chars, but does not include spaces, the number 6 was chosen to act as a safety buffer incase a response is above average length.
#define MAX_MESSAGE_LENGTH (MAX_TOKENS * CHARS_PER_TOKEN)

/* The messages you send to OpenAI DO NOT PERSIST in the model,
so everytime you send a message to the OpenAI API, you'll want to include 
as many previous messages in order for the model to respond with context.
The value below determines the depth of that context.  A small number means the responses
will not have as much memory, but will cost less and require much less memory.
A large value will allow for more rapport in the assistant repsonses, 
but will cost much more and take up load more memory. 
It depends on what you're after.  10 has been a good number for me.
When testing code not related to reponses, I recommend using a small number, like 4. */
#define MAX_MESSAGES 4  // Min 2, Max 20

/* When sending message, they all go into a JSON doc.  The sizes of this doc 
depends on the size of the previous choices. The value below is based on a 
MAX_MESSAGE_LENGTH of 375 and MAX_MESSAGES of 20.  Should you have a larger values for 
these, you'll likely want to adjust this value. You can use the ArduinoJSON Assistant 
to help you calculate a size:

  https://arduinojson.org/v6/assistant 
  https://arduinojson.org/v6/how-to/determine-the-capacity-of-the-jsondocument/

Also included with this repo is a text file with sample JSON data to 
play around with sizing.*/
#define DYNAMIC_JSON_DOC_SERIALIZE_SIZE 12288  // bytes

/*************************************************************************/
/*******  GLOBALS  ******************************************************/
/*************************************************************************/

/******  Message and Message Array *********
  The "chat" format used by the OpenAI API is an array of {role, content} pairs.
  I am using the singular term "message" to refer to one of these pairs.

  Everytime you communicate with the model, you must send all your 
  previous messages, as the model messages do not persist from one 
  API call to the next.  To say this again, as it is key... 
  
  If you want the model responses to be couched in the context of previous
  messages, you must include all the messages from the user and from the assistant
  in chronological order (oldest to newest).
 
  To manage all these messages, we implement the following:
  1) A message struct for handling each {role, content} pair
  2) A messages array, to hold all the message(s).

  The messages array is treated as a circular buffer.  When the number of messages exceeds
  the length of the array, the newest message overwrites the oldest messaage.  
  
  This limits the "depth" of backward context that can be maintained with the chatbot.
  If you increase MAX_MESSAGES you'll increase this depth, and also increase total cost, 
  as well as increase memory allocated for the messages array.

  The messages array is updated by every state, and is used extensively throughout the program.
*/
struct message {
  enum roles role;
  char content[MAX_MESSAGE_LENGTH];
} messages[MAX_MESSAGES];

/******* System message *********
  As mentioned ealier, the system message is a special message meant to steer the models response.
  In the current configuation, the system message is NOT stored in the messages array, but rather inserted
  into the JSON packet during the API call, prior to the last message sent.
  The system message can be used for fun, and to configure the kind of response you want from the chatBot. */
message systemMessage = { sys, "Respond as if you were a pirate." };

// Used when API is not responding, prior to making another API call.
message noConnect = { assistant, "I'm sorry, I seem to be having a brain fart, let me think on that again." };

/* The number of characters in the assistant response.  
This is used extensively in how the message response is displayed. */
unsigned int responseLength;

// OLED Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE);  // High speed I2C

/*************** Keyboard Input ****************/
PS2KeyAdvanced keyboard;
PS2KeyMap keymap;

/*************************************************************************/
/*******  FUNCTIONS ******************************************************/
/*************************************************************************/

/*
 * Function:  displayMsg 
 * -------------------------
 * Displays contents of char array to OLED with text wrapping.
 * If text exceeds text exceeds the number of lines, it scrolls
 * the lines up to display the text. 
 * 
 * Clears OLED when starts.
 * 
 * This function is used for displaying user text while tying,
 * text from the assistant, system message undates, and ALERT messages.
 * It is also invoked when a user presses the up and down arrows to 
 * review the response.    
 * 
 * msg[]: The source char array to display from
 * endIdx:  The last index to display from, non-inclusive
 * startIdx:  The first index to display from, inclusive, defaults to 0
 * setDelay:  If true, delays and redraws display everytime a space ' ' is encountered, 
 *            defaults to false    
 * 
 * returns: void
 */
void displayMsg(char msg[], int endIdx, int startIdx = 0, bool setDelay = false) {

  // Clear display and position cursor in top left of screen.
  u8g2.clearBuffer();
  u8g2.setCursor(0, 0);

  // Track which "line" the text is on. 0 indexed.
  int lineNum = 0;

  bool firstTime = true;  // Used to control when and when not to delay text displaying

  /* 'i' gets modulated when the text scrolls, and is the index of the text on the display.
  If the message is long, and the text scrolls up, a specifc character will be displayed 
  and then cleared multiple times.

  'count' is the number of chars that have been displayed thus far from the char array, 
  it is used to trigger the scroll action. */
  int i, count;

  // Display chars from startIdx to endIdx
  for (i = startIdx, count = 1; i < endIdx; i++, count++) {

    // Move cursor to left and down one line for text wrapping. (Always occurs first time through)
    if (i % MAX_CHAR_PER_OLED_ROW == 0) {
      lineNum++;
      u8g2.setCursor(0, FONT_HEIGHT * lineNum);
    }

    u8g2.print(msg[i]);  // Write char to display buffer

    /* Display and delay at spaces ' '.
    this effect is meant to mimic the chatGTP web interface */
    if ((msg[i] == ' ' && setDelay)) {

      /* Only delay the first time the text is shown or when the text scrolls up
      we only want to delay displaying of the next new line, not the previously drawn lines */
      if (firstTime || lineNum == MAX_OLED_ROWS) {
        delay(300);
        u8g2.sendBuffer();  // Display all text.
      }
    }

    /* If you exceed the number of chars avaiable on the screen, 
      clear the display, and move i backward so that the next time through we'll
      redraw all but one of the previous lines to make room for the next line.
      In this way, the text auto "scrolls" up when a long message is displayed. */
    if ((count != 0) && ((count % MAX_CHARS_ON_SCREEN) == 0)) {

      u8g2.clearBuffer();  // Clear all text in the buffer.

      i -= MAX_CHARS_ON_SCREEN - MAX_CHAR_PER_OLED_ROW;  // Move back total lines - 1
      lineNum = 0;
      firstTime = false;  // When we re-display the previous lines, we don't want to delay.
    }
  }

  u8g2.sendBuffer();
}


/*
 * Function:  displayFace
 * -------------------------
 * Displays 3 faces repeatly and randomly, as well as a message on screen.  
 * Very amateur animation for blinking and mouth movement.
 * 
 * displayTime: Roughly how long the animation will last. 
 *              This does not account for code execution time, 
 *              and is an underestimate of total run time, but close enough. 
 * displayMessage:  What message to show below the face.
 * delayInterval:   Adjust the "frame rate" of the animation.
 *
 * returns: void
 */
void displayFace(long displayTime, const char displayMessage[], long delayInterval = 100) {

  long iterations = displayTime / delayInterval;
  int bitmapIndex;       // The bitmap to display
  int marginTop = -10;   // How many pixels from top to display bitmap (must be negative)
  int marginBottom = 5;  // How many pixels from bottom to display text

  randomSeed(analogRead(0));

  for (int i = 0; i <= iterations; i++) {

    int randomBlink = random(1, iterations);  // Pick a random number
    if (i % randomBlink == 0) {               // blink
      bitmapIndex = eyes_closed;
    } else if (i % 2 == 0) {
      bitmapIndex = eyes_open_2;
    } else {
      bitmapIndex = eyes_open_1;
    }

    u8g2.clearBuffer();
    u8g2.drawXBM(0, marginTop, SCREEN_WIDTH, SCREEN_HEIGHT, bitmaps[bitmapIndex]);  // Put face in buffer
    u8g2.setCursor(0, SCREEN_HEIGHT - marginBottom);
    u8g2.print(displayMessage);  // Put text in buffer
    u8g2.sendBuffer();           // Display buffer

    delay(delayInterval);
  }
}

/*
 * Function:  displayResponse
 * -------------------------
 * Displays the reponse from openAPI to the OLED.
 * 
 * struct StateVars * pStateVars:  struct of state variables, the members used are:
 *        states* state: current state
 *        int* displayOffset: used for adjusting which line of text to display first
 *        int* msgCount: current number of messages in message array
 * 
 * returns: void
*/
void displayResponse(struct StateVars * pStateVars) {

  if (pStateVars->state == DISPLAY_RESPONSE || pStateVars->state == REVIEW_RESPONSE) {

    Serial.println("|- Print Response -------------------------------|");

    /*  Determine the most recent message index - recall, the most recent message IS NOT always
    the latest element in the messages[] array. */
    int responseIdx = (pStateVars->msgCount % MAX_MESSAGES) - 1 < 0  // Check if you've reached the last index
                        ? MAX_MESSAGES - 1                 // If so, we'll want to print the last index
                        : pStateVars->msgCount % MAX_MESSAGES - 1;   // Otherwise, circle back

    /* Calculate the start and end display indices for the response and for  "response scrubbing" 
    (ie, when the user presses up and down arrows to look through response on OLED) */
    int startIdx;
    int endIdx;

    /*  Prepare start and stop indexes to display a new response one word at a time.  If the number of text lines
    exceeds the available space, we'll shift all the text up one row as we keep displaying. */
    if (pStateVars->state == DISPLAY_RESPONSE) {

      startIdx = 0;
      endIdx = responseLength;

      // Reset display offset every time a new message is received
      pStateVars->displayOffset = 0;

      /*  Prepare start and stop indexes if the user is reviewing the response with up and down arrows.
      This means the reponse was long and the total number of text lines exceeded
      the aviable space to show on the screen. */
    } else if (pStateVars->state == REVIEW_RESPONSE) {

      // How many spaces are needed to complete the last row
      byte spacesToCompleteLastRow = MAX_CHAR_PER_OLED_ROW - responseLength % MAX_CHAR_PER_OLED_ROW;

      // Count full rows of text in response
      int fullRowsOfText = (responseLength + spacesToCompleteLastRow) / MAX_CHAR_PER_OLED_ROW;  // This should always be a whole number

      // Calculate index at the end of the last row in response
      int endFrameLastIdx = (fullRowsOfText * MAX_CHAR_PER_OLED_ROW);

      // Calculate the first index in the "End of Respone 'Frame'"
      int endFrameFirstIdx = endFrameLastIdx - MAX_CHARS_ON_SCREEN;

      // Calculate display adjustment due to up/down arrow presses
      int scrubAdj = pStateVars->displayOffset * MAX_CHAR_PER_OLED_ROW;

      // Determine start/ end indices
      startIdx = endFrameFirstIdx + scrubAdj;

      // Start index can never be negative
      if (startIdx < 0) {
        startIdx = 0;
        (pStateVars->displayOffset)++;  // Negates an up arrow press in case user keeps pressing up arrow when already
                              // at the beginning of a message so displayOffset will not accumulate presses
      }

      endIdx = startIdx + MAX_CHARS_ON_SCREEN - 1;
    }

    // Display message
    displayMsg(messages[responseIdx].content, endIdx, startIdx, pStateVars->state == DISPLAY_RESPONSE ? true : false);
    pStateVars->state = GET_USER_INPUT;  // Prepare for new user input
  }
}

/*
 * Function:  printToConsoleMessageArray
 * -------------------------
 * Prints all contents of the messages array to the console,
 * as well as the length of characters of the most recent reponse.
 *
 * returns: void
 */
void printToConsoleMessageArray() {

  Serial.println("    |------------------- Messages[] --------------------");

  for (int i = 0; i < MAX_MESSAGES; i++) {
    Serial.print("    | ");
    Serial.print(i);
    Serial.print(" - ");
    Serial.println(messages[i].content);
  }

  Serial.print("    | Size of most recent reponse -> ");
  Serial.println(responseLength);
  Serial.println("    |----------------------------------------------------");
}


/*
 * Function:  generateJSONRequestBody
 * -------------------------
 * Creates a JSON formatted object of all the messages 
 * in the messages array.  It all inserts the system message into 
 * this JSON object prior to the most recent message.
 *
 * int numMessages:  Number of messages in the messages array
 *
 * returns: DynamicJsonDocument
 */
DynamicJsonDocument generateJsonRequestBody(int numMessages) {

  // Generate the JSON document that will be sent to OpenAI.
  DynamicJsonDocument doc(DYNAMIC_JSON_DOC_SERIALIZE_SIZE);

  // Add static parameters that get sent with all messages https://platform.openai.com/docs/api-reference/chat/create
  doc["model"] = "gpt-3.5-turbo";  // Current model, will soon be gpt-4...
  doc["max_tokens"] = MAX_TOKENS;

  // Create nested array that will hold all the system, user, and assistant messages
  JsonArray messagesJSON = doc.createNestedArray("messages");

  /* Our array messages[] is used like a circular buffer.  
    If the size of messages[] is 10, and we add an 11th message, 
    then messages[0] is replaced with the 11th message. 
      
    This means that messages[0] may hold a message that is newer 
    (more recent chronologically) than messages[1].
      
    When we send the messages to OpenAI, the messages need to be in order
    from oldest to newest.  So messagesJSON[0], DOES NOT always map to
    messages[0].  In the case above, messagesJSON[0] would equal messages[1]
    since messages[1] was the oldest message sent.

    To maintain this chronological mapping from messages[] to messagesJSON[]
    we introduce a new index. */
  int oldestMsgIdx = 0;

  /* If the total number of messages sent between user 
    and agent exceeds the max, circle back around. */
  if (numMessages >= MAX_MESSAGES) {
    oldestMsgIdx = numMessages % MAX_MESSAGES;
  }

  /* Copy all message(s) from messages[] to messagesJSON[].
    Additionally, inject the system message prior to the most recent message sent.
    'i' is used to index messagesJSON[], and 'oldestMsgIdx' is used to index messages[]  */
  for (int i = 0; i < numMessages && i < MAX_MESSAGES; i++) {

    // Inject system message before most recent message
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

#ifdef DEBUG
  Serial.println("    | JSON to be sent:");
  serializeJsonPretty(doc, Serial);
  Serial.println("");
#endif

  return doc;
}

/*
 * Function:  postRequest
 * -------------------------
 * Makes a POST request to OpenAI 
 *
 * DynamicJsonDocument * pJSONRequestBody: The JSON Request body to send with the POST
 * WiFiClientSecure * pClient: The wifi object handling the sending
 *
 * returns: void
 */
void postRequest(DynamicJsonDocument* pJsonRequestBody, WiFiClientSecure* pClient) {

  Serial.println("    | Connected to OpenAI");
  // Make request
  pClient->println("POST https://api.openai.com/v1/chat/completions HTTP/1.1");
  // Send headers
  pClient->print("Host: ");
  pClient->println(server);
  pClient->println("Content-Type: application/json");
  pClient->print("Content-Length: ");
  pClient->println(measureJson(*pJsonRequestBody));
  pClient->print("Authorization: Bearer ");
  pClient->println(openAI_Private_key);
  pClient->println("Connection: Close");
  /* The empty println below inserts a stand-alone carriage return and newline (CRLF) 
      which is part of the HTTP protocol following sending headers and prior to sending the body. */
  pClient->println();
  serializeJson(*pJsonRequestBody, *pClient);  // Serialize the JSON doc and append to client object
  pClient->println();                          // Send the body to the server

  Serial.println("    | JSON sent");
}

/*
 * Function:  putResponseInMsgArray
 * -------------------------
 * Applies filter to JSON reponse and saves response to messages array. 
 *
 * WiFiClientSecure * pClient: The wifi object handling the response
 * int numMessages:  Number of messages in the messages array
 *
 * returns: bool - 0 for failure to extract JSON, 1 for success
 */
bool putResponseInMsgArray(WiFiClientSecure* pClient, int numMessages) {

  pClient->find("\r\n\r\n");  // This search gets us to the body section of the http response

  /* Create a filter for the returning JSON 
        https://arduinojson.org/news/2020/03/22/version-6-15-0/ */
  StaticJsonDocument<500> filter;
  JsonObject filter_choices_0_message = filter["choices"][0].createNestedObject("message");
  filter_choices_0_message["role"] = true;
  filter_choices_0_message["content"] = true;

  // Deserialize the JSON
  StaticJsonDocument<2000> jsonResponse;
  DeserializationError error = deserializeJson(jsonResponse, *pClient, DeserializationOption::Filter(filter));

  // If deserialization fails, exit immediately and try again.
  if (error) {

    displayFace(DESERIALIZE_FAIL_INTERVAL, DeserializeFailMsg);
    pClient->stop();

    Serial.print("    | deserializeJson() failed->");
    Serial.println(error.c_str());

    return 0;
  }

  // Update messages[] with new message details
  messages[numMessages % MAX_MESSAGES].role = assistant;                                                                                // Assign incoming message role as 'assistant'
  strncpy(messages[numMessages % MAX_MESSAGES].content, jsonResponse["choices"][0]["message"]["content"] | "...", MAX_MESSAGE_LENGTH);  // Copy content

  // Measure the length of the response
  responseLength = measureJson(jsonResponse["choices"][0]["message"]["content"]);

  return 1;
}

/*
 * Function:  waitForServerResponse
 * -------------------------
 * Holds program in loop while waiting for response from server.
 * Times out after defined interval.  Displays waiting face to OLED. 
 *
 * WiFiClientSecure * pClient: The wifi object handling the response
 *
 * returns: bool - 0 for timeout, 1 for success
 */
bool waitForServerResponse(WiFiClientSecure* pClient) {

  bool responseSuccess = true;
  long startWaitTime = millis();  // Measure how long it takes

  while (pClient->available() == 0) {
    // While waiting, show a face animation.
    displayFace(WAITING_FOR_API_RESPONSE_INTERVAL, WaitingForApiResponseMsg);

    /* If you've been waiting too long, perhaps something went wrong,
        break out and try again. */
    if (millis() - startWaitTime > SERVER_RESPONSE_WAIT_TIME) {
      Serial.println("    | SERVER_RESPONSE_WAIT_TIME exceeded.");
      return false;
    }
  }

  return responseSuccess;
}


/*
 * Function:  getResponse
 * -------------------------
 * Form JSON request body and send HTTPS request to openAI.
 * Parse the reponse. Update messages array with new response.
 *  
 * states* pState: current state
 * int* pMsgCount: current number of messages in message array
 *
 * returns: void
 */
void getResponse(States* pState, int* pMsgCount) {

  if (*pState == GET_RESPONSE) {

    Serial.println("|- Start API Call -------------------------------|");
    Serial.print("    | msgCount->");
    Serial.println(*pMsgCount);

    // Create a secure wifi client
    WiFiClientSecure client;
    client.setCACert(rootCACertificate);

    // Generate JSON Request body from messages array
    DynamicJsonDocument jsonRequestBody = generateJsonRequestBody(*pMsgCount);

    // Connect to OpenAI
    int conn = client.connect(server, PORT);

    // If connection is successful, send JSON
    if (conn == 1) {
      // Send JSON Request body to OpenAI API endpoint URL
      postRequest(&jsonRequestBody, &client);

#ifdef DEBUG_SERVER_RESPONSE_BREAKING
      /* Seeing the headers of the server response can be extremely useful to troubleshooting
      connection errors.  However, this readout of the server response header breaks 
      how the message is parsed from the response.  So you'll be able to send and receive one message,
      but no more.  So make sure you only use this when debugging server response issues. */

      String line = client.readStringUntil('X');
      Serial.print(line);
#endif

      //  Wait for OpenAI response
      bool responseSuccess = waitForServerResponse(&client);

      // If you receive a response, parse the JSON and copy the response to messages[]
      if (responseSuccess) {

        bool responseSaved = putResponseInMsgArray(&client, *pMsgCount);

        if (responseSaved) {

          (*pMsgCount)++;              // We successfully received and saved a new message
          *pState = DISPLAY_RESPONSE;  // Now display response

        } else {
          // An error occured durring parsing, exit and try again (error message handled in parsing function)
          return;
        }

#ifdef DEBUG
        printToConsoleMessageArray();
#endif

      } else {
        // Server did not responsd to POST request, go through loop and try again.
        displayFace(API_RESPONSE_FAIL_INTERVAL, ApiResponseFailMsg);
        Serial.println("    | Server did not respond. Trying again.");
      }

    } else {
      // Failed to connect to server, go through loop and try again.
      displayFace(SERVER_CONNECTION_FAIL_INTERVAL, ServerConnectionFailMsg);
      Serial.println("    | Could not connect to server. Trying again.");
    }

    // Disconnect from server after response received, server timeout, or connection failure
    client.stop();
  }
}

/*
 * Function:  getUserInput
 * -------------------------
 * Parse keyboard input into Command Keys (i.e. Shift, Backspace, etc) and Text
 *    If Command Keys, execute appropriate command  
 *    If Text, save to either messages array or systemMessage
 *  
 * struct StateVars* pStateVars: state variables used for managing input and display
 *
 * returns: void
 */
void getUserInput(struct StateVars* pStateVars) {

  // If the user has pressed a key during an input/update state
  if (keyboard.available() && (pStateVars->state == GET_USER_INPUT || pStateVars->state == UPDATE_SYS_MSG)) {

    /* This is a 16 bit value, the 8 MSB are individual flags for functional keys (like shift, control, etc)
    While the 8 LSB make up a unique code for the specific key that was pressed. */
    int key = keyboard.read();
    byte base = key & 0xff;                   // This is the 8 bit unique code for a character
    byte remappedKey = keymap.remapKey(key);  // remapKey returns a 0 if the key is not standard ASCII/UTF-8 code.
                                              // This is used to filter out non-display character keys, like Fn, Alt, etc

    // Check if up or down arrow pressed
    boolean arrowKeyPressed = (base == PS2_KEY_UP_ARROW) || (base == PS2_KEY_DN_ARROW);

    // Printable and Select Command Keys
    if (remappedKey > 0 || arrowKeyPressed) {

      // Signals the downstream print function to re-display buffer
      pStateVars->bufferChange = true;

      switch (base) {

        case PS2_KEY_ENTER:

          //keypressEnter(&state, &inputIdx, &msgPtr, &msgCount, &bufferChange, &clearInput);

          Serial.println("KeyPressed-> Enter");

          /* Pressing the Enter/Return key has different effects depending on the state and whether 
          the user has input any text yet or not.

          state          |  Major Action
          GET_USER_INPUT |  Change state to GET_RESPONSE, set msg role as 'user'
          UPDATE_SYS_MSG |  Change state to GET_USER_INPUT, set msg role as 'sys' */
          if (pStateVars->state == GET_USER_INPUT) {

            // Only change state if user has typed text
            if (pStateVars->inputIdx != 0) {
              pStateVars->msgPtr->role = user;
              pStateVars->msgCount++;
              pStateVars->inputIdx = 0;  // Reset Input Index for next response
              pStateVars->state = GET_RESPONSE;
            } else {
              /* User pressed enter with no text entered, this can happen easily if a user presses 
              Enter/Return after the response is shown, thinking they need to clear the display with enter. */
              u8g2.clearDisplay();
            }

            Serial.println("  | User message submitted");

            // User is updating system message
          } else if (pStateVars->state == UPDATE_SYS_MSG) {

            pStateVars->msgPtr->role = sys;  // New system message has been added, update the message role
            displayMsg((char*)SystemMsgUpdateSuccessAlert, ALERT_MSG_LENGTH);

            pStateVars->state = GET_USER_INPUT;
            pStateVars->bufferChange = false;  // Do not update display
            Serial.println(systemMessage.content);
            Serial.println("  | System message updated.");
          }

          // Any time enter is pressed, clear the current input for the next message
          pStateVars->clearInput = true;

          Serial.print("    | state-> ");
          Serial.println(pStateVars->state);
          break;

        case PS2_KEY_DELETE:
        case PS2_KEY_BS:

          pStateVars->inputIdx = pStateVars->inputIdx > 0 ? pStateVars->inputIdx - 1 : 0;
          pStateVars->msgPtr->content[pStateVars->inputIdx] = ' ';

          Serial.println("KeyPressed-> Backspace/Delete ");
          Serial.print("  | Input Index->");
          Serial.println(pStateVars->inputIdx);
          break;

        case PS2_KEY_TAB:
        case PS2_KEY_SPACE:

          if (pStateVars->inputIdx < MAX_MESSAGE_LENGTH - 1) {
            pStateVars->msgPtr->content[pStateVars->inputIdx] = ' ';
            pStateVars->inputIdx++;
          }

          Serial.println("KeyPressed-> Space/Tab");
          Serial.print("  | Input Index->");
          Serial.println(pStateVars->inputIdx);
          break;

        /* The Escape key changes allows the user to enter a new system message. */
        case PS2_KEY_ESC:
          displayMsg((char*)SystemMsgUpdateInitiateAlert, ALERT_MSG_LENGTH);
          pStateVars->state = UPDATE_SYS_MSG;
          pStateVars->inputIdx = 0;
          pStateVars->bufferChange = false;  // Do not update display

          Serial.println("KeyPressed-> Esc");
          Serial.print("  | state-> ");
          Serial.println(pStateVars->state);
          break;

        /* Up and down arrow keys are used when the user is reviewing a long response. */
        case PS2_KEY_UP_ARROW:

          if (pStateVars->inputIdx == 0) {        // Ensure user is not typing a new message (maybe they pressed arrow key by accident)
            pStateVars->displayOffset--;          // Move the display index back one line
            pStateVars->state = REVIEW_RESPONSE;  // Make sure the change is displayed on the screen
          }

          Serial.println("KeyPressed-> Up Arrow");
          Serial.print("  | displayOffset->");
          Serial.println(pStateVars->displayOffset);
          break;

        case PS2_KEY_DN_ARROW:

          if (pStateVars->inputIdx == 0) {  // Ensure user is not typing a new message (maybe they pressed arrow key by accident)
            pStateVars->displayOffset++;    // Move the display index forward one line

            if (pStateVars->displayOffset > 0) {  // displayOffset of 0 represents the bottom of the message,
              pStateVars->displayOffset = 0;      // so you can't move below that.
            }

            pStateVars->state = REVIEW_RESPONSE;  // Make sure the change is displayed on the screen
          }

          Serial.println("KeyPressed-> Down Arrow");
          Serial.print("  | displayOffset->");
          Serial.println(pStateVars->displayOffset);
          break;

        default:

          if (pStateVars->clearInput) {

            /* Clear all the previous data in the message content pointed to by msgPtr. */
            memset(pStateVars->msgPtr->content, 0, MAX_MESSAGE_LENGTH);

            pStateVars->inputIdx = 0;        // Return index to 0 for new message
            pStateVars->clearInput = false;  // Reset flag

            Serial.println("  | Message cleared.");
          }

          // Assign incoming char to a message if there is still room
          if (pStateVars->inputIdx < MAX_MESSAGE_LENGTH - 1) {

            pStateVars->msgPtr->content[pStateVars->inputIdx] = char(remappedKey);
            pStateVars->inputIdx++;

            Serial.print(char(remappedKey));

            /* If you have come to the end of the msg content, 
            add a visual indicator to let the user know. */
          } else if (pStateVars->inputIdx == MAX_MESSAGE_LENGTH - 1) {
            pStateVars->msgPtr->content[pStateVars->inputIdx] = '<';
            pStateVars->inputIdx++;

            Serial.print("  | You've Reached the end of Input Index->");
            Serial.println(pStateVars->inputIdx);
          }
      }  // Close switch-case
    }    // Close if Command/printable key
  }      // Close if keyboard input available
}  // Close getUserInput

/*************************************************************************/
/*******   SETUP    ******************************************************/
/*************************************************************************/

void setup(void) {

  Serial.begin(9600);

  delay(3000);
  Serial.println("ChatGPTuino.  A terminal for interacting with OpenAI.");
  Serial.println("Setup Started...");

  // Keyboard setup
  keyboard.begin(DATAPIN, IRQPIN);
  keyboard.setNoBreak(1);         // No break codes for keys (when key released)
  keyboard.setNoRepeat(1);        // Don't repeat shift, ctrl, etc
  keymap.selectMap((char*)"US");  // Select the country for your type of keyboard (only tested on US)

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

  // Show a face and message, and then instructions.
  displayFace(BOOT_ALERT_INTERVAL, BootScreenMsg);
  displayMsg((char*)WelcomeInstructionsAlert, ALERT_MSG_LENGTH);

  Serial.println("...Setup Ended");
}

/*************************************************************************/
/*******  LOOP      ******************************************************/
/*************************************************************************/

void loop(void) {

  // this 
  static StateVars stateVars = {
    GET_USER_INPUT,  // state
    0,               // msgCount
    0,               // inputIndex
    false,           // clearInput
    0                // displayOffset
  };

  stateVars.msgPtr = (stateVars.state == GET_USER_INPUT)
                       ? &messages[stateVars.msgCount % MAX_MESSAGES]  // This implements the circular nature of the messages array
                       : &systemMessage;

  stateVars.bufferChange = false;

  /*********** GET USER INPUT *************************************************/
  getUserInput(&stateVars);

  /*********** GET RESPONSE FROM OPEN_AI ***************************************/
  getResponse(&stateVars.state, &stateVars.msgCount);

  /*********** DISPLAY USER KEYBOARD INPUT AS IT IS TYPED **********************/
  if (stateVars.bufferChange && (stateVars.state == GET_USER_INPUT || stateVars.state == UPDATE_SYS_MSG)) {
    displayMsg(stateVars.msgPtr->content, stateVars.inputIdx);
  }

  /*********** DISPLAY AGENT RESPONSE ******************************************/
  // displayResponse(&stateVars.state, &stateVars.displayOffset, &stateVars.msgCount);
  displayResponse(&stateVars);

}  // close void loop