# 🤖 ESP32-S3 WiFi Robot Car — Đồ Án Tốt Nghiệp

Robot xe điều khiển từ xa qua WiFi, tích hợp camera livestream và cảm biến siêu âm tránh vật cản, xây dựng trên nền tảng  ESP32-S3 Eye.

## 📋 Tổng quan

Hệ thống cho phép người dùng điều khiển robot qua trình duyệt web (giao diện responsive), xem video trực tiếp từ camera, điều chỉnh tốc độ, và tự động dừng khi phát hiện vật cản.
## 🗂️ Cấu trúc source code
├── Project_DATN_Sourcecode.ino   # File chính: khởi tạo WiFi, camera, FreeRTOS tasks
├── app_httpd.cpp                 # Web server, stream MJPEG, điều khiển động cơ, cảm biến
└── camera_pins.h                 # Định nghĩa GPIO cho nhiều model camera ESP32
## 🚀 Tính năng

- **Livestream video** qua MJPEG trên port 81 (`/stream`)
- **Điều khiển hướng** tiến / lùi / trái / phải / dừng qua HTTP GET (`/action?go=<dir>`)
- **Điều chỉnh tốc độ** bằng thanh slider (0–100%) qua PWM (`/speed?value=<n>`)
- **Tránh vật cản tự động**: dừng khi phát hiện vật thể trong vòng 25 cm phía trước
- **Giám sát WiFi**: tự động dừng động cơ và reconnect khi mất kết nối
- **Bật/tắt camera** qua giao diện web (`/camera_toggle`)
- **Timeout lệnh**: tự động dừng nếu không nhận lệnh mới sau 5 giây

## 🌐 Giao diện Web

Truy cập `http://<IP_của_ESP32>` trên trình duyệt để mở giao diện điều khiển.

| Endpoint | Chức năng |
|---|---|
| `/` | Trang điều khiển chính |
| `/stream` | Luồng video MJPEG (port 81) |
| `/action?go=forward` | Di chuyển (forward/backward/left/right/stop) |
| `/speed?value=70` | Đặt tốc độ (0–100) |
| `/obstacles` | Trạng thái vật cản (JSON) |
| `/camera_toggle` | Bật/tắt camera |

## 🛠️ Cài đặt & Nạp code

### Yêu cầu

- Arduino IDE ≥ 2.x hoặc PlatformIO
- Board package: **esp32 by Espressif** (≥ 2.0.0)
- Thư viện: `esp_camera`, `esp_http_server` (có sẵn trong ESP32 Arduino Core)

### Các bước

1. Mở `Project_DATN_Sourcecode.ino` bằng Arduino IDE.
2. Đảm bảo 3 file (`*.ino`, `app_httpd.cpp`, `camera_pins.h`) nằm **cùng thư mục**.
3. Cập nhật thông tin WiFi trong file `.ino`:
   ```cpp
   const char *ssid = "TEN_WIFI_CUA_BAN";
   const char *password = "MAT_KHAU_WIFI";
   ```
4. Chọn board: **ESP32S3 Dev Module** (hoặc tương đương).
5. Chọn đúng cổng COM, nạp code.
6. Mở Serial Monitor (115200 baud) để lấy địa chỉ IP.
7. Truy cập `http://<IP>` trên trình duyệt.


## 📐 Kiến trúc phần mềm

Hệ thống sử dụng **FreeRTOS** với 2 task song song:
Core 1: motorAndSensorTask  (priority 20) — cập nhật cảm biến, kiểm tra vật cản
Core 0: wifiMonitorTask     (priority  5) — giám sát kết nối WiFi
Main  : HTTP Server (port 80) + Stream Server (port 81)
