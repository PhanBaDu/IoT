/*
 * =============================================================================
 *  PROJECT IoT Nhóm 6 - ESP8266
 * =============================================================================
 *  Mô tả  : Node IoT đọc cảm biến nhiệt độ / độ ẩm (DHT11), điều khiển LED,
 *           hiển thị OLED, kết nối MQTT, đồng bộ NTP, tự động bật/tắt đèn
 *           theo giờ (18h-6h).
 *
 *  Phân cụm:
 *    - Đọc DHT11 mỗi 20 giây
 *    - Hiển thị giá trị lên màn hình OLED SH1106 128x64
 *    - Gửi dữ liệu cảm biến lên Node-RED qua MQTT (TLS/HiveMQ Cloud)
 *    - Nhận lệnh LED từ Node-RED (slider, JSON, text)
 *    - Tự động bật đèn lúc tối (18h), tắt đèn lúc sáng (6h)
 *    - Cảnh báo khi nhiệt độ / độ ẩm vượt ngưỡng
 *
 *  Phần cứng: ESP8266 (Wemos D1 Mini), DHT11, OLED SH1106 128x64 I2C
 *
 *  Phiên bản: 1.0
 *  Ngày     : 03/2026
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
// 1. ĐỊNH NGHĨA CHÂN GPIO
// =============================================================================
#define DHT_PIN     0    // Cảm biến DHT11 - Chân Data (GPIO 0 / D3)
#define OLED_SCL    4    // OLED SH1106    - Chân SCL (GPIO 4 / D2)
#define OLED_SDA    5    // OLED SH1106    - Chân SDA (GPIO 5 / D1)
#define LED_PIN    12    // Đèn LED        - Chân PWM (GPIO 12 / D6)
#define DHTTYPE    DHT11 // Loại cảm biến DHT (DHT11 / DHT22)


// =============================================================================
// 2. CẤU HÌNH WIFI
// =============================================================================
const char *ssid     = "Quy Dong";     // Tên mạng WiFi
const char *password = "12345678";    // Mật khẩu WiFi


// =============================================================================
// 3. CẤU HÌNH MQTT (HiveMQ Cloud - TLS)
// =============================================================================
const char *mqtt_server = "b1c8f2cc5cd4416fb74b671a407dc0ff.s1.eu.hivemq.cloud";
const int   mqtt_port   = 8883;      // Port TLS (có mật khẩu)
const char *mqtt_user   = "hivemq.webclient.1774528740272";
const char *mqtt_pass   = "&SG7@O0px!A3Iju6w%aD";

/*
 * Tóm tắt các topic MQTT:
 *
 *  iot/env/data      - ESP gửi dữ liệu cảm biến lên Node-RED (nhiệt độ, độ ẩm,
 *                      cảnh báo). Node-RED chỉ nhận, không gửi về topic này.
 *
 *  iot/led/power/set - Node-RED gửi lệnh điều khiển đèn cho ESP.
 *                      ESP nhận và xử lý lệnh, không gửi trả lời về topic này.
 *
 *  iot/led/status    - ESP gửi trạng thái LED (bật/tắt, độ sáng) về Node-RED
 *                      sau khi nhận lệnh hoặc khi trạng thái thay đổi.
 *                      Dùng để Node-RED đồng bộ giao diện dashboard.
 */
const char *mqtt_env     = "iot/env/data";      // Gửi: dữ liệu cảm biến
const char *mqtt_led     = "iot/led/power/set"; // Nhận: lệnh LED từ Node-RED
const char *mqtt_led_sts = "iot/led/status";    // Gửi: trạng thái LED


// =============================================================================
// 4. KHỞI TẠO CÁC ĐỐI TƯỢNG (biến toàn cục)
// =============================================================================
WiFiClientSecure espClient;                     // Kết nối TLS đến HiveMQ Cloud
PubSubClient mqtt(espClient);                  // Client MQTT (TLS)
DHT dht(DHT_PIN, DHTTYPE);                     // Cảm biến nhiệt độ / độ ẩm

/*
 * OLED SH1106 128x64 - Giao thức I2C
 * Mặc định I2C ESP8266: SCL=GPIO4 (D2), SDA=GPIO5 (D1)
 * U8G2_R0        : không xoay màn hình
 * U8X8_PIN_NONE  : không dùng chân reset của OLED
 */
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


