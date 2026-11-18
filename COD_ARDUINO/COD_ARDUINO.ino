#include "DHT.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Stepper.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h> 
#define TAPO_DEBUG_MODE 
#include "tapo_device.h" 
// ==========================================
// --- 0. SETARI RETEA & BAZA DATE ---
// ==========================================
const char* ssid = "PETRICA";           
const char* password = "lbrh9348"; 
const char* XAMPP_IP = "http://10.176.199.203"; 
WebServer server(80); 

// ==========================================
// --- 1. SETARI HARDWARE ---
// ==========================================
#define DHTPIN 13          
#define DHTTYPE DHT11     
DHT dht(DHTPIN, DHTTYPE);
#define RELAY_PIN 26          // RELEU VENTILATOR
#define RELAY_LIGHT_PIN 27    // RELEU LUMINĂ 
// --- SETARI PWM VENTILATOR ---
#define FAN_PWM_PIN 16        
const int fanFreq = 25000;    
const int fanResolution = 8;  
const int speedNormal = 128;  // Viteza cand e pe AUTO sau MANUAL ON
#define TACH_PIN 17                  
volatile int pulseCount = 0;         // Contorul de impulsuri
unsigned long lastRPMCalcTime = 0;
int rpmCurent = 0;                  
// --- INIȚIALIZARE ECRAN TFT ---
TFT_eSPI tft = TFT_eSPI(); 
// --- CULORI DESIGN MODERN ---
#define FUNDAL_DARK   0x10A2 
#define CARD_DARK     0x2124 
#define TEXT_ALB      0xFFFF
#define TEXT_GRI      0xAD75 
#define ALB_ELECTRIC  0x051D
#define VERDE_ON      0x07E0
#define ROSU_OFF      0xF800
#define GALBEN_MOD    0xFFE0
// --- SETARI MOTOR STEPPER ---
const int pasiPerRotatie = 2048;  
const int pasiSfertDeTur = 512;   
Stepper motorUsa(pasiPerRotatie, 32, 33, 25, 14); 

// ==========================================
// --- 2. SETARI BEC TAPO L530 ---
// ==========================================
TapoDevice becTapo;
const char* ip_bec = "192.168.0.146"; 
const char* email_tapo = "adigiurgiu7@yahoo.com"; 
const char* parola_tapo = "!Dorel64Caine";     
bool tapoConectat = false; 

// ==========================================
// --- 3. VARIABILE SISTEM ---
// ==========================================
float tempCurenta = 0.0;float tempDorita = 26.0; 
bool ventilatorPornit = false;String modVentilatie = "AUTO"; 
int paginaEcran = 0;bool releuLuminaPornit = true; 
bool becSoftPornit = true;int luminozitate = 100;        
String culoareBec = "#FFFFFF";unsigned long becUptimeStart = 0; 
float consumTotalWh = 0.0;bool usaDeschisa = false; 
bool confirmaReset = false;String mesajReset = "";
unsigned long mesajResetExpira = 0;unsigned long lastReadTime = 0; 
bool redeseneazaEcran = true;unsigned long lastSyncTime = 0;
unsigned long blocSyncPanaLa = 0;String mesajVentTemp = "";
unsigned long mesajVentExpira = 0;

// ==========================================
// --- 3b. DIAGNOSTIC (consola de comenzi din site) ---
// ==========================================
// Comenzile ajung la ESP32 prin diag.php (XAMPP), care verifica pe server ca utilizatorul e admin.
// ESP32 accepta /cmd doar de la IP-ul XAMPP si doar cu cheia de mai jos.
// Daca schimbi IP-ul XAMPP sau cheia, modifica la fel si in diag.php.
const char* DIAG_KEY = "SmartHome2026-diag";       // identica cu $CHEIE_ESP din diag.php
const char* DIAG_IP_XAMPP = "10.176.199.203";       // IP-ul calculatorului cu XAMPP
bool ecranOprit = false;                           // true dupa comanda 28h (ignoram atingerile)
unsigned long ultimulAccesDHT = 0;                 // DHT11 accepta max. o citire pe secunda

// Rezultatul unei comenzi de diagnostic (structura e definita aici, inainte de prima functie,
// ca sa o poata folosi prototipurile generate automat de Arduino IDE).
struct DiagRez {
  String descriere;
  String linii[12];
  int nrLinii = 0;
  String status = "EROARE";     // OK / EROARE / NEVERIFICAT
  String motiv;
  unsigned long ms = 0;         // durata masurata; daca ramane 0, se foloseste durata totala
  void adauga(const String& l) { if (nrLinii < 12) linii[nrLinii++] = l; }
};

// ==========================================
// --- 4. TRACKER JURNAL IP & VITEZA DINAMICA ---
// ==========================================
#define MAX_CLIENTI 5
struct ClientIP { String ip; unsigned long lastSeen; };
ClientIP jurnalClienti[MAX_CLIENTI];

void inregistreazaClient() {
  String ipCurent = server.client().remoteIP().toString();
  if (ipCurent == "" || ipCurent == "(IP unset)") return;
  int foundIndex = -1, oldestIndex = 0;
  unsigned long oldestTime = 0xFFFFFFFF;

  for (int i = 0; i < MAX_CLIENTI; i++) {
    if (jurnalClienti[i].ip == ipCurent) { foundIndex = i; break; }
    if (jurnalClienti[i].lastSeen < oldestTime || jurnalClienti[i].ip == "") 
    { oldestTime = jurnalClienti[i].lastSeen; oldestIndex = i; }
  }
  if (foundIndex != -1) { jurnalClienti[foundIndex].lastSeen = millis(); } 
  else { jurnalClienti[oldestIndex].ip = ipCurent; jurnalClienti[oldestIndex].lastSeen = millis(); }
}
int citesteClientiActivi() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  HTTPClient http;
  String url = String(XAMPP_IP) + "/smarthome/sesiune_activa.php?action=count";
  http.begin(url);
  http.setConnectTimeout(1500);
  http.setTimeout(1500);
  int httpCode = http.GET();
  int nr = 0;
  if (httpCode == 200) {
    String payload = http.getString();
    int idx = payload.indexOf("\"activi\":");
    if (idx != -1) {
      nr = payload.substring(idx + 9).toInt();
    }
  }
  http.end();
  return nr;
}

int calculeazaViteza() {
  int clientiActivi = citesteClientiActivi();
  if (clientiActivi == 0) return 0;
  if (clientiActivi == 1) return 64;
  if (clientiActivi == 2) return 128;
  if (clientiActivi == 3) return 191;
  return 255;
}

// ==========================================
// --- 5. FUNCTIE SINCRONIZARE BAZA DATE ---
// ==========================================
void sincronizareSetariDB() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(XAMPP_IP) + "/smarthome/get_settings.php";
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        float vechiPrag=tempDorita; String vechiMod=modVentilatie;
        bool vechiReleu=releuLuminaPornit; bool vechiBec=becSoftPornit;
        int vechiLum=luminozitate; String vechiCul=culoareBec;
        bool vechiUsa=usaDeschisa;
        if (doc.containsKey("vent_prag")) tempDorita = doc["vent_prag"].as<String>().toFloat(); 
        if (doc.containsKey("vent_mod")) { modVentilatie = doc["vent_mod"].as<String>(); }
        if (doc.containsKey("lumina_releu")) releuLuminaPornit = (doc["lumina_releu"].as<String>() == "ON");
        if (doc.containsKey("lumina_bec")) becSoftPornit = (doc["lumina_bec"].as<String>() == "ON");
        if (doc.containsKey("lumina_intensitate")) luminozitate = doc["lumina_intensitate"].as<String>().toInt();
        if (doc.containsKey("lumina_culoare")) culoareBec = doc["lumina_culoare"].as<String>();
        if (doc.containsKey("usa_status")) usaDeschisa = (doc["usa_status"].as<String>() == "OPEN");
        if (releuLuminaPornit) digitalWrite(RELAY_LIGHT_PIN, LOW); 
        else digitalWrite(RELAY_LIGHT_PIN, HIGH);
        if (vechiPrag != tempDorita || vechiMod != modVentilatie ||
            vechiReleu != releuLuminaPornit || vechiBec != becSoftPornit ||
            vechiLum != luminozitate || vechiCul != culoareBec ||
            vechiUsa != usaDeschisa) {
          redeseneazaEcran = true; 
        }
      }
    }
    http.end();
  }
}

// ==========================================
// --- 5b. SCRIERE SETARE IN BAZA DE DATE ---
// ==========================================
// Trimite o setare in DB prin update_settings.php?nume=...&valoare=...
bool trimiteSetareDB(String nume, String valoare) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[DB] WiFi deconectat - nu trimit");
    return false;
  }
  String valEnc = valoare;
  valEnc.replace(" ", "%20");
  HTTPClient http;
  String url = String(XAMPP_IP) + "/smarthome/update_settings.php?nume=" + nume + "&valoare=" + valEnc;
  http.begin(url);
  int httpCode = http.GET();
  String raspuns = http.getString();  
  http.end();
  Serial.print("[DB] URL: "); Serial.println(url);
  Serial.print("[DB] Cod HTTP: "); Serial.println(httpCode);
  Serial.print("[DB] Raspuns: "); Serial.println(raspuns);
  return (httpCode == 200);
}

