/*
 * =============================================================================
 *  PROJECT IoT Nhom 6 - ESP8266
 * =============================================================================
 *  Mo ta: Node IoT doc cam bien nhiet do / do am (DHT11), dieu khien LED,
 *         hien thi OLED, ket noi MQTT, dong bo thoi gian NTP, tu dong bat/tat
 *         den theo gio (18h-6h).
 *
 *  Cac tinh nang chinh:
 *    - Doc nhiet do & do am moi 2 giay
 *    - Hien thi gia tri len man hinh OLED SH1106
 *    - Gui du lieu cam bien len Node-RED qua MQTT
 *    - Nhan lenh bat/tat LED tu Node-RED (slider, JSON, text)
 *    - Tu dong bat den luc toi (18h) va tat den luc sang (6h)
 *    - Canh bao khi nhiet do / do am vuot nguong
 *
 *  Phan cung: ESP8266 (Wemos D1 Mini), DHT11, OLED SH1106 128x64 I2C
 * =============================================================================
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>


// =============================================================================
// 1. DINH NGHIA CHAN GPIO
// =============================================================================
#define DHT_PIN     0       // Cam bien DHT11 - Chan Data (GPIO 0)
#define OLED_SCL    4       // OLED - Chan SCL (GPIO 4)
#define OLED_SDA    5       // OLED - Chan SDA (GPIO 5)
#define LED_PIN    12       // Den LED - Chan PWM (GPIO 12)
#define DHTTYPE    DHT11    // Loai cam bien DHT (DHT11 / DHT22)


// =============================================================================
// 2. CAU HINH WIFI
// =============================================================================
const char *WIFI_SSID   = "Anh đủ";
const char *WIFI_PASS   = "123123123";


// =============================================================================
// 3. CAU HINH MQTT
// =============================================================================
// Server HiveMQ Cloud (TLS, port 8883)
const char *MQTT_SERVER  = "b1c8f2cc5cd4416fb74b671a407dc0ff.s1.eu.hivemq.cloud";
const int   MQTT_PORT    = 8883;
const char *MQTT_USER    = "hivemq.webclient.1774528740272";
const char *MQTT_PASS    = "&SG7@O0px!A3Iju6w%aD";

// Cac topic MQTT
//   - iot/env/data    : ESP gui du lieu cam bien len Node-RED (nhiet do, do am, canh bao)
//   - iot/led/power/set: Node-RED gui lenh dieu khien LED den ESP
//   - iot/led/status  : ESP gui lai trang thai LED hien tai ve Node-RED
const char *TOPIC_ENV     = "iot/env/data";      // Gui du lieu cam bien
const char *TOPIC_LED_CMD = "iot/led/power/set";  // Nhan lenh LED
const char *TOPIC_LED_STS = "iot/led/status";     // Gui trang thai LED


// =============================================================================
// 4. KHOI TAO CAC DOI TUONG
// =============================================================================
WiFiClientSecure espClient;   // Ket noi TLS den HiveMQ
PubSubClient     mqttClient;  // Thu vien MQTT
DHT              dht(DHT_PIN, DHTTYPE);

// OLED SH1106 128x64 - Giao thuc I2C (SCL=GPIO4, SDA=GPIO5)
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);


// =============================================================================
// 5. BIEN TRANG THAI
// =============================================================================
bool ledState       = false;  // Trang thai LED (bat/tat)
int  brightness     = 0;      // Do sang LED (0-100%)
float temperature   = 0.0;   // Nhiet do hien tai (C)
float humidity      = 0.0;    // Do am hien tai (%)

// Luu trang thai LED da gui len MQTT lan cuoi
// Dung de tranh gui trung lap cung mot gia tri
bool lastLedStateSent       = false;
int  lastBrightnessSent     = -1;

// Luu do sang truoc khi tu dong tat
// De luc bat lai, den se khoi phuc dung do sang cu
int  savedBrightness        = 100;

// Thoi diem nguoi dung thao tac tay cuoi cung
// Neu chua thao tac tay trong 60 phut -> cho phep che do tu dong
unsigned long lastManualTime     = 0;
const unsigned long MANUAL_TIMEOUT = 60UL * 60UL * 1000UL;  // 60 phut


// =============================================================================
// 6. HANG SO & NGUONG CANH BAO
// =============================================================================
const float THRESHOLD_TEMP_HIGH = 38.0;  // Nguong nhiet do cao
const float THRESHOLD_TEMP_LOW  = 18.0;  // Nguong nhiet do thap
const float THRESHOLD_HUMI_HIGH = 80.0;  // Nguong do am cao
const float THRESHOLD_HUMI_LOW  = 30.0;  // Nguong do am thap

// Khoang thoi gian doc cam bien (ms)
const unsigned long DHT_INTERVAL = 20000UL;  // Doc moi 20 giay

// Thoi diem doc cam bien cuoi cung
unsigned long lastDHTTime      = 0;
unsigned long lastMQTTReconnect = 0;


// =============================================================================
// 7. CAU HINH THOI GIAN (NTP)
// =============================================================================
// Server NTP de dong bo dong ho
const char *NTP_SERVER = "pool.ntp.org";

// Muc gio Viet Nam (ICT = UTC+7), khong co daylight saving
const char *TZ_INFO    = "ICT-7";


// =============================================================================
// ===========================  HAM THIET BI  ==================================
// =============================================================================

/*
 * Ham: khoiTaoGPIO
 * Mo ta : Cai dat che do cho cac chan GPIO
 */
