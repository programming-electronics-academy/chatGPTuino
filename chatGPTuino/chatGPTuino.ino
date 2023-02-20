#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// Blinking Interval
#define CURSOR_BLINK_INTERVAL 250

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
char chatBuffer[MAX_LINES * 2][MAX_CHAR_PER_LINE + 1] = {};
char inputBuffer[INPUT_BUFFER_LENGTH] = {};

// OLED Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE);  // High speed I2C

void setup(void) {
  Serial.begin(9600);
  u8g2.begin();
  //https://github.com/olikraus/u8g2/wiki/fntlist12
  u8g2.setFont(u8g2_font_6x13_tf);  // choose a suitable
}

void loop(void) {

  //Track input buffer index
  static byte inputIndex = 0;
  static boolean inputBufferFull = false;
  boolean bufferChange = false;

  // If input and buffer not full, assign characters to buffer
  if (Serial.available()) {

    byte input = Serial.read();
    bufferChange = true;

    // TESTING -> If delete key entered, clear last char and adjust index
    if (input == 'D') {

      inputBuffer[inputIndex] = ' ';
      inputIndex =  inputIndex > 0 ? inputIndex - 1 : 0;
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
      
      inputBuffer[INPUT_BUFFER_LENGTH-1] = '#';

      Serial.print("EXCEEDS-> ");
      Serial.println(inputIndex);
    }
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

      // Draw chat buffer
      for (int i = start; i < MAX_CHAR_PER_LINE + start; i++) {
        u8g2.print(inputBuffer[i]);  // write something to the internal memory
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
