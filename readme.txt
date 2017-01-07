This is a fork of Daniel Noguerol's SIO2Arduino Project.

I made the following modifications:
 * Added the SdFat library to the project folder. Apparently it requires an old version so -since the library is GPL- I've included it here. Copy the folder to your Arduino libraries path before compiling the sketch.
 * Added several forward declarations so the code compiles under recent versions of the Arduino IDE.
 * Moved the RESET button code to the setup function so it's only checked during the device startup.
 * Changed the behavior of the button so it doesn't trigger continuously while it's pressed.
 
With the changes above the optional automount feature can coexist peacefully with the image selection mechanism, so you get both options if you select them.

I also created a simple PCB to wire everything together in a more compact device.
You can find more information here: http://blog.damnsoft.org/custom-pcb-for-sio2arduino/

------------------------------------------------------------------
:: ORIGINAL README FOLLOWS ::
------------------------------------------------------------------
SIO2Arduino is an open-source implementation of the Atari SIO device protocol that runs on Arduino hardware. Arduino hardware is open-source and can be assembled by hand or purchased preassembled.

Note: You will need the SdFat library (http://code.google.com/p/sdfatlib/) in your Arduino libraries directory in order to compile.

For more information on SIO2Arduino, see the website at: 
http://www.whizzosoftware.com/sio2arduino

For more information on the Arduino platform, go to:
http://www.arduino.cc