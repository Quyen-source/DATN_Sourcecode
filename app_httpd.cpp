#include "Arduino.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp32-hal-ledc.h"
#include "sdkconfig.h"
#include <pgmspace.h>

#define MOTOR_1_PIN_1    42
#define MOTOR_1_PIN_2    41
#define MOTOR_2_PIN_1    40
#define MOTOR_2_PIN_2    39
#define ENABLE_PIN       1

#define TRIG_PIN_LEFT    19
#define ECHO_PIN_LEFT    20
#define TRIG_PIN_RIGHT   48
#define ECHO_PIN_RIGHT   38
#define TRIG_PIN_FRONT   45
#define ECHO_PIN_FRONT   46

#define CAMERA_POWER_PIN 2
#define CONNECT_WIFI_GPIO 14

#define DISTANCE_THRESHOLD 25
#define SOUND_SPEED 0.0343 
#define ECHO_TIMEOUT_US 30000
#define COMMAND_TIMEOUT 5

String valueString = String(0);
volatile int motorSpeed = 0;
volatile bool motorState = false;
esp_timer_handle_t motorTimer;
volatile bool motorsEnabled = false;
String currentDirection = "stop";
volatile unsigned long lastCommandTime = 0;

// Web server streaming constants
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;
volatile bool obstacleLeft = false;
volatile bool obstacleRight = false;
volatile bool obstacleFront = false;
volatile bool cameraOn = true; 

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<html>
  <head>
    <title>ĐỒ ÁN TỐT NGHIỆP</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link href="https://fonts.googleapis.com/css2?family=Roboto&display=swap" rel="stylesheet">
    <style>
      body { 
        font-family: 'Roboto', Arial, sans-serif; 
        text-align: center; 
        margin: 0 auto; 
        padding: 20px; 
      }
      .content-container {
        max-width: 90%;
        margin: 0 auto;
      }
      .image-container {
        max-width: 100%;
        margin: 20px auto;
        overflow: hidden;
      }
      table { 
        margin: 20px auto; 
      }
      td { 
        padding: 8px; 
      }
      .button {
        background-color: #2f4468;
        border: none;
        color: white;
        padding: 10px 20px;
        text-align: center;
        text-decoration: none;
        display: inline-block;
        font-size: 18px;
        margin: 6px 3px;
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        -khtml-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
      }
      .button2 { 
        background-color: #555555; 
      }
      .toggle-button-on { 
        background-color: #4CAF50; 
      }
      .toggle-button-off { 
        background-color: #f44336; 
      }
      img#photo {
        width: auto;
        max-width: 100%;
        max-height: 60vh;
        transform: rotate(90deg);
        transform-origin: center;
        object-fit: contain;
        display: block;
        margin: 20px auto;
      }
      .slider { 
        width: 200px; 
      }
      .obstacle-alert {
        margin-top: 10px;
        font-size: 18px;
        font-weight: bold;
        text-align: center;
      }
      .alert-danger {
        color: #f44336;
      }
      .alert-success {
        color: #4CAF50;
      }
    </style>
    <script>
      let cameraState = true;

      function sendCommand(dir) {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/action?go=" + dir, true);
        xhr.send();
      }

      function updateMotorSpeed(pos) {
        document.getElementById('motorSpeed').innerHTML = pos;
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/speed?value=" + pos, true);
        xhr.send();
      }

      function updateObstacles() {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/obstacles", true);
        xhr.onreadystatechange = function() {
          if (xhr.readyState == 4 && xhr.status == 200) {
            var data = JSON.parse(xhr.responseText);
            var alertDiv = document.getElementById('obstacleAlert');
            if (data.left || data.right || data.front) {
              alertDiv.innerHTML = "Có vật cản";
              alertDiv.className = "obstacle-alert alert-danger";
            } else {
              alertDiv.innerHTML = "Không có vật cản";
              alertDiv.className = "obstacle-alert alert-success";
            }
          }
        };
        xhr.send();
      }

      function toggleCamera() {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/camera_toggle", true);
        xhr.onreadystatechange = function() {
          if (xhr.readyState == 4 && xhr.status == 200) {
            cameraState = !cameraState;
            var button = document.getElementById('cameraToggleButton');
            if (cameraState) {
              button.innerHTML = "Tắt Camera";
              button.className = "button toggle-button-on";
              document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream";
            } else {
              button.innerHTML = "Bật Camera";
              button.className = "button toggle-button-off";
              document.getElementById("photo").src = "";
            }
          }
        };
        xhr.send();
      }

      window.onload = function() {
        document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream";
        document.getElementById('motorSpeed').innerHTML = 0;
        document.getElementById('motorSlider').value = 0;
        setInterval(updateObstacles, 500);
        document.getElementById('cameraToggleButton').className = "button toggle-button-on";
        updateObstacles();
      };
    </script>
  </head>
  <body>
    <div class="content-container">
      <h1>ĐỒ ÁN TỐT NGHIỆP</h1>
      <div class="image-container">
        <img src="" id="photo">
      </div>
      <div id="obstacleAlert" class="obstacle-alert alert-success">Không có vật cản</div>
      <table>
        <tr><td colspan="3" align="center">
          <button class="button"
            onmousedown="sendCommand('forward')" onmouseup="sendCommand('stop')"
            ontouchstart="sendCommand('forward')" ontouchend="sendCommand('stop')">Tiến</button>
        </td></tr>
        <tr>
          <td align="center">
            <button class="button"
              onmousedown="sendCommand('left')" onmouseup="sendCommand('stop')"
              ontouchstart="sendCommand('left')" ontouchend="sendCommand('stop')">Trái</button>
          </td>
          <td align="center">
            <button class="button button2" onclick="sendCommand('stop')">Dừng</button>
          </td>
          <td align="center">
            <button class="button"
              onmousedown="sendCommand('right')" onmouseup="sendCommand('stop')"
              ontouchstart="sendCommand('right')" ontouchend="sendCommand('stop')">Phải</button>
          </td>
        </tr>
        <tr><td colspan="3" align="center">
          <button class="button"
            onmousedown="sendCommand('backward')" onmouseup="sendCommand('stop')"
            ontouchstart="sendCommand('backward')" ontouchend="sendCommand('stop')">Lùi</button>
        </td></tr>
        <tr><td colspan="3" align="center">
          <button id="cameraToggleButton" class="button toggle-button-on" onclick="toggleCamera()">Tắt Camera</button>
        </td></tr>
      </table>
      <p>Tốc độ động cơ: <span id="motorSpeed">0</span></p>
      <input type="range" min="0" max="100" step="25" class="slider" id="motorSlider" oninput="updateMotorSpeed(this.value)" value="0"/>
    </div>
  </body>