// ==========================================
// --- 5d. RESETARE DISPOZITIV (user+parola la default) ---
// ==========================================
bool reseteazaDispozitiv() {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = String(XAMPP_IP) + "/smarthome/resetare_dispozitiv.php";
  http.begin(url);
  int httpCode = http.GET();
  String raspuns = http.getString();
  http.end();
  Serial.print("[RESET] Cod HTTP: "); Serial.println(httpCode);
  Serial.print("[RESET] Raspuns: "); Serial.println(raspuns);
  return (httpCode == 200);
}
// ==========================================
// Face un HTTP scurt catre un server extern. Daca raspunde => avem internet.
// Folosit doar cand vrem sa intram pe modul DINAMIC.
bool areInternet() {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.begin("http://clients3.google.com/generate_204");
  http.setConnectTimeout(2000);   
  http.setTimeout(2000);
  int httpCode = http.GET();
  http.end();
  return (httpCode == 204 || httpCode == 200);
}

// ==========================================
// --- FUNCTII AJUTATOARE TAPO & USA ---
// ==========================================
bool asiguraConexiuneTapo() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!tapoConectat) tapoConectat = becTapo.begin(ip_bec, email_tapo, parola_tapo);
  return tapoConectat;
}

void aplicaCuloareTapo(String hexCode) {
  if(hexCode.length() != 7) return;
  long rgb = strtol(hexCode.substring(1).c_str(), NULL, 16);
  float r = ((rgb >> 16) & 0xFF) / 255.0; float g = ((rgb >> 8) & 0xFF) / 255.0; float b = (rgb & 0xFF) / 255.0;
  float cmax = max(r, max(g, b)); float cmin = min(r, min(g, b)); float delta = cmax - cmin;
  int hue = 0;
  if (delta != 0) {
    if (cmax == r) hue = 60 * ((g - b) / delta);
    else if (cmax == g) hue = 60 * (((b - r) / delta) + 2);
    else hue = 60 * (((r - g) / delta) + 4);
    if (hue < 0) hue += 360;
  }
  int saturation = (cmax == 0) ? 0 : (delta / cmax) * 100;
  if(asiguraConexiuneTapo()) { becTapo.set_color(hue, saturation); }
}

void actioneazaUsa() {
  if (usaDeschisa) {
    motorUsa.step(-pasiSfertDeTur);usaDeschisa = false;
  } else {
    motorUsa.step(pasiSfertDeTur);usaDeschisa = true;
  }
  redeseneazaEcran = true;digitalWrite(32, LOW); 
  digitalWrite(25, LOW);digitalWrite(33, LOW); 
  digitalWrite(14, LOW);
}

// ==========================================
// --- 6. API WEB ---
// ==========================================
void handleStatus() { 
  inregistreazaClient(); 
  server.sendHeader("Access-Control-Allow-Origin", "*"); 
  int activeUsers = citesteClientiActivi();
  int fanPercent = 0;
  if (modVentilatie == "DINAMIC") {
    if (activeUsers == 0) fanPercent = 0;
    else if (activeUsers == 1) fanPercent = 25;
    else if (activeUsers == 2) fanPercent = 50;
    else if (activeUsers == 3) fanPercent = 75;
    else fanPercent = 100;
  } else if (ventilatorPornit) {
    fanPercent = 50; 
  }
  String json = "{\"temp\":" + String(tempCurenta) + ",\"target\":" + String(tempDorita) + 
                ",\"fan\":" + String(ventilatorPornit ? "true" : "false") + ",\"mod\":\"" + modVentilatie + "\"" +
                ",\"clienti\":" + String(activeUsers) + ",\"procent\":" + String(fanPercent) +
                ",\"rpm\":" + String(rpmCurent) + "}"; 
  server.send(200, "application/json", json); 
}

void handleSetVentMode() { 
  inregistreazaClient(); 
  server.sendHeader("Access-Control-Allow-Origin", "*"); 
  
  if (server.hasArg("mod")) {
    modVentilatie = server.arg("mod");
    
    if (modVentilatie == "MANUAL ON") {
        ventilatorPornit = true;
        digitalWrite(RELAY_PIN, LOW); 
        ledcWrite(FAN_PWM_PIN, speedNormal);
    }
    else if (modVentilatie == "MANUAL OFF") {
        ventilatorPornit = false;
        digitalWrite(RELAY_PIN, HIGH); 
        ledcWrite(FAN_PWM_PIN, 0);
    }
    else if (modVentilatie == "DINAMIC") {
        int vit = calculeazaViteza();
        if (vit > 0) {
            ventilatorPornit = true;
            digitalWrite(RELAY_PIN, LOW);
            ledcWrite(FAN_PWM_PIN, vit);
        } else {
            ventilatorPornit = false;
            digitalWrite(RELAY_PIN, HIGH);
            ledcWrite(FAN_PWM_PIN, 0);
        }
    }
    redeseneazaEcran = true; 
    server.send(200, "text/plain", "OK"); 
  } else {
    server.send(400, "text/plain", "Lipsește modul!");
  }
}

void handleSetTemp() { inregistreazaClient(); server.sendHeader("Access-Control-Allow-Origin", "*"); if (server.hasArg("val")) { tempDorita = server.arg("val").toFloat(); redeseneazaEcran = true; server.send(200, "text/plain", "OK"); } }
void handleLightStatus()
 { inregistreazaClient(); 
 server.sendHeader("Access-Control-Allow-Origin", "*"); 
 unsigned long currentUptime = (releuLuminaPornit && becSoftPornit) ? (millis() - becUptimeStart) / 1000 : 0;
  if (releuLuminaPornit && becSoftPornit) consumTotalWh += ((9.0 * (luminozitate / 100.0)) / 3600.0) * 2.0;
   String json = "{\"releuOn\":" + String(releuLuminaPornit ? "true" : "false") 
   + ",\"softOn\":" + String(becSoftPornit ? "true" : "false") + ",\"luminozitate\":" + String(luminozitate) 
   + ",\"culoare\":\"" + culoareBec + "\",\"uptimeSec\":" + String(currentUptime) + ",\"consumWh\":"
    + String(consumTotalWh, 4) + "}"; server.send(200, "application/json", json); }
void handleToggleLightRelay() { 
  inregistreazaClient(); server.sendHeader("Access-Control-Allow-Origin", "*"); releuLuminaPornit = !releuLuminaPornit;
   if (releuLuminaPornit) { digitalWrite(RELAY_LIGHT_PIN, LOW); becSoftPornit = true; becUptimeStart = millis(); }
    else { digitalWrite(RELAY_LIGHT_PIN, HIGH); becSoftPornit = false; tapoConectat = false; }
     blocSyncPanaLa = millis() + 6000; redeseneazaEcran = true; server.send(200, "text/plain", "OK"); }
void handleToggleLightSoft() 
{ inregistreazaClient(); server.sendHeader("Access-Control-Allow-Origin", "*"); if (!releuLuminaPornit) 
{ server.send(400, "text/plain", "Releul oprit"); return; } becSoftPornit = !becSoftPornit; 
if (becSoftPornit) becUptimeStart = millis(); if(asiguraConexiuneTapo()) { if(becSoftPornit) becTapo.on();
 else becTapo.off(); } blocSyncPanaLa = millis() + 6000; redeseneazaEcran = true; server.send(200, "text/plain", "OK"); }
void handleSetLightParams() { inregistreazaClient(); server.sendHeader("Access-Control-Allow-Origin", "*"); 
if (server.hasArg("lum")) { luminozitate = server.arg("lum").toInt(); 
if(asiguraConexiuneTapo()) becTapo.set_brightness(luminozitate); } if (server.hasArg("cul"))
 { culoareBec = server.arg("cul"); aplicaCuloareTapo(culoareBec); } server.send(200, "text/plain", "OK"); }
