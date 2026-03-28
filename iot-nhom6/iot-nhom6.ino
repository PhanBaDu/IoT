#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>

// ============ GPIO (dung dung theo muc luc) ============
#define DHT_PIN     0    // Cam bien nhiet do / do am (DHT11) - GPIO 0
#define OLED_SCL    4    // OLED - SCL - GPIO 4
#define OLED_SDA    5    // OLED - SDA - GPIO 5
#define LED_PIN    12    // Den LED - GPIO 12
#define DHTTYPE    DHT11

// ============ WIFI ============
const char *ssid = "Anh đủ";
const char *password = "123123123";

// ============ MQTT ============
const char *mqtt_server = "b1c8f2cc5cd4416fb74b671a407dc0ff.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char *mqtt_user = "hivemq.webclient.1774528740272";
const char *mqtt_pass = "&SG7@O0px!A3Iju6w%aD";

// Topics
//  - iot/env/data  : chi gui du lieu CAM BIEN (nhiet do, do am, chuyen dong, canh bao)
//  - iot/led/status: gui trang thai LED ve Node-RED (slider dung topic nay de cap nhat trang thai)
//  - iot/led/power/set: nhan lenh LED tu Node-RED
const char *mqtt_env     = "iot/env/data";
const char *mqtt_led     = "iot/led/power/set";
const char *mqtt_led_sts = "iot/led/status";

// ============ OBJECTS ============
WiFiClientSecure espClient;
PubSubClient mqtt(espClient);
DHT dht(DHT_PIN, DHTTYPE);

// OLED SH1106 128x64 - Kham thi, dung I2C mac dinh (SCL=GPIO4, SDA=GPIO5)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ============ TRANG THAI ============
bool ledState = false;
int brightness = 0;
float temperature = 0;
float humidity = 0;

// 上次发到 iot/led/status 的值（避免重复；含亮度变化也推送，便于 Node-RED 同步）
bool lastLedStateSent = false;
int lastBrightnessPublished = -1;

// Luu do sang truoc khi tu dong tat sang (de luc bat lai giu nguyen)
int lastBrightness = 100;

// Manual override: neu nguoi dung bam tay, danh dau de schedule bo qua
unsigned long lastManualTime = 0;
const unsigned long MANUAL_WINDOW = 60 * 60 * 1000UL;  // 60 phut

// ============ TIMERS ============
unsigned long lastDHT = 0;
unsigned long lastMQTT = 0;
const unsigned long INTERVAL_DHT = 2000;

// Nguong canh bao
const float TEMP_MAX = 38.0;   // Nhiet do cao qua muc
const float TEMP_MIN = 18.0;   // Nhiet do thap qua muc
const float HUMI_MAX = 80.0;   // Do am cao qua muc
const float HUMI_MIN = 30.0;   // Do am thap qua muc

// ============ TIME (NTP) ============
const char *NTP_SERVER = "pool.ntp.org";
// Asia/Ho_Chi_Minh: ICT (Indochina Time) = UTC+7, khong co daylight saving
const char *TZ_STRING = "ICT-7";

void syncTime()
{
  setenv("TZ", TZ_STRING, 1);
  tzset();
  configTime(0, 0, NTP_SERVER);
  Serial.println("Dang lay gio tu NTP...");
  time_t now = time(nullptr);
  int wait = 0;
  while (now < 8 * 3600 * 2 && wait < 10) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    wait++;
  }
  Serial.println();
  if (now > 8 * 3600 * 2) {
    struct tm *t = localtime(&now);
    Serial.printf("[TIME] Gio HN: %02d:%02d:%02d | Ngay: %02d/%02d/%04d\n",
                  t->tm_hour, t->tm_min, t->tm_sec,
                  t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
    Serial.println("Dong ho sync thanh cong!");
  } else {
    Serial.println("Dong ho chua sync duoc.");
  }
}

bool isEveningPeriod()
{
  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  int h = t->tm_hour;
  return (h >= 18 || h < 6);   // 18:00 - 05:59
}

bool isMorningOffPeriod()
{
  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  int h = t->tm_hour;
  return (h >= 6 && h < 18);   // 06:00 - 17:59
}

// 仅在 ledState 或 brightness 与上次发布不同时推送到 iot/led/status
void publishLedStatusIfNeeded()
{
  if (!mqtt.connected())
    return;
  if (ledState == lastLedStateSent && brightness == lastBrightnessPublished)
    return;
  lastLedStateSent = ledState;
  lastBrightnessPublished = brightness;
  char ledBuf[64];
  snprintf(ledBuf, sizeof(ledBuf),
           "{\"ledState\":%s,\"brightness\":%d}",
           ledState ? "true" : "false", brightness);
  mqtt.publish(mqtt_led_sts, ledBuf);
}

