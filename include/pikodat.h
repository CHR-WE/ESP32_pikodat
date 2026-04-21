#define LED_PIN   13
#define LED_COUNT 13
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
uint8_t LEDdim = 26; // ~10% von 255
unsigned long long ptick = 0;

// ########## OUTPUT ##########
uint32_t warmWhite(uint8_t brightness /*0..255*/) {
  uint8_t r = (uint16_t)255 * brightness / 255;
  uint8_t g = (uint16_t)160 * brightness / 255;
  uint8_t b = (uint16_t)60  * brightness / 255;
  return strip.Color(r, g, b);
}
uint32_t randomAllowedColor() {
  // 0..3
  switch (random(0, 4)) {
    case 0: return warmWhite(255);                 // warmweiß
    case 1: return strip.Color(255, 0, 0);         // grü
    case 2: return strip.Color(0, 255, 0);         // nrot
    default:return strip.Color(255, 255, 0);       // gelb
  }
}
uint32_t Color_G() {return strip.Color(255, 0, 0);}
uint32_t Color_R() {return strip.Color(0, 255, 0);}

void init_piOUT(){
  strip.begin();
  strip.setBrightness(255);
  strip.clear();
  //for (int 0 = 1; i < LED_COUNT; i++) strip.setPixelColor(i, 0);
  strip.show();
}

void task_piOUT_0(){
  static unsigned long last = 0;
  const unsigned long interval = 500;
  unsigned long now = millis();
  if (now - last < interval) return;
  last = now;

  int idxOff = random(0, LED_COUNT);
  int idxOn  = random(0, LED_COUNT);
  if (random(100) < 50) {// Dieser Code wird mit 30 % Wahrscheinlichkeit ausgeführt
    strip.setPixelColor(idxOn, randomAllowedColor());   // Eine an mit Zufallsfarbe
  }
  strip.setPixelColor(idxOff, 0);                     // Eine aus (muss nicht an gewesen sein)

  //strip.setPixelColor(LED_COUNT-1, 0); 
  strip.setPixelColor(LED_COUNT-2, 0); 
  //if(ptick%2==0 ){strip.setPixelColor(LED_COUNT-1, Color_R()); }
  if(ptick%2==1 ){strip.setPixelColor(LED_COUNT-2, Color_G()); }

  strip.show();
}

// Zufälliges grünes Muster, das pro Aufruf um 1 weiter wandert (Ring)
void task_piOUT1() {
  static unsigned long last = 0;
  const unsigned long interval = 150;
  static uint16_t mask = 0;   // 13-bit Muster (für LED_COUNT=13)
  static uint8_t offset = 0;
  unsigned long now = millis();
  if (now - last < interval) return;
  last = now; 
  if (mask == 0 || (ptick % 32) == 0) { // Neues Zufallsmuster ab und zu (oder beim Start)
    mask = 0;
    for (uint8_t i = 0; i < LED_COUNT; i++) {if (random(100) < 25) mask |= (1U << i); }
    if (mask == 0) mask = (1U << random(0, LED_COUNT));
  }
  for (uint8_t i = 0; i < LED_COUNT; i++) {
    uint8_t src = (i + LED_COUNT - offset) % LED_COUNT;
    bool on = (mask >> src) & 1U;
    strip.setPixelColor(i, on ? Color_G() : 0);
  }
  strip.show();
  offset = (offset + 1) % LED_COUNT;
}


// ########## INPUT ##########
constexpr uint8_t IN_COUNT = 11;
const uint8_t pins[IN_COUNT] = {14,27,26,25,33,32,18,19,21,22,23};

void init_piIN(){
  for (uint8_t i = 0; i < IN_COUNT; i++) {
    pinMode(pins[i], INPUT_PULLDOWN);
  }

  Serial.println("ESP32 Inputs ready (INPUT_PULLDOWN).");
  Serial.print("Pin order: ");
  for (uint8_t i = 0; i < IN_COUNT; i++) {
    Serial.print(pins[i]);
    if (i < IN_COUNT - 1) Serial.print(',');
  }
  Serial.println();
}
void task_piIN_0() {
  static char lastBits[IN_COUNT + 1] = {0};  // merkt sich den letzten ausgegebenen Zustand
  char bits[IN_COUNT + 1];
  for (uint8_t i = 0; i < IN_COUNT; i++) {
    bits[i] = digitalRead(pins[i]) ? '1' : '0';
  }
  bits[IN_COUNT] = '\0';
  if (strcmp(bits, lastBits) != 0) {         // nur wenn geändert
    strcpy(lastBits, bits);
    Serial.print("IN:");
    Serial.println(bits);
  }
}

void init_PIKODAT(){
  Serial.println("init PikoDAT");
  init_piIN();
  init_piOUT();

}

void task_PIKODAT() {
  ptick++;  
  if(g_mode== 0){
    task_piOUT_0();
    task_piIN_0();
  }
  else if (g_mode == 1) {
    task_piOUT1();
    task_piIN_0();
  }
}
