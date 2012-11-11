#include <EEPROM.h>

/* 
 Open Diversity / Open_Duo58 - Video based diversity software
 Idea by Rangarid
 Hardware design by Daniel
 Implementation and refinement by Nils, Nachbrenner, Rangarid, Nabazul
 
 Copyright 2011-2012 by Nils, Nachbrenner, Rangarid, Nabazul
 
 This file is part of Open Diversity
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. */

#include <Wire.h>
#include "config.h"
#include <DogLcd.h> // DOGM LCD lib from Wayoda get it from http://code.google.com/p/doglcd/
#include <EEPROM.h>

//LM1881
#define VSYNC1 2
#define VSYNC2 3

//ADG794
#define IN 10
#define EN 11

//LEDs
#define LED1 12
#define LED2 13

//Buzzer
#define BUZZER 8

//RSSI
#define RSSI1 A2
#define RSSI2 A3

//Battery
#define VBAT A0

//Button Pin
#define buttonPin A1

//LCD Backlight
#define backlightPin 9

// PCF Address
#define channelswitch B00111000

// initialize the library with the numbers of the interface SI,CLK,RS,CSB
DogLcd lcd(4, 5, 7, 6);

// Array fro Syncs
volatile int syncs[2] = {
  0,0};

//Array for Channel Bytes to the PFC and Mhz Numbers
const byte channelArray[17]={
  B00000000, B11111111, B11101110, B11011101, B11001100, B10111011, B10101010, B10011001, B10001000, B01110111, B01100110, B01010101, B01000100, B00110011, B00100010, B00010001, B00000000};

const int mhzArray[17]={
  0000, 5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945, 5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866};

// Needed Variables
int source = 1;
int buttonstate = 0;
int interval = 200;
int deltaS = 0;
volatile int activeSource = 1;
int etest = 0;
float lowvoltageE = 99;

void setup()
{
  // Boot Buzzertone
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH);

  // Check EEPROM for stored Settings

  etest = EEPROM.read(1);

  // If previous Settings stored on EEPROM read them out
  if (etest != 255) {
    beeponswitch = EEPROM.read(1);
    kfps = EEPROM.read(2);
    krssi = EEPROM.read(3);
    sensitivity = EEPROM.read(4);
    lowvoltageE = EEPROM.read(5);
    lowvoltage = lowvoltageE / 10; 
  }

  // set up the LCD type and the contrast setting for the display 
  lcd.begin(DOG_LCD_M163,30);
  lcd.noCursor();
  // fade in Backlight
  for(int fadeValue = 0 ; fadeValue <= 255; fadeValue +=5) {
    analogWrite(backlightPin, fadeValue);        
    delay(10);    
  }  


#ifdef DEBUG
  Serial.begin(9600);
#endif

  //vsync1 and vsync2
  pinMode(VSYNC1,INPUT);
  pinMode(VSYNC2,INPUT);

  //ADG794 switch inputs
  //IN LOW, EN LOW: vid0
  //IN HIGH, EN LOW: vid1
  pinMode(IN, OUTPUT);
  pinMode(EN, OUTPUT);

  // Select Source 1
  digitalWrite(IN, LOW);
  digitalWrite(EN, LOW);

  //LEDs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  //LED 0/1 HIGH for on
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);

  //interrupts to read vsync
  attachInterrupt(0, sync0, RISING);
  attachInterrupt(1, sync1, RISING); 

  // Select Channel
  Wire.begin();
  Wire.beginTransmission(channelswitch);
  Wire.write(channelArray[channel]);
  Wire.endTransmission();
}