void khoiTaoGPIO()
{
    pinMode(LED_PIN, OUTPUT);
    analogWrite(LED_PIN, 0);  // LED ban dau tat
}

/*
 * Ham: khoiTaoOLED
 * Mo ta : Khoi tao man hinh OLED, hien thi thong bao khoi dong
 */
void khoiTaoOLED()
{
    oled.begin();
    oled.clearBuffer();
    oled.setFont(u8g2_font_8x13_tf);
    oled.drawStr(0, 20, "Dang khoi tao...");
    oled.drawStr(0, 36, "WiFi + Asia/HN...");
    oled.sendBuffer();
}


// =============================================================================
// ===========================  WIFI  ===========================================
// =============================================================================

/*
 * Ham: ketNoiWiFi
 * Mo ta : Ket noi den mang WiFi, doi cho den khi thanh cong
 */
void ketNoiWiFi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Dang ket noi WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
}


// =============================================================================
// ===========================  THOI GIAN (NTP)  ================================
// =============================================================================

/*
 * Ham: dongBoThoiGian
 * Mo ta : Dong bo dong ho ESP8266 voi server NTP (gio Viet Nam UTC+7)
 */
void dongBoThoiGian()
{
    setenv("TZ", TZ_INFO, 1);
    tzset();
    configTime(0, 0, NTP_SERVER);

    Serial.println("Dang lay gio tu NTP...");
    time_t now = time(nullptr);
    int count = 0;

    // Cho toi 10 lan (5 giay) de nhan duoc gio
    while (now < 8 * 3600 * 2 && count < 10) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        count++;
    }
    Serial.println();

    if (now > 8 * 3600 * 2) {
        struct tm *t = localtime(&now);
        Serial.printf("[TIME] Gio HN: %02d:%02d:%02d | Ngay: %02d/%02d/%04d\n",
                      t->tm_hour, t->tm_min, t->tm_sec,
                      t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
        Serial.println("Dong ho dong bo thanh cong!");
    } else {
        Serial.println("Dong ho chua dong bo duoc.");
    }
}


// =============================================================================
// ===========================  MQTT  ===========================================
// =============================================================================

/*
 * Ham: guiTrangThaiLED
 * Mo ta : Gui trang thai LED hien tai len MQTT
 *         Chi gui khi gia tri thay doi so voi lan gui truoc
 */
