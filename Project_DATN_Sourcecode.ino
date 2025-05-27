#include <WiFi.h>
#include "esp_camera.h"
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"

const char *ssid = "Nhap_SSID";
const char *password = "Nhap_password";

#define CONNECT_WIFI_GPIO 14
void setupUltrasonic();
void setupMotors();
void setupCameraPower();
void startCameraServer();
void motorAndSensorTask(void *pvParameters);
void wifiMonitorTask(void *pvParameters);

void setup() {
  Serial.begin(115200);
  pinMode(CONNECT_WIFI_GPIO, OUTPUT);
  digitalWrite(CONNECT_WIFI_GPIO, LOW);
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_SVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 30;
  config.fb_count = 1;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 20;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_SVGA);
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(CONNECT_WIFI_GPIO, LOW);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  digitalWrite(CONNECT_WIFI_GPIO, HIGH);

  setupUltrasonic();
  setupMotors();
  setupCameraPower();

  startCameraServer();

  xTaskCreatePinnedToCore(motorAndSensorTask, "MotorSensorTask", 16384, NULL,20, NULL, 1);
  xTaskCreatePinnedToCore(wifiMonitorTask, "WiFiMonitorTask", 8192, NULL, 10, NULL, 0);
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}
void loop() {
}
