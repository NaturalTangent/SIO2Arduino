/*
 * sio2arduino.ino - An Atari 8-bit device emulator for Arduino.
 *
 * Copyright (c) 2012 Whizzo Software LLC (Daniel Noguerol)
 * ** Minor changes by Elias Zacarias (January 2017)
 *
 * This file is part of the SIO2Arduino project which emulates
 * Atari 8-bit SIO devices on Arduino hardware.
 *
 * SIO2Arduino is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * SIO2Arduino is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SIO2Arduino; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "config.h"
#include <SdFat.h>
#include <SdFatUtil.h>
#include "atari.h"
#include "sio_channel.h"
#include "disk_drive.h"
#ifdef LCD_DISPLAY
#include <LiquidCrystal.h>
#endif

/* Forward declarations */
DriveStatus* getDeviceStatus(int deviceId);
SectorDataInfo* readSector(int deviceId, unsigned long sector, byte *data);
boolean writeSector(int deviceId, unsigned long sector, byte* data, unsigned long length);
boolean format(int deviceId, int density);
int getFileList(int startIndex, int count, FileEntry *entries);
void mountFileIndex(int deviceId, int ix);
void changeDirectory(int ix);

/**
 * Global variables
 */
DriveAccess driveAccess(getDeviceStatus, readSector, writeSector, format);
DriveControl driveControl(getFileList, mountFileIndex, changeDirectory);
SIOChannel sioChannel(PIN_ATARI_CMD, &SIO_UART, &driveAccess, &driveControl);
Sd2Card card;
SdVolume volume;
SdFile currDir;
SdFile file; // TODO: make this unnecessary
DiskDrive drive1;
#ifdef SELECTOR_BUTTON
boolean isSwitchPressed = false;
unsigned long lastSelectionPress;
boolean isFileOpened;
#endif
#ifdef LCD_DISPLAY
LiquidCrystal lcd(PIN_LCD_RD,PIN_LCD_ENABLE,PIN_LCD_DB4,PIN_LCD_DB5,PIN_LCD_DB6,PIN_LCD_DB7);
#endif

#ifdef LCD_KEYPAD_SHEILD

int lcd_key     = 0;
int adc_key_in  = 0;
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// read the buttons
int read_LCD_buttons()
{

 adc_key_in = analogRead(0);      // read the value from the sensor
 // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
 // we add approx 50 to those values and check to see if we are close
 if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
 // For V1.1 us this threshold
 if (adc_key_in < 50)
 {
   LOG_MSG("btnRIGHT\n");
   return btnRIGHT;
 }

 if (adc_key_in < 250)
 {
   LOG_MSG("btnUP\n");
   return btnUP;
 }

 if (adc_key_in < 450)
 {
   LOG_MSG("btnDOWN\n");
   return btnDOWN;
 }

 if (adc_key_in < 650)
 {
   LOG_MSG("btnLEFT\n");
   return btnLEFT;
 }

 if (adc_key_in < 850)
 {
   LOG_MSG("btnSELECT\n");
   return btnSELECT;
 }

 // For V1.0 comment the other threshold and use the one below:
/*
 if (adc_key_in < 50)   return btnRIGHT;
 if (adc_key_in < 195)  return btnUP;
 if (adc_key_in < 380)  return btnDOWN;
 if (adc_key_in < 555)  return btnLEFT;
 if (adc_key_in < 790)  return btnSELECT;
*/


 return btnNONE;  // when all others fail, return this...
}

#endif