// ============ CALLBACK MQTT ============
// Chi xu ly lenh LED tu Node-RED. Khong gui gi ve khi nhan.
void mqttCallback(char *topic, byte *payload, unsigned int len)
{
  if (String(topic) != mqtt_led)
    return;

  String msg;
  for (unsigned int i = 0; i < len; i++)
    msg += (char)payload[i];

  // ---- JSON: {"brightness":70} hoac {"ledState":true} ----
  if (msg.startsWith("{")) {
    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, msg);
    if (!err) {
      if (doc.containsKey("brightness")) {
        brightness = doc["brightness"].as<int>();
        brightness = constrain(brightness, 0, 100);
        analogWrite(LED_PIN, map(brightness, 0, 100, 0, 255));
        ledState = (brightness > 0);
        lastBrightness = brightness;
        lastManualTime = millis();
        Serial.printf("[LED] brightness: %d%%\n", brightness);
      } else if (doc.containsKey("ledState")) {
        if (doc["ledState"].as<bool>()) {
          brightness = 100;
          analogWrite(LED_PIN, 255);
          ledState = true;
          lastBrightness = 100;
          lastManualTime = millis();
          Serial.println("[LED] ON");
        } else {
          brightness = 0;
          analogWrite(LED_PIN, 0);
          ledState = false;
          lastBrightness = 0;
          lastManualTime = millis();
          Serial.println("[LED] OFF");
        }
      }
    }
    publishLedStatusIfNeeded();
    return;
  }

  // ---- Chuoi chi co chu so 0-100 (slider) ----
  bool onlyDigits = msg.length() > 0;
  for (unsigned int i = 0; i < msg.length(); i++) {
    if (!isdigit((unsigned char)msg[i])) {
      onlyDigits = false;
      break;
    }
  }
  if (onlyDigits) {
    brightness = msg.toInt();
    brightness = constrain(brightness, 0, 100);
    analogWrite(LED_PIN, map(brightness, 0, 100, 0, 255));
    ledState = (brightness > 0);
    lastBrightness = brightness;
    lastManualTime = millis();
    Serial.printf("[LED] brightness (raw): %d%%\n", brightness);
    publishLedStatusIfNeeded();
    return;
  }

  // ---- Plain text: "true"/"false"/"on"/"off" ----
  bool on = (msg == "true" || msg == "on");
  if (!on && msg != "false" && msg != "off")
    return;

  brightness = on ? 100 : 0;
  analogWrite(LED_PIN, on ? 255 : 0);
  ledState = on;
  lastBrightness = brightness;
  lastManualTime = millis();
  Serial.println(on ? "[LED] ON" : "[LED] OFF");

  publishLedStatusIfNeeded();
}

// ============ KET NOI WIFI ============
void connectWiFi()
{
  WiFi.begin(ssid, password);
  Serial.print("Dang ket noi WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
}

// ============ KET NOI MQTT ============
void connectMQTT()
{
  String clientId = "esp8266-nhom6-" + String(random(1000, 9999));
  if (mqtt.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
    Serial.println("MQTT connected!");
    mqtt.subscribe(mqtt_led);
  } else {
    Serial.print("MQTT that bai, rc=");
    Serial.println(mqtt.state());
  }
}

// ============ GUI DU LIEU CAM BIEN SANG NODE-RED ============
// Topic: iot/env/data
// Payload: {"temperature":28.0,"humidity":65.0,
//           "tempAlertHigh":false,"tempAlertLow":false,
//           "humiAlertHigh":false,"humiAlertLow":false,
//           "message":"Moi truong binh thuong"}
void publishEnvData(bool tempHigh, bool tempLow, bool humiHigh, bool humiLow)
{
  char buf[300];
  const char *msg;

  if (tempHigh)       msg = "Nhiet do qua cao!";
  else if (tempLow)   msg = "Nhiet do qua thap!";
  else if (humiHigh)  msg = "Do am qua cao!";
  else if (humiLow)   msg = "Do am qua thap!";
  else                msg = "Moi truong binh thuong";

  snprintf(buf, sizeof(buf),
           "{\"temperature\":%.1f,\"humidity\":%.1f,"
           "\"tempAlertHigh\":%s,\"tempAlertLow\":%s,"
           "\"humiAlertHigh\":%s,\"humiAlertLow\":%s,"
           "\"message\":\"%s\"}",
           temperature, humidity,
           tempHigh  ? "true" : "false",
           tempLow   ? "true" : "false",
           humiHigh  ? "true" : "false",
           humiLow   ? "true" : "false",
           msg);
  mqtt.publish(mqtt_env, buf);

  publishLedStatusIfNeeded();
}

// ============ HIEN THI OLED ============
#define OLED_FONT u8g2_font_8x13_tf
static const uint8_t OLED_Y_ROW1 = 14;
static const uint8_t OLED_Y_ROW2 = 28;
static const uint8_t OLED_Y_ROW3 = 42;
static const uint8_t OLED_Y_ROW4 = 56;

void displayOLED(bool tempHigh, bool tempLow, bool humiHigh, bool humiLow)
{
  char buf[40];

  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);

  if (!tempHigh && !tempLow) {
    sprintf(buf, "T:%.1f%cC %d-%d%cC", temperature, 0xB0,
            (int)TEMP_MIN, (int)TEMP_MAX, 0xB0);
    u8g2.drawStr(0, OLED_Y_ROW1, buf);
  } else {
    sprintf(buf, "T:%.1f%cC", temperature, 0xB0);
    u8g2.drawStr(0, OLED_Y_ROW1, buf);
    u8g2.drawStr(72, OLED_Y_ROW1, tempHigh ? "Cao!" : "Thap!");
  }

  if (!humiHigh && !humiLow) {
    sprintf(buf, "H:%.1f%% %d-%d%%", humidity, (int)HUMI_MIN, (int)HUMI_MAX);
    u8g2.drawStr(0, OLED_Y_ROW2, buf);
  } else {
    sprintf(buf, "H:%.1f%%", humidity);
    u8g2.drawStr(0, OLED_Y_ROW2, buf);
    u8g2.drawStr(72, OLED_Y_ROW2, humiHigh ? "Cao!" : "Thap!");
  }

  sprintf(buf, "Den: %s %d%%", ledState ? "BAT" : "TAT", brightness);
  u8g2.drawStr(0, OLED_Y_ROW3, buf);

  bool isAuto = (millis() - lastManualTime) >= MANUAL_WINDOW;
  u8g2.drawStr(0, OLED_Y_ROW4, isAuto ? "Che do: AUTO" : "Che do: NHAN TAY");

  u8g2.sendBuffer();
}