void loop()
{
  // Turn Bootbuzzer of
  digitalWrite(BUZZER, LOW);

  long timeVideo = millis();
  long timeLCD = millis();

  long timeBuzzer = timeVideo;
  boolean alarm = false;

  while(true)
  {
    long currentTime = millis();

    if (analogRead(buttonPin) < 500) {
      buttondetect();
      switch (buttonstate) {
      case 1:
        // increase channel
        if (channel == 16) {
          channel = 1;
        }
        else {
          channel = channel + 1;
        }
        Wire.begin();
        Wire.beginTransmission(channelswitch);
        Wire.write(channelArray[channel]);
        Wire.endTransmission();
        break;
      case 3:
        // decrease channel
        if (channel == 1){
          channel = 16;
        }
        else {
          channel = channel - 1;
        }
        Wire.begin();
        Wire.beginTransmission(channelswitch);
        Wire.write(channelArray[channel]);
        Wire.endTransmission();
        break;
      case 5:
        if (screen == 1) {
          screen = 2;
          lcd.clear(); 
        }
        else {
          screen = 1;
          lcd.clear(); 
        }
        break;
      }
      delay(500);
      if (analogRead(buttonPin) < 500) {
        digitalWrite(BUZZER, HIGH);
        delay(50);
        digitalWrite(BUZZER, LOW);
        delay(50);
        digitalWrite(BUZZER, HIGH);
        delay(50);
        digitalWrite(BUZZER, LOW);
        switch (buttonstate) {
        case 1:
          autosearch();
          break;
        case 3:
          lcd.clear();
          delay(500);
          break;
        case 5:
          menu();
          break;
        }
        delay(500);
      }
    }

    //Low Voltage Alarm
    if (currentTime - timeBuzzer > 500)
    {
      if (checkVoltage())
      {
        digitalWrite(BUZZER, alarm);
        timeBuzzer = currentTime;
        alarm = !alarm;
      }
      else
      {
        timeBuzzer = currentTime;
      }
    }

    if (currentTime - timeVideo >= interval)
    {
      selectVideo();

#ifdef DEBUG
      Serial.print("DeltaS" ); 
      Serial.print(deltaS); 
      Serial.print(" Channel: ");
      Serial.print(channel);
      Serial.print(" Voltage: ");
      Serial.print(analogRead(VBAT)*(21.1/1023.0));
      Serial.print(" A1 ");
      Serial.print(analogRead(buttonPin));
#endif

      timeVideo = currentTime;
    }

    // Refresh LCD Screen
    if (currentTime - timeLCD >= lcdrefresh)
    {
      if (screen == 1) {
        lcd.setCursor(0, 0);
        lcd.print("R2:");
        lcd.setCursor(4, 0);
        lcd.print(analogRead(RSSI2));
        lcd.print("  ");
        lcd.setCursor(9, 0);
        lcd.print("R1:");
        lcd.setCursor(13, 0);
        lcd.print(analogRead(RSSI1));
        lcd.print("  ");
        lcd.setCursor(0, 1);
        lcd.print("Ch:");
        lcd.print("  ");
        lcd.setCursor(4, 1);
        lcd.print(channel);
        lcd.print(" ");
        lcd.setCursor(8, 1);
        lcd.print(mhzArray[channel]);
        lcd.setCursor(12, 1);
        lcd.print("Mhz");
        lcd.setCursor(0, 2);
        lcd.print("Bat: ");
        lcd.print(analogRead(VBAT)*(21.1/1023.0));
        lcd.print(" V");
      }
      else {
        lcd.setCursor(0, 0);
        lcd.print("R2:");
        lcd.setCursor(4, 0);
        lcd.print(analogRead(RSSI2));
        lcd.print("  ");
        lcd.setCursor(9, 0);
        lcd.print("R1:");
        lcd.setCursor(13, 0);
        lcd.print(analogRead(RSSI1));
        lcd.print("  ");
        lcd.setCursor(0, 1);
        lcd.print("Active RX: ");
        lcd.print(source);
        lcd.setCursor(0, 2);
        lcd.print("Bat: ");
        lcd.print(analogRead(VBAT)*(21.1/1023.0));
        lcd.print(" V");
      }
      timeLCD = currentTime;
    }
    delay(50);
  }
}

// Select Videosource Routine
void selectVideo()
{
  // Calculate Delta Switching
  deltaS = ((syncs[1] - syncs[0])*kfps) + ((analogRead(RSSI2) - analogRead(RSSI1))*krssi);

  //Check if we have to switch
  if (deltaS > sensitivity)
  {
    activeSource = 2;
  }
  else if (deltaS < (sensitivity*(-1)))
  {
    activeSource = 1;
  }
  // Switch source if new source needs to be selected
  if (source != activeSource)
  {
    if (activeSource == 1) {
      digitalWrite(EN, LOW);
      digitalWrite(IN, LOW);
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, LOW);
    }
    else 
    {
      digitalWrite(EN, LOW);
      digitalWrite(IN, HIGH);
      digitalWrite(LED1, LOW);
      digitalWrite(LED2, HIGH);
    }

    // Buzzer Beep ?
    if (beeponswitch == 1)
    {
      digitalWrite(BUZZER, HIGH);
      delay(5);
      digitalWrite(BUZZER, LOW); 
    }
  }
  source = activeSource;
  clearSyncs();
}