void guiTrangThaiLED()
{
    // Kiem tra da ket noi MQTT chua
    if (!mqttClient.connected()) return;

    // Kiem tra co thay doi gi khong
    if (ledState == lastLedStateSent && brightness == lastBrightnessSent) return;

    // Cap nhat gia tri da gui
    lastLedStateSent   = ledState;
    lastBrightnessSent = brightness;

    // Tao chuoi JSON chua trang thai LED
    char buf[64];
    snprintf(buf, sizeof(buf),
             "{\"ledState\":%s,\"brightness\":%d}",
             ledState ? "true" : "false",
             brightness);

    mqttClient.publish(TOPIC_LED_STS, buf);
}

/*
 * Ham: ketNoiMQTT
 * Mo ta : Ket noi den server MQTT, dang ky nhận lenh LED
 */
void ketNoiMQTT()
{
    // Tao ID client ngau nhien de tranh trung lap
    String clientId = "esp8266-nhom6-" + String(random(1000, 9999));

    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
        Serial.println("MQTT connected!");
        // Dang ky nhan lenh LED tu Node-RED
        mqttClient.subscribe(TOPIC_LED_CMD);
    } else {
        Serial.print("MQTT that bai, ma loi: ");
        Serial.println(mqttClient.state());
    }
}

/*
 * Ham: xuLyLenhLED
 * Mo ta : Xu ly cac lenh LED nhan duoc tu Node-RED qua MQTT
 *         Ho tro 3 dinh dang: JSON, so nguyen (slider), van ban (true/false/on/off)
 *
 * Vi du tin nhan:
 *   {"brightness":70}     -> Dat do sang 70%
 *   {"ledState":true}    -> Bat den
 *   "50"                  -> Slider 50%
 *   "true" / "on"        -> Bat den
 *   "false" / "off"      -> Tat den
 */
void xuLyLenhLED(String message)
{
    // ---- Dinh dang JSON: {"brightness":70} hoac {"ledState":true} ----
    if (message.startsWith("{")) {
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, message);

        if (!err) {
            // Xu ly do sang tu JSON
            if (doc.containsKey("brightness")) {
                brightness = doc["brightness"].as<int>();
                brightness = constrain(brightness, 0, 100);  // Gioi han 0-100%
                analogWrite(LED_PIN, map(brightness, 0, 100, 0, 255));
                ledState = (brightness > 0);
                savedBrightness = brightness;
                lastManualTime  = millis();

                Serial.printf("[LED] Do sang: %d%%\n", brightness);
            }
            // Xu ly trang thai tu JSON
            else if (doc.containsKey("ledState")) {
                if (doc["ledState"].as<bool>()) {
                    // Bat den
                    brightness = 100;
                    analogWrite(LED_PIN, 255);
                    ledState = true;
                    savedBrightness = 100;
                    lastManualTime  = millis();
                    Serial.println("[LED] BAT");
                } else {
                    // Tat den
                    brightness = 0;
                    analogWrite(LED_PIN, 0);
                    ledState = false;
                    savedBrightness = 0;
                    lastManualTime  = millis();
                    Serial.println("[LED] TAT");
                }
            }
        }
        guiTrangThaiLED();
        return;
    }

    // ---- Dinh dang so nguyen (slider tu Node-RED): "0" - "100" ----
    bool isNumeric = true;
    for (unsigned int i = 0; i < message.length(); i++) {
        if (!isdigit((unsigned char)message[i])) {
            isNumeric = false;
            break;
        }
    }

    if (isNumeric) {
        brightness = message.toInt();
        brightness = constrain(brightness, 0, 100);
        analogWrite(LED_PIN, map(brightness, 0, 100, 0, 255));
        ledState = (brightness > 0);
        savedBrightness = brightness;
        lastManualTime  = millis();

        Serial.printf("[LED] Do sang (slider): %d%%\n", brightness);
        guiTrangThaiLED();
        return;
    }

    // ---- Dinh dang van ban: "true"/"false"/"on"/"off" ----
    bool lenhBat = (message == "true" || message == "on");

    if (!lenhBat && message != "false" && message != "off") return;

    brightness = lenhBat ? 100 : 0;
    analogWrite(LED_PIN, lenhBat ? 255 : 0);
    ledState = lenhBat;
    savedBrightness = brightness;
    lastManualTime  = millis();

    Serial.println(lenhBat ? "[LED] BAT" : "[LED] TAT");
    guiTrangThaiLED();
}