void setup() {
#ifdef DEBUG
  // set up logging serial port
  LOGGING_UART.begin(115200);
#endif

  // initialize serial port to Atari
  SIO_UART.begin(19200);

#ifndef LCD_KEYPAD_SHEILD
  // set pin modes
  #ifdef SELECTOR_BUTTON
  pinMode(PIN_SELECTOR, INPUT);
  #endif
  #ifdef RESET_BUTTON
  pinMode(PIN_RESET, INPUT);
  #endif
#endif

  #ifdef LCD_DISPLAY
  // set up LCD if appropriate
  lcd.begin(16, 2);
  lcd.print("SIO2Arduino");
  lcd.setCursor(0,1);
  #endif

  // initialize SD card
  LOG_MSG("Initializing SD card...");
  pinMode(PIN_SD_CS, OUTPUT);

  if (!card.init(SPI_HALF_SPEED, PIN_SD_CS)) {
    LOG_MSG_CR(" failed.");
    #ifdef LCD_DISPLAY
      lcd.print("SD Init Error");
    #endif
    return;
  }

  if (!volume.init(&card)) {
    LOG_MSG_CR(" failed.");
    #ifdef LCD_DISPLAY
      lcd.print("SD Volume Error");
    #endif
    return;
  }

  if (!currDir.openRoot(&volume)) {
    LOG_MSG_CR(" failed.");
    #ifdef LCD_DISPLAY
      lcd.print("SD Root Error");
    #endif
    return;
  }

  LOG_MSG_CR(" done.");
  #ifdef LCD_DISPLAY
    lcd.print("READY");
  #endif

#ifdef LCD_KEYPAD_SHEILD
  if(read_LCD_buttons() == btnLEFT)
  {
    mountFilename(0, "AUTORUN.ATR");
  }
#else
  #ifdef RESET_BUTTON
  // watch the reset button
  if (digitalRead(PIN_RESET) == HIGH) {
    mountFilename(0, "AUTORUN.ATR");
  }
  #endif
#endif
}

void loop() {
  // let the SIO channel do its thing
  sioChannel.runCycle();

  boolean newSwitchState = (read_LCD_buttons() == btnSELECT);

   #ifdef LCD_KEYPAD_SHEILD
    if(newSwitchState&& (isSwitchPressed == false) && millis() - lastSelectionPress > 250) {
      lastSelectionPress = millis();
      changeDisk(0);
    }
    isSwitchPressed = newSwitchState;
   #else
     #ifdef SELECTOR_BUTTON
     // watch the selector button (accounting for debounce)
     if ((digitalRead(PIN_SELECTOR) == HIGH) && (isSwitchPressed == false) && millis() - lastSelectionPress > 250) {
       lastSelectionPress = millis();
       changeDisk(0);
     }
     isSwitchPressed = digitalRead(PIN_SELECTOR);
     #endif
   #endif

  #ifdef ARDUINO_TEENSY
    if (SIO_UART.available())
      SIO_CALLBACK();
  #endif
}

void SIO_CALLBACK() {
  // inform the SIO channel that an incoming byte is available
  sioChannel.processIncomingByte();
}

DriveStatus* getDeviceStatus(int deviceId) {
  return drive1.getStatus();
}

SectorDataInfo* readSector(int deviceId, unsigned long sector, byte *data) {
  if (drive1.hasImage()) {
    return drive1.getSectorData(sector, data);
  } else {
    return NULL;
  }
}

boolean writeSector(int deviceId, unsigned long sector, byte* data, unsigned long length) {
  return (drive1.writeSectorData(sector, data, length) == length);
}

boolean format(int deviceId, int density) {
  char name[13];

  // get current filename
  file.getFilename(name);

  // close and delete the current file
  file.close();
  file.remove();

  LOG_MSG("Remove old file: ");
  LOG_MSG_CR(name);

  // open new file for writing
  file.open(&currDir, name, O_RDWR | O_SYNC | O_CREAT);

  LOG_MSG("Created new file: ");
  LOG_MSG_CR(name);

  // allow the virtual drive to format the image (and possibly alter its size)
  if (drive1.formatImage(&file, density)) {
    // set the new image file for the drive
    drive1.setImageFile(&file);
    return true;
  } else {
    return false;
  }
}