// Reset Synchs
void clearSyncs()
{
  syncs[0] = 0;
  syncs[1] = 0;
}

//Interrupt Service Routine for vsync0
void sync0()
{
  syncs[0]++;
}

//Interrupt Service Routine for vsync1
void sync1()
{
  syncs[1]++;
}

boolean checkVoltage()
{
  float voltage = analogRead(VBAT)*(21.1/1023.0);
  if (abs(voltage) <= abs(lowvoltage))
  {
    return true;
  }
  return false;
}

// Menu Routine
void menu()
{
  boolean inmenu = true;
  int menustate = 1;
  lcd.clear(); 
  lcd.setCursor(0, 1);
  lcd.print(" -- Settings -- ");
  delay(500);
  while(inmenu == true)
  {
    delay(1000);
    lcd.clear();
    while (menustate == 1)
    {
      lcd.setCursor(0, 0);
      lcd.print("> Sensitivity");
      lcd.setCursor(2, 1);
      lcd.print(sensitivity);
      lcd.print(" ");
      lcd.setCursor(2, 2);
      lcd.print("-");
      lcd.setCursor(6, 2);
      lcd.print("NEXT");
      lcd.setCursor(13, 2);
      lcd.print("+");
      if (analogRead(buttonPin) < 500) {
        buttondetect();
        delay(500);
        switch (buttonstate) {
        case 1:
          if (sensitivity < 100)
          {
            sensitivity = sensitivity + 10;
          } 
          break;
        case 3:
          if (sensitivity > 10)
          {
            sensitivity = sensitivity - 10;
          } 
          break;
        case 5:
          menustate = 2;
          lcd.clear();
          break;
        }
      }
    }
    while (menustate == 2)
    {
      lcd.setCursor(0, 0);
      lcd.print("> K-RSSI");
      lcd.setCursor(2, 1);
      lcd.print(krssi);
      lcd.print(" ");
      lcd.setCursor(2, 2);
      lcd.print("-");
      lcd.setCursor(6, 2);
      lcd.print("NEXT");
      lcd.setCursor(13, 2);
      lcd.print("+");
      if (analogRead(buttonPin) < 500) {
        buttondetect();
        delay(500);
        switch (buttonstate) {
        case 1:
          if (krssi < 10)
          {
            krssi = krssi + 1;
          } 
          break;
        case 3:
          if (krssi > 0)
          {
            krssi = krssi - 1;
          } 
          break;
        case 5:
          menustate = 3;
          lcd.clear();
          break;
        }
      }
    }
    while (menustate == 3)
    {
      lcd.setCursor(0, 0);
      lcd.print("> K-Video");
      lcd.setCursor(2, 1);
      lcd.print(kfps);
      lcd.print(" ");
      lcd.setCursor(2, 2);
      lcd.print("-");
      lcd.setCursor(6, 2);
      lcd.print("NEXT");
      lcd.setCursor(13, 2);
      lcd.print("+");
      if (analogRead(buttonPin) < 500) {
        buttondetect();
        delay(500);
        switch (buttonstate) {
        case 1:
          if (kfps < 20)
          {
            kfps = kfps + 1;
          } 
          break;
        case 3:
          if (kfps > 0)
          {
            kfps = kfps - 1;
          } 
          break;
        case 5:
          menustate = 4;
          lcd.clear();
          break;
        }
      }
    }
    while (menustate == 4)
    {
      lcd.setCursor(0, 0);
      lcd.print("> Alarm Voltage");
      lcd.setCursor(2, 1);
      lcd.print(lowvoltage, 1);
      lcd.print(" V");
      lcd.setCursor(2, 2);
      lcd.print("-");
      lcd.setCursor(6, 2);
      lcd.print("NEXT");
      lcd.setCursor(13, 2);
      lcd.print("+");
      if (analogRead(buttonPin) < 500) {
        buttondetect();
        delay(500);
        switch (buttonstate) {
        case 1:
          if (lowvoltage < 16)
          {
            lowvoltage = lowvoltage + 0.1;
          } 
          break;
        case 3:
          if (lowvoltage > 0)
          {
            lowvoltage = lowvoltage - 0.1;
          } 
          break;
        case 5:
          menustate = 5;
          lcd.clear();
          break;
        }
      }
    }
    while (menustate == 5)
    {
      lcd.setCursor(0, 0);
      lcd.print("> Beep on switch");
      lcd.setCursor(2, 1);
      lcd.print(beeponswitch);
      lcd.setCursor(2, 2);
      lcd.print("-");
      lcd.setCursor(6, 2);
      lcd.print("NEXT");
      lcd.setCursor(13, 2);
      lcd.print("+");
      if (analogRead(buttonPin) < 500) {
        buttondetect();
        delay(500);
        switch (buttonstate) {
        case 1:
          beeponswitch = 1;
          break;
        case 3:
          beeponswitch = 0;
          break;
        case 5:
          menustate = 6;
          lcd.clear();
          break;
        }
      }
    }
    while (menustate == 6)
    {
      lcd.setCursor(0, 0);
      lcd.print("> Exit Menu?");

      lcd.setCursor(0, 2);
      lcd.print("Exit");
      lcd.setCursor(6, 2);
      lcd.print("NEXT");
      lcd.setCursor(12, 2);
      lcd.print("Save");
      if (analogRead(buttonPin) < 500) {
        buttondetect();
        delay(500);
        switch (buttonstate) {
        case 1:
          menustate = 0;
          EEPROM.write(1, beeponswitch);
          EEPROM.write(2, kfps);
          EEPROM.write(3, krssi);
          EEPROM.write(4, sensitivity); 
          lowvoltageE = lowvoltage * 10;
          EEPROM.write(5, lowvoltageE);
          inmenu = false;
          break;
        case 3:
          menustate = 0;
          inmenu = false;
          break;
        case 5:
          menustate = 1;
          lcd.clear();
          break;
        }
      }
    }
  }
  lcd.clear();
}