/*
 * Ham: mqttCallback
 * Mo ta : Ham callback duoc goi khi nhan duoc tin nhan MQTT
 *         Chi xu ly tin nhan tu topic LED
 */
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    // Chi xu ly topic LED, bo qua cac topic khac
    if (String(topic) != TOPIC_LED_CMD) return;

    // Chuyen payload thanh chuoi
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }

    xuLyLenhLED(msg);
}


// =============================================================================
// ===========================  CAM BIEN  =======================================
// =============================================================================

/*
 * Ham: docCamBien
 * Mo ta : Doc nhiet do va do am tu cam bien DHT11
 * Tra ve: true neu doc thanh cong, false neu that bai
 */
bool docCamBien()
{
    temperature = dht.readTemperature();
    humidity    = dht.readHumidity();

    // Kiem tra loi doc cam bien
    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("[LOI] Doc DHT11 that bai!");
        return false;
    }

    Serial.printf("[DHT] Nhiet do: %.1f%cC  |  Do am: %.1f%%\n",
                  temperature, 0xB0, humidity);
    return true;
}

/*
 * Ham: kiemTraCanhBao
 * Mo ta : Kiem tra nhiet do va do am co vuot nguong canh bao khong
 */
void kiemTraCanhBao()
{
    bool nhietDo_cao = (temperature > THRESHOLD_TEMP_HIGH);
    bool nhietDo_thap = (temperature < THRESHOLD_TEMP_LOW);
    bool doAm_cao    = (humidity > THRESHOLD_HUMI_HIGH);
    bool doAm_thap   = (humidity < THRESHOLD_HUMI_LOW);

    if (nhietDo_cao) {
        Serial.printf("       [CANH BAO] Nhiet do qua CAO: %.1f%cC > %.1f%cC\n",
                      temperature, 0xB0, THRESHOLD_TEMP_HIGH, 0xB0);
    }
    if (nhietDo_thap) {
        Serial.printf("       [CANH BAO] Nhiet do qua THAP: %.1f%cC < %.1f%cC\n",
                      temperature, 0xB0, THRESHOLD_TEMP_LOW, 0xB0);
    }
    if (doAm_cao) {
        Serial.printf("       [CANH BAO] Do am qua CAO: %.1f%% > %.1f%%\n",
                      humidity, THRESHOLD_HUMI_HIGH);
    }
    if (doAm_thap) {
        Serial.printf("       [CANH BAO] Do am qua THAP: %.1f%% < %.1f%%\n",
                      humidity, THRESHOLD_HUMI_LOW);
    }
}


// =============================================================================
// ===========================  GUI MQTT  =======================================
// =============================================================================

/*
 * Ham: guiDuLieuCamBien
 * Mo ta : Gui du lieu cam bien (nhiet do, do am, canh bao) len Node-RED qua MQTT
 *
 * Payload JSON:
 *   {
 *     "temperature": 28.0,
 *     "humidity": 65.0,
 *     "tempAlertHigh": false,
 *     "tempAlertLow": false,
 *     "humiAlertHigh": false,
 *     "humiAlertLow": false,
 *     "message": "Moi truong binh thuong"
 *   }
 */
