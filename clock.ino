/*
* M5Cardputer Clock with SD Config
* 2026 
* @Arktopru
*/
#include <M5Cardputer.h>
#include <WiFi.h>
#include <time.h>
#include <M5GFX.h>
#include <SD.h>
#include <ArduinoJson.h>   // Установите через менеджер библиотек
#include "Open_24_Display_St24pt7b.h"

// --- ЗНАЧЕНИЯ ПО УМОЛЧАНИЮ (если нет файла) ---
const char* default_ssid = "YOUR_SSID";
const char* default_password = "YOUR_SSID_PASSWORD";
const char* default_ntpServer = "pool.ntp.org";
long default_gmtOffset = 10 * 3600;   // 10 часов (ваш пояс)

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ НАСТРОЕК ---
char config_ssid[32] = "";
char config_password[64] = "";
char config_ntpServer[32] = "";
long config_gmtOffset = default_gmtOffset;

struct tm timeinfo;
char timeStr[20];
char dateStr[20];
bool wifiConnected = false;
bool showDate = false;

M5Canvas sprite(&M5Cardputer.Display);
unsigned long nextUpdate = 0;

// ------------------------------------------------------------------
bool loadConfig() {
  // Инициализация SD
  if (!SD.begin(12)) {   // Пин CS для M5Cardputer обычно 4
    Serial.println("SD card init failed");
    return false;
  }

  File file = SD.open("/clock.json");
  if (!file) {
    Serial.println("Config file not found, using defaults");
    return false;
  } else {
    Serial.println("Config file found");
  }

  // Читаем файл в строку
  String jsonString;
  while (file.available()) {
    jsonString += (char)file.read();
  }
  file.close();

  // Парсим JSON
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    Serial.println("JSON parse failed");
    return false;
  }

  // Извлекаем значения
  if (doc.containsKey("ssid")) {
    strlcpy(config_ssid, doc["ssid"], sizeof(config_ssid));
  } else {
    strcpy(config_ssid, default_ssid);
  }

  if (doc.containsKey("password")) {
    strlcpy(config_password, doc["password"], sizeof(config_password));
  } else {
    strcpy(config_password, default_password);
  }

  if (doc.containsKey("gmtOffset")) {
    config_gmtOffset = doc["gmtOffset"];
  } else {
    config_gmtOffset = default_gmtOffset;
  }

  if (doc.containsKey("ntpServer")) {
    strlcpy(config_ntpServer, doc["ntpServer"], sizeof(config_ntpServer));
  } else {
    strcpy(config_ntpServer, default_ntpServer);
  }

  return true;
}

// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);

  sprite.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
  sprite.setRotation(0);

  // --- Загружаем конфиг с SD ---
  bool configLoaded = loadConfig();

  // Если конфиг не загрузился, используем умолчания
  if (!configLoaded) {
    strcpy(config_ssid, default_ssid);
    strcpy(config_password, default_password);
    strcpy(config_ntpServer, default_ntpServer);
    config_gmtOffset = default_gmtOffset;
  }

  // --- Подключение к Wi-Fi ---
  M5Cardputer.Display.setFont(&fonts::FreeMono9pt7b);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.setCursor(10, 20);
  M5Cardputer.Display.print("Connecting to ");
  M5Cardputer.Display.println(config_ssid);

  WiFi.begin(config_ssid, config_password);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 30) {
    delay(500);
    M5Cardputer.Display.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    configTime(config_gmtOffset, 0, config_ntpServer);
    delay(1000);
    getLocalTime(&timeinfo);
  } else {
    M5Cardputer.Display.println("\nWiFi not connected");
  }

  delay(1500);

  drawInterface();
  uint32_t ms = millis() % 1000;
  nextUpdate = millis() + (ms == 0 ? 1000 : (1000 - ms));
}

// ------------------------------------------------------------------
void loop() {
  M5Cardputer.update();

  if (millis() >= nextUpdate) {
    getLocalTime(&timeinfo);
    drawInterface();
    uint32_t ms = millis() % 1000;
    nextUpdate = millis() + (ms == 0 ? 1000 : (1000 - ms));
  }

  if (M5Cardputer.Keyboard.isChange()) {
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

    if (status.ctrl) {
      syncTimeManual();
      uint32_t ms = millis() % 1000;
      nextUpdate = millis() + (ms == 0 ? 1000 : (1000 - ms));
    }

    if (status.space) {
      showDate = !showDate;
      drawInterface();
      uint32_t ms = millis() % 1000;
      nextUpdate = millis() + (ms == 0 ? 1000 : (1000 - ms));
      delay(200);
    }
  }
}

void syncTimeManual() {
  M5Cardputer.Display.fillScreen(BLACK);
  M5Cardputer.Display.setFont(&fonts::FreeMono9pt7b);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextDatum(MC_DATUM);
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.drawString("NTP syncing...", M5Cardputer.Display.width()/2, M5Cardputer.Display.height()/2);

  if (wifiConnected) {
    configTime(config_gmtOffset, 0, config_ntpServer);
    delay(1500);
    if (getLocalTime(&timeinfo)) {
      M5Cardputer.Display.drawString("OK!", M5Cardputer.Display.width()/2, M5Cardputer.Display.height()/2 + 30);
    } else {
      M5Cardputer.Display.drawString("Error", M5Cardputer.Display.width()/2, M5Cardputer.Display.height()/2 + 30);
    }
    delay(1000);
  } else {
    M5Cardputer.Display.drawString("No WiFi", M5Cardputer.Display.width()/2, M5Cardputer.Display.height()/2 + 30);
    delay(1000);
  }
  drawInterface();
}

void drawInterface() {
  sprite.fillScreen(BLACK);
  sprite.setFont(&fonts::FreeMono9pt7b);
  sprite.setTextSize(1);
  sprite.setTextColor(WHITE);

  sprite.drawRect(5, 5, sprite.width() - 10, sprite.height() - 10, WHITE);
  sprite.drawRect(8, 8, sprite.width() - 16, sprite.height() - 16, WHITE);

  sprite.setTextDatum(TL_DATUM);
  if (wifiConnected) {
    sprite.setTextColor(GREEN);
    sprite.drawString("NTP: On", 15, 15);
  } else {
    sprite.setTextColor(RED);
    sprite.drawString("NTP: Off", 15, 15);
  }

  sprite.setTextColor(WHITE);
  sprite.setTextDatum(TR_DATUM);
  sprite.drawString("CLOCK", sprite.width() - 15, 15);

  sprite.setFont(&Open_24_Display_St24pt7b);
  sprite.setTextSize(1);
  sprite.setTextColor(WHITE);
  sprite.setTextDatum(MC_DATUM);

  if (showDate) {
    sprintf(dateStr, "%02d.%02d.%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    sprite.setFont(&fonts::FreeMono9pt7b);
    sprite.drawString("DATE", sprite.width() / 2, sprite.height() / 2 - 30);
    sprite.setFont(&Open_24_Display_St24pt7b);
    sprite.drawString(dateStr, sprite.width() / 2, sprite.height() / 2 + 10);
  } else {
    sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    sprite.drawString(timeStr, sprite.width() / 2, sprite.height() / 2);
    sprite.setFont(&fonts::FreeMono9pt7b);
    sprintf(dateStr, "%02d/%02d", timeinfo.tm_mday, timeinfo.tm_mon + 1);
    sprite.drawString(dateStr, sprite.width() / 2, sprite.height() - 15);
  }

  sprite.pushSprite(0, 0);
}
