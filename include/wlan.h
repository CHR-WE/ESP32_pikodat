#pragma once
#include "secrets.h"
// ########## WiFi + OTA ##########
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "esp_wpa2.h"     // WPA2-Enterprise
#include "esp_wifi.h"
#include "start.htm"  
WebServer server(80);

//#include "kenbak.htm"  
//#include "kenbak1.h"
//#include "ajax.h"



// --- Statische IP für RZalpha ---
IPAddress RZ_IP     (192, 168, 5, 111);
IPAddress RZ_GW     (192, 168, 5, 1);
IPAddress RZ_SUBNET (255, 255, 255, 0);
IPAddress RZ_DNS1   (8, 8, 8, 8);
const int KNOWN_N = sizeof(known) / sizeof(known[0]);

// --- Auswahl: eduroam bevorzugen? ---
// Standard: false (zuerst bekannte PSK-Netze, dann eduroam)
// Setze z.B. im Sketch einmalig: setPreferEduroam(true);
static bool gPreferEduroam = false;   // true;  
inline void setPreferEduroam(bool v) { gPreferEduroam = v; }
inline bool getPreferEduroam() { return gPreferEduroam; }


struct ScanChoice {
  int knownIndex;     // -1 wenn keins
  bool eduroamSeen;
  int bestKnownRssi;
  int eduroamRssi;
};

ScanChoice scanBestKnownOrEduroam() {
  ScanChoice c;
  c.knownIndex = -1;
  c.eduroamSeen = false;
  c.bestKnownRssi = -1000;
  c.eduroamRssi = -1000;
  int n = WiFi.scanNetworks(false, true);
  if (n <= 0) return c;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    // eduroam merken
    if (ssid == EDUROAM_SSID) {
      c.eduroamSeen = true;
      if (rssi > c.eduroamRssi) c.eduroamRssi = rssi;
    }
    // bekannte PSK-Netze prüfen
    for (int k = 0; k < KNOWN_N; k++) {
      if (ssid == known[k].ssid) {
        if (rssi > c.bestKnownRssi) {
          c.bestKnownRssi = rssi;
          c.knownIndex = k;
        }
      }
    }
  }
  WiFi.scanDelete();
  return c;
}


// Minimal-HTML: Startseite + Upload-Formular
/*
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<title>ESP32 Web + OTA</title></head><body>
<h2>ESP32 läuft</h2>
<p><a href="/update">OTA Update</a></p>
</body></html>)HTML";

const char UPDATE_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<title>ESP32 OTA</title></head><body>
<h2>Firmware-Update</h2>
<form method="POST" action="/update" enctype="multipart/form-data"><input type="file" name="update" accept=".bin" required> <input type="submit" value="Upload"></form>
<p>Nach erfolgreichem Upload startet der ESP32 neu.</p>
</body></html>)HTML";
*/

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}


/*
void handleUpdateForm() {
  server.send(200, "text/html; charset=utf-8", UPDATE_HTML);
}
void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    // Serial.printf("Update start: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // nutzt freie OTA-Partition
      Update.printError(Serial); }}
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {Update.printError(Serial);}} 
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { // true = setze Boot-Partition auf neue Firmware
      // Serial.printf("Update success, size: %u\n", upload.totalSize);
    } else {Update.printError(Serial);}} 
  else if (upload.status == UPLOAD_FILE_ABORTED) {Update.abort();}}
*/
bool connectEduroam() {
  Serial.println("Verbinde mit eduroam (WPA2-Enterprise) ...");
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_STA);
  // SSID setzen (ohne Passwort!)
  WiFi.begin(EDUROAM_SSID);
  // Enterprise-Konfig
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)EDUROAM_IDENTITY, strlen(EDUROAM_IDENTITY));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t*)EDUROAM_USERNAME, strlen(EDUROAM_USERNAME));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t*)EDUROAM_PASSWORD, strlen(EDUROAM_PASSWORD));
  // WPA2 Enterprise aktivieren
  esp_wifi_sta_wpa2_ent_enable();
  unsigned long start = millis();
  Serial.print("WLAN verbinden");
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("eduroam OK, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("eduroam Verbindung fehlgeschlagen.");
  return false;
}

void init_WiFi() {
  Serial.println("init WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);
  Serial.println("Scan nach bekannten WLANs / eduroam...");
  ScanChoice choice = scanBestKnownOrEduroam();
  bool connected = false;
  bool triedEduroam = false;

// Reihenfolge abhängig von gPreferEduroam:
// - true  : zuerst eduroam (wenn sichtbar), danach bekannte PSK-Netze
// - false : zuerst bekannte PSK-Netze, danach eduroam (wie bisher)
if (getPreferEduroam() && choice.eduroamSeen) {
  triedEduroam = true;
  connected = connectEduroam();
}

// 1) bekannte PSK-Netze (entweder als Primärwahl oder als Fallback nach eduroam)
if (!connected && choice.knownIndex >= 0) {
  Serial.print("Verbinde mit bekanntem WLAN: ");
  Serial.println(known[choice.knownIndex].ssid);
  // DHCP "resetten" (wichtig, falls vorher statisch aktiv war)
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
/*
  if (strcmp(known[choice.knownIndex].ssid, "RZalpha") == 0) {
    Serial.println("RZalpha erkannt -> statische IP setzen");
    if (!WiFi.config(RZ_IP, RZ_GW, RZ_SUBNET, RZ_DNS1, RZ_DNS1)) {
      Serial.println("WiFi.config() fehlgeschlagen!");
    }
  }
*/
  WiFi.begin(known[choice.knownIndex].ssid, known[choice.knownIndex].pass);

  unsigned long start = millis();
  Serial.print("WLAN verbinden");
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  connected = (WiFi.status() == WL_CONNECTED);
  if (connected) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Bekanntes WLAN fehlgeschlagen.");
  }
}

// 2) eduroam (Fallback)
if (!connected && choice.eduroamSeen && !triedEduroam) {
  connected = connectEduroam();
  triedEduroam = true;
}

  if (!connected) {
    Serial.println("Kein WLAN verbunden.");
    return;
  }

  server.on("/", HTTP_GET, handleRoot);
/*
  server.on("/ajax", HTTP_GET, [](){ handleAjaxPage(server); });
  server.on("/api/gpio", HTTP_GET, [](){ handleApiGpio(server); });
  server.on("/api/ui", HTTP_POST, [](){ handleApiUi(server); });
*/
/*
server.on("/kenbak", HTTP_GET, handleKenbak);
 server.on("/api/remote", HTTP_POST, handleApiRemote);
server.on("/api/key",    HTTP_POST, handleApiKey);
server.on("/api/state",  HTTP_GET,  handleApiState);
*/
/*
  server.on("/update", HTTP_GET, handleUpdateForm);// GET zeigt Formular
  server.on("/update", HTTP_POST, []() {    // POST nimmt Upload an
      bool ok = !Update.hasError();         // Antwort nach Upload (wichtig: erst senden, dann reboot)
      server.send(ok ? 200 : 500, "text/plain", ok ? "OK, reboot..." : "FAIL");
      delay(200);
      if (ok) ESP.restart();
    },
    handleUpdateUpload
  );
  */
  server.begin();
  Serial.println("HTTP Server gestartet.");
}

void task_WiFi(){
  server.handleClient();
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 10000) return;
  lastCheck = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WLAN weg -> reconnect...");
    init_WiFi();
  }
}
