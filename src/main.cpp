#include <Arduino.h>
#include <cstring>
#include "wlan.h"
#include "tty.h"
#include "pikodat.h"


bool  LED_State = false;
unsigned long long tick = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("PikoDAT 0.01 (" __DATE__ " " __TIME__ ")");
  init_TTY();
  init_PIKODAT();
  init_WiFi(); 
  Serial.println("init complete .....");
}

void loop() {
  tick++;
  if(tick%503==0 ){LED_State = !LED_State;}    
  if(tick%509==0 ){task_PIKODAT();}
  if(tick%5000==0){Serial.print(".");} 
  task_WiFi();
  task_TTY();
}