// =============================================================================
// 5. BIẾN TRẠNG THÁI CẢM BIẾN
// =============================================================================
bool ledState   = false;   // Trạng thái đèn: false = tắt, true = bật
int  brightness = 0;       // Độ sáng LED: 0-100 (quy đổi 0-255 cho PWM)
float temperature = 0.0f;   // Nhiệt độ hiện tại (°C), lấy từ DHT11
float humidity    = 0.0f;   // Độ ẩm hiện tại (%), lấy từ DHT11

// Lưu trạng thái cảnh báo DHT11 (dùng chung cho OLED + MQTT callback)
bool tempHighFlag = false;
bool tempLowFlag  = false;
bool humiHighFlag = false;
bool humiLowFlag  = false;


// =============================================================================
// 6. BIẾN TRẠNG THÁI GỬI MQTT
// =============================================================================
/*
 * lastLedStateSent / lastBrightnessPublished
 *   - Lưu trạng thái LED đã gửi lên topic iot/led/status lần cuối.
 *   - Dùng để tránh gửi trùng lặp cùng một giá trị.
 *   - Mỗi khi ledState hoặc brightness thay đổi, hàm publishLedStatusIfNeeded()
 *     sẽ gửi giá trị mới lên MQTT.
 */
bool lastLedStateSent       = false;
int  lastBrightnessPublished = -1;


// =============================================================================
// 7. BIẾN TRẠNG THÁI TỰ ĐỘNG THEO GIỜ
// =============================================================================
/*
 * lastBrightness
 *   - Lưu độ sáng hiện tại (0-100) trước khi chế độ tự động tắt đèn.
 *   - Khi đèn bật trở lại, sẽ khôi phục đúng độ sáng này.
 *   - Giá trị mặc định = 100 (nếu chưa từng bật đèn).
 */
int lastBrightness = 100;

/*
 * lastManualTime
 *   - Thời điểm (ms) người dùng thao tác tay cuối cùng trên đèn.
 *   - Dùng để xác định người dùng có đang ở "chế độ tay" hay "chế độ tự động".
 *   - MANUAL_WINDOW = 60 phút: nếu 60 phút không có thao tác tay,
 *     chương trình sẽ chuyển sang chế độ tự động (bật/tắt đèn theo giờ).
 */
unsigned long lastManualTime   = 0;
const unsigned long MANUAL_WINDOW = 60UL * 60UL * 1000UL;  // 60 phút = 3.600.000 ms


// =============================================================================
// 8. TIMER & CHU KỲ XỬ LÝ
// =============================================================================
unsigned long lastDHT = 0;      // Thời điểm đọc DHT11 lần cuối (ms)
unsigned long lastMQTT = 0;     // Thời điểm thử kết nối MQTT lần cuối (ms)
unsigned long lastNTPSync = 0;  // Thời điểm đồng bộ NTP lần cuối (ms)

/*
 * INTERVAL_DHT
 *   - Chu kỳ đọc cảm biến DHT11 = 20 giây.
 *   - DHT11 có tần số đọc tối thiểu khoảng 1-2 giây, nhưng để 20 giây
 *     giúp giảm lượng MQTT, tiết kiệm pin và tránh ù ẩm.
 */
const unsigned long INTERVAL_DHT = 20000UL;  // 20.000 ms = 20 giây

/*
 * INTERVAL_NTP
 *   - Chu kỳ đồng bộ lại NTP = 1 giờ.
 *   - ESP8266 không có RTC (đồng hồ thực) nên sau khi mất nguồn, thời gian
 *     sẽ bị reset. Đồng bộ lại định kỳ giúp duy trì giờ chính xác.
 */
const unsigned long INTERVAL_NTP = 3600000UL;  // 1 giờ = 3.600.000 ms


// =============================================================================
// 9. NGƯỠNG CẢNH BÁO
// =============================================================================
const float TEMP_MAX = 38.0f;   // Ngưỡng nhiệt độ CAO: > 38°C
const float TEMP_MIN = 18.0f;   // Ngưỡng nhiệt độ THẤP: < 18°C
const float HUMI_MAX = 80.0f;   // Ngưỡng độ ẩm CAO: > 80%
const float HUMI_MIN = 30.0f;   // Ngưỡng độ ẩm THẤP: < 30%


