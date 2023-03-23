#include <PS2KeyAdvanced.h>
// USE THESE KEY DEFINE -> https://github.com/techpaul/PS2KeyAdvanced/blob/master/src/PS2KeyAdvanced.h
#include <PS2KeyMap.h>


#define DATAPIN 6  // (USB Data -)  (PS2 pin 1)
#define IRQPIN 5   // (USB Data +)  (PS2 pin 5)

PS2KeyAdvanced keyboard;
PS2KeyMap keymap;

void printBin(unsigned int aByte) {

  for (int aBit = 15; aBit >= 0; aBit--) {
    Serial.write(bitRead(aByte, aBit) ? '1' : '0');

    if (aBit % 8 == 0) {
      Serial.print(" ");
    }
  }
}



void setup() {
  Serial.begin(9600);

  keyboard.begin(DATAPIN, IRQPIN);
  keyboard.setNoBreak(1);
  keyboard.setNoRepeat(1);
}


void loop() {

  char buff[] = "fffff";
  static int index = 0;

  if (keyboard.available()) {

    unsigned int raw_code = keyboard.read();

    Serial.print("BIN->");
    printBin(raw_code);

    Serial.print("MASKED->");
    printBin(raw_code & 0xff);

    Serial.print("  HEX-> ");
    Serial.print(raw_code, HEX);
    Serial.print("  DEC-> ");
    Serial.print(raw_code);
    Serial.print(" UTF-8-> ");
    Serial.println(char(raw_code));

    unsigned int remappedCode = keymap.remapKey(raw_code);

    Serial.print("BIN->");
    printBin(remappedCode);
    Serial.print("MASKED->");
    printBin(remappedCode & 0xff);
    Serial.print("  HEX-> ");
    Serial.print(remappedCode, HEX);
    Serial.print("  DEC-> ");
    Serial.print(remappedCode);
    Serial.print(" UTF-8-> ");
    Serial.print(char(remappedCode));
    Serial.println(" <-Remapped");

    Serial.println("***************************************************");
    Serial.print(" PS2_KEY_ENTER ");
    Serial.print("BIN->");
    printBin(PS2_KEY_ENTER);
    Serial.print("  HEX-> ");
    Serial.print(PS2_KEY_ENTER, HEX);
    Serial.print("  DEC-> ");
    Serial.println(PS2_KEY_ENTER);
    Serial.println("***************************************************");



    if (remappedCode == 0) {

      Serial.println("");
      Serial.println("Non Display Pressed.");







      switch (raw_code & 0xff) {
        case PS2_KEY_UP_ARROW:
          Serial.println("Up Arrow");
          break;
      }
    }
  }
}