void guiDuLieuCamBien(bool nhietDoCao, bool nhietDoThap,
                      bool doAmCao, bool doAmThap)
{
    char buf[300];
    const char *thongBao;

    // Xac dinh thong bao theo tinh trang
    if (nhietDoCao)    thongBao = "Nhiet do qua CAO!";
    else if (nhietDoThap) thongBao = "Nhiet do qua THAP!";
    else if (doAmCao)  thongBao = "Do am qua CAO!";
    else if (doAmThap) thongBao = "Do am qua THAP!";
    else               thongBao = "Moi truong BINH THUONG";

    // Tao chuoi JSON
    snprintf(buf, sizeof(buf),
             "{\"temperature\":%.1f,\"humidity\":%.1f,"
             "\"tempAlertHigh\":%s,\"tempAlertLow\":%s,"
             "\"humiAlertHigh\":%s,\"humiAlertLow\":%s,"
             "\"message\":\"%s\"}",
             temperature, humidity,
             nhietDoCao    ? "true" : "false",
             nhietDoThap   ? "true" : "false",
             doAmCao       ? "true" : "false",
             doAmThap      ? "true" : "false",
             thongBao);

    // Gui len MQTT
    mqttClient.publish(TOPIC_ENV, buf);
    guiTrangThaiLED();
}


// =============================================================================
// ===========================  OLED  ===========================================
// =============================================================================

/*
 * Ham: hienThiOLED
 * Mo ta : Hien thi thong tin len man hinh OLED SH1106
 *         Giao dien 4 dong:
 *           Dong 1: Nhiet do + Nguong
 *           Dong 2: Do am + Nguong
 *           Dong 3: Trang thai den LED
 *           Dong 4: Che do hoat dong (Auto / Tay)
 */
void hienThiOLED(bool nhietDoCao, bool nhietDoThap,
                bool doAmCao, bool doAmThap)
{
    char buf[40];

    oled.clearBuffer();
    oled.setFont(u8g2_font_8x13_tf);

    // Dong 1: Nhiet do
    if (!nhietDoCao && !nhietDoThap) {
        sprintf(buf, "T:%.1f%cC %d-%d%cC", temperature, 0xB0,
                (int)THRESHOLD_TEMP_LOW, (int)THRESHOLD_TEMP_HIGH, 0xB0);
        oled.drawStr(0, 14, buf);
    } else {
        sprintf(buf, "T:%.1f%cC", temperature, 0xB0);
        oled.drawStr(0, 14, buf);
        oled.drawStr(72, 14, nhietDoCao ? "CAO!" : "THAP!");
    }

    // Dong 2: Do am
    if (!doAmCao && !doAmThap) {
        sprintf(buf, "H:%.1f%% %d-%d%%", humidity,
                (int)THRESHOLD_HUMI_LOW, (int)THRESHOLD_HUMI_HIGH);
        oled.drawStr(0, 28, buf);
    } else {
        sprintf(buf, "H:%.1f%%", humidity);
        oled.drawStr(0, 28, buf);
        oled.drawStr(72, 28, doAmCao ? "CAO!" : "THAP!");
    }

    // Dong 3: Trang thai den LED
    sprintf(buf, "Den: %s %d%%", ledState ? "BAT" : "TAT", brightness);
    oled.drawStr(0, 42, buf);

    // Dong 4: Che do hoat dong
    bool cheDoTuDong = (millis() - lastManualTime) >= MANUAL_TIMEOUT;
    oled.drawStr(0, 56, cheDoTuDong ? "Che do: AUTO" : "Che do: TAY");

    oled.sendBuffer();
}


// =============================================================================
// ===========================  TU DONG THEO GIO  ===============================
// =============================================================================

/*
 * Ham: kiemTraBuoiToi
 * Mo ta : Kiem tra xem hien tai co phai la buoi toi (18h-06h) khong
 * Tra ve: true neu la 18h-06h, false neu la 06h-18h
 */
bool kiemTraBuoiToi()
{
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    int gio = t->tm_hour;
    return (gio >= 18 || gio < 6);  // 18:00 - 05:59
}

/*
 * Ham: kiemTraBuoiSang
 * Mo ta : Kiem tra xem hien tai co phai la buoi sang (06h-18h) khong
 * Tra ve: true neu la 06h-18h, false neu la 18h-06h
 */
bool kiemTraBuoiSang()
{
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    int gio = t->tm_hour;
    return (gio >= 6 && gio < 18);  // 06:00 - 17:59
}