// =============================================================================
// 10. CẤU HÌNH NTP - THỜI GIAN
// =============================================================================
/*
 * Đồng bộ giờ qua NTP (Network Time Protocol) để:
 *   - Hiển thị giờ chính xác trên Serial / OLED
 *   - Dựa vào tự động bật/tắt đèn theo giờ (18h-6h)
 *
 * Server  : pool.ntp.org (trả về giờ UTC)
 * Lệch múi: UTC+7 (Việt Nam, không có giờ mùa hè / daylight saving)
 *   -> Tham số offset = 7 * 3600 = 25200 giây
 *
 * Cú pháp hàm configTime():
 *   configTime(gmtOffset_sec, daylightOffset_sec, server)
 *     gmtOffset_sec      : số giây lệch so với UTC (+7h = +25200s cho VN)
 *     daylightOffset_sec : số giây DST (Việt Nam = 0, không có giờ hè)
 *     server             : địa chỉ server NTP
 */
const char *NTP_SERVER = "pool.ntp.org";          // Server NTP quốc tế
const long  GMT_OFFSET_SEC_VIETNAM = 7L * 3600L;  // Lệch UTC+7 (giây)

/*
 * ntpSynced
 *   - Flag cho biết đồng hồ NTP đã được đồng bộ thành công chưa.
 *   - Ban đầu = false, được đặt = true trong hàm syncTime() khi NTP thành công.
 *   - Dùng để đảm bảo schedule (bật/tắt đèn theo giờ) chỉ chạy khi giờ đã chính
 *     xác, tránh trường hợp đồng hồ bị reset về epoch 0 (00:00 1/1/1970 UTC).
 */
bool ntpSynced = false;


// =============================================================================
// ========================  ĐỊNH NGHĨA HÀM  ====================================
// =============================================================================


// -----------------------------------------------------------------------------
// THỜI GIAN (NTP)
// -----------------------------------------------------------------------------

/*
 * Hàm: syncTime
 * Mục đích: Đồng bộ đồng hồ ESP8266 với server NTP (giờ UTC+7 Việt Nam)
 *
 * Các bước:
 *   1. Gọi configTime() để cấu hình offset +7h và server NTP
 *   2. Gọi time(nullptr) để lấy giờ hiện tại (số giây từ 01/01/1970 UTC)
 *   3. Chờ tối đa 10 lần (5 giây) để nhận được giờ
 *   4. Nếu thành công: gọi localtime() để chuyển đổi sang cấu trúc tm
 *      (giờ local VN = UTC + 7h)
 *   5. In kết quả ra Serial
 */
void syncTime()
{
  /*
   * gmtOffset_sec      = 7 * 3600 = 25200 giây  (UTC+7)
   * daylightOffset_sec = 0       (Việt Nam không có DST)
   */
  configTime(GMT_OFFSET_SEC_VIETNAM, 0, NTP_SERVER);

  Serial.println("Đang lấy giờ từ NTP...");
  time_t now = time(nullptr);
  int wait = 0;

  /*
   * now < 8*3600*2 = 57600 nghĩa là giờ chưa đồng bộ (còn là 0 hoặc giá trị nhỏ).
   * Chờ tối đa 10 lần, mỗi lần delay 500ms -> tối đa 5 giây.
   */
  while (now < 8 * 3600 * 2 && wait < 10) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    wait++;
  }
  Serial.println();

  if (now > 8 * 3600 * 2) {
    /*
     * localtime() chuyển đổi UTC -> giờ địa phương
     * theo configTime(25200, 0, ...) thì giờ VN = UTC + 7h
     */
    struct tm *t = localtime(&now);
    Serial.printf("[TIME] Giờ HN: %02d:%02d:%02d | Ngày: %02d/%02d/%04d\n",
                  t->tm_hour, t->tm_min, t->tm_sec,
                  t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
    Serial.println("Đồng hồ sync thành công!");
    ntpSynced = true;       // Đánh dấu NTP đã đồng bộ thành công
    lastNTPSync = millis(); // Lưu thời điểm đồng bộ
  } else {
    Serial.println("Đồng hồ chưa sync được.");
  }
}


// -----------------------------------------------------------------------------
// KIỂM TRA KHOẢNG THỜI GIAN TRONG NGÀY
// -----------------------------------------------------------------------------

/*
 * Hàm: isEveningPeriod
 * Mục đích: Kiểm tra hiện tại có phải là buổi tối hoặc đêm khuya
 * Trả về : true nếu 18:00 giờ tối - 05:59 sáng khuya
 * Ví dụ  : 01:35 AM (01:35 sáng) -> true (đêm khuya, vẫn trong khoảng 18h-6h)
 *           14:00 (14:00 chiều)   -> false (giờ hành chính)
 *           20:00 (20:00 tối)     -> true (buổi tối)
 *
 * Giả sử: time() trả về giờ địa phương (đã cộng +7h)
 */
