
# chatGPTuino

For more details and a video walk through go to: https://bit.ly/chatGPTuino

In this exploratory project you’ll interface an Arduino compatible wireless ESP32 with the OpenAI API and create a standalone “retro” chatGPT terminal using a keyboard as the input device and an OLED as the display.

This is really just the starting point.  

Once you understand how to access and parse the chatGPT API from a wireless microcontroller you open up a ton of new and unexplored possibilities. From autonomously intelligent control of hardware to next level human interface devices.  

What we are doing here is pulling the power of AI out of the cloud and figuring out how to substantiate it.

__Hardware is eating AI, and you’re the chef.__

## chatGPT Terminal operation in a nutshell
Here is a quick outline of this simple chatGPT Terminal.

You type your chat on an old school keyboard
An Arduino compatible ESP32 records your keypresses and sends a POST request to the OpenAI API
When the response comes back, you show the text on an OLED.
Repeat.

Pretty straight forward!

## Major components for the chatGPT terminal

There are 3 major components you’ll need for this project:  

* Microcontroller Dev Board
* PS/2 Keyboard
* OLED

### Microcontroller Dev Board

For this project we’ll use the FeatherS2 - ESP32-S2 by Unexpected Maker as the brains of the project.  The ESP32 is a WiFi (and bluetooth) enabled microcontroller, and they have tons of great development boards for them.

You definitely don’t need this exact model, any ESP32 (or ESP8266) would probably work just fine.  
PS/2 Keyboard

### You’ll also need a PS/2 style keyboard - these are old school keyboards that have what is not a very outdated connector type.  The reason I went with this style keyboard is because the software to read the incoming keypresses is simple to implement and available in multiple different Arduino libraries.

### OLED

Finally you’ll need an OLED display to show the text in stunning 8 kilopixel resolution!  OK, maybe not the most high resolution screen you could dream up, but these 128 x 64 px displays are everywhere.

You can use different display sizes as long as the pixel ratio stays the same, otherwise you’ll need to adjust the code.  I’ve used 0.94”, 1.3”, and 2.42” OLED displays.

## The Connections

This wiring diagram lays out the connections you’ll need between the major components of your chatGPT terminal.  This shows using an I2C OLED display, you’ll need to change up the connections for using SPI. 
https://www.programmingelectronics.com/wp-content/uploads/2023/06/image7.png


## chatGPT terminal Code
The code flow for this system is not too complicated.
https://www.programmingelectronics.com/wp-content/uploads/2023/06/image1.png

Basically, the system is always waiting for keyboard input.  If the keyboard input is part of a chat message, it saves it to a special message array.  If it’s a command key, like SHIFT, or ENTER, then it take the appropriate action.

When a message is submitted by the user, an API call is made to the chatGPT API to get a response.

The response is then parsed by the code, and the pertinent information saved to the messages array.

Finally, it’s displayed on the OLED one word at a time (mimicking the web chatGPT interface), where the user can then use the up and down arrows to scroll through the response.

And that's pretty much a full cycle.  _Read keypresses, hit API, display response._

There is also a way to inject system messages that help steer the response from chatGPT.  The default system message is “Respond like a pirate”.  To change the system message you press escape and you’ll be prompted to type a new one.

For more details and a video walk through go to: https://bit.ly/chatGPTuino