/*
 * Ham: xuLyTuDongTheoGio
 * Mo ta : Tu dong bat den luc toi (18h), tat den luc sang (06h)
 *         Chi hoat dong khi nguoi dung chua thao tac tay trong 60 phut
 */
void xuLyTuDongTheoGio()
{
    // Tinh thoi gian da troi qua tu lan thao tac tay cuoi
    unsigned long elapsed = (millis() >= lastManualTime)
                            ? (millis() - lastManualTime)
                            : 0;

    // Neu nguoi dung van con thao tac tay trong 60 phut -> bo qua
    if (elapsed < MANUAL_TIMEOUT) return;

    // Buoi toi (18h-06h): Tu dong bat den
    if (kiemTraBuoiToi() && !ledState) {
        brightness = (savedBrightness > 0) ? savedBrightness : 100;
        analogWrite(LED_PIN, map(brightness, 0, 100, 0, 255));
        ledState = true;
        Serial.println("[TU DONG] Bat den luc toi (18h-06h)");
        guiTrangThaiLED();
    }
    // Buoi sang (06h-18h): Tu dong tat den
    else if (kiemTraBuoiSang() && ledState) {
        savedBrightness = brightness;  // Luu lai do sang hien tai
        brightness = 0;
        analogWrite(LED_PIN, 0);
        ledState = false;
        Serial.println("[TU DONG] Tat den luc sang (06h-18h)");
        guiTrangThaiLED();
    }
}


// =============================================================================
// ===========================  MAIN  ==========================================
// =============================================================================

void setup()
{
    Serial.begin(115200);
    delay(500);

    // Khoi tao phan cung
    khoiTaoGPIO();
    dht.begin();
    khoiTaoOLED();

    // Ket noi mang
    ketNoiWiFi();
    dongBoThoiGian();

    // Cau hinh MQTT
    espClient.setInsecure();  // Bo qua xac thuc certificate (cho HiveMQ Cloud)
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    randomSeed(micros());

    Serial.println("=== ESP8266 IoT Nhom 6 - Khoi tao xong ===");
}

void loop()
{
    // ---- Kiem tra va ket noi lai WiFi neu bi mat ----
    if (WiFi.status() != WL_CONNECTED) {
        ketNoiWiFi();
        dongBoThoiGian();
    }

    // ---- Kiem tra va ket noi lai MQTT neu bi mat ----
    if (!mqttClient.connected()) {
        if (millis() - lastMQTTReconnect >= 5000) {
            lastMQTTReconnect = millis();
            ketNoiMQTT();
        }
    }
    mqttClient.loop();

    // ---- Xu ly tu dong bat/tat den theo gio ----
    xuLyTuDongTheoGio();

    // ---- Doc cam bien DHT11 (moi 2 giay) ----
    if (millis() - lastDHTTime >= DHT_INTERVAL) {
        lastDHTTime = millis();

        // Doc nhiet do va do am
        if (docCamBien()) {
            // Kiem tra canh bao
            kiemTraCanhBao();

            // Gui du lieu len MQTT (neu da ket noi)
            if (mqttClient.connected()) {
                bool nhietDoCao  = (temperature > THRESHOLD_TEMP_HIGH);
                bool nhietDoThap = (temperature < THRESHOLD_TEMP_LOW);
                bool doAmCao     = (humidity > THRESHOLD_HUMI_HIGH);
                bool doAmThap    = (humidity < THRESHOLD_HUMI_LOW);

                guiDuLieuCamBien(nhietDoCao, nhietDoThap, doAmCao, doAmThap);
            }
        }

        // Cap nhat man hinh OLED
        bool nhietDoCao  = (temperature > THRESHOLD_TEMP_HIGH);
        bool nhietDoThap = (temperature < THRESHOLD_TEMP_LOW);
        bool doAmCao     = (humidity > THRESHOLD_HUMI_HIGH);
        bool doAmThap    = (humidity < THRESHOLD_HUMI_LOW);
        hienThiOLED(nhietDoCao, nhietDoThap, doAmCao, doAmThap);
    }
}