bool isEveningPeriod()
{
  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  int h = t->tm_hour;
  return (h >= 18 || h < 6);   // 18:00 (6h chiều) - 05:59 (6h sáng)
}

/*
 * Hàm: isMorningOffPeriod
 * Mục đích: Kiểm tra hiện tại có phải là ban ngày (6h sáng - 17h59)
 * Trả về : true nếu 06:00 sáng <= giờ < 18:00 chiều
 * Ví dụ  : 08:00 AM (08:00 sáng) -> true
 *           13:00 (13:00 trưa)    -> true
 *           01:35 AM (01:35 sáng)  -> false (đêm khuya)
 */
bool isMorningOffPeriod()
{
  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  int h = t->tm_hour;
  return (h >= 6 && h < 18);   // 06:00 - 17:59
}


// -----------------------------------------------------------------------------
// GỬI TRẠNG THÁI LED LÊN MQTT
// -----------------------------------------------------------------------------

/*
 * Hàm: publishLedStatusIfNeeded
 * Mục đích: Gửi trạng thái LED (bật/tắt, độ sáng) lên topic iot/led/status
 *
 * Chỉ gửi khi:
 *   - Đã kết nối MQTT rồi
 *   - ledState HOẶC brightness thay đổi so với giá trị đã gửi lần cuối
 *
 * Payload JSON: {"ledState":true/false,"brightness":0-100}
 * Mục đích: Đồng bộ trạng thái đèn trên dashboard Node-RED
 */
void publishLedStatusIfNeeded()
{
  if (!mqtt.connected())
    return;

  // Nếu giá trị không thay đổi so với lần gửi trước -> bỏ qua
  if (ledState == lastLedStateSent && brightness == lastBrightnessPublished)
    return;

  // Cập nhật giá trị đã gửi
  lastLedStateSent       = ledState;
  lastBrightnessPublished = brightness;

  char ledBuf[64];
  snprintf(ledBuf, sizeof(ledBuf),
           "{\"ledState\":%s,\"brightness\":%d}",
           ledState ? "true" : "false",
           brightness);

  mqtt.publish(mqtt_led_sts, ledBuf);
}


// -----------------------------------------------------------------------------
// CALLBACK MQTT - XỬ LÝ LỆNH TỪ NODE-RED
// -----------------------------------------------------------------------------

/*
 * Hàm: mqttCallback
 * Mục đích: Xử lý tin nhắn nhận được từ topic iot/led/power/set
 *
 * Chỉ xử lý tin nhắn từ topic mqtt_led.
 * Các định dạng tin nhắn hỗ trợ:
 *
 *   1. JSON  : {"brightness":70}   -> Đặt độ sáng 70%
 *              {"ledState":true}   -> Bật đèn (100%)
 *              {"ledState":false}  -> Tắt đèn (0%)
 *
 *   2. Số    : "50"                -> Slider: đặt độ sáng 50%
 *
 *   3. Văn bản: "true" / "on"      -> Bật đèn
 *               "false" / "off"    -> Tắt đèn
 *
 * Sau khi xử lý, gọi publishLedStatusIfNeeded() để gửi trạng thái về
 * Node-RED (đồng bộ dashboard).
 *
 * Tham số:
 *   topic   : topic MQTT của tin nhắn
 *   payload : nội dung tin nhắn (bytes thuần)
 *   len     : độ dài tin nhắn (số bytes)
 */