// ============ SETUP ============
void setup()
{
  Serial.begin(115200);
  delay(500);

  // --- GPIO ---
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, 0);  // LED ban dau tat

  // --- DHT11 ---
  dht.begin();

  // --- OLED SH1106 ---
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);
  u8g2.drawStr(0, 20, "Dang khoi tao...");
  u8g2.drawStr(0, 36, "WiFi + Asia/HN...");
  u8g2.sendBuffer();

  // --- WiFi + Dong ho NTP ---
  connectWiFi();
  syncTime();

  // --- MQTT ---
  espClient.setInsecure();
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);
  randomSeed(micros());

  Serial.println("=== ESP8266 IoT Nhom 6 - Khoi tao xong ===");
}

// ============ LOOP ============
void loop()
{
  // --- WiFi ---
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    syncTime();
  }

  // --- MQTT ---
  if (!mqtt.connected()) {
    if (millis() - lastMQTT > 5000) {
      lastMQTT = millis();
      connectMQTT();
    }
  }
  mqtt.loop();

  // --- TU DONG BAT / TAT DEN THEO GIO (neu chua co thao tac tay) ---
  unsigned long elapsed = (millis() >= lastManualTime)
    ? (millis() - lastManualTime)
    : 0;

  if (elapsed >= MANUAL_WINDOW) {
    if (isEveningPeriod() && !ledState) {
      // 18h-6h: tu dong bat den
      brightness = lastBrightness > 0 ? lastBrightness : 100;
      analogWrite(LED_PIN, map(brightness, 0, 100, 0, 255));
      ledState = true;
      Serial.println("[SCHEDULE] Tu dong bat den (18h-6h)");
      publishLedStatusIfNeeded();
    } else if (isMorningOffPeriod() && ledState) {
      // 6h-18h: tu dong tat den
      lastBrightness = brightness;
      brightness = 0;
      analogWrite(LED_PIN, 0);
      ledState = false;
      Serial.println("[SCHEDULE] Tu dong tat den (6h-18h)");
      publishLedStatusIfNeeded();
    }
  }

  // --- DOC CAM BIEN (2s) ---
  if (millis() - lastDHT >= INTERVAL_DHT) {
    lastDHT = millis();

    // DHT11
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("[ERR] Doc DHT11 that bai!");
    } else {
      Serial.printf("[DHT] T:%.1fC  H:%.1f%%\n", temperature, humidity);
    }

    // Canh bao nhiet do / do am cao va thap
    bool tempHigh = !isnan(temperature) && (temperature > TEMP_MAX);
    bool tempLow  = !isnan(temperature) && (temperature < TEMP_MIN);
    bool humiHigh = !isnan(humidity)    && (humidity > HUMI_MAX);
    bool humiLow  = !isnan(humidity)    && (humidity < HUMI_MIN);

    if (tempHigh) {
      Serial.printf("       [CANH BAO] Nhiet do qua cao: %.1fC > %.1fC\n",
                    temperature, TEMP_MAX);
    }
    if (tempLow) {
      Serial.printf("       [CANH BAO] Nhiet do qua thap: %.1fC < %.1fC\n",
                    temperature, TEMP_MIN);
    }
    if (humiHigh) {
      Serial.printf("       [CANH BAO] Do am qua cao: %.1f%% > %.1f%%\n",
                    humidity, HUMI_MAX);
    }
    if (humiLow) {
      Serial.printf("       [CANH BAO] Do am qua thap: %.1f%% < %.1f%%\n",
                    humidity, HUMI_MIN);
    }

    // Gui MQTT & hien thi OLED
    if (mqtt.connected()) {
      publishEnvData(tempHigh, tempLow, humiHigh, humiLow);
    }
    displayOLED(tempHigh, tempLow, humiHigh, humiLow);
  }
}
