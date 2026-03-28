# BÁO CÁO BÀI TẬP LỚN

## HỆ THỐNG GIÁM SÁT MÔI TRƯỜNG VÀ ĐIỀU KHIỂN THIẾT BỊ QUA GIAO THỨC MQTT
### (ESP8266 + DHT11 + OLED SH1106 + Node-RED Dashboard)

---

**Môn:** Đồ án chuyên ngành  
**GV hướng dẫn:** ...  
**Sinh viên thực hiện:** ...  

**Mã nguồn:** [https://github.com/PhanBaDu/esp8266-mqtt-iot](https://github.com/PhanBaDu/esp8266-mqtt-iot)  
**Bảo mật:** Mọi thông tin nhạy cảm (MQTT user, password, Wi-Fi password) trong tài liệu này được thay bằng `****`.

---

## MỤC LỤC

1. [Đặt vấn đề](#1-đặt-vấn-đề)  
2. [Nội dung thực hiện](#2-nội-dung-thực-hiện)  
   - 2.1. Thu thập dữ liệu môi trường (DHT11)  
   - 2.2. Hiển thị OLED SH1106  
   - 2.3. Truyền dữ liệu qua MQTT (HiveMQ Cloud)  
   - 2.4. Xử lý dữ liệu bằng Node-RED  
   - 2.5. Hiển thị dữ liệu trên Dashboard  
   - 2.6. Cảnh báo môi trường  
   - 2.7. Điều khiển đèn từ xa qua MQTT  
   - 2.8. Đồng bộ trạng thái thiết bị  
   - 2.9. Tự động bật/tắt đèn theo giờ (NTP)  
3. [Thiết kế mạch IoT](#3-thiết-kế-mạch-iot)  
   - 3.1. Thành phần phần cứng  
   - 3.2. Kết nối phần cứng (GPIO)  
   - 3.3. Giải thích các linh kiện  
4. [Nguyên lý hoạt động](#4-nguyên-lý-hoạt-động)  
   - 4.1. Sơ đồ luồng hệ thống (Flow)  
   - 4.2. Luồng giám sát: Cảm biến → Dashboard  
   - 4.3. Luồng điều khiển: Dashboard → ESP8266 → LED  
   - 4.4. Luồng đồng bộ trạng thái LED  
   - 4.5. Luồng tự động theo giờ  
   - 4.6. Luồng duy trì kết nối  
5. [Cài đặt hệ thống](#5-cài-đặt-hệ-thống)  
   - 5.1. Yêu cầu phần mềm  
   - 5.2. Cài đặt Arduino IDE + thư viện  
   - 5.3. Cấu hình thông tin Wi-Fi & MQTT  
   - 5.4. Build, nạp firmware và Serial Monitor  
   - 5.5. Cài đặt Node.js & Node-RED  
   - 5.6. Cài đặt node-red-dashboard  
   - 5.7. Import flow Node-RED từ `node-red.json`  
   - 5.8. Cấu hình MQTT broker trong Node-RED  
   - 5.9. Truy cập Dashboard & kiểm tra kết nối  
6. [Kết quả thực hiện](#6-kết-quả-thực-hiện)  
   - 6.1. Các hình ảnh minh họa  
   - 6.2. Phân tích luồng hoạt động thực tế  
7. [Kết luận và hướng phát triển](#7-kết-luận-và-hướng-phát-triển)  
   - 7.1. Các công việc đã hoàn thành  
   - 7.2. Hạn chế và hướng phát triển tiếp theo  

**Phụ lục A** — Mã nguồn chương trình vi điều khiển (ESP8266)  
**Phụ lục B** — Node-RED Flow (JSON) & Dashboard  
**Phụ lục C** — Các nội dung bổ sung  

---

## 1. Đặt vấn đề

Internet of Things (IoT) là xu hướng kết nối các thiết bị vật lý với mạng internet, cho phép thu thập dữ liệu từ cảm biến và điều khiển thiết bị từ xa. Trong thực tế, nhu cầu giám sát nhiệt độ, độ ẩm trong phòng lab, kho hoặc không gian nhỏ là rất thiết thực: người dùng cần biết thông số môi trường hiện tại, được cảnh báo khi vượt ngưỡng an toàn, và có thể bật/tắt hoặc điều chỉnh độ sáng thiết bị (ví dụ đèn LED) từ giao diện trình duyệt mà không cần có mặt tại chỗ.

Đề tài xây dựng một hệ thống IoT hoàn chỉnh gồm ba thành phần chính:

- **Thiết bị đầu cuối (Edge):** Vi điều khiển **ESP8266 (Wemos D1 Mini)** kết nối với **cảm biến DHT11** (nhiệt độ & độ ẩm), **LED** có điều khiển PWM, và **màn hình OLED SH1106 128x64** hiển thị thông tin cục bộ.
- **Tầng trung gian (Middleware):** **MQTT Broker** **HiveMQ Cloud** (TLS, port 8883) làm trung tâm truyền tin giữa thiết bị và giao diện.
- **Tầng ứng dụng (Application):** **Node-RED Dashboard** cung cấp giao diện giám sát trực quan (gauge, biểu đồ), cảnh báo (toast notification), điều khiển đèn (switch, slider), và đồng bộ trạng thái hai chiều hoàn toàn qua trình duyệt.

> **Lưu ý bảo mật:** Thông tin đăng nhập MQTT trong tài liệu này được thay bằng `****`. File `iot-nhom6.ino` chứa thông tin thật — không đưa lên GitHub công khai sau khi nộp báo cáo.

---

## 2. Nội dung thực hiện

Hệ thống thực hiện đầy đủ chín nội dung chính theo yêu cầu đề tài.

---

### 2.1. Thu thập dữ liệu môi trường (Cảm biến DHT11)

**Cảm biến:** DHT11 — đo nhiệt độ (°C) và độ ẩm tương đối (%) không khí.  
**Chân kết nối trên ESP8266:** GPIO 0 (`DHT_PIN` trong sketch).  
**Chu kỳ đọc:** Mỗi `INTERVAL_DHT = 20000` mili-giây (20 giây), được quản lý bằng timer `lastDHT` trong `loop()`.

**Luồng xử lý trong firmware:**

1. Gọi `dht.readTemperature()` và `dht.readHumidity()` để đọc nhiệt độ & độ ẩm từ DHT11.
2. Kiểm tra `isnan()` — nếu lỗi thì bỏ qua chu kỳ này.
3. Cập nhật biến toàn cục `temperature`, `humidity`.
4. So sánh với 4 ngưỡng: `TEMP_MAX = 38.0°C`, `TEMP_MIN = 18.0°C`, `HUMI_MAX = 80.0%`, `HUMI_MIN = 30.0%` — tạo 4 cờ: `tempHighFlag`, `tempLowFlag`, `humiHighFlag`, `humiLowFlag`.
5. Ghi log ra Serial (`Serial.printf`) với định dạng:

   ```
   [DHT] T:28.0C  H:65.0%
          [CANH BAO] Nhiet do qua thap: 28.0C < 18.0C
   ```

6. Đóng gói **JSON** bằng `snprintf` với định dạng:

   ```json
   {
     "temperature": 28.0,
     "humidity": 65.0,
     "tempAlertHigh": false,
     "tempAlertLow": false,
     "humiAlertHigh": false,
     "humiAlertLow": false,
     "message": "Môi trường bình thường"
   }
   ```

7. Gọi `mqtt.publish(mqtt_env, buf)` — topic **`iot/env/data`**.
8. Gọi `publishLedStatusIfNeeded()` để đồng bộ trạng thái LED (nếu có thay đổi).
9. Gọi `displayOLED()` để cập nhật màn hình OLED ngay lập tức.

> **Thông tin bảo mật:** Topic `iot/env/data` publish từ ESP8266 lên HiveMQ; mật khẩu MQTT trong code được thay bằng `****` trong bản báo cáo này.

---

### 2.2. Hiển thị OLED SH1106

Màn hình **OLED SH1106 128x64 I2C** hiển thị thông tin cục bộ ngay trên thiết bị, giúp người dùng quan sát nhanh mà không cần mở Dashboard.

**Thông số kỹ thuật:**

|| Thông số | Giá trị |
||----------|---------|
|| Giao thức | I2C |
|| Chân SCL | GPIO 4 (D2) |
|| Chân SDA | GPIO 5 (D1) |
|| Độ phân giải | 128 × 64 pixel |
|| Thư viện | U8G2lib |

**Bố cục 3 dòng trên màn hình:**

| Dòng | Nội dung bình thường | Nội dung khi cảnh báo |
|------|---------------------|-----------------------|
| Dòng 1 | `T:28.0°C 18-38°C` | `T:28.0°C Cao!` hoặc `T:28.0°C Thap!` |
| Dòng 2 | `H:65.0% 30-80%` | `H:65.0% Cao!` hoặc `H:65.0% Thap!` |
| Dòng 3 | `Den: TAT 0%` hoặc `Den: BAT 100%` | |

OLED được cập nhật ngay lập tức mỗi khi có thao tác LED (MQTT callback) hoặc mỗi chu kỳ DHT (20 giây).

---

### 2.3. Truyền dữ liệu qua MQTT (HiveMQ Cloud)

| Thông số | Giá trị |
|----------|---------|
| **Broker** | HiveMQ Cloud *(hostname báo cáo: `****`)* |
| **Port** | **8883** — TLS |
| **Protocol** | **MQTT v3.1.1** qua `WiFiClientSecure` + `PubSubClient` |
| **Topic gửi dữ liệu môi trường** | **`iot/env/data`** — ESP8266 publish |
| **Topic điều khiển LED** | **`iot/led/power/set`** — ESP8266 subscribe |
| **Topic trạng thái LED** | **`iot/led/status`** — ESP8266 publish |

**Vai trò ESP8266 trong MQTT:**
- Là **publisher** trên topic `iot/env/data` — gửi dữ liệu cảm biến định kỳ (20 giây).
- Là **publisher** trên topic `iot/led/status` — gửi trạng thái LED sau khi thay đổi (tránh feedback loop với slider).
- Là **subscriber** trên topic `iot/led/power/set` — nhận lệnh điều khiển LED từ Node-RED.

**Trong firmware:**
- `connectMQTT()` tạo `clientId` ngẫu nhiên (`"esp8266-nhom6-xxxx"`), gọi `mqtt.connect(clientId, mqtt_user, mqtt_pass)`.
- Khi kết nối thành công → subscribe `mqtt_led` và in log: `MQTT connected!`.
- MQTT TLS dùng `espClient.setInsecure()` (bỏ qua xác thực certificate — phù hợp demo, cần cải thiện bảo mật ở phiên bản production).

---

### 2.4. Xử lý dữ liệu bằng Node-RED

Node-RED nhận message từ MQTT, parse JSON và phân luồng xử lý.

**Cấu trúc flow chính:**

```
[mqtt in: iot/env/data]
        │
        ▼
[HandleData: function — parse JSON + tách 4 nhánh]
   ├──[temperature]──► [temperature function]──► [ui_gauge] + [ui_chart]
   ├──[humidity]─────► [humidity function]─────► [ui_gauge] + [ui_chart]
   ├──[alert]────────► [alert function]────────► [ui_toast]
   └──[ledState]────► [ledState function]──────► [ui_switch]
```

```
[mqtt in: iot/led/status]
        │
        ▼
[Sync slider + switch tu LED status: function — 2 output]
   ├──[brightness]──► [ui_slider]  (đồng bộ slider)
   └──[ledState]────► [ui_switch] (đồng bộ switch)
```

```
[ui_slider: Điều chỉnh độ sáng]
        │
        ▼
[Set LED State Payload: function — 2 output]
   ├──[brightness JSON]──► [slider2mqtt001]──► [mqtt out: iot/led/power/set]
   └──[ledState bool]────► [ui_switch]        (đồng bộ switch)
```

```
[ui_switch: Bật đèn / Tắt đèn]
        │
        ▼
[Format LED State: function — 2 output]
   ├──[{ledState}]──► [mqtt out: iot/led/power/set]
   └──[0/100]───────► [ui_slider]             (đồng bộ slider)
```

**Chi tiết các node function:**

| Tên node | Mã xử lý (logic chính) | Đầu ra |
|----------|------------------------|--------|
| `HandleData` | `JSON.parse(msg.payload)` nếu là string; trả về **4 bản sao** cho 4 nhánh (không có brightness để tránh feedback loop) | 4 output |
| `temperature` | `msg.payload = msg.payload.temperature; return msg;` | → Gauge + Chart |
| `humidity` | `msg.payload = msg.payload.humidity; return msg;` | → Gauge + Chart |
| `alert` | Kiểm tra `tempAlertHigh/Low`, `humiAlertHigh/Low` → gán message tương ứng | → Toast |
| `ledState` | `msg.payload = msg.payload.ledState` nếu có, ngược lại `return null` | → Switch đồng bộ |
| `Sync slider + switch tu LED status` | Tách `brightness` → output 0 (slider), `ledState` → output 1 (switch) | 2 output |
| `Set LED State Payload` | `[0]` → `{brightness: x}` cho MQTT, `[1]` → `b > 0` cho switch | 2 output |
| `Format LED State` | `[0]` → `{ledState: true/false}` cho MQTT, `[1]` → `100/0` cho slider | 2 output |
| `slider2mqtt001` | Change node: đặt `topic = iot/led/power/set`, xóa `ledState` khỏi msg | → mqtt out |

---

### 2.5. Hiển thị dữ liệu trên Dashboard

**Giao diện Node-RED Dashboard** (tab **Trang Chủ**, nhóm **Nhiệt Độ**, **Độ Ẩm**, **Đèn**):

| Thành phần | Loại | Mô tả |
|------------|------|--------|
| **Nhiệt độ hiện tại** | `ui_gauge` | Kim chỉ giá trị nhiệt độ (°C), min=0, max=50 |
| **Biểu đồ nhiệt độ** | `ui_chart` | Đồ thị đường theo thời gian thực (dạng line, HH:mm:ss) |
| **Độ ẩm hiện tại** | `ui_gauge` | Kim chỉ giá trị độ ẩm (%), min=0, max=100 |
| **Biểu đồ độ ẩm** | `ui_chart` | Đồ thị đường theo thời gian thực |
| **Điều chỉnh độ sáng (%)** | `ui_slider` | Slider 0–100%, bước 1 |
| **Bật đèn / Tắt đèn** | `ui_switch` | Toggle bật/tắt hoàn toàn |

Gauge nhiệt độ dùng ba vùng màu: xanh (≤vùng an toàn), vàng (trung gian), đỏ (nguy hiểm). Biểu đồ tự động xóa điểm cũ sau 1 giờ (`removeOlderUnit: 3600`).

---

### 2.6. Cảnh báo môi trường

**Cơ chế:** Khi ESP8266 phát hiện nhiệt độ > 38°C, < 18°C, độ ẩm > 80% hoặc < 30%, firmware đặt 4 cờ tương ứng và gắn chuỗi `message` trong JSON.

**Luồng cảnh báo trên Node-RED:**

1. JSON từ ESP8266 chứa `"tempAlertHigh":true`, `"tempAlertLow":true`, `"humiAlertHigh":true`, `"humiAlertLow":true` (hoặc kết hợp).
2. Node function **`alert`** kiểm tra lần lượt: nếu đúng → gán `msg.payload` thành chuỗi cảnh báo, gửi xuống node **`ui_toast`**.
3. Toast hiển thị ở **góc dưới bên trái** trong 3 giây (`displayTime: 3`).

**4 loại cảnh báo:**

| Cờ | Ngưỡng | Message |
|----|--------|---------|
| `tempAlertHigh` | > 38°C | "Nhiet do qua cao!" |
| `tempAlertLow` | < 18°C | "Nhiet do qua thap!" |
| `humiAlertHigh` | > 80% | "Do am qua cao!" |
| `humiAlertLow` | < 30% | "Do am qua thap!" |

---

### 2.7. Điều khiển đèn từ xa qua MQTT

Người dùng thao tác trên Dashboard (nhóm **Đèn**) để gửi lệnh LED qua MQTT.

**Bật / Tắt đèn (Switch)**

- Node **`ui_switch`** gửi giá trị `true` (bật) hoặc `false` (tắt).
- Function **`Format LED State`** đóng gói thành JSON: `{"ledState": true}` hoặc `{"ledState": false}`.
- Output thứ 2: `100` (bật) hoặc `0` (tắt) → gửi về slider để đồng bộ.
- Node **`mqtt out`** publish lên **`iot/led/power/set`** (QoS 1).

**Điều chỉnh độ sáng (Slider)**

- **`ui_slider`** trả giá trị số nguyên **0–100 (%)**.
- Function **`Set LED State Payload`** đóng gói output 0: `{"brightness": 70}`.
- Change node **`slider2mqtt001`** đặt `topic = iot/led/power/set` và xóa `ledState` để tránh feedback loop (vì `iot/led/status` từ ESP8266 không bao gồm brightness).
- Output thứ 2: `true/false` → gửi về switch để đồng bộ.
- Node **`mqtt out`** publish lên **`iot/led/power/set`**.

**Xử lý trên ESP8266 (`mqttCallback`):**

Callback kiểm tra đúng topic, ghép payload thành chuỗi, rồi xử lý theo **3 nhánh ưu tiên**:

| Nhánh | Điều kiện | Ví dụ payload | Hành động |
|-------|-----------|---------------|-----------|
| **JSON** (ưu tiên 1) | payload bắt đầu bằng `{` và có `brightness` | `{"brightness":70}` | Đọc brightness → PWM 0-255 |
| **JSON** (ưu tiên 2) | payload là JSON và có `ledState` | `{"ledState":true}` | Đặt brightness=100 hoặc 0 |
| **Số thuần** | toàn bộ payload là chữ số | `70` | Coi là % → PWM |
| **Chuỗi text** | `"true"/"on"` hoặc `"false"/"off"` | `true` | Đặt brightness=100 hoặc 0 |

**Hàm xử lý PWM:**

1. `brightness = constrain(brightness, 0, 100)` — giới hạn phạm vi.
2. `analogWrite(LED_PIN, map(brightness, 0, 100, 0, 255))` — xuất PWM ra **GPIO 12 (D6)**.
3. `ledState = (brightness > 0)` — cập nhật trạng thái logic.
4. `lastBrightness = brightness` — lưu để khôi phục khi bật lại.
5. `lastManualTime = millis()` — đánh dấu thao tác tay (bắt đầu tính 60 phút tự động).
6. Log ra Serial: `[LED] brightness: 70%`.

---

### 2.8. Đồng bộ trạng thái thiết bị

**Mục tiêu thiết kế:** Khi trạng thái LED thay đổi (do người dùng điều khiển từ Dashboard), switch và slider trên giao diện phản ánh đúng trạng thái thực của thiết bị — tạo cảm giác đồng bộ hai chiều.

**Phân tích hệ thống:**

- **Phía ESP8266:** Biến `ledState` và `brightness` được cập nhật mỗi khi nhận lệnh từ MQTT (trong `mqttCallback`).
- **Phía firmware publish:** Hàm `publishLedStatusIfNeeded()` gửi JSON lên `iot/led/status` **chỉ khi** giá trị thay đổi so với lần gửi trước (`lastLedStateSent`, `lastBrightnessPublished`).
- **Phía Node-RED:** Flow `LED Status Listener` nhận từ `iot/led/status` → tách thành `brightness` (slider) và `ledState` (switch) → cập nhật UI.
- **Phía JSON `iot/env/data`:** **Không bao gồm** `brightness` hay `ledState` để tránh feedback loop giữa slider và flow chính.

**Kết quả:** Switch và slider luôn phản ánh trạng thái thực của LED, kể cả khi ESP8266 thay đổi trạng thái (ví dụ: chế độ tự động theo giờ).

---

### 2.9. Tự động bật/tắt đèn theo giờ (NTP)

**Đồng bộ thời gian qua NTP:**

- Server: `pool.ntp.org`
- Múi giờ: **UTC+7** (Việt Nam), `configTime(25200, 0, NTP_SERVER)`
- Chu kỳ đồng bộ lại: **1 giờ** (`INTERVAL_NTP = 3600000ms`)
- Flag `ntpSynced` đảm bảo schedule chỉ chạy khi giờ đã chính xác.

**Luật tự động:**

| Thời gian | Hành động |
|-----------|-----------|
| **18:00 – 05:59** (18h tối – 6h sáng) | Bật đèn (khôi phục `lastBrightness` hoặc 100%) |
| **06:00 – 17:59** (6h sáng – 18h chiều) | Tắt đèn (lưu `lastBrightness` trước khi tắt) |

**Chế độ thủ công vs tự động:**

- Sau mỗi thao tác LED từ MQTT, `lastManualTime = millis()`.
- Trong vòng **60 phút** (`MANUAL_WINDOW = 60 * 60 * 1000ms`) không có thao tác tay → chuyển sang chế độ tự động.
- **First boot** (`lastManualTime == 0`): bật/tắt đèn ngay theo giờ hiện tại, không cần đợi 60 phút.

**Serial log khi tự động:**

```
[SCHEDULE] Tu dong bat den (18h-6h)
[SCHEDULE] Tu dong tat den (6h-18h)
```

---

## 3. Thiết kế mạch IoT

Hệ thống được lắp mạch thật theo sơ đồ dưới đây. Không sử dụng mô phỏng Wokwi.

---

### 3.1. Thành phần phần cứng

| STT | Thành phần | Mô tả |
|-----|-----------|--------|
| 1 | **ESP8266 (Wemos D1 Mini)** | Vi điều khiển chính — Wi-Fi, GPIO, analogWrite PWM |
| 2 | **Cảm biến DHT11** | Đo nhiệt độ (0~50°C, ±2°C) và độ ẩm (20~90% RH, ±5% RH) |
| 3 | **OLED SH1106 128x64 I2C** | Màn hình hiển thị thông tin cục bộ |
| 4 | **LED đơn (màu đỏ)** | Hiển thị trạng thái / thí nghiệm PWM |

> **Lưu ý:** Không mô tả các linh kiện không có trong mạch (cảm biến ánh sáng, relay, LCD 1602, buzzer…) — chỉ liệt kê đúng thiết bị đã dùng.

---

### 3.2. Kết nối phần cứng (GPIO)

| Chân ESP8266 (GPIO) | Nối đến | Ghi chú |
|---------------------|---------|---------|
| **3.3V** | DHT11 VCC, OLED VCC | Nguồn cho cảm biến và OLED |
| **GND** | DHT11 GND, OLED GND, LED Cathode | Chân nối đất chung |
| **GPIO 0 (D3)** | DHT11 DATA | Bus dữ liệu 1 dây — trùng `DHT_PIN` |
| **GPIO 4 (D2)** | OLED SCL | Clock I2C |
| **GPIO 5 (D1)** | OLED SDA | Data I2C |
| **GPIO 12 (D6)** | LED Anode (A) qua điện trở 330Ω | PWM output — trùng `LED_PIN` |

**Sơ đồ đấu dây đơn giản:**

```
  ESP8266 3.3V ─── DHT11 VCC
  ESP8266 GND ─── DHT11 GND
  ESP8266 GPIO0 ─── DHT11 DATA

  ESP8266 3.3V ─── OLED VCC
  ESP8266 GND ─── OLED GND
  ESP8266 GPIO4 ─── OLED SCL
  ESP8266 GPIO5 ─── OLED SDA

  ESP8266 GPIO12 ─── R330 (330Ω) ─── LED Anode (A)
  ESP8266 GND ─── LED Cathode (C)
```

---

### 3.3. Giải thích các linh kiện

**1) ESP8266 (Wemos D1 Mini)**

Vi điều khiển single-core Tensilica LX106, tích hợp Wi-Fi 802.11 b/g/n. Trong project này, ESP8266 đảm nhiệm: kết nối Wi-Fi, giao tiếp MQTT qua TLS, đọc cảm biến DHT11, điều khiển PWM LED thông qua `analogWrite()` (không dùng LEDC như ESP32), giao tiếp I2C với OLED SH1106, và đồng bộ thời gian NTP.

**2) Cảm biến DHT11**

| Thông số | Giá trị |
|----------|---------|
| Nguồn cấp | 3.3 V – 5 V |
| Dải nhiệt độ | 0°C ~ +50°C |
| Sai số nhiệt độ | ±2°C |
| Dải độ ẩm | 20 ~ 90% RH |
| Sai số độ ẩm | ±5% RH |
| Giao tiếp | Single-wire (1-wire), chuẩn giao thức DHT của Aosong |

DHT11 trả dữ liệu theo định dạng số 40-bit: 16 bit cho độ ẩm, 16 bit cho nhiệt độ, 8 bit checksum. Thư viện `DHT.h` trong Arduino IDE đã lo phần giải mã.

**3) OLED SH1106 128x64 I2C**

Màn hình OLED đơn sắc giao tiếp qua I2C, không có chân reset (dùng `U8X8_PIN_NONE`). Thư viện `U8G2lib` cung cấp API vẽ chuỗi, hỗ trợ nhiều font. Địa chỉ I2C mặc định của SH1106 là `0x3C`.

**4) LED đơn + Điện trở 330Ω**

LED đỏ có điện áp hoạt động khoảng 1.8–2.2V và dòng tối đa 20mA. Điện trở 330Ω được chọn để giới hạn dòng qua LED. ESP8266 dùng `analogWrite()` trên GPIO 12 để điều khiển PWM 8-bit (0–255).

---

## 4. Nguyên lý hoạt động

---

### 4.1. Sơ đồ luồng hệ thống (System Flow Diagram)

```
┌──────────┐    GPIO0      ┌──────────────────────────────────────────────────────┐
│  DHT11   │───────────────│                        ESP8266                          │
│ (cảm biến)│              │  • WiFi.begin()         • publishEnvData()             │
└──────────┘              │  • WiFiClientSecure     • mqttCallback()                │
        ┌──────────────┐   │  • PubSubClient         • analogWrite(PWM) → GPIO12      │
        │   OLED SH1106│   │  • U8G2                 • displayOLED()                  │
        │  (I2C 0x3C)  │   │         │                        │                      │
        │              │   │         │                        │                      │
        │ GPIO4 SCL    │   │         │                        │                      │
        │ GPIO5 SDA    │   │         │                        │                      │
        └──────────────┘   │         │                        │                      │
                           │         │ publish("iot/env/data")│                      │
                           │         │ publish("iot/led/status")                     │
        ┌──────────────┐   │         │ ◄──────────────────────┤                      │
        │ MQTT Broker  │   │         │ subscribe              │                      │
        │  HiveMQ      │   │         │ ("iot/led/power/set")  │                      │
        │  :8883 TLS    │   │         │                        │                      │
        └──────┬───────┘   └─────────┼────────────────────────┼──────────────────────┘
               │                     │                        │
               │   iot/env/data     │     iot/led/power/set   │  iot/led/status
               │   (JSON payload)   │     (JSON / số / chuỗi) │  (JSON payload)
               │                    │                        │   (ESP→NR sync)
               ▼                     │                        │
┌──────────────────────────────┐      │                        │
│         Node-RED             │      │                        │
│  ┌─────────────────────────┐ │      │                        │
│  │ mqtt in: iot/env/data   │ │      │                        │
│  └───────────┬─────────────┘ │      │                        │
│              │               │      │                        │
│  ┌───────────▼──────────────┐│      │                        │
│  │ HandleData (function)     ││      │                        │
│  │ parse JSON → 4 nhánh      ││      │                        │
│  └───────┬──────┬──┬──┬─────┘│      │                        │
│          │      │  │  │       │      │                        │
│   ┌──────▼──┐┌─▼──┐▼─▼──┐┌─▼──────▼───┐                    │
│   │temp/humi│ │alert│ │ledState│                        │
│   └────┬────┘ └─┬──┘ └──┬┘ └────┬──────┘                    │
│        │        │        │        │                            │
│   ┌────▼────────▼────────▼────────▼────┐                  │
│   │         Dashboard (UI)                │                  │
│   │  Gauge | Chart | Toast | Switch | Slider                 │
│   └────────────────┬───────────────────┘                     │
│                    │                                        │
│  ┌─────────────────▼───────────────────┐                     │
│  │ mqtt in: iot/led/status            │                     │
│  └─────────────┬─────────────────────┘                     │
│                │                                              │
│  ┌─────────────▼─────────────────────────────────┐         │
│  │ Sync slider + switch (function — 2 output)      │         │
│  └─────────────┬──────────────┬──────────────────┘         │
│       │        │              │                               │
│   ui_slider  ui_switch                                        │
│                    │                                          │
│            ┌───────▼────────┐                                 │
│            │ mqtt out        │                                 │
│            │ iot/led/power/set │                                │
│            └───────┬─────────┘                                 │
│                    │ (tới ESP8266)                              │
└────────────────────┼───────────────────────────────────────────┘
                     │
                     └─────────────────► (lặp lại luồng)
```

---

### 4.2. Luồng giám sát: Cảm biến → Dashboard

**Bước 1 — Đo:** DHT11 đo nhiệt độ & độ ẩm tại chân **GPIO 0**.

**Bước 2 — Đọc:** `loop()` kiểm tra timer `lastDHT`; đủ 20000ms → gọi `dht.readTemperature()` và `dht.readHumidity()`.

**Bước 3 — Xử lý:** Tính 4 cờ cảnh báo, log Serial, đóng gói JSON.

**Bước 4 — Gửi MQTT:** `mqtt.publish("iot/env/data", jsonBuffer)` → HiveMQ Cloud.

**Bước 5 — Node-RED nhận:** node `mqtt in` nhận payload, `HandleData` parse JSON.

**Bước 6 — Tách luồng:** HandleData trả 4 bản sao msg, mỗi bản qua function chuyên biệt → gauge, chart, toast, switch.

**Bước 7 — Hiển thị:** Dashboard cập nhật gauge, biểu đồ tự động vẽ điểm mới theo thời gian. Đồng thời OLED SH1106 cũng được cập nhật.

---

### 4.3. Luồng điều khiển: Dashboard → ESP8266 → LED

**Bước 1 — Thao tác:** Người dùng gạt **Switch** (bật/tắt) hoặc kéo **Slider** (0–100%) trên Dashboard.

**Bước 2 — Đóng gói:** Function node tương ứng tạo JSON (`{"ledState":true}` hoặc `{"brightness":70}`).

**Bước 3 — Gửi MQTT:** node `mqtt out` publish lên **`iot/led/power/set`** (QoS 1).

**Bước 4 — ESP8266 nhận:** `mqtt.loop()` kích hoạt `mqttCallback(char*, byte*, unsigned int)`.

**Bước 5 — Xử lý callback:** Kiểm tra topic, ghép payload, xử lý 3 nhánh (JSON / số / chuỗi) → `analogWrite()`.

**Bước 6 — Điều khiển PWM:** `analogWrite(GPIO12, map(brightness,0,100,0,255))` → LED sáng/tắt/tối theo mức. Đồng thời `displayOLED()` cập nhật màn hình OLED.

**Bước 7 — Log & publish status:** Serial in ra `[LED] brightness: 70%`. `publishLedStatusIfNeeded()` gửi `{"ledState":true,"brightness":70}` lên `iot/led/status` (nếu giá trị thay đổi).

---

### 4.4. Luồng đồng bộ trạng thái LED

**Bước 1 — ESP8266 publish** `{"ledState":true,"brightness":70}` lên `iot/led/status` sau mỗi thay đổi LED.

**Bước 2 — Node-RED `LED Status Listener`** nhận từ `mqtt in: iot/led/status`.

**Bước 3 — `Sync slider + switch tu LED status`** tách:
- Output 0: `{payload: 70}` → **`ui_slider`** (đồng bộ giá trị slider về 70%)
- Output 1: `{payload: true}` → **`ui_switch`** (đồng bộ switch sang bật)

**Kết quả:** Dù LED thay đổi do người dùng Dashboard hay do chế độ tự động theo giờ, giao diện luôn hiển thị đúng trạng thái thực.

---

### 4.5. Luồng tự động theo giờ

**Điều kiện:** `ntpSynced == true` và `elapsed >= MANUAL_WINDOW` (hoặc first boot).

**Mỗi vòng loop:**

1. `elapsed = millis() - lastManualTime` — tính thời gian không thao tác.
2. Nếu `lastManualTime == 0` (first boot) hoặc `elapsed >= 60 phút`:
   - `isEveningPeriod()` (18h–6h): bật đèn, `analogWrite(255)`, `ledState=true`.
   - `isMorningOffPeriod()` (6h–18h): tắt đèn, `analogWrite(0)`, `ledState=false`.
3. Sau khi thay đổi → `publishLedStatusIfNeeded()` → gửi `iot/led/status` → Dashboard tự động đồng bộ.

---

### 4.6. Luồng duy trì kết nối

**Wi-Fi:**

```cpp
// loop()
if (WiFi.status() != WL_CONNECTED) {
    connectWiFi(); // WiFi.begin() + chờ WL_CONNECTED
    ntpSynced = false; // Mất WiFi -> cần đồng bộ NTP lại
}
```

**MQTT reconnect:**

```cpp
// loop()
if (!mqtt.connected()) {
    if (millis() - lastMQTT > 5000) { // Chờ 5s
        lastMQTT = millis();
        connectMQTT(); // Tạo clientId, mqtt.connect, subscribe LED topic
    }
}
mqtt.loop(); // Xử lý keep-alive, gọi callback
```

**NTP định kỳ:**

```cpp
// loop()
if (WiFi.status() == WL_CONNECTED) {
    if (millis() - lastNTPSync >= INTERVAL_NTP) { // 1 giờ
        syncTime();
    }
}
```

- Timer `lastMQTT` tránh spam reconnect liên tục (thử lại mỗi 5 giây).
- `mqtt.loop()` luôn được gọi mỗi vòng loop để duy trì session MQTT và nhận callback kịp thời.

---

## 5. Cài đặt hệ thống

Hướng dẫn từng bước để thiết lập môi trường phát triển, nạp firmware và chạy toàn bộ hệ thống. Làm theo thứ tự từ trên xuống dưới để có một hệ thống chạy hoàn chỉnh.

---

### 5.1. Yêu cầu phần mềm

| Phần mềm / công cụ | Phiên bản tối thiểu | Ghi chú |
|--------------------|--------------------|---------|
| **Arduino IDE** | 1.8.x trở lên | Nạp firmware ESP8266 |
| **Board ESP8266** (trong Board Manager) | 3.x | Hỗ trợ Wemos D1 Mini |
| **Node.js** | 18.x LTS trở lên | Chạy Node-RED |
| **Node-RED** | 3.x | Xử lý flow & Dashboard |
| **node-red-dashboard** | 3.6.x | Giao diện giám sát & điều khiển |
| **Git** | Bất kỳ | Clone repo |

---

### 5.2. Cài đặt Arduino IDE + thư viện

**Bước 1 — Cài Arduino IDE**

Truy cập [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software), tải và cài đặt bản phù hợp hệ điều hành.

**Bước 2 — Cài board ESP8266**

1. Mở **File → Preferences**.
2. Thêm URL board: `http://arduino.esp8266.com/stable/package_esp8266com_index.json` vào ô **Additional Boards Manager URLs**.
3. Mở **Tools → Board → Boards Manager…**
4. Tìm **`esp8266`** → bấm **Install** (cần internet, có thể mất 5–10 phút).

**Bước 3 — Chọn board Wemos D1 Mini**

**Tools → Board → ESP8266 Boards → LOLIN(WEMOS) D1 R2 & mini** (hoặc tương đương).

**Bước 4 — Cài thư viện Arduino**

Mở **Sketch → Include Library → Manage Libraries…**, tìm và cài:

| Thư viện | Phiên bản |
|----------|-----------|
| **PubSubClient** | 2.8.x |
| **DHT sensor library** | 1.4.x |
| **U8g2** | 2.x |
| **ArduinoJson** | 6.x |

**Bước 5 — Cài driver USB–UART**

Nếu máy tính không nhận Wemos D1 Mini qua USB, cài driver CH340:

- **CH340:** tìm và cài driver CH340 theo hướng dẫn trên mạng phù hợp hệ điều hành.

---

### 5.3. Cấu hình thông tin Wi-Fi & MQTT

Mở **`iot-nhom6.ino`** trong Arduino IDE và chỉnh sửa các thông tin sau:

**Wi-Fi:**

```cpp
const char *ssid     = "Tên_WiFi_của_bạn";
const char *password = "Mật_khẩu_WiFi";
```

**MQTT (HiveMQ Cloud):**

```cpp
const char *mqtt_server = "b1c8f2cc5cd4416fb74b671a407dc0ff.s1.eu.hivemq.cloud";
const int   mqtt_port   = 8883;
const char *mqtt_user   = "****";    // Thay bằng tài khoản HiveMQ thật
const char *mqtt_pass   = "****";    // Thay bằng mật khẩu HiveMQ thật
```

> **Bảo mật:** Không commit file chứa mật khẩu thật lên GitHub. Sau khi nộp báo cáo, nên đổi mật khẩu MQTT trên HiveMQ Cloud để tránh lộ thông tin.

---

### 5.4. Build, nạp firmware và Serial Monitor

**Bước 1 — Build firmware**

Trong Arduino IDE: **Sketch → Verify/Compile** (`Ctrl+R`).

**Bước 2 — Nạp firmware vào ESP8266**

1. Cắm Wemos D1 Mini vào máy tính qua cáp USB.
2. Chọn đúng **port** trong **Tools → Port** (ví dụ `COM3` trên Windows, `/dev/cu.SLAB_USBtoUART` trên macOS).
3. Nhấn **Sketch → Upload** (`Ctrl+U`) hoặc biểu tượng Upload.

**Bước 3 — Mở Serial Monitor**

Nhấn biểu tượng **Serial Monitor** (biểu tượng kính lúp) hoặc `Ctrl+Shift+M`. Đặt baud rate **115200**.

**Kết quả mong đợi trên Serial Monitor:**

```
Đang kết nối WiFi.
WiFi connected! IP: 192.168.x.x
Đang lấy giờ từ NTP...
[TIME] Giờ HN: 14:30:00 | Ngày: 29/03/2026
Đồng hồ sync thành công!
MQTT connected!
=== ESP8266 IoT Nhom 6 - Khoi tao xong ===
[DHT] T:28.0C  H:65.0%
[LED] brightness: 100%
[SCHEDULE] Tu dong bat den (18h-6h)
```

---

### 5.5. Cài đặt Node.js & Node-RED

**Bước 1 — Cài Node.js**

Tải và cài **Node.js LTS** từ [https://nodejs.org/](https://nodejs.org/) (chọn bản Recommended for most users).

Kiểm tra:

```bash
node --version
npm --version
```

**Bước 2 — Cài Node-RED toàn cục**

```bash
npm install -g --unsafe-perm node-red
```

Kiểm tra:

```bash
node-red --version
```

**Bước 3 — Chạy Node-RED**

```bash
node-red
```

Khi khởi động thành công, terminal hiển thị thông báo dạng:

```
Welcome to Node-RED
...
29 Mar xx:xx:xx - [info] Server now running at http://127.0.0.1:1880/
```

Mở trình duyệt tại **http://127.0.0.1:1880**.

---

### 5.6. Cài đặt node-red-dashboard

1. Trong giao diện Node-RED, nhấn **☰ (menu) → Manage Palette → Install**.
2. Tìm: **`node-red-dashboard`**.
3. Bấm **Install** → chờ cài đặt (cần internet).
4. Sau khi cài xong, bấm **Deploy**.

> Phiên bản flow trong `node-red.json` tương thích với **node-red-dashboard 3.6.x**.

---

### 5.7. Import Node-RED Flow từ `node-red.json`

**Bước 1 — Mở file `node-red.json`**

Trong thư mục project, mở **`node-red.json`**.

**Bước 2 — Copy toàn bộ nội dung**

`Ctrl+A` → `Ctrl+C` để chọn và copy toàn bộ nội dung.

File này là một **mảng JSON** bắt đầu bằng `[{` và kết thúc bằng `}]`, chứa toàn bộ cấu hình flow (nodes, kết nối, function code, Dashboard UI).

**Bước 3 — Import vào Node-RED**

1. Trong Node-RED: nhấn **☰ → Import**.
2. Chọn thẻ **Clipboard**.
3. Paste nội dung đã copy vào ô.
4. Bấm **Import**.
5. Flow `MQTT` xuất hiện trong editor — bấm **Deploy** (nút đỏ trên góc phải).

---

### 5.8. Cấu hình MQTT Broker trong Node-RED

Sau khi import flow, cần khai báo thông tin HiveMQ trong node **mqtt-broker** để Node-RED kết nối đúng broker.

1. Trong editor Node-RED, tìm node **mqtt-broker** (trong danh sách node bên trái hoặc trong flow).
2. Nhấn đúp để mở cấu hình.
3. Điền các trường:

   | Trường | Giá trị |
   |--------|---------|
   | **Server** | `b1c8f2cc5cd4416fb74b671a407dc0ff.s1.eu.hivemq.cloud` |
   | **Port** | `8883` |
   | | Use TLS ☑ |
   | **Username** | `****` *(tài khoản HiveMQ thật)* |
   | **Password** | `****` *(mật khẩu HiveMQ thật)* |
   | Protocol Level | `MQTT v3.1.1 (4)` |

4. Bấm **Update** → **Deploy**.

> **Lưu ý:** Username và password trong báo cáo này dùng `****`. Trên máy demo cần điền thông tin thật trùng với `iot-nhom6.ino` trên ESP8266.

---

### 5.9. Truy cập Dashboard & kiểm tra kết nối

**Mở Dashboard**

- Cách 1: Trong Node-RED, nhấn nút **Dashboard** (biểu tượng grid/gauge ở góc phải thanh bên).
- Cách 2: Truy cập trực tiếp **http://127.0.0.1:1880/ui** (hoặc port mà Node-RED hiển thị).

**Kiểm tra kết nối**

1. Đảm bảo ESP8266 đang chạy và kết nối MQTT thành công (`MQTT connected!` trên Serial Monitor).
2. Trên Dashboard, kiểm tra:
   - **Gauge nhiệt độ** và **độ ẩm** cập nhật mỗi ~20 giây.
   - **Biểu đồ** vẽ đường theo thời gian.
3. Thử kéo **Slider** độ sáng → LED trên mạch thay đổi độ sáng.
4. Thử gạt **Switch** → LED bật/tắt hoàn toàn.
5. Thử tạo nhiệt độ cao (>38°C) hoặc thấp (<18°C) → **Toast cảnh báo** xuất hiện.
6. Quan sát **OLED SH1106** hiển thị thông tin cục bộ cùng lúc.

---

## 6. Kết quả thực hiện

*(Chèn ảnh chụp màn hình tại các vị trí TODO, đặt chú thích ngắn bên dưới mỗi ảnh.)*

---

### 6.1. Các hình ảnh minh họa

| STT | Hình ảnh | Nội dung | Mô tả luồng thể hiện |
|-----|---------|----------|----------------------|
| 1 | *(TODO: chèn ảnh mạch thật)* | Sơ đồ mạch thật | GPIO0→DHT11, GPIO12→LED+R330, GPIO4/5→OLED, 3V3+GND |
| 2 | *(TODO: chèn ảnh Serial Monitor)* | Log nhiệt độ, độ ẩm, MQTT, LED, NTP | Firmware đang chạy, publish JSON, nhận lệnh, schedule |
| 3 | *(TODO: chèn ảnh OLED)* | Màn hình OLED SH1106 | Hiển thị T, H, trạng thái đèn, ngưỡng |
| 4 | *(TODO: chèn ảnh Node-RED flow)* | Tab MQTT với các node & kết nối | mqtt in → HandleData → 4 nhánh → Dashboard + LED Status Listener |
| 5 | *(TODO: chèn ảnh Dashboard)* | Trang Chủ: Gauge, Chart, Switch, Slider | Giao diện giám sát & điều khiển trên trình duyệt |

---

### 6.2. Phân tích luồng hoạt động thực tế

**Kịch bản 1 — Giám sát nhiệt độ & độ ẩm (bình thường)**

1. DHT11 đo được nhiệt độ 28.0°C, độ ẩm 65.0%.
2. ESP8266 đóng gói JSON: `{"temperature":28.0,"humidity":65.0,"tempAlertHigh":false,"tempAlertLow":false,"humiAlertHigh":false,"humiAlertLow":false,"message":"Môi trường bình thường"}`.
3. Publish lên `iot/env/data`.
4. Node-RED nhận → `HandleData` parse → nhánh `temperature` trích giá trị 28.0 → cập nhật gauge và thêm điểm vào chart.
5. Dashboard hiển thị ngay lập tức. OLED cũng hiển thị `T:28.0°C 18-38°C`. Chu kỳ lặp lại mỗi 20 giây.

**Kịch bản 2 — Cảnh báo khi vượt ngưỡng**

1. DHT11 đo nhiệt độ 39.5°C (> 38°C).
2. ESP8266 đặt `"tempAlertHigh":true`, `"message":"Nhiệt độ quá cao!"`.
3. JSON publish: `{"temperature":39.5,"humidity":65.0,"tempAlertHigh":true,...,"message":"Nhiệt độ quá cao!"}`.
4. Nhánh `temperature` → gauge đổi màu (vùng đỏ), chart vẽ điểm cao.
5. Nhánh `alert` → kiểm tra `tempAlertHigh===true` → lấy message → `ui_toast` bật lên: **"Nhiet do qua cao!"**.
6. Toast hiển thị ở góc dưới bên trái trong 3 giây. OLED hiển thị `T:39.5°C Cao!`.

**Kịch bản 3 — Điều khiển LED từ Dashboard**

1. Người dùng kéo **Slider** độ sáng về 70%.
2. `Set LED State Payload` đóng gói output 0: `{"brightness":70}`.
3. `slider2mqtt001` đặt `topic = iot/led/power/set`, xóa `ledState`.
4. `mqtt out` publish lên `iot/led/power/set`.
5. ESP8266 nhận → `mqttCallback` kiểm tra: payload là JSON, có `brightness` → `brightness=70`.
6. `analogWrite(12, map(70,0,100,0,255)=178)`. Serial in: `[LED] brightness: 70%`.
7. `publishLedStatusIfNeeded()` gửi `{"ledState":true,"brightness":70}` lên `iot/led/status`.
8. `Sync slider + switch` nhận → output 1: `{payload: true}` → `ui_switch` đồng bộ sang bật.

**Kịch bản 4 — Tự động bật đèn lúc 18h**

1. Đồng hồ chỉ 18:00. `ntpSynced = true`.
2. `elapsed >= MANUAL_WINDOW` (60 phút không thao tác).
3. `isEveningPeriod()` → `true`.
4. ESP8266: `brightness = lastBrightness (hoặc 100)`, `analogWrite(255)`, `ledState=true`.
5. `publishLedStatusIfNeeded()` → `{"ledState":true,"brightness":100}` lên `iot/led/status`.
6. `ui_slider` đồng bộ về 100%, `ui_switch` đồng bộ sang bật.
7. Serial: `[SCHEDULE] Tu dong bat den (18h-6h)`.

---

## 7. Kết luận và hướng phát triển

---

### 7.1. Các công việc đã hoàn thành

Hệ thống IoT hoàn chỉnh gồm ba tầng đã được triển khai và kiểm chứng trên thiết bị thật:

1. **Thu thập dữ liệu môi trường** — ESP8266 đọc DHT11 định kỳ 20 giây, xử lý 4 ngưỡng (cao/thấp nhiệt độ & độ ẩm), đóng gói JSON chứa nhiệt độ, độ ẩm, 4 cờ cảnh báo và thông điệp.
2. **Hiển thị OLED cục bộ** — Màn hình SH1106 128x64 I2C hiển thị nhiệt độ, độ ẩm, trạng thái đèn và ngưỡng/cảnh báo ngay trên thiết bị.
3. **Truyền dữ liệu qua MQTT** — HiveMQ Cloud (TLS 8883), ESP8266 vừa là publisher (telemetry + LED status) vừa là subscriber (điều khiển LED). Tự động reconnect Wi-Fi, MQTT và đồng bộ NTP định kỳ.
4. **Xử lý và hiển thị** — Node-RED Dashboard với gauge nhiệt độ/độ ẩm, biểu đồ thời gian thực, toast cảnh báo khi vượt ngưỡng.
5. **Điều khiển thiết bị** — Switch bật/tắt + Slider 0–100% độ sáng LED qua PWM (`analogWrite`, 8-bit). Firmware xử lý 3 dạng payload tương thích nhiều nguồn điều khiển.
6. **Đồng bộ hai chiều** — ESP8266 publish `iot/led/status` sau mỗi thay đổi LED (kể cả tự động theo giờ), Node-RED đồng bộ slider và switch về đúng trạng thái thực.
7. **Tự động theo giờ** — Chế độ tự động bật đèn 18h–6h, tắt đèn 6h–18h dựa trên NTP. Chế độ thủ công 60 phút cho phép người dùng tạm quên schedule khi cần.
8. **Hướng dẫn triển khai đầy đủ** — Từ cài Arduino IDE + thư viện, cấu hình, build/nạp, Node-RED, đến chạy thực tế.

---

### 7.2. Hạn chế và hướng phát triển tiếp theo

| Hạn chế / Hướng phát triển | Mô tả | Ghi chú |
|---------------------------|--------|---------|
| **Bảo mật TLS** | Đang dùng `setInsecure()` — bỏ qua xác thực certificate. | Thay bằng `espClient.setCACert()` với certificate của HiveMQ để production-ready. |
| **Lưu trữ dữ liệu dài hạn** | Dữ liệu gauge/chart chỉ hiển thị real-time trên Dashboard; không lưu database, không export CSV. | Có thể thêm node `influxdb`, `mysql`, hoặc `mqtt-persist` vào flow Node-RED. |
| **Mở rộng phần cứng** | Chưa có relay (điều khiển thiết bị 220V), chưa có cảm biến khác (ánh sáng, khí gas, chuyển động…). | Chỉ bổ sung khi có phần cứng thật và code tương ứng. |
| **Xác thực người dùng Dashboard** | Node-RED Dashboard mặc định không có login. | Bật `adminAuth` hoặc `httpNodeAuth` trong `settings.js`. |
| **OTA Update** | Chưa triển khai cập nhật firmware từ xa qua Wi-Fi. | Có thể dùng `ESPhttpUpdate` hoặc `ArduinoOTA`. |
| **Web server trên ESP8266** | Chưa triển khai trang HTML/JS tĩnh chạy trên ESP8266. | Tạo thêm handler cho trang cấu hình Wi-Fi (WiFiManager). |
| **Độ chính xác DHT11** | DHT11 có sai số cao hơn DHT22 (±2°C, ±5% RH). | Nâng cấp lên DHT22 nếu cần độ chính xác cao hơn. |

---

## PHỤ LỤC A — Mã nguồn chương trình vi điều khiển (ESP8266)

**File nguồn:** `iot-nhom6/iot-nhom6.ino` trong thư mục project.

Toàn bộ firmware nằm trong một file `.ino` duy nhất, bao gồm: định nghĩa chân GPIO, cấu hình Wi-Fi, MQTT, NTP, các hàm xử lý (callback, OLED, DHT, LED PWM, schedule), và chương trình chính (`setup()`, `loop()`).

Khi nộp báo cáo, thay thế các dòng chứa thông tin nhạy cảm bằng `****`:

```cpp
const char *ssid     = "****";
const char *password = "****";
const char *mqtt_user   = "****";
const char *mqtt_pass   = "****";
```

---

## PHỤ LỤC B — Node-RED Flow (JSON) & Dashboard

**File nguồn:** `node-red.json` trong thư mục project — toàn bộ nội dung là mảng JSON export của Node-RED.

**Cách sử dụng:**

1. Mở `node-red.json` → `Ctrl+A` → `Ctrl+C`.
2. Trong Node-RED: **☰ → Import → Clipboard → Paste → Import → Deploy**.

**Kiểm tra bảo mật trước khi công bố:** Mở file JSON, tìm node `mqtt-broker`, xóa username/password nếu còn dạng plain text, thay bằng `****` và cấu hình lại trên máy demo.

**Không có mã nguồn web HTML/JS/CSS riêng** — giao diện Dashboard hoàn toàn do **node-red-dashboard** sinh ra từ flow JSON.

*(Chèn ảnh chụp Node-RED editor và Dashboard tại đây.)*

---

## PHỤ LỤC C — Các nội dung bổ sung

- **Ảnh mạch thật:** *(chèn tại đây nếu giảng viên yêu cầu hình ảnh thực tế)*.
- **Hướng dẫn vận hành ngắn cho người dùng cuối:** *(bổ sung nếu cần)*.
- **Script Python kiểm tra MQTT (tùy chọn):**

```bash
# Cài paho-mqtt
pip install paho-mqtt

# Ví dụ subscribe topic iot/env/data
python3 -c "
import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    print(f'Topic: {msg.topic} | Payload: {msg.payload.decode()}')

client = mqtt.Client()
client.username_pw_set('****', '****')
client.tls_set()
client.connect('b1c8f2cc5cd4416fb74b671a407dc0ff.s1.eu.hivemq.cloud', 8883)
client.subscribe('iot/env/data')
client.on_message = on_message
client.loop_forever()
"
```

---

*Tài liệu này tổng hợp đầy đủ nội dung thực hiện, luồng hoạt động, hướng dẫn cài đặt và kết quả. Cấu trúc bám theo khung báo cáo chuẩn: Đặt vấn đề → Nội dung thực hiện → Thiết kế mạch → Nguyên lý → Cài đặt → Kết quả → Kết luận → Phụ lục. Người soạn chỉnh định dạng (font, canh lề, số trang) khi chuyển sang Word/Google Docs.*