void mqttCallback(char *topic, byte *payload, unsigned int len)
{
  // Chỉ xử lý topic iot/led/power/set, bỏ qua các topic khác
  if (String(topic) != mqtt_led)
    return;

  // Chuyển payload (bytes) thành String để xử lý
  String msg;
  for (unsigned int i = 0; i < len; i++)
    msg += (char)payload[i];

  // ===== ĐỊNH DẠNG 1: JSON =====
  if (msg.startsWith("{")) {
    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, msg);

    if (!err) {
      // {"brightness":70} - Đặt độ sáng từ 0-100%
      if (doc.containsKey("brightness")) {
        brightness = doc["brightness"].as<int>();
        brightness = constrain(brightness, 0, 100);                    // Giới hạn 0-100
        analogWrite(LED_PIN, map(brightness, 0, 100, 0, 255));      // PWM 0-255
        ledState = (brightness > 0);
        lastBrightness = brightness;       // Lưu lại để khôi phục
        lastManualTime = millis();          // Đánh dấu thao tác tay
        Serial.printf("[LED] brightness: %d%%\n", brightness);
        displayOLED(tempHighFlag, tempLowFlag, humiHighFlag, humiLowFlag); // Cập nhật OLED ngay
      }
      // {"ledState":true/false} - Bật hoặc tắt đèn
      else if (doc.containsKey("ledState")) {
        if (doc["ledState"].as<bool>()) {
          brightness = 100;
          analogWrite(LED_PIN, 255);
          ledState = true;
          lastBrightness = 100;
          lastManualTime = millis();
          Serial.println("[LED] ON");
          displayOLED(tempHighFlag, tempLowFlag, humiHighFlag, humiLowFlag);
        } else {
          brightness = 0;
          analogWrite(LED_PIN, 0);
          ledState = false;
          lastBrightness = 0;
          lastManualTime = millis();
          Serial.println("[LED] OFF");
          displayOLED(tempHighFlag, tempLowFlag, humiHighFlag, humiLowFlag);
        }
      }
    }
    publishLedStatusIfNeeded();
    return;
  }

  // ===== ĐỊNH DẠNG 2: SỐ NGUYÊN (slider Node-RED) =====
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
    displayOLED(tempHighFlag, tempLowFlag, humiHighFlag, humiLowFlag); // Cập nhật OLED ngay
    return;
  }

  // ===== ĐỊNH DẠNG 3: VĂN BẢN (true/false/on/off) =====
  bool on = (msg == "true" || msg == "on");
  if (!on && msg != "false" && msg != "off")
    return;  // Không phải lệnh hợp lệ -> bỏ qua

  brightness = on ? 100 : 0;
  analogWrite(LED_PIN, on ? 255 : 0);
  ledState = on;
  lastBrightness = brightness;
  lastManualTime = millis();
  Serial.println(on ? "[LED] ON" : "[LED] OFF");

  publishLedStatusIfNeeded();
  displayOLED(tempHighFlag, tempLowFlag, humiHighFlag, humiLowFlag); // Cập nhật OLED ngay
}


// -----------------------------------------------------------------------------
// KẾT NỐI WIFI
// -----------------------------------------------------------------------------

/*
 * Hàm: connectWiFi
 * Mục đích: Kết nối ESP8266 đến mạng WiFi
 *
 * - Sử dụng WiFi.begin(ssid, password) để bắt đầu kết nối.
 * - Vòng while đợi cho đến khi WiFi.status() == WL_CONNECTED.
 * - Mỗi lần delay(500) để tránh watchdog timer reset.
 * - Sau khi kết nối thành công, in địa chỉ IP ra Serial.
 */
