#pragma once
#include <Arduino.h>   // bringt delay(), ESP, Serial, String, millis(), etc.
#include <cstring>     // strcmp, strcpy
#include <cstdlib>     // atoi
#ifdef ARDUINO
  #include <strings.h> // strcasecmp (POSIX), auf Arduino/ESP32 meist vorhanden
#endif
#include "esp_heap_caps.h"

// ########## TTY ##########
static const uint8_t MAX_LINE  = 96;
static const uint8_t MAX_TOKENS = 10;
// global / static: aktueller Mode (Default nach Wunsch)
static int g_mode = 0;

static void cmd_HELP() { 
    Serial.println("HELP ...        - zeigt diesen Hilfetext");
    Serial.println("R               - ESP32 Reset (Restart)");
    Serial.println("M [x]           - mode anzeigen oder auf x setzen und anzeigen");
    Serial.println("MEM             - zeigt Speicher (Heap/PSRAM)");
    Serial.println("IP              - zeigt aktuelle IP (WLAN)");
}

static void cmd_MODE() {    
    Serial.print("mode=");
    Serial.println(g_mode);
}

static void cmd_SETMODE(int mode) {
    g_mode = mode;
    cmd_MODE(); // nach dem Setzen direkt anzeigen
}

static void cmd_MEM() {
  Serial.println("MEM:");
  Serial.printf("  FreeHeap:        %u bytes\n", ESP.getFreeHeap());
  Serial.printf("  MaxAllocHeap:    %u bytes\n", ESP.getMaxAllocHeap());
  Serial.printf("  Internal Free:   %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("  Internal Largest:%u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  Serial.printf("  PSRAM total:     %u bytes\n", ESP.getPsramSize());
  Serial.printf("  PSRAM free:      %u bytes\n", ESP.getFreePsram());
}
static void cmd_IP() {
  Serial.println("IP:");
  wl_status_t st = WiFi.status();
  Serial.printf("  Status:         %d\n", (int)st);
  if (st != WL_CONNECTED) {
    Serial.println("  Nicht verbunden.");
    return;
  }
  Serial.printf("  SSID:           %s\n", WiFi.SSID().c_str());
  Serial.printf("  RSSI:           %d dBm\n", WiFi.RSSI());
  Serial.printf("  LocalIP:        %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("  Gateway:        %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("  Subnet:         %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf("  DNS1:           %s\n", WiFi.dnsIP(0).toString().c_str());
  Serial.printf("  MAC:            %s\n", WiFi.macAddress().c_str());
}

static void cmd_ndf(const char* cmd) {
        Serial.print("Unbekannter Befehl: ");
        Serial.println(cmd);
        Serial.println("Tippe HELP");
}

void init_TTY(){
  Serial.println("init ESP-TTY");
  Serial.println("> ");
}
void task_TTY() {
  static char lineBuf[MAX_LINE];
  static uint8_t pos = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;          // optional: CR ignorieren
    if (c == '\b' || c == 0x7F) {       // Backspace unterstützen (Terminal abhängig: 0x08 oder 0x7F)
      if (pos > 0) {
        pos--;                  // Echo im Terminal "löschen"
        Serial.print("\b \b");        
      }
      continue;
    }
    if (c != '\n') Serial.print(c);       // Echo (optional)
    if (c == '\n') {              // Zeilenende => verarbeiten
      Serial.println();             // saubere neue Zeile im Terminal
      lineBuf[pos] = '\0';
      pos = 0;
      char* argv[MAX_TOKENS];         // --- Tokenisieren an Leerzeichen ---
      int argc = 0;
      char* p = lineBuf;            // führende Leerzeichen überspringen
      while (*p == ' ') p++;
      while (*p != '\0' && argc < MAX_TOKENS) {
        argv[argc++] = p;                   // Start des Tokens
        while (*p != '\0' && *p != ' ') p++;    // bis Leerzeichen / Ende
        if (*p == '\0') break;
        *p = '\0';                          // Token terminieren
        p++;
        while (*p == ' ') p++;              // mehrere Leerzeichen überspringen
      }
      if (argc == 0) {
        Serial.print("> ");
        return;
      }

      if (strcasecmp(argv[0], "HELP") == 0)     { cmd_HELP();      } 
      else if (strcasecmp(argv[0], "R") == 0)   { Serial.println("Reset...");  delay(100); ESP.restart(); }
      else if (strcasecmp(argv[0], "M") == 0) {
        if (argc < 2) {cmd_MODE(); } 
        else          { int mode = atoi(argv[1]); cmd_SETMODE(mode);}}
      else if (strcasecmp(argv[0], "MEM") == 0) { cmd_MEM();}
      else if (strcasecmp(argv[0], "IP") == 0)  { cmd_IP(); }
      else                                      { cmd_ndf(argv[0]);}
      Serial.print("> ");
      continue;
    }
    if (pos < MAX_LINE - 1) {         // Normales Zeichen puffern (Overflow-Schutz)
      lineBuf[pos++] = c;
    } else {                  // Zeile zu lang -> reset
      pos = 0;
      Serial.println();
      Serial.println("Zeile zu lang.");
      Serial.print("> ");
    }
  }
}