void handleDoorStatus() 
{ inregistreazaClient(); server.sendHeader("Access-Control-Allow-Origin", "*"); 
String json = "{\"usaDeschisa\":" + String(usaDeschisa ? "true" : "false") + "}"; 
server.send(200, "application/json", json); }
void handleToggleDoor() { inregistreazaClient(); server.sendHeader("Access-Control-Allow-Origin", "*"); 
actioneazaUsa(); blocSyncPanaLa = millis() + 6000; server.send(200, "text/plain", "OK"); }
// /heartbeat?ip=X  — JS-ul trimite asta cand tab-ul intra in background (sendBeacon)
// Actualizeaza lastSeen fara sa conteze ca request-ul vine "tarziu" din cauza throttle-ului.
void handleHeartbeat() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("ip")) { server.send(400, "text/plain", "Lipseste ip"); return; }
  String ipClient = server.arg("ip");
  if (ipClient == "" || ipClient == "(IP unset)") { server.send(400, "text/plain", "IP invalid"); return; }
  bool gasit = false;
  for (int i = 0; i < MAX_CLIENTI; i++) {
    if (jurnalClienti[i].ip == ipClient) {
      jurnalClienti[i].lastSeen = millis();
      gasit = true;
      break;
    }
  }
  if (!gasit) {
    int oldestIndex = 0;
    unsigned long oldestTime = 0xFFFFFFFF;
    for (int i = 0; i < MAX_CLIENTI; i++) {
      if (jurnalClienti[i].ip == "") { oldestIndex = i; break; }
      if (jurnalClienti[i].lastSeen < oldestTime) { oldestTime = jurnalClienti[i].lastSeen; oldestIndex = i; }
    }
    jurnalClienti[oldestIndex].ip = ipClient;
    jurnalClienti[oldestIndex].lastSeen = millis();
  }
  server.send(200, "text/plain", "OK");
}

// /clientPleaca?ip=X  — JS-ul trimite asta la logout sau inchidere tab (sendBeacon)
// Pune lastSeen=0 asa ca utilizatorul e scos din count instant, fara sa astepte 10 min.
void handleClientPleaca() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("ip")) { server.send(400, "text/plain", "Lipseste ip"); return; }
  String ipClient = server.client().remoteIP().toString();
 for (int i = 0; i < MAX_CLIENTI; i++) {
    if (jurnalClienti[i].ip == ipClient) {
      jurnalClienti[i].ip = "";         // ȘTERGEM COMPLET IP-ul din memorie
      jurnalClienti[i].lastSeen = 0;    // Resetăm cronometrul
      break;
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleClients() 
{ inregistreazaClient(); server.sendHeader("Access-Control-Allow-Origin", "*"); 
String json = "["; bool first = true; for (int i = 0; i < MAX_CLIENTI; i++) { if (jurnalClienti[i].ip != "") 
{ if (!first) json += ","; 
long secAgo = (millis() - jurnalClienti[i].lastSeen) / 1000; json += "{\"ip\":\"" + jurnalClienti[i].ip +
 "\",\"secAgo\":" + String(secAgo) + ",\"status\":\"" + String((secAgo < 10) ? "Online" : "Offline") + "\"}";
  first = false; } } json += "]"; server.send(200, "application/json", json); }
void handleSystemStatus() { inregistreazaClient(); 
server.sendHeader("Access-Control-Allow-Origin", "*"); 
String json = "{\"ssid\":\"" + WiFi.SSID() + "\",\"ip\":\"" + WiFi.localIP().toString() +
 "\",\"uptime\":" + String(millis() / 1000) + ",\"free_heap\":" + String(ESP.getFreeHeap()) +
  ",\"total_heap\":" + String(ESP.getHeapSize()) + "}"; server.send(200, "application/json", json); }

// ==========================================
// --- 6b. DIAGNOSTIC: COMENZI PENTRU ECRAN (SPI) SI SENZOR DHT11 ---
// ==========================================
// Ecran ILI9341 (SPI):   04h, 0Ah, 0Ch, 28h, 29h
// Senzor DHT11:          dht.temp, dht.hum, dht.all, dht.raw, dht.test
//                        (DHT11 NU este I2C: foloseste un singur fir de date, pinul DHTPIN)
// Resetare:              reset (ecran + senzor la starea initiala)
//
// Citirile din ecran folosesc tft.readcommand8(registru, 1): indexul 0 este octetul "dummy",
// datele utile incep de la indexul 1 (vezi exemplul TFT_Read_Reg din biblioteca TFT_eSPI).
// Atentie: 04h returneaza ID-uri specifice producatorului modulului. Codul cipului ILI9341
// (00 93 41) se citeste cu D3h, de aceea comanda 04h le citeste pe amandoua.

// Text JSON sigur: scapam ghilimelele/backslash si inlocuim caracterele de control.
String diagEsc(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) { o += ' '; }
    else { o += c; }
  }
  return o;
}

// Octet in hexadecimal cu 2 cifre: 0x0A -> "0A"
String diagHex(uint8_t v) {
  String s = String(v, HEX);
  s.toUpperCase();
  if (v < 0x10) s = "0" + s;
  return s;
}
String diagHex3(uint8_t a, uint8_t b, uint8_t c) { return diagHex(a) + " " + diagHex(b) + " " + diagHex(c); }

// Raspuns plauzibil la 0Ah: nu e 0x00/0xFF (MISO nelegat sau blocat) si bitii D1,D0 sunt 0.
bool diagPmValid(uint8_t pm) { return pm != 0x00 && pm != 0xFF && (pm & 0x03) == 0; }

// ---------- ECRAN ILI9341 (SPI) ----------
void diagTft04(DiagRez& r) {
  r.descriere = "citește ID-ul afișajului";
  uint8_t m1 = tft.readcommand8(0x04, 1), m2 = tft.readcommand8(0x04, 2), m3 = tft.readcommand8(0x04, 3);
  uint8_t c1 = tft.readcommand8(0xD3, 1), c2 = tft.readcommand8(0xD3, 2), c3 = tft.readcommand8(0xD3, 3);
  r.adauga("ID primit: " + diagHex3(c1, c2, c3) + "   (cip, registrul D3h)");
  r.adauga("ID așteptat: 00 93 41");
  r.adauga("ID modul: " + diagHex3(m1, m2, m3) + "   (registrul 04h, setat de producătorul modulului)");
  if (c1 == 0x00 && c2 == 0x93 && c3 == 0x41) {
    r.status = "OK";
    r.motiv = "cipul este ILI9341";
  } else {
    r.status = "EROARE";
    r.motiv = "ID-ul trebuie să fie 00 93 41 (verifică SDO/MISO pe D19 și SPI_READ_FREQUENCY din User_Setup.h)";
  }
}

void diagTft0A(DiagRez& r) {
  r.descriere = "citește modul de alimentare al afișajului";
  uint8_t pm = tft.readcommand8(0x0A, 1);
  r.adauga("Răspuns: 0x" + diagHex(pm));
  if (!diagPmValid(pm)) {
    r.status = "EROARE";
    r.motiv = "citire invalidă – verifică SDO/MISO pe D19";
    return;
  }
  r.adauga(String("Booster: ") + ((pm & 0x80) ? "ON" : "OFF") + "  |  Idle: " + ((pm & 0x40) ? "ON" : "OFF") +
           "  |  Partial: " + ((pm & 0x20) ? "ON" : "OFF"));
  r.adauga(String("Sleep: ") + ((pm & 0x10) ? "OUT (activ)" : "IN") + "  |  Normal: " + ((pm & 0x08) ? "ON" : "OFF") +
           "  |  Display: " + ((pm & 0x04) ? "ON" : "OFF"));
  if (pm & 0x80) {
    r.status = "OK";
  } else {
    r.status = "EROARE";
    r.motiv = "booster oprit sau cu defect (D7 = 0)";
  }
}

// 0Ch = RDDCOLMOD (formatul pixelilor). D6:D4 = interfata RGB, D2:D0 = interfata MCU (SPI).
// 101 = 16 biti/pixel, 110 = 18 biti/pixel. Pe SPI conteaza doar D2:D0; interfata RGB nu e folosita (D6:D4 = 000)
// => pe modulul testat se citeste 0x05 (16 biti pe MCU). Unele module returneaza 0x55.
// (09h / RDDST nu se poate citi pe acest modul: citirea indexata returneaza zgomot.)
String diagFmtPixel(uint8_t f) { return (f == 0) ? "neutilizată" : (f == 5) ? "16 biți/pixel" : (f == 6) ? "18 biți/pixel" : "necunoscut"; }

void diagTft0C(DiagRez& r) {
  r.descriere = "citește formatul pixelilor";
  uint8_t v = tft.readcommand8(0x0C, 1);
  r.adauga("Răspuns: 0x" + diagHex(v) + "   (așteptat 0x05 sau 0x55)");
  if (v == 0x00 || v == 0xFF || (v & 0x88) != 0) {   // D7 si D3 sunt rezervati (0)
    r.status = "EROARE";
    r.motiv = "citire invalidă – verifică SDO/MISO pe D19";
    return;
  }
  r.adauga("Interfață RGB: " + diagFmtPixel((v >> 4) & 0x07) + "  |  Interfață MCU (SPI): " + diagFmtPixel(v & 0x07));
  if ((v & 0x07) == 5) {
    r.status = "OK";
  } else {
    r.status = "EROARE";
    r.motiv = "formatul MCU (biții D2:D0) trebuie să fie 16 biți/pixel (101)";
  }
}