void connectWiFi()
{
  WiFi.begin(ssid, password);
  Serial.print("Đang kết nối WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);   // Chờ 0.5s, tránh ESP8266 watchdog reset
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
}


// -----------------------------------------------------------------------------
// KẾT NỐI MQTT
// -----------------------------------------------------------------------------

/*
 * Hàm: connectMQTT
 * Mục đích: Kết nối ESP8266 đến HiveMQ Cloud qua MQTT/TLS
 *
 * Các bước:
 *   1. Tạo client ID ngẫu nhiên (tránh trùng lặp khi ESP reset nhiều lần)
 *   2. Gọi mqtt.connect() với user/password (TLS)
 *   3. Nếu thành công: đăng ký nhận topic iot/led/power/set
 *   4. Nếu thất bại: in mã lỗi MQTT (rc) ra Serial
 *
 * Mã lỗi MQTT thường gặp:
 *   -2 : MQTT_CONNECTION_TIMEOUT
 *   -4 : MQTT_CONNECTION_UNAUTHORIZED (user/pass sai)
 *   -5 : MQTT_BAD_CREDENTIALS
 */
void connectMQTT()
{
  // Tạo ID ngẫu nhiên: "esp8266-nhom6-xxxx" (xxxx: 1000-9999)
  String clientId = "esp8266-nhom6-" + String(random(1000, 9999));

  if (mqtt.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
    Serial.println("MQTT connected!");
    mqtt.subscribe(mqtt_led);   // Đăng ký nhận lệnh LED từ Node-RED
  } else {
    Serial.print("MQTT thất bại, rc=");
    Serial.println(mqtt.state());  // In mã lỗi ra Serial
  }
}


// -----------------------------------------------------------------------------
// GỬI DỮ LIỆU CẢM BIẾN SANG NODE-RED (MQTT)
// -----------------------------------------------------------------------------

/*
 * Hàm: publishEnvData
 * Mục đích: Gửi dữ liệu cảm biến (nhiệt độ, độ ẩm, cảnh báo) lên topic iot/env/data
 *
 * Payload JSON (300 byte dự kiến):
 *   {
 *     "temperature": 28.0,
 *     "humidity": 65.0,
 *     "tempAlertHigh": false,
 *     "tempAlertLow": false,
 *     "humiAlertHigh": false,
 *     "humiAlertLow": false,
 *     "message": "Môi trường bình thường"
 *   }
 *
 * Tham số:
 *   tempHigh, tempLow : true nếu nhiệt độ vượt ngưỡng TEMP_MAX / TEMP_MIN
 *   humiHigh, humiLow : true nếu độ ẩm vượt ngưỡng HUMI_MAX / HUMI_MIN
 *
 * Sau khi gửi dữ liệu cảm biến, gọi publishLedStatusIfNeeded() để đồng bộ
 * trạng thái LED (nếu có thay đổi).
 */
void publishEnvData(bool tempHigh, bool tempLow, bool humiHigh, bool humiLow)
{
  char buf[300];
  const char *msg;

  // Xác định thông báo theo tình trạng môi trường
  if (tempHigh)       msg = "Nhiệt độ quá cao!";
  else if (tempLow)  msg = "Nhiệt độ quá thấp!";
  else if (humiHigh) msg = "Độ ẩm quá cao!";
  else if (humiLow)  msg = "Độ ẩm quá thấp!";
  else               msg = "Môi trường bình thường";

  // Tạo chuỗi JSON
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


// -----------------------------------------------------------------------------
// HIỂN THỊ OLED SH1106 128x64
// -----------------------------------------------------------------------------
#define OLED_FONT u8g2_font_8x13_tf   // Font 8x13 pixel, tiếng Anh

// Tọa độ Y của 3 dòng trên màn hình OLED 64 pixel
static const uint8_t OLED_Y_ROW1 = 14;
static const uint8_t OLED_Y_ROW2 = 28;
static const uint8_t OLED_Y_ROW3 = 42;

/*
 * Hàm: displayOLED
 * Mục đích: Hiển thị thông tin lên màn hình OLED SH1106
 *
 * Bố cục 3 dòng:
 *   Dòng 1: Nhiệt độ (nếu cảnh báo: hiển "Cao!" hoặc "Thấp!")
 *   Dòng 2: Độ ẩm    (nếu cảnh báo: hiển "Cao!" hoặc "Thấp!")
 *   Dòng 3: Trạng thái đèn LED (BẬT/TẮT + %)
 *
 * Tham số:
 *   tempHigh, tempLow : có hiển thị cảnh báo nhiệt độ không
 *   humiHigh, humiLow : có hiển thị cảnh báo độ ẩm không
 */
void displayOLED(bool tempHigh, bool tempLow, bool humiHigh, bool humiLow)
{
  char buf[40];

  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);

  // ----- Dòng 1: Nhiệt độ -----
  if (!tempHigh && !tempLow) {
    // Môi trường bình thường: hiển giá trị + ngưỡng
    sprintf(buf, "T:%.1f%cC %d-%d%cC", temperature, 0xB0,
            (int)TEMP_MIN, (int)TEMP_MAX, 0xB0);
    u8g2.drawStr(0, OLED_Y_ROW1, buf);
  } else {
    // Có cảnh báo: hiển giá trị + cảnh báo ở bên phải
    sprintf(buf, "T:%.1f%cC", temperature, 0xB0);
    u8g2.drawStr(0, OLED_Y_ROW1, buf);
    u8g2.drawStr(72, OLED_Y_ROW1, tempHigh ? "Cao!" : "Thap!");
  }

  // ----- Dòng 2: Độ ẩm -----
  if (!humiHigh && !humiLow) {
    sprintf(buf, "H:%.1f%% %d-%d%%", humidity, (int)HUMI_MIN, (int)HUMI_MAX);
    u8g2.drawStr(0, OLED_Y_ROW2, buf);
  } else {
    sprintf(buf, "H:%.1f%%", humidity);
    u8g2.drawStr(0, OLED_Y_ROW2, buf);
    u8g2.drawStr(72, OLED_Y_ROW2, humiHigh ? "Cao!" : "Thap!");
  }

  // ----- Dòng 3: Trạng thái đèn LED -----
  sprintf(buf, "Den: %s %d%%", ledState ? "BAT" : "TAT", brightness);
  u8g2.drawStr(0, OLED_Y_ROW3, buf);

  // Gửi dữ liệu từ buffer ra màn hình OLED
  u8g2.sendBuffer();
}


// =============================================================================
// ===========================  CHƯƠNG TRÌNH CHÍNH  =============================
// =============================================================================

/*
 * Hàm: setup
 * Mục đích: Khởi tạo phần cứng và kết nối ban đầu (chỉ chạy 1 lần)
 *
 * Thứ tự khởi tạo:
 *   1. Serial    - Để debug
 *   2. GPIO      - Cài đặt chân LED
 *   3. DHT11     - Khởi tạo cảm biến nhiệt độ / độ ẩm
 *   4. OLED      - Khởi tạo màn hình, hiển thị thông báo
 *   5. WiFi      - Kết nối mạng (chờ tới thành công)
 *   6. NTP       - Đồng bộ thời gian (chờ tới thành công)
 *   7. MQTT      - Cấu hình server, callback, kết nối
 */
void setup()
{
  Serial.begin(115200);   // Tốc độ Serial: 115200 baud
  delay(500);

  // --- Khởi tạo GPIO ---
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, 0);  // LED ban đầu: tắt

  // --- Khởi tạo DHT11 ---
  dht.begin();

  // --- Khởi tạo OLED SH1106 ---
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);
  u8g2.drawStr(0, 20, "Đang khởi tạo...");
  u8g2.drawStr(0, 36, "WiFi + Asia/HN...");
  u8g2.sendBuffer();

  // --- Kết nối WiFi + Đồng bộ NTP ---
  connectWiFi();
  syncTime();

  // --- Khởi tạo MQTT ---
  espClient.setInsecure();              // Bỏ qua xác thực certificate (TLS)
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);       // Hàm xử lý tin nhắn nhận được
  randomSeed(micros());                 // Khởi tạo bộ sinh số ngẫu nhiên

  Serial.println("=== ESP8266 IoT Nhom 6 - Khoi tao xong ===");

  // Hiển thị OLED ngay lần đầu (không cần chờ 20s)
  displayOLED(false, false, false, false);
}