void buttondetect()
{
  // read the input on Button pin:
  int reading = analogRead(buttonPin);
  // + button detection
  if (reading < 500 && reading > 300) {
    buttonstate = 3;
    digitalWrite(BUZZER, HIGH);
    delay(50);
    digitalWrite(BUZZER, LOW);
  }
  // - button detection
  if (reading < 200) {
    buttonstate = 5;
    digitalWrite(BUZZER, HIGH);
    delay(50);
    digitalWrite(BUZZER, LOW);
  }
  // menu button detection
  if (reading < 300 && reading > 200) {
    buttonstate = 1;
    digitalWrite(BUZZER, HIGH);
    delay(50);
    digitalWrite(BUZZER, LOW);
  }
}

// Channelsearch Routine
void autosearch()
{
  lcd.clear();
  for (int i = 1; i < 17; i++) 
  {
    digitalWrite(BUZZER, HIGH);
    delay(10);
    digitalWrite(BUZZER, LOW);
    Wire.begin();
    Wire.beginTransmission(channelswitch);
    Wire.write(channelArray[i]);
    Wire.endTransmission();
    delay(1000);
    lcd.setCursor(0, 0);
    lcd.print("R2:");
    lcd.setCursor(4, 0);
    lcd.print(analogRead(RSSI2));
    lcd.print("  ");
    lcd.setCursor(9, 0);
    lcd.print("R1:");
    lcd.setCursor(13, 0);
    lcd.print(analogRead(RSSI1));
    lcd.print("  ");
    lcd.setCursor(0, 1);
    lcd.print("Ch:");
    lcd.print("  ");
    lcd.setCursor(4, 1);
    lcd.print(i);
    lcd.print(" ");
    lcd.setCursor(8, 1);
    lcd.print(mhzArray[i]);
    lcd.setCursor(12, 1);
    lcd.print("Mhz");
    lcd.setCursor(0, 2);
    lcd.print("Autosearch..... ");
    if (analogRead(RSSI1) > 200 || analogRead(RSSI2) > 200) 
    {
      delay(500);
      channel = i;
      digitalWrite(BUZZER, HIGH);
      delay(50);
      digitalWrite(BUZZER, LOW);
      delay(50);
      digitalWrite(BUZZER, HIGH);
      delay(50);
      digitalWrite(BUZZER, LOW);
      break;
    }
  }
  lcd.clear();
}