// 29h = Display ON, 28h = Display OFF. Comenzile de scriere nu au confirmare pe SPI, asa ca
// verificam efectul citind inapoi registrul 0Ah (bitul D2 = display pornit).
void diagTftOnOff(DiagRez& r, bool pornit) {
  r.descriere = pornit ? "Display ON (pornește afișajul)" : "Display OFF (oprește afișajul)";
  tft.writecommand(pornit ? 0x29 : 0x28);
  ecranOprit = !pornit;
  delay(20);
  uint8_t pm = tft.readcommand8(0x0A, 1);
  if (!pornit) r.adauga("Notă: lumina din spate rămâne aprinsă (LED legat la 3.3V)");
  if (!diagPmValid(pm)) {
    r.status = "NEVERIFICAT";
    r.motiv = "comanda a fost trimisă, dar starea nu a putut fi citită înapoi (0Ah invalid; verifică SDO/MISO pe D19)";
    return;
  }
  bool arataPornit = (pm & 0x04) != 0;
  r.adauga(String("Verificare: 0Ah = 0x") + diagHex(pm) + " → bit D2 (display) = " + (arataPornit ? "1 (ON)" : "0 (OFF)"));
  if (arataPornit == pornit) {
    r.status = "OK";
  } else {
    r.status = "EROARE";
    r.motiv = "starea citită din 0Ah nu corespunde comenzii trimise";
  }
}

// ---------- SENZOR DHT11 (un singur fir) ----------
// DHT11 accepta maximum o citire pe secunda: daca ultimul acces a fost prea recent, asteptam.
void diagAsteaptaDHT() { while (millis() - ultimulAccesDHT < 1100) delay(10); }

// Masoara cat timp sta pinul DHT la nivelul dat (in microsecunde). 0xFFFFFFFF = timeout.
uint32_t diagPulsDHT(bool nivel) {
  uint32_t t0 = micros();
  while (digitalRead(DHTPIN) == nivel) {
    if ((uint32_t)(micros() - t0) > 250) return 0xFFFFFFFF;
  }
  return micros() - t0;
}

// Citeste direct cadrul de 40 de biti (5 octeti) de pe pinul DHT, fara biblioteca DHT.
// Returneaza: 0 = OK, 1 = senzorul nu raspunde (timeout), 2 = checksum gresit.
int diagCitesteDHTBrut(uint8_t d[5]) {
  for (int i = 0; i < 5; i++) d[i] = 0;
  uint32_t durate[80];
  pinMode(DHTPIN, INPUT_PULLUP);
  delay(1);
  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, LOW);
  delay(20);                       // semnal de start: minim 18 ms pe LOW (DHT11)
  pinMode(DHTPIN, INPUT_PULLUP);
  delayMicroseconds(55);

  bool timeout = false;
  noInterrupts();                  // partea cu timing critic
  if (diagPulsDHT(LOW) == 0xFFFFFFFF) timeout = true;
  else if (diagPulsDHT(HIGH) == 0xFFFFFFFF) timeout = true;
  else {
    for (int i = 0; i < 80 && !timeout; i += 2) {
      durate[i] = diagPulsDHT(LOW);
      durate[i + 1] = diagPulsDHT(HIGH);
      if (durate[i] == 0xFFFFFFFF || durate[i + 1] == 0xFFFFFFFF) timeout = true;
    }
  }
  interrupts();
  if (timeout) return 1;

  for (int i = 0; i < 40; i++) {   // bit 1 daca pulsul HIGH e mai lung decat pulsul LOW de ~50 us
    d[i / 8] <<= 1;
    if (durate[2 * i + 1] > durate[2 * i]) d[i / 8] |= 1;
  }
  return (d[4] == (uint8_t)(d[0] + d[1] + d[2] + d[3])) ? 0 : 2;
}

void diagDhtTemp(DiagRez& r) {
  r.descriere = "citește temperatura";
  diagAsteaptaDHT();
  unsigned long t0 = millis();
  float t = dht.readTemperature(false, true);   // force = citire noua, nu valoarea din memorie
  r.ms = millis() - t0;
  ultimulAccesDHT = millis();
  if (isnan(t)) {
    r.status = "EROARE";
    r.motiv = "citire eșuată (timeout sau checksum) – verifică senzorul pe pinul 13";
    return;
  }
  r.adauga("Temperatură: " + String(t, 1) + " °C");
  r.status = "OK";
}

void diagDhtHum(DiagRez& r) {
  r.descriere = "citește umiditatea";
  diagAsteaptaDHT();
  unsigned long t0 = millis();
  float h = dht.readHumidity(true);
  r.ms = millis() - t0;
  ultimulAccesDHT = millis();
  if (isnan(h)) {
    r.status = "EROARE";
    r.motiv = "citire eșuată (timeout sau checksum) – verifică senzorul pe pinul 13";
    return;
  }
  r.adauga("Umiditate: " + String(h, 1) + " %");
  r.status = "OK";
}

void diagDhtAll(DiagRez& r) {
  r.descriere = "citește temperatura și umiditatea (o singură măsurătoare)";
  diagAsteaptaDHT();
  unsigned long t0 = millis();
  bool ok = dht.read(true);                     // o singura citire reala de pe senzor
  r.ms = millis() - t0;
  ultimulAccesDHT = millis();
  if (!ok) {
    r.status = "EROARE";
    r.motiv = "citire eșuată (timeout sau checksum) – verifică senzorul pe pinul 13";
    return;
  }
  float t = dht.readTemperature();              // aceeasi masuratoare (fara citire noua)
  float h = dht.readHumidity();
  float hi = dht.computeHeatIndex(t, h, false); // index de caldura in °C
  r.adauga("Temperatură: " + String(t, 1) + " °C");
  r.adauga("Umiditate: " + String(h, 1) + " %");
  r.adauga("Index de căldură: " + String(hi, 1) + " °C");
  r.status = "OK";
}

void diagDhtRaw(DiagRez& r) {
  r.descriere = "citește cadrul brut de 40 de biți de la senzor";
  diagAsteaptaDHT();
  uint8_t d[5];
  unsigned long t0 = millis();
  int rez = diagCitesteDHTBrut(d);
  r.ms = millis() - t0;
  ultimulAccesDHT = millis();
  if (rez == 1) {
    r.status = "EROARE";
    r.motiv = "senzorul nu răspunde (timeout) – verifică cablul de date de pe pinul 13 și alimentarea";
    return;
  }
  uint8_t calc = (uint8_t)(d[0] + d[1] + d[2] + d[3]);
  float umid = d[0] + d[1] * 0.1;
  float temp = d[2] + (d[3] & 0x0F) * 0.1;
  if (d[3] & 0x80) temp = -1 - d[2] + (d[3] & 0x0F) * 0.1;   // semn negativ (variante noi de DHT11)
  r.adauga("Cadru (5 octeți): " + diagHex(d[0]) + " " + diagHex(d[1]) + " " + diagHex(d[2]) + " " + diagHex(d[3]) + " " + diagHex(d[4]));
  r.adauga("Umiditate: " + String(umid, 1) + " %   |   Temperatură: " + String(temp, 1) + " °C");
  r.adauga("Checksum: primit " + diagHex(d[4]) + ", calculat " + diagHex(calc));
  if (rez == 0) {
    r.status = "OK";
  } else {
    r.status = "EROARE";
    r.motiv = "checksum greșit – cadru corupt (zgomot pe linie sau citire prea rapidă)";
  }
}

void diagDhtTest(DiagRez& r) {
  const int N = 3;
  r.descriere = "autotest senzor: " + String(N) + " citiri consecutive";
  int ok = 0;
  float tmin = 1000, tmax = -1000, hmin = 1000, hmax = -1000;
  unsigned long t0 = millis();
  for (int i = 0; i < N; i++) {
    diagAsteaptaDHT();
    bool citit = dht.read(true);
    ultimulAccesDHT = millis();
    if (citit) {
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      ok++;
      if (t < tmin) tmin = t;
      if (t > tmax) tmax = t;
      if (h < hmin) hmin = h;
      if (h > hmax) hmax = h;
    }
  }
  r.ms = millis() - t0;
  r.adauga("Citiri reușite: " + String(ok) + " / " + String(N));
  if (ok > 0) {
    r.adauga("Temperatură min / max: " + String(tmin, 1) + " / " + String(tmax, 1) + " °C");
    r.adauga("Umiditate min / max: " + String(hmin, 1) + " / " + String(hmax, 1) + " %");
  }
  if (ok == N) {
    r.status = "OK";
  } else if (ok == 0) {
    r.status = "EROARE";
    r.motiv = "niciun răspuns de la senzor – verifică cablul de date de pe pinul 13 și alimentarea";
  } else {
    r.status = "EROARE";
    r.motiv = String(N - ok) + " din " + String(N) + " citiri au eșuat – senzor instabil sau cablu slab";
  }
}