/*
 * Hàm: loop
 * Mục đích: Vòng lặp chính - chạy liên tục sau khi setup() xong
 *
 * Mỗi chu kỳ loop():
 *   1. Kiểm tra WiFi - nếu mất kết nối -> tự động kết nối lại
 *   2. Kiểm tra MQTT  - nếu mất kết nối -> thử kết nối lại sau 5s
 *   3. Gọi mqtt.loop() để xử lý tin nhắn MQTT (gọi callback)
 *   4. Xử lý tự động bật/tắt đèn theo giờ (nếu ở chế độ AUTO)
 *   5. Đọc cảm biến DHT11 (chu kỳ 20s) -> gửi MQTT + cập nhật OLED
 */
void loop()
{
  // ----- Kiểm tra & kết nối lại WiFi (nếu bị mất) -----
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    ntpSynced = false;  // Mất WiFi -> coi như cần đồng bộ NTP lại
  }

  // ----- Đồng bộ NTP định kỳ (1 giờ) -----
  /*
   * ESP8266 không có pin RTC nên mất nguồn sẽ mất giờ.
   * Đồng bộ lại NTP mỗi 1 giờ để đảm bảo giờ luôn chính xác.
   */
  if (WiFi.status() == WL_CONNECTED) {
    if (millis() - lastNTPSync >= INTERVAL_NTP) {
      Serial.println("[NTP] Đồng bộ lại...");
      syncTime();
    }
  }

  // ----- Kiểm tra & kết nối lại MQTT (nếu bị mất) -----
  if (!mqtt.connected()) {
    if (millis() - lastMQTT > 5000) {  // Chờ 5s trước khi thử lại
      lastMQTT = millis();
      connectMQTT();
    }
  }
  mqtt.loop();   // Xử lý nhận/trả tin nhắn MQTT (gọi callback)

  // ----- Tự động bật/tắt đèn theo giờ -----
  /*
   * elapsed = thời gian đã trôi qua (ms) kể từ lần thao tác tay cuối.
   * Nếu elapsed >= MANUAL_WINDOW (60 phút) -> cho phép chế độ tự động.
   * Nếu lastManualTime == 0 (first boot):
   *   - Nếu đang trong giờ bật đèn (18h-6h) -> bật đèn ngay
   *   - Nếu đang trong giờ tắt đèn (6h-18h) -> tắt đèn ngay
   * Lưu ý: chỉ chạy khi ntpSynced == true, vì trước khi NTP sync,
   *   time(nullptr) trả về epoch 0 (00:00 1/1/1970 VN = 7h) -> schedule sai giờ.
   */
  unsigned long elapsed = (millis() >= lastManualTime)
    ? (millis() - lastManualTime)
    : 0;

  if (ntpSynced) {
    if (lastManualTime == 0) {
      // First boot: bật/tắt đèn ngay theo giờ hiện tại, không cần đợi 60 phút
      if (isEveningPeriod() && !ledState) {
        brightness = (lastBrightness > 0) ? lastBrightness : 100;
        analogWrite(LED_PIN, map(brightness, 0, 100, 0, 255));
        ledState = true;
        Serial.println("[SCHEDULE] First boot: bat den (18h-6h)");
        publishLedStatusIfNeeded();
        displayOLED(tempHighFlag, tempLowFlag, humiHighFlag, humiLowFlag);
      } else if (isMorningOffPeriod() && ledState) {
        lastBrightness = brightness;
        brightness = 0;
        analogWrite(LED_PIN, 0);
        ledState = false;
        Serial.println("[SCHEDULE] First boot: tat den (6h-18h)");
        publishLedStatusIfNeeded();
        displayOLED(tempHighFlag, tempLowFlag, humiHighFlag, humiLowFlag);
      }
    } else if (elapsed >= MANUAL_WINDOW) {
      // Đã hết 60 phút không thao tác tay: chạy schedule bình thường
      if (isEveningPeriod() && !ledState) {
        brightness = (lastBrightness > 0) ? lastBrightness : 100;
        analogWrite(LED_PIN, map(brightness, 0, 100, 0, 255));
        ledState = true;
        Serial.println("[SCHEDULE] Tu dong bat den (18h-6h)");
        publishLedStatusIfNeeded();
        displayOLED(tempHighFlag, tempLowFlag, humiHighFlag, humiLowFlag);
      } else if (isMorningOffPeriod() && ledState) {
        lastBrightness = brightness;
        brightness = 0;
        analogWrite(LED_PIN, 0);
        ledState = false;
        Serial.println("[SCHEDULE] Tu dong tat den (6h-18h)");
        publishLedStatusIfNeeded();
        displayOLED(tempHighFlag, tempLowFlag, humiHighFlag, humiLowFlag);
      }
    }
  }

  // ----- Đọc cảm biến DHT11 (chu kỳ 20s) -----
  if (millis() - lastDHT >= INTERVAL_DHT) {
    lastDHT = millis();

    // --- Đọc nhiệt độ & độ ẩm ---
    temperature = dht.readTemperature();
    humidity    = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("[ERR] Đọc DHT11 thất bại!");
    } else {
      Serial.printf("[DHT] T:%.1fC  H:%.1f%%\n", temperature, humidity);
    }

    // --- Kiểm tra cảnh báo ngưỡng ---
    bool tempHigh = !isnan(temperature) && (temperature > TEMP_MAX);
    bool tempLow  = !isnan(temperature) && (temperature < TEMP_MIN);
    bool humiHigh = !isnan(humidity)    && (humidity > HUMI_MAX);
    bool humiLow  = !isnan(humidity)    && (humidity < HUMI_MIN);

    // Lưu trạng thái cảnh báo vào biến toàn cục (dùng chung cho OLED ngay lập tức khi thao tác LED)
    tempHighFlag = tempHigh;
    tempLowFlag  = tempLow;
    humiHighFlag = humiHigh;
    humiLowFlag  = humiLow;

    if (tempHigh) {
      Serial.printf("       [CANH BAO] Nhiệt độ quá cao: %.1fC > %.1fC\n",
                    temperature, TEMP_MAX);
    }
    if (tempLow) {
      Serial.printf("       [CANH BAO] Nhiệt độ quá thấp: %.1fC < %.1fC\n",
                    temperature, TEMP_MIN);
    }
    if (humiHigh) {
      Serial.printf("       [CANH BAO] Độ ẩm quá cao: %.1f%% > %.1f%%\n",
                    humidity, HUMI_MAX);
    }
    if (humiLow) {
      Serial.printf("       [CANH BAO] Độ ẩm quá thấp: %.1f%% < %.1f%%\n",
                    humidity, HUMI_MIN);
    }

    // --- Gửi MQTT & cập nhật OLED ---
    if (mqtt.connected()) {
      publishEnvData(tempHigh, tempLow, humiHigh, humiLow);
    }
    displayOLED(tempHigh, tempLow, humiHigh, humiLow);
  }
}