void changeDisk(int deviceId) {
  dir_t dir;
  char name[13];
  boolean imageChanged = false;

  while (!imageChanged) {
    // get next dir entry
    int8_t result = currDir.readDir((dir_t*)&dir);

    // if we got back a 0, rewind the directory and get the first dir entry
    if (!result) {
      currDir.rewind();
      result = currDir.readDir((dir_t*)&dir);
    }

    // if we have a valid file response code, open it
    if (result > 0 && isValidFilename((char*)&dir.name)) {
      createFilename(name, (char*)dir.name);
      imageChanged = mountFilename(deviceId, name);
    }
  }
}

boolean isValidFilename(char *s) {
  return (  s[0] != '.' &&    // ignore hidden files
            s[0] != '_' && (  // ignore bogus files created by OS X
             (s[8] == 'A' && s[9] == 'T' && s[10] == 'R')
          || (s[8] == 'X' && s[9] == 'F' && s[10] == 'D')
#ifdef PRO_IMAGES
          || (s[8] == 'P' && s[9] == 'R' && s[10] == 'O')
#endif
#ifdef ATX_IMAGES
          || (s[8] == 'A' && s[9] == 'T' && s[10] == 'X')
#endif
#ifdef XEX_IMAGES
          || (s[8] == 'X' && s[9] == 'E' && s[10] == 'X')
#endif
          )
        );
}

void createFilename(char* filename, char* name) {
  for (int i=0; i < 8; i++) {
    if (name[i] != ' ') {
      *(filename++) = name[i];
    }
  }
  if (name[8] != ' ') {
    *(filename++) = '.';
    *(filename++) = name[8];
    *(filename++) = name[9];
    *(filename++) = name[10];
  }
  *(filename++) = '\0';
}

/**
 * Returns a list of files in the current directory.
 *
 * startIndex = the first valid file in the directory to start from
 * count = how many files to return
 * entries = a pointer to the a FileEntry array to hold the returned data
 */
int getFileList(int startIndex, int count, FileEntry *entries) {
  dir_t dir;
  int currentEntry = 0;

  currDir.rewind();

  int ix = 0;
  while (ix < count) {
    if (currDir.readDir((dir_t*)&dir) < 1) {
      break;
    }
    if (isValidFilename((char*)&dir.name) || (DIR_IS_SUBDIR(&dir) && dir.name[0] != '.')) {
      if (currentEntry >= startIndex) {
        memcpy(entries[ix].name, dir.name, 11);
        if (DIR_IS_SUBDIR(&dir)) {
          entries[ix].isDirectory = true;
        } else {
          entries[ix].isDirectory = false;
        }
        ix++;
      }
      currentEntry++;
    }
  }

  return ix;
}

/**
 * Changes the SD card directory.
 *
 * ix = index number (or -1 to go to parent directory)
 */
void changeDirectory(int ix) {
  FileEntry entries[1];
  char name[13];
  SdFile subDir;

  if (ix > -1) {
    getFileList(ix, 1, entries);
    createFilename(name, entries[0].name);
    if (subDir.open(&currDir, name, O_READ)) {
      currDir = subDir;
    }
  } else {
    if (subDir.openRoot(&volume)) {
      currDir = subDir;
    }
  }
}

/**
 * Mount a file with the given index number.
 *
 * deviceId = the drive ID
 * ix = the index of the file to mount
 */
void mountFileIndex(int deviceId, int ix) {
  FileEntry entries[1];
  char name[13];

  // figure out what filename is associated with the index
  getFileList(ix, 1, entries);

  // build a full 8.3 filename
  createFilename(name, entries[0].name);

  // mount the image
  mountFilename(deviceId, name);
}

/**
 * Mount a file with the given name.
 *
 * deviceId = the drive ID
 * name = the name of the file to mount
 */
boolean mountFilename(int deviceId, char *name) {
  // close previously open file
  if (file.isOpen()) {
    file.close();
  }

  if (file.open(&currDir, name, O_RDWR | O_SYNC) && drive1.setImageFile(&file)) {
    LOG_MSG_CR(name);

    #ifdef LCD_DISPLAY
    lcd.clear();
    lcd.print(name);
    lcd.setCursor(0,1);
    #endif

    return true;
  }

  return false;
}