// ---------- RESETARE LA STAREA INITIALA ----------
void diagReset(DiagRez& r) {
  r.descriere = "resetare ecran și senzor la starea inițială";
  unsigned long t0 = millis();
  tft.init();                                        // reset hardware + secventa de initializare ILI9341
  tft.setRotation(1);
  uint16_t calData[5] = { 275, 3620, 264, 3532, 1 }; // aceeasi calibrare ca in setup()
  tft.setTouch(calData);
  ecranOprit = false;
  dht.begin();
  ultimulAccesDHT = millis();
  redeseneazaEcran = true;                           // loop() redeseneaza pagina curenta
  r.adauga("Ecran: reinițializat (reset hardware, rotație 1, calibrare touch)");
  r.adauga("Senzor DHT11: reinițializat");
  uint8_t pm = tft.readcommand8(0x0A, 1);
  if (diagPmValid(pm)) {
    r.adauga("Verificare: 0Ah = 0x" + diagHex(pm));
    if ((pm & 0x94) == 0x94) {                       // booster ON, sleep OUT, display ON
      r.status = "OK";
    } else {
      r.status = "EROARE";
      r.motiv = "ecranul nu a revenit în starea normală (se aștepta booster ON, sleep OUT, display ON)";
    }
  } else {
    r.status = "NEVERIFICAT";
    r.motiv = "resetarea a fost făcută, dar starea ecranului nu a putut fi citită (verifică SDO/MISO pe D19)";
  }
  r.ms = millis() - t0;
}

// ---------- ENDPOINT /cmd ----------
// Raspuns: {"cmd","descriere","linii":[...],"status":"OK|EROARE|NEVERIFICAT","motiv","ms"}
void handleCmd() {
  // Nu apelam inregistreazaClient() (IP-ul ar fi cel al XAMPP-ului) si nu setam CORS:
  // browserul nu are voie sa apeleze direct aceasta adresa.
  String ipClient = server.client().remoteIP().toString();
  if (ipClient != String(DIAG_IP_XAMPP) || !server.hasArg("k") || server.arg("k") != String(DIAG_KEY)) {
    server.send(403, "application/json", "{\"status\":\"EROARE\",\"motiv\":\"Acces interzis\"}");
    return;
  }
  if (!server.hasArg("c")) {
    server.send(400, "application/json", "{\"status\":\"EROARE\",\"motiv\":\"Lipsește comanda\"}");
    return;
  }

  String c = server.arg("c");
  DiagRez r;
  unsigned long t0 = millis();

  if (c == "04h") diagTft04(r);
  else if (c == "0Ah") diagTft0A(r);
  else if (c == "0Ch") diagTft0C(r);
  else if (c == "28h") diagTftOnOff(r, false);
  else if (c == "29h") diagTftOnOff(r, true);
  else if (c == "dht.temp") diagDhtTemp(r);
  else if (c == "dht.hum") diagDhtHum(r);
  else if (c == "dht.all") diagDhtAll(r);
  else if (c == "dht.raw") diagDhtRaw(r);
  else if (c == "dht.test") diagDhtTest(r);
  else if (c == "reset") diagReset(r);
  else {
    r.descriere = "comandă necunoscută";
    r.status = "EROARE";
    r.motiv = "comandă necunoscută";
  }
  if (r.ms == 0) r.ms = millis() - t0;

  String json = "{\"cmd\":\"" + diagEsc(c) + "\",\"descriere\":\"" + diagEsc(r.descriere) + "\",\"linii\":[";
  for (int i = 0; i < r.nrLinii; i++) {
    if (i > 0) json += ",";
    json += "\"" + diagEsc(r.linii[i]) + "\"";
  }
  json += "],\"status\":\"" + r.status + "\",\"motiv\":\"" + diagEsc(r.motiv) + "\",\"ms\":" + String(r.ms) + "}";
  server.send(200, "application/json", json);
}

