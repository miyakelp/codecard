/*
  codecard
  
  Copyright (c) 2018 Noel Portugal.  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  
  - Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

// These dependencies need to be loaeded in this specific sequence. 
#include "config.h" // contains all dependencies includes and global variables
#include "utils.h"
#include "memory.h"
#include "wifi.h"
#include "icons.h"
#include "templates.h"
#include "dataParser.h"
#include "httpClient.h"
#include "buttonEvents.h"
#include "serialBmp.h"
#include "cli.h"


void setup() {
  pinMode(WAKE_PIN, OUTPUT);
  digitalWrite(WAKE_PIN, HIGH); //immediately set wake pin to HIGH to keep the chip enabled
    
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP); 

  btn1State = digitalRead(BUTTON1_PIN);
  btn2State = digitalRead(BUTTON2_PIN);
  startTime = millis();
  delay(100);
  
  while (digitalRead(BUTTON1_PIN) == HIGH || digitalRead(BUTTON2_PIN) == HIGH){
    startTime++;
    if (startTime > (100000 * 8)) { break; }
  }
  Serial.begin(BAUD_SPEED);
  display.init();
  display.setRotation(3);
  EEPROM.begin(eepromSize);

  if (btn1State == HIGH && btn2State == HIGH){
    defaultScreen();
    help();
    wifiConnect();
    startTime = millis();
    // intial setup
    String initalsetup = getFromMemory("initalsetup");
    if (initalsetup == "true") {
      Serial.println("It is inital setup, loading defaults...");
      loadDefaults();
    }
    return;
  }
  
  if (startTime < 250000) {
    if (btn1State == HIGH){
      pushButton("a", 1);
    }else if (btn2State == HIGH){
      pushButton("b", 1);
    }
  }else  {
    if (btn1State == HIGH){
      pushButton("a", 2);
    }else if (btn2State == HIGH){
      pushButton("b", 2);
    }
  }

  shutdown();
}

void shutdown() {
  display.powerOff();
  Serial.println("Shuting down...");
  delay(100);
  digitalWrite(WAKE_PIN, LOW);
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readString();
    input.trim();
    evalInput(input);
    startTime = currentTime;
  }
  
  currentTime = millis();
  if (currentTime - startTime >= 1000 * 240)
  {
    startTime = currentTime;
    shutdown();
  }
  
}