</html>
)rawliteral";

void IRAM_ATTR motorTimerCallback(void *arg) {
  if (motorSpeed == 0 || !motorsEnabled) {
    digitalWrite(ENABLE_PIN, LOW);
    motorState = false;
    return;
  }
  int cycleTimeUs = 20000;
  int onTimeUs = (motorSpeed * cycleTimeUs) / 100;
  int offTimeUs = cycleTimeUs - onTimeUs;
  if (motorState) {
    digitalWrite(ENABLE_PIN, LOW);
    motorState = false;
    esp_timer_stop(motorTimer);
    esp_timer_start_once(motorTimer, offTimeUs);
  } else {
    digitalWrite(ENABLE_PIN, HIGH);
    motorState = true;
    esp_timer_stop(motorTimer);
    esp_timer_start_once(motorTimer, onTimeUs);
  }
}

esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=UTF-8");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

esp_err_t stream_handler(httpd_req_t *req) {
  if (!cameraOn) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera is turned off");
    return ESP_FAIL;
  }

  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 60, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          Serial.println("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }
    if (!cameraOn) {
      break;
    }
  }
  return res;
}

void stopMotors(const char* reason) {
  Serial.printf("Stopping motors: %s\n", reason);
  esp_timer_stop(motorTimer);
  digitalWrite(MOTOR_1_PIN_1, LOW);
  digitalWrite(MOTOR_1_PIN_2, LOW);
  digitalWrite(MOTOR_2_PIN_1, LOW);
  digitalWrite(MOTOR_2_PIN_2, LOW);
  digitalWrite(ENABLE_PIN, LOW);
  motorState = false;
  motorsEnabled = false;
  currentDirection = "stop";
}