// ==========================================
// --- 7. DESENARE ECRAN TFT ---
// ==========================================
void deseneazaDashboard() {
  tft.fillScreen(FUNDAL_DARK);
  tft.setTextSize(1);  // IMPORTANT: resetam multiplicatorul, altfel ramane 2 din ecranul WiFi

  // Ecran 320x240 (landscape). Margini si spatii
  int margine = 8;
  int spatiu = 8;
  int w = (320 - margine * 2 - spatiu) / 2;   // latime card = 148
  int hButon = 32;
  int yJos = 240 - margine - hButon;           // y buton meniu = 200
  int hCard = (yJos - margine - spatiu) / 2;    // inaltime card ~= 88

  int xStanga = margine;
  int xDreapta = margine + w + spatiu;
  int centruStanga = xStanga + w / 2;
  int centruDreapta = xDreapta + w / 2;

  int yR1 = margine;
  int yR2 = margine + hCard + spatiu;

  // ============ CARD 1: TEMP INTERIOR ============
  tft.fillRoundRect(xStanga, yR1, w, hCard, 8, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("TEMP. INTERIOR", centruStanga, yR1 + 10, 2);  // titlu font 2
  tft.setTextColor(TEXT_ALB, CARD_DARK);
  tft.drawCentreString(String(tempCurenta, 1) + " C", centruStanga, yR1 + 40, 4);  // valoare mare

  // ============ CARD 2: VENTILATIE ============
  tft.fillRoundRect(xDreapta, yR1, w, hCard, 8, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("VENTILATIE", centruDreapta, yR1 + 10, 2);

  // Modul preluat din aplicatie (font 1 ca sa incapa "MANUAL OFF")
  uint16_t culMod;
  String txtMod;
  if (modVentilatie == "MANUAL ON")       { culMod = GALBEN_MOD;   txtMod = "MANUAL ON"; }
  else if (modVentilatie == "MANUAL OFF") { culMod = ROSU_OFF;     txtMod = "MANUAL OFF"; }
  else if (modVentilatie == "DINAMIC")    { culMod = ALB_ELECTRIC; txtMod = "DINAMIC"; }
  else                                    { culMod = TEXT_ALB;     txtMod = "AUTO"; }
  tft.setTextColor(culMod, CARD_DARK);
  tft.drawCentreString(txtMod, centruDreapta, yR1 + 38, 2);

  // Starea releului ventilator
  if (ventilatorPornit) {
    tft.setTextColor(VERDE_ON, CARD_DARK);
    tft.drawCentreString("PORNIT", centruDreapta, yR1 + 62, 2);
  } else {
    tft.setTextColor(ROSU_OFF, CARD_DARK);
    tft.drawCentreString("OPRIT", centruDreapta, yR1 + 62, 2);
  }

  // ============ CARD 3: ILUMINAT ============
  tft.fillRoundRect(xStanga, yR2, w, hCard, 8, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("ILUMINAT", centruStanga, yR2 + 10, 2);

  if (!releuLuminaPornit) {
    tft.setTextColor(ROSU_OFF, CARD_DARK);
    tft.drawCentreString("FARA CURENT", centruStanga, yR2 + 45, 2);
  } else if (becSoftPornit) {
    tft.setTextColor(VERDE_ON, CARD_DARK);
    tft.drawCentreString("APRINS", centruStanga, yR2 + 38, 2);
    tft.setTextColor(TEXT_ALB, CARD_DARK);
    tft.drawCentreString("Intens: " + String(luminozitate) + "%", centruStanga, yR2 + 64, 1);  // font 1, mic
  } else {
    tft.setTextColor(GALBEN_MOD, CARD_DARK);
    tft.drawCentreString("STINS", centruStanga, yR2 + 45, 2);
  }

  // ============ CARD 4: USA ============
  tft.fillRoundRect(xDreapta, yR2, w, hCard, 8, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("USA INTRARE", centruDreapta, yR2 + 10, 2);

  if (usaDeschisa) {
    tft.setTextColor(GALBEN_MOD, CARD_DARK);
    tft.drawCentreString("DESCHISA", centruDreapta, yR2 + 45, 2);
  } else {
    tft.setTextColor(VERDE_ON, CARD_DARK);
    tft.drawCentreString("INCHISA", centruDreapta, yR2 + 45, 2);
  }

  // ============ BUTON MENIU JOS ============
  tft.fillRoundRect(margine, yJos, 320 - margine * 2, hButon, 6, ALB_ELECTRIC);
  tft.setTextColor(TEXT_ALB, ALB_ELECTRIC);
  tft.drawCentreString("MENIURI", 160, yJos + 9, 2);
}

void deseneazaMeniu() {
  tft.fillScreen(FUNDAL_DARK);
  tft.setTextSize(1);  // IMPORTANT: resetam multiplicatorul ramas din ecranul WiFi
  tft.setTextColor(ALB_ELECTRIC, FUNDAL_DARK);
  tft.drawCentreString("MENIU", 160, 10, 2);

  // --- Grila 2x2 de butoane mari, fara iconite ---
  int w = 145, h = 55, spatiuX = 10, colGap = 10, rowGap = 12;
  int yStart = 40;
  int centruC1 = spatiuX + (w / 2);
  int centruC2 = spatiuX + w + colGap + (w / 2);

  int yR1 = yStart;
  int yR2 = yStart + h + rowGap;

  tft.setTextColor(TEXT_ALB, CARD_DARK);

  // --- 1. VENTILATIE (sus-stanga) ---
  tft.fillRoundRect(spatiuX, yR1, w, h, 6, CARD_DARK);
  tft.drawCentreString("1. VENTILATIE", centruC1, yR1 + (h / 2) - 8, 2);

  // --- 2. ILUMINAT (sus-dreapta) ---
  tft.fillRoundRect(spatiuX + w + colGap, yR1, w, h, 6, CARD_DARK);
  tft.drawCentreString("2. ILUMINAT", centruC2, yR1 + (h / 2) - 8, 2);

  // --- 3. USA (jos-stanga) ---
  tft.fillRoundRect(spatiuX, yR2, w, h, 6, CARD_DARK);
  tft.drawCentreString("3. USA", centruC1, yR2 + (h / 2) - 8, 2);

  // --- 4. RESETARE (jos-dreapta) ---
  tft.fillRoundRect(spatiuX + w + colGap, yR2, w, h, 6, CARD_DARK);
  tft.drawCentreString("4. RESETARE", centruC2, yR2 + (h / 2) - 8, 2);

  // --- Buton INAPOI jos ---
  tft.fillRoundRect(8, 200, 304, 34, 6, ROSU_OFF);
  tft.setTextColor(TEXT_ALB, ROSU_OFF);
  tft.drawCentreString("INAPOI LA DASHBOARD", 160, 210, 2);
}

// --- SUBMENIURI (placeholder - le umplem pe rand) ---
void deseneazaHeaderSub(const char* titlu) {
  tft.fillScreen(FUNDAL_DARK);
  tft.setTextSize(1);
  tft.setTextColor(ALB_ELECTRIC, FUNDAL_DARK);
  tft.drawCentreString(titlu, 160, 10, 2);
  // buton INAPOI jos
  tft.fillRoundRect(8, 200, 304, 34, 6, ROSU_OFF);
  tft.setTextColor(TEXT_ALB, ROSU_OFF);
  tft.drawCentreString("INAPOI LA MENIU", 160, 210, 2);
}

void deseneazaSubVentilatie() {
  deseneazaHeaderSub("VENTILATIE");

  // 3 carduri pe orizontala, sus
  int w = 98, h = 95, gap = 8;
  int x1 = 8;
  int x2 = x1 + w + gap;
  int x3 = x2 + w + gap;
  int yCard = 38;
  int c1 = x1 + w / 2;
  int c2 = x2 + w / 2;
  int c3 = x3 + w / 2;

  // ===== CARD 1: CONTROL MOD (selectabil) =====
  tft.fillRoundRect(x1, yCard, w, h, 6, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("CONTROL MOD", c1, yCard + 8, 1);
  tft.drawCentreString("(apasa)", c1, yCard + h - 16, 1);

  uint16_t culMod;
  if (modVentilatie == "MANUAL ON")       culMod = GALBEN_MOD;
  else if (modVentilatie == "MANUAL OFF") culMod = ROSU_OFF;
  else if (modVentilatie == "DINAMIC")    culMod = ALB_ELECTRIC;
  else                                    culMod = VERDE_ON;  // AUTO
  tft.setTextColor(culMod, CARD_DARK);
  // afisam modul pe doua randuri daca e "MANUAL ON/OFF"
  if (modVentilatie == "MANUAL ON") {
    tft.drawCentreString("MANUAL", c1, yCard + 38, 2);
    tft.drawCentreString("ON", c1, yCard + 56, 2);
  } else if (modVentilatie == "MANUAL OFF") {
    tft.drawCentreString("MANUAL", c1, yCard + 38, 2);
    tft.drawCentreString("OFF", c1, yCard + 56, 2);
  } else {
    tft.drawCentreString(modVentilatie, c1, yCard + 45, 2);
  }

  // ===== CARD 2: STARE RELEU (doar afisare) =====
  tft.fillRoundRect(x2, yCard, w, h, 6, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("STARE RELEU", c2, yCard + 8, 1);
  if (ventilatorPornit) {
    tft.setTextColor(VERDE_ON, CARD_DARK);
    tft.drawCentreString("PORNIT", c2, yCard + 45, 2);
  } else {
    tft.setTextColor(ROSU_OFF, CARD_DARK);
    tft.drawCentreString("OPRIT", c2, yCard + 45, 2);
  }

  // ===== CARD 3: SET PRAG (selectabil + / -) =====
  tft.fillRoundRect(x3, yCard, w, h, 6, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("SET PRAG", c3, yCard + 6, 1);
  // sageata sus (+)
  tft.setTextColor(VERDE_ON, CARD_DARK);
  tft.drawCentreString("+", c3, yCard + 18, 2);
  // valoarea curenta
  tft.setTextColor(TEXT_ALB, CARD_DARK);
  tft.drawCentreString(String((int)tempDorita) + "C", c3, yCard + 42, 4);
  // sageata jos (-)
  tft.setTextColor(ROSU_OFF, CARD_DARK);
  tft.drawCentreString("-", c3, yCard + h - 22, 2);

  // ===== mesaj temporar (ex. "FARA NET") =====
  if (mesajVentTemp != "" && millis() < mesajVentExpira) {
    tft.setTextColor(ROSU_OFF, FUNDAL_DARK);
    tft.drawCentreString(mesajVentTemp, 160, 150, 2);
  }
}

void deseneazaSubIluminat() {
  deseneazaHeaderSub("ILUMINAT");

  // 3 carduri pe orizontala, sus (aceleasi dimensiuni ca la ventilatie)
  int w = 98, h = 95, gap = 8;
  int x1 = 8;
  int x2 = x1 + w + gap;
  int x3 = x2 + w + gap;
  int yCard = 38;
  int c1 = x1 + w / 2;
  int c2 = x2 + w / 2;
  int c3 = x3 + w / 2;

  // ===== CARD 1: STARE BEC (doar afisare) =====
  tft.fillRoundRect(x1, yCard, w, h, 6, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("STARE BEC", c1, yCard + 8, 1);
  if (!releuLuminaPornit) {
    // fara curent => becul nu poate fi aprins
    tft.setTextColor(ROSU_OFF, CARD_DARK);
    tft.drawCentreString("OPRIT", c1, yCard + 45, 2);
  } else if (becSoftPornit) {
    tft.setTextColor(VERDE_ON, CARD_DARK);
    tft.drawCentreString("PORNIT", c1, yCard + 45, 2);
  } else {
    tft.setTextColor(GALBEN_MOD, CARD_DARK);
    tft.drawCentreString("OPRIT", c1, yCard + 45, 2);
  }

  // ===== CARD 2: CURENT PRIZA / RELEU (selectabil) =====
  tft.fillRoundRect(x2, yCard, w, h, 6, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("CURENT PRIZA", c2, yCard + 8, 1);
  tft.drawCentreString("(apasa)", c2, yCard + h - 16, 1);
  if (releuLuminaPornit) {
    tft.setTextColor(VERDE_ON, CARD_DARK);
    tft.drawCentreString("TRECE", c2, yCard + 38, 2);
    tft.drawCentreString("CURENT", c2, yCard + 56, 2);
  } else {
    tft.setTextColor(ROSU_OFF, CARD_DARK);
    tft.drawCentreString("OPRIT", c2, yCard + 45, 2);
  }

  // ===== CARD 3: INTENSITATE (selectabil + / -) =====
  tft.fillRoundRect(x3, yCard, w, h, 6, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("INTENSITATE", c3, yCard + 6, 1);
  // sageata sus (+)
  tft.setTextColor(VERDE_ON, CARD_DARK);
  tft.drawCentreString("+", c3, yCard + 18, 2);
  // valoarea curenta
  tft.setTextColor(TEXT_ALB, CARD_DARK);
  tft.drawCentreString(String(luminozitate) + "%", c3, yCard + 42, 4);
  // sageata jos (-)
  tft.setTextColor(ROSU_OFF, CARD_DARK);
  tft.drawCentreString("-", c3, yCard + h - 22, 2);
}

void deseneazaSubUsa() {
  deseneazaHeaderSub("USA");

  // 2 carduri mari pe orizontala
  int w = 145, h = 100, gap = 14;
  int x1 = 10;
  int x2 = x1 + w + gap;
  int yCard = 45;
  int c1 = x1 + w / 2;
  int c2 = x2 + w / 2;

  // ===== CARD 1: STATUS USA (doar afisare) =====
  tft.fillRoundRect(x1, yCard, w, h, 6, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("STATUS USA", c1, yCard + 14, 2);
  if (usaDeschisa) {
    tft.setTextColor(GALBEN_MOD, CARD_DARK);
    tft.drawCentreString("DESCHISA", c1, yCard + 55, 2);
  } else {
    tft.setTextColor(VERDE_ON, CARD_DARK);
    tft.drawCentreString("INCHISA", c1, yCard + 55, 2);
  }

  // ===== CARD 2: COMANDA USA (selectabil) =====
  tft.fillRoundRect(x2, yCard, w, h, 6, CARD_DARK);
  tft.setTextColor(TEXT_GRI, CARD_DARK);
  tft.drawCentreString("COMANDA USA", c2, yCard + 14, 2);
  // arata ce actiune va face apasarea
  if (usaDeschisa) {
    tft.setTextColor(ROSU_OFF, CARD_DARK);
    tft.drawCentreString("INCHIDE", c2, yCard + 55, 2);
  } else {
    tft.setTextColor(VERDE_ON, CARD_DARK);
    tft.drawCentreString("DESCHIDE", c2, yCard + 55, 2);
  }
}

void deseneazaSubResetare() {
  if (!confirmaReset) {
    // ===== ECRAN NORMAL: buton mare RESETARE =====
    deseneazaHeaderSub("RESETARE USER/PAROLA");

    tft.setTextColor(TEXT_GRI, FUNDAL_DARK);
    tft.drawCentreString("Reseteaza user-ul si parola", 160, 50, 2);
    tft.drawCentreString("la valori default", 160, 70, 2);
    tft.setTextColor(TEXT_ALB, FUNDAL_DARK);
    tft.drawCentreString("user: admin   parola: 1234", 160, 95, 2);

    // buton RESETARE (rosu, in mijloc)
    tft.fillRoundRect(60, 130, 200, 50, 8, ROSU_OFF);
    tft.setTextColor(TEXT_ALB, ROSU_OFF);
    tft.drawCentreString("RESETARE", 160, 147, 4);

    // mesaj rezultat (dupa resetare)
    if (mesajReset != "" && millis() < mesajResetExpira) {
      tft.setTextColor(VERDE_ON, FUNDAL_DARK);
      tft.drawCentreString(mesajReset, 160, 188, 2);
    }
  } else {
    // ===== ECRAN CONFIRMARE: DA / NU =====
    tft.fillScreen(FUNDAL_DARK);
    tft.setTextSize(1);
    tft.setTextColor(GALBEN_MOD, FUNDAL_DARK);
    tft.drawCentreString("CONFIRMARE", 160, 20, 4);
    tft.setTextColor(TEXT_ALB, FUNDAL_DARK);
    tft.drawCentreString("Sigur vrei sa resetezi", 160, 70, 2);
    tft.drawCentreString("user-ul si parola?", 160, 92, 2);

    // buton DA (verde, stanga)
    tft.fillRoundRect(20, 140, 130, 60, 8, VERDE_ON);
    tft.setTextColor(TEXT_ALB, VERDE_ON);
    tft.drawCentreString("DA", 85, 158, 4);

    // buton NU (rosu, dreapta)
    tft.fillRoundRect(170, 140, 130, 60, 8, ROSU_OFF);
    tft.setTextColor(TEXT_ALB, ROSU_OFF);
    tft.drawCentreString("NU", 235, 158, 4);
  }
}

void actualizeazaEcran() {
  if (paginaEcran == 0) deseneazaDashboard();
  else if (paginaEcran == 1) deseneazaMeniu();
  else if (paginaEcran == 2) deseneazaSubVentilatie();
  else if (paginaEcran == 3) deseneazaSubIluminat();
  else if (paginaEcran == 4) deseneazaSubUsa();
  else if (paginaEcran == 5) deseneazaSubResetare();
}

void setup() {
  Serial.begin(115200);
  
  pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, HIGH); 
  pinMode(RELAY_LIGHT_PIN, OUTPUT); digitalWrite(RELAY_LIGHT_PIN, LOW); 
  motorUsa.setSpeed(15); 
  dht.begin();
  ledcAttach(FAN_PWM_PIN, fanFreq, fanResolution);
  ledcWrite(FAN_PWM_PIN, 0); 
  pinMode(TACH_PIN, INPUT_PULLUP);
  tft.init();
  tft.setRotation(1); 
  uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
  tft.setTouch(calData);
  tft.fillScreen(FUNDAL_DARK);
  tft.setTextColor(TEXT_ALB, FUNDAL_DARK); 
  tft.setTextSize(2); 
  tft.setCursor(10, 50);
  tft.println("Conectare la WiFi...");
  tft.setCursor(10, 80);
  tft.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

  delay(2000); 
  sincronizareSetariDB();
  lastSyncTime = millis();   // NOU: pornim cronometrul de sincronizare

  server.on("/status", handleStatus);
  server.on("/setTarget", handleSetTemp);
  server.on("/setVentMode", handleSetVentMode);
  server.on("/lightStatus", handleLightStatus);
  server.on("/toggleLightRelay", handleToggleLightRelay);
  server.on("/toggleLightSoft", handleToggleLightSoft);
  server.on("/setLightParams", handleSetLightParams);
  server.on("/doorStatus", handleDoorStatus);
  server.on("/toggleDoor", handleToggleDoor);
  server.on("/clients", handleClients); 
  server.on("/system", handleSystemStatus);
  server.on("/heartbeat", handleHeartbeat);      
  server.on("/clientPleaca", handleClientPleaca);
  server.on("/cmd", handleCmd);   // consola de diagnostic (doar de la XAMPP, cu cheie) 

  server.begin();
  becUptimeStart = millis(); 
  actualizeazaEcran(); 
}

void loop() {
  server.handleClient(); 
  if (millis() - lastSyncTime > 5000 && millis() > blocSyncPanaLa) {
    lastSyncTime = millis();
    sincronizareSetariDB();
  }
  if (millis() - lastRPMCalcTime >= 1000) { 
    noInterrupts();
    int impulsuriCurente = pulseCount;
    pulseCount = 0; 
    interrupts();
    rpmCurent = impulsuriCurente * 30;
    lastRPMCalcTime = millis();
  }

  // --- LOGICA MODURILOR DE VENTILATIE ---
  if (millis() - lastReadTime > 2000) {
    lastReadTime = millis();
    float t = dht.readTemperature(); ultimulAccesDHT = millis(); 
    if (!isnan(t)) {
      if (t != tempCurenta) { tempCurenta = t; redeseneazaEcran = true; }
      if (modVentilatie == "AUTO") { 
        if (tempCurenta >= tempDorita && !ventilatorPornit) { 
            ventilatorPornit = true; 
            digitalWrite(RELAY_PIN, LOW); 
            ledcWrite(FAN_PWM_PIN, speedNormal); 
            redeseneazaEcran = true; 
        } 
        else if (tempCurenta < tempDorita && ventilatorPornit) { 
            ventilatorPornit = false; 
            digitalWrite(RELAY_PIN, HIGH); 
            ledcWrite(FAN_PWM_PIN, 0); 
            redeseneazaEcran = true; 
        }
      }
      else if (modVentilatie == "DINAMIC") {
        int vit = calculeazaViteza();
        if (vit > 0) {
           if (!ventilatorPornit) { 
             ventilatorPornit = true; 
             digitalWrite(RELAY_PIN, LOW);
             redeseneazaEcran = true;
           }
           ledcWrite(FAN_PWM_PIN, vit);
        } else {
           if (ventilatorPornit) { 
             ventilatorPornit = false; 
             digitalWrite(RELAY_PIN, HIGH); 
             ledcWrite(FAN_PWM_PIN, 0);
             redeseneazaEcran = true;  
           }
        }
      }
    }
  }

  // --- CITIRE TOUCHSCREEN ---
  uint16_t touchX = 0, touchY = 0;
  bool apasat = tft.getTouch(&touchX, &touchY);
  if (ecranOprit) apasat = false;   // ecran oprit din consola (28h): ignoram atingerile

  // --- CORECTIE MAPARE TOUCH (axele X si Y erau inversate fata de setRotation 1) ---
  // Valori brute masurate in cele 4 colturi:
  //   stanga-sus (X:289,Y:7), dreapta-sus (X:301,Y:237),
  //   stanga-jos (X:16,Y:9),  dreapta-jos (X:23,Y:239)
  // => Y brut (7..239) corespunde X-ului ecranului (0..320)
  // => X brut (16..301) inversat corespunde Y-ului ecranului (0..240)
  if (apasat) {
    uint16_t rawX = touchX;
    uint16_t rawY = touchY;
    int mx = map(rawY, 7, 239, 0, 320);   // X ecran
    int my = map(rawX, 16, 301, 240, 0);  // Y ecran (inversat)
    if (mx < 0) mx = 0;
    if (mx > 319) mx = 319;
    if (my < 0) my = 0;
    if (my > 239) my = 239;
    touchX = mx;
    touchY = my;
  }

  if (apasat) {
    if (paginaEcran == 0) {
      // --- DASHBOARD = DOAR AFISARE ---
      // Singura actiune permisa: butonul MENIURI de jos.
      // Cardurile (temp/ventilatie/lumina/usa) NU mai modifica nimic la atingere.
      if (touchY >= 198) {
        paginaEcran = 1;
        redeseneazaEcran = true;
      } 
    } 
    else if (paginaEcran == 1) {
      if (touchY >= 200) {
        // INAPOI LA DASHBOARD
        paginaEcran = 0;
        redeseneazaEcran = true;
      }
      else {
        // --- Grila 2x2 (carduri intre y=40 si y=162) ---
        // Rand sus: y 40..95   Rand jos: y 107..162
        // Col stanga: x<160    Col dreapta: x>160
        bool randSus = (touchY >= 40 && touchY < 100);
        bool randJos = (touchY >= 100 && touchY < 175);
        bool colStanga = (touchX < 160);

        if (randSus && colStanga)        { paginaEcran = 2; redeseneazaEcran = true; }  // 1. VENTILATIE
        else if (randSus && !colStanga)  { paginaEcran = 3; redeseneazaEcran = true; }  // 2. ILUMINAT
        else if (randJos && colStanga)   { paginaEcran = 4; redeseneazaEcran = true; }  // 3. USA
        else if (randJos && !colStanga)  { paginaEcran = 5; confirmaReset = false; redeseneazaEcran = true; }  // 4. RESETARE
      }
    }
    // --- SUBMENIURI (le construim pe rand) ---
    else if (paginaEcran == 2) {
      // ===== SUBMENIU VENTILATIE =====
      if (touchY >= 200) {
        paginaEcran = 1; redeseneazaEcran = true;
      }
      // Cardurile sunt intre y=38 si y=133. Latime card ~98.
      // Card MOD: x 8..106 | Stare releu: x 114..212 (NESELECTABIL) | Set prag: x 220..318
      else if (touchY >= 38 && touchY < 133) {

        // --- CARD 1: CONTROL MOD (x < 106) ---
        if (touchX < 110) {
          // Ciclam: AUTO -> MANUAL ON -> MANUAL OFF -> DINAMIC -> AUTO
          String nouMod;
          if (modVentilatie == "AUTO")            nouMod = "MANUAL ON";
          else if (modVentilatie == "MANUAL ON")  nouMod = "MANUAL OFF";
          else if (modVentilatie == "MANUAL OFF") nouMod = "DINAMIC";
          else                                    nouMod = "AUTO";  // de la DINAMIC

          // Daca vrem DINAMIC dar nu avem internet -> blocam, sarim la AUTO
          if (nouMod == "DINAMIC" && !areInternet()) {
            nouMod = "AUTO";
            mesajVentTemp = "FARA NET - DINAMIC BLOCAT";
            mesajVentExpira = millis() + 2500;
          }

          modVentilatie = nouMod;

          // Aplicam local efectul pe releu/PWM
          if (modVentilatie == "MANUAL ON") {
            ventilatorPornit = true;
            digitalWrite(RELAY_PIN, LOW);
            ledcWrite(FAN_PWM_PIN, speedNormal);
          } else if (modVentilatie == "MANUAL OFF") {
            ventilatorPornit = false;
            digitalWrite(RELAY_PIN, HIGH);
            ledcWrite(FAN_PWM_PIN, 0);
          }
          // AUTO si DINAMIC sunt gestionate de logica din loop()

          // Trimitem in baza de date + punem sync pe pauza 6 sec
          trimiteSetareDB("vent_mod", modVentilatie);
          blocSyncPanaLa = millis() + 6000;
          redeseneazaEcran = true;
        }

        // --- CARD 3: SET PRAG (x > 220) ---
        else if (touchX > 214) {
          // jumatatea de sus a cardului = +1 ; jumatatea de jos = -1
          int mijlocCard = 38 + 95 / 2;  // ~85
          if (touchY < mijlocCard) {
            if (tempDorita < 35) tempDorita += 1;   // limita sus 35
          } else {
            if (tempDorita > 15) tempDorita -= 1;   // limita jos 15
          }
          trimiteSetareDB("vent_prag", String((int)tempDorita));
          blocSyncPanaLa = millis() + 6000;
          redeseneazaEcran = true;
        }
        // CARD 2 (Stare releu) = neselectabil, nu facem nimic
      }
    }
    else if (paginaEcran == 3) {
      // ===== SUBMENIU ILUMINAT =====
      if (touchY >= 200) {
        paginaEcran = 1; redeseneazaEcran = true;
      }
      // Carduri intre y=38 si y=133.
      // Stare bec: x 8..106 (NESELECTABIL) | Curent priza: x 114..212 | Intensitate: x 220..318
      else if (touchY >= 38 && touchY < 133) {

        // --- CARD 2: CURENT PRIZA / RELEU (mijloc, x 110..214) ---
        if (touchX >= 110 && touchX <= 214) {
          releuLuminaPornit = !releuLuminaPornit;
          if (releuLuminaPornit) {
            digitalWrite(RELAY_LIGHT_PIN, LOW);   // da curent
            becSoftPornit = true;
            becUptimeStart = millis();
          } else {
            digitalWrite(RELAY_LIGHT_PIN, HIGH);  // taie curent
            becSoftPornit = false;
            tapoConectat = false;
          }
          trimiteSetareDB("lumina_releu", releuLuminaPornit ? "ON" : "OFF");
          blocSyncPanaLa = millis() + 6000;
          redeseneazaEcran = true;
        }

        // --- CARD 3: INTENSITATE (dreapta, x > 214) ---
        else if (touchX > 214) {
          int mijlocCard = 38 + 95 / 2;  // ~85
          if (touchY < mijlocCard) {
            luminozitate += 10;
            if (luminozitate > 100) luminozitate = 100;
          } else {
            luminozitate -= 10;
            if (luminozitate < 10) luminozitate = 10;  // minim 10%
          }
          // aplicam pe becul Tapo daca are curent
          if (releuLuminaPornit && asiguraConexiuneTapo()) {
            becTapo.set_brightness(luminozitate);
          }
          trimiteSetareDB("lumina_intensitate", String(luminozitate));
          blocSyncPanaLa = millis() + 6000;
          redeseneazaEcran = true;
        }
        // CARD 1 (Stare bec) = neselectabil
      }
    }
    else if (paginaEcran == 4) {
      // ===== SUBMENIU USA =====
      if (touchY >= 200) {
        paginaEcran = 1; redeseneazaEcran = true;
      }
      // Carduri intre y=45 si y=145. Status: x 10..155 (NESELECTABIL) | Comanda: x 169..314
      else if (touchY >= 45 && touchY < 145) {
        // --- CARD 2: COMANDA USA (dreapta, x > 160) ---
        if (touchX > 160) {
          actioneazaUsa();   // misca motorul + inverseaza usaDeschisa + taie curentul in bobine
          trimiteSetareDB("usa_status", usaDeschisa ? "OPEN" : "CLOSED");
          blocSyncPanaLa = millis() + 6000;
          redeseneazaEcran = true;
        }
        // CARD 1 (Status) = neselectabil
      }
    }
    else if (paginaEcran == 5) {
      // ===== SUBMENIU RESETARE =====
      if (!confirmaReset) {
        // --- ECRAN NORMAL ---
        // buton INAPOI jos
        if (touchY >= 200) {
          paginaEcran = 1;
          redeseneazaEcran = true;
        }
        // buton RESETARE (x 60..260, y 130..180)
        else if (touchX >= 60 && touchX <= 260 && touchY >= 130 && touchY <= 180) {
          confirmaReset = true;   // trecem la ecranul de confirmare
          redeseneazaEcran = true;
        }
      } else {
        // --- ECRAN CONFIRMARE ---
        // buton DA (x 20..150, y 140..200)
        if (touchX >= 20 && touchX <= 150 && touchY >= 140 && touchY <= 200) {
          bool ok = reseteazaDispozitiv();
          mesajReset = ok ? "RESETAT! user:admin parola:1234" : "EROARE LA RESETARE";
          mesajResetExpira = millis() + 4000;
          confirmaReset = false;  // ne intoarcem la ecranul normal (cu mesajul)
          redeseneazaEcran = true;
        }
        // buton NU (x 170..300, y 140..200)
        else if (touchX >= 170 && touchX <= 300 && touchY >= 140 && touchY <= 200) {
          confirmaReset = false;  // anulam, inapoi la ecranul normal
          redeseneazaEcran = true;
        }
      }
    }
    delay(400); 
  }
  if (redeseneazaEcran) { actualizeazaEcran(); redeseneazaEcran = false; }
}