esp_err_t cmd_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char variable[32] = {0,};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) != ESP_OK) {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  currentDirection = String(variable);

  if (!strcmp(variable, "stop")) {
    stopMotors("Button released or stop command");
    lastCommandTime = millis(); // Cập nhật thời gian khi nhả nút
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
  }

  if (WiFi.status() != WL_CONNECTED) {
    stopMotors("No WiFi connection");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
  }

  if (obstacleLeft || obstacleRight || obstacleFront) {
    if (!strcmp(variable, "forward")) {
      stopMotors("Obstacle detected");
      lastCommandTime = millis(); // Cập nhật thời gian khi dừng do vật cản
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
      return httpd_resp_send(req, NULL, 0);
    }
  }

  if (!strcmp(variable, "forward")) {
    Serial.println("Forward");
    motorsEnabled = true;
    digitalWrite(MOTOR_1_PIN_1, LOW);
    digitalWrite(MOTOR_1_PIN_2, HIGH);
    digitalWrite(MOTOR_2_PIN_1, LOW);
    digitalWrite(MOTOR_2_PIN_2, HIGH);
  } else if (!strcmp(variable, "left")) {
    Serial.println("Left");
    motorsEnabled = true;
    digitalWrite(MOTOR_1_PIN_1, HIGH);
    digitalWrite(MOTOR_1_PIN_2, LOW);
    digitalWrite(MOTOR_2_PIN_1, LOW);
    digitalWrite(MOTOR_2_PIN_2, HIGH);
  } else if (!strcmp(variable, "right")) {
    Serial.println("Right");
    motorsEnabled = true;
    digitalWrite(MOTOR_1_PIN_1, LOW);
    digitalWrite(MOTOR_1_PIN_2, HIGH);
    digitalWrite(MOTOR_2_PIN_1, HIGH);
    digitalWrite(MOTOR_2_PIN_2, LOW);
  } else if (!strcmp(variable, "backward")) {
    Serial.println("Backward");
    motorsEnabled = true;
    digitalWrite(MOTOR_1_PIN_1, HIGH);
    digitalWrite(MOTOR_1_PIN_2, LOW);
    digitalWrite(MOTOR_2_PIN_1, HIGH);
    digitalWrite(MOTOR_2_PIN_2, LOW);
  } else {
    stopMotors("Invalid command");
    lastCommandTime = millis(); // Cập nhật thời gian khi lệnh không hợp lệ
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  if (motorsEnabled && motorSpeed > 0) {
    esp_timer_stop(motorTimer);
    motorState = false;
    esp_timer_start_once(motorTimer, 0);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

esp_err_t speed_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char value[32] = {0,};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "value", value, sizeof(value)) != ESP_OK) {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  valueString = String(value);
  motorSpeed = atoi(value);
  
  if (motorSpeed == 0) {
    stopMotors("Speed set to zero");
    lastCommandTime = millis(); // Cập nhật thời gian khi tốc độ bằng 0
  } else if (motorsEnabled) {
    esp_timer_stop(motorTimer);
    motorState = false;
    esp_timer_start_once(motorTimer, 0);
    Serial.printf("Motor speed set to %d%%\n", motorSpeed);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

esp_err_t obstacles_handler(httpd_req_t *req) {
  char json_response[100];
  snprintf(json_response, sizeof(json_response), 
           "{\"left\":%s,\"right\":%s,\"front\":%s}", 
           obstacleLeft ? "true" : "false", 
           obstacleRight ? "true" : "false", 
           obstacleFront ? "true" : "false");
  
  httpd_resp_set_type(req, "application/json; charset=UTF-8");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json_response, strlen(json_response));
}

esp_err_t camera_toggle_handler(httpd_req_t *req) {
  cameraOn = !cameraOn;
  digitalWrite(CAMERA_POWER_PIN, cameraOn ? LOW : HIGH);
  Serial.printf("Camera turned %s\n", cameraOn ? "ON" : "OFF");

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

float measureDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) {
    return 1000.0;
  }
  float distance = (duration * SOUND_SPEED) / 2;
  if (distance > 400 || distance < 2) {
    return 1000.0;
  }
  return distance;
}

void updateObstacles() {
  float distanceLeft = measureDistance(TRIG_PIN_LEFT, ECHO_PIN_LEFT);
  vTaskDelay(pdMS_TO_TICKS(2));
  float distanceRight = measureDistance(TRIG_PIN_RIGHT, ECHO_PIN_RIGHT);
  vTaskDelay(pdMS_TO_TICKS(2));
  float distanceFront = measureDistance(TRIG_PIN_FRONT, ECHO_PIN_FRONT);
  vTaskDelay(pdMS_TO_TICKS(2));

  obstacleLeft = (distanceLeft < DISTANCE_THRESHOLD && distanceLeft > 0);
  obstacleRight = (distanceRight < DISTANCE_THRESHOLD && distanceRight > 0);
  obstacleFront = (distanceFront < DISTANCE_THRESHOLD && distanceFront > 0);
}

void motorAndSensorTask(void *pvParameters) {
  while (1) {
    updateObstacles();
    if (motorsEnabled && motorSpeed > 0) {
      if ((obstacleLeft || obstacleRight || obstacleFront) && currentDirection == "forward") {
        stopMotors("Obstacle detected");
        lastCommandTime = millis(); // Cập nhật thời gian khi dừng do vật cản
      }
      if (WiFi.status() != WL_CONNECTED) {
        stopMotors("WiFi disconnected in motor task");
        lastCommandTime = millis(); // Cập nhật thời gian khi mất WiFi
      }
    }
    if (currentDirection == "stop" && lastCommandTime > 0 && millis() - lastCommandTime > COMMAND_TIMEOUT) {
      stopMotors("Command timeout after stop");
      lastCommandTime = 0; // Reset thời gian sau khi dừng hoàn toàn
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void setupUltrasonic() {
  pinMode(TRIG_PIN_LEFT, OUTPUT);
  pinMode(ECHO_PIN_LEFT, INPUT);
  pinMode(TRIG_PIN_RIGHT, OUTPUT);
  pinMode(ECHO_PIN_RIGHT, INPUT);
  pinMode(TRIG_PIN_FRONT, OUTPUT);
  pinMode(ECHO_PIN_FRONT, INPUT);

  digitalWrite(TRIG_PIN_LEFT, LOW);
  digitalWrite(TRIG_PIN_RIGHT, LOW);
  digitalWrite(TRIG_PIN_FRONT, LOW);
}

void setupMotors() {
  pinMode(MOTOR_1_PIN_1, OUTPUT);
  pinMode(MOTOR_1_PIN_2, OUTPUT);
  pinMode(MOTOR_2_PIN_1, OUTPUT);
  pinMode(MOTOR_2_PIN_2, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);

  digitalWrite(MOTOR_1_PIN_1, LOW);
  digitalWrite(MOTOR_1_PIN_2, LOW);
  digitalWrite(MOTOR_2_PIN_1, LOW);
  digitalWrite(MOTOR_2_PIN_2, LOW);
  digitalWrite(ENABLE_PIN, LOW);

  esp_timer_create_args_t timer_args = {
    .callback = &motorTimerCallback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "motor_timer"
  };
  esp_timer_create(&timer_args, &motorTimer);
}

void setupCameraPower() {
  pinMode(CAMERA_POWER_PIN, OUTPUT);
  digitalWrite(CAMERA_POWER_PIN, LOW);
  cameraOn = true;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.task_priority = 10;
  config.stack_size = 8192;
  config.max_open_sockets = 2;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri       = "/action",
    .method    = HTTP_GET,
    .handler   = cmd_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t speed_uri = {
    .uri       = "/speed",
    .method    = HTTP_GET,
    .handler   = speed_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t obstacles_uri = {
    .uri       = "/obstacles",
    .method    = HTTP_GET,
    .handler   = obstacles_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t camera_toggle_uri = {
    .uri       = "/camera_toggle",
    .method    = HTTP_GET,
    .handler   = camera_toggle_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
    httpd_register_uri_handler(camera_httpd, &speed_uri);
    httpd_register_uri_handler(camera_httpd, &obstacles_uri);
    httpd_register_uri_handler(camera_httpd, &camera_toggle_uri);
  }
  config.server_port += 1; 
  config.ctrl_port += 1;
  config.task_priority = 15;
  config.stack_size = 30720;
  config.max_open_sockets = 2;
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_uri_t stream_uri = {
      .uri       = "/stream",
      .method    = HTTP_GET,
      .handler   = stream_handler,
      .user_ctx  = NULL
    };
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void wifiMonitorTask(void *pvParameters) {
  const int maxReconnectAttempts = 5;
  int reconnectAttempts = 0;
  while (1) {
    if (WiFi.status() == WL_CONNECTED) {
      digitalWrite(CONNECT_WIFI_GPIO, HIGH);
      reconnectAttempts = 0;
      Serial.println("WiFi connected");
    } else {
      digitalWrite(CONNECT_WIFI_GPIO, LOW);
      stopMotors("WiFi disconnected");
      lastCommandTime = millis();
      if (reconnectAttempts < maxReconnectAttempts) {
        Serial.printf("WiFi disconnected, attempting to reconnect (%d/%d)...\n", reconnectAttempts + 1, maxReconnectAttempts);
        WiFi.reconnect();
        reconnectAttempts++;
      } else {
        Serial.println("Max WiFi reconnect attempts reached, stopping attempts");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
}
