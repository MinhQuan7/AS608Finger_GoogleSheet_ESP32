/*************************************************************
  Download latest ERa library here:
    https://github.com/eoh-jsc/era-lib/releases/latest
    https://www.arduino.cc/reference/en/libraries/era
    https://registry.platformio.org/libraries/eoh-ltd/ERa/installation

    ERa website:                https://e-ra.io
    ERa blog:                   https://iotasia.org
    ERa forum:                  https://forum.eoh.io
    Follow us:                  https://www.fb.com/EoHPlatform
 *************************************************************/

// Enable debug console
#define ERA_DEBUG

/* Define MQTT host */
#define DEFAULT_MQTT_HOST "mqtt1.eoh.io"

// You should get Auth Token in the ERa App or ERa Dashboard
#define ERA_AUTH_TOKEN "ba26672a-af8d-430b-abab-ed8c4a390ead"

/* Define setting button */
// #define BUTTON_PIN              0

#if defined(BUTTON_PIN)
// Active low (false), Active high (true)
#define BUTTON_INVERT false
#define BUTTON_HOLD_TIMEOUT 5000UL

// This directive is used to specify whether the configuration should be erased.
// If it's set to true, the configuration will be erased.
#define ERA_ERASE_CONFIG false
#endif

// Khai báo chân kết nối cảm biến vân tay
#define FINGERPRINT_RX_PIN 16
#define FINGERPRINT_TX_PIN 17

// Cấu hình xác thực vân tay nâng cao
#define CONFIDENCE_THRESHOLD 70    // Ngưỡng độ tin cậy tối thiểu
#define SAMPLING_DURATION 1500     // Thời gian lấy mẫu (1.5 giây)
#define MIN_SAMPLES 2              // Số mẫu tối thiểu
#define MAX_SAMPLES 8              // Số mẫu tối đa
#define SAMPLE_INTERVAL 150        // Khoảng cách giữa các mẫu (ms)
#define FINGER_DETECT_TIMEOUT 3000 // Timeout để phát hiện ngón tay liên tục

#include <Arduino.h>
#include <ERa.hpp>
#include <Adafruit_Fingerprint.h>
#include <WiFiClientSecure.h>

const char ssid[] = "eoh.io";
const char pass[] = "Eoh@2020";

WiFiClient mbTcpClient;

#if defined(ERA_AUTOMATION)
#include <Automation/ERaSmart.hpp>

#if defined(ESP32) || defined(ESP8266)
#include <Time/ERaEspTime.hpp>
/* NTP Time */
ERaEspTime syncTime;
TimeElement_t ntpTime;
#else
#define USE_BASE_TIME

#include <Time/ERaBaseTime.hpp>
ERaBaseTime syncTime;
#endif

ERaSmart smart(ERa, syncTime);
#endif

#if defined(BUTTON_PIN)
#include <ERa/ERaButton.hpp>

ERaButton button;

#if ERA_VERSION_NUMBER >= ERA_VERSION_VAL(1, 6, 0)
static void eventButton(uint16_t pin, ButtonEventT event)
{
  if (event != ButtonEventT::BUTTON_ON_HOLD)
  {
    return;
  }
  ERa.switchToConfig(ERA_ERASE_CONFIG);
  (void)pin;
}
#elif ERA_VERSION_NUMBER >= ERA_VERSION_VAL(1, 2, 0)
static void eventButton(uint8_t pin, ButtonEventT event)
{
  if (event != ButtonEventT::BUTTON_ON_HOLD)
  {
    return;
  }
  ERa.switchToConfig(ERA_ERASE_CONFIG);
  (void)pin;
}
#else
static void eventButton(ButtonEventT event)
{
  if (event != ButtonEventT::BUTTON_ON_HOLD)
  {
    return;
  }
  ERa.switchToConfig(ERA_ERASE_CONFIG);
}
#endif

#if defined(ESP32)
#include <pthread.h>

pthread_t pthreadButton;

static void *handlerButton(void *args)
{
  for (;;)
  {
    button.run();
    ERaDelay(10);
  }
  pthread_exit(NULL);
}

void initButton()
{
  pinMode(BUTTON_PIN, INPUT);
  button.setButton(BUTTON_PIN, digitalRead, eventButton,
                   BUTTON_INVERT)
      .onHold(BUTTON_HOLD_TIMEOUT);
  pthread_create(&pthreadButton, NULL, handlerButton, NULL);
}
#elif defined(ESP8266)
#include <Ticker.h>

Ticker ticker;

static void handlerButton()
{
  button.run();
}

void initButton()
{
  pinMode(BUTTON_PIN, INPUT);
  button.setButton(BUTTON_PIN, digitalRead, eventButton,
                   BUTTON_INVERT)
      .onHold(BUTTON_HOLD_TIMEOUT);
  ticker.attach_ms(100, handlerButton);
}
#elif defined(ARDUINO_AMEBA)
#include <GTimer.h>

const uint32_t timerIdButton{0};

static void handlerButton(uint32_t data)
{
  button.run();
  (void)data;
}

void initButton()
{
  pinMode(BUTTON_PIN, INPUT);
  button.setButton(BUTTON_PIN, digitalReadArduino, eventButton,
                   BUTTON_INVERT)
      .onHold(BUTTON_HOLD_TIMEOUT);
  GTimer.begin(timerIdButton, (100 * 1000), handlerButton);
}
#endif
#endif

/* This function will run every time ERa is connected */
ERA_CONNECTED()
{
  ERA_LOG(ERA_PSTR("ERa"), ERA_PSTR("ERa connected!"));
}

/* This function will run every time ERa is disconnected */
ERA_DISCONNECTED()
{
  ERA_LOG(ERA_PSTR("ERa"), ERA_PSTR("ERa disconnected!"));
}

// Khởi tạo đối tượng HardwareSerial
HardwareSerial mySerial(1);

// Khởi tạo đối tượng Adafruit_Fingerprint
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// define Google Script
const char *GOOGLE_SCRIPT_HOST = "script.google.com";
const char *GOOGLE_SCRIPT_PATH = "/macros/s/AKfycbzGK4ZjPJ2sGRgtomSgY2NvdqyUXxt4BubWoxK-NzxRWeupqlEA33jgtxRW98Yp9ge1sQ/exec";

// Cấu trúc để lưu thông tin xác thực
struct FingerprintAuth
{
  uint16_t id;
  uint16_t confidence;
  bool isValid;
};

// Biến toàn cục để tránh gửi duplicate
static uint16_t lastValidatedID = 0;
static unsigned long lastValidationTime = 0;
static bool authenticationInProgress = false;

void sendToGoogleSheet(int id, const char *time, const char *date)
{
  WiFiClientSecure client;
  client.setInsecure();

  // Tạo payload JSON
  String payload = "{\"time\":\"" + String(time) + "\",\"id\":\"" + String(id) + "\",\"date\":\"" + String(date) + "\"}";

  // Tạo HTTP request thủ công
  String request =
      "POST " + String(GOOGLE_SCRIPT_PATH) + " HTTP/1.1\r\n" +
      "Host: " + GOOGLE_SCRIPT_HOST + "\r\n" +
      "Content-Type: application/json\r\n" +
      "Content-Length: " + String(payload.length()) + "\r\n" +
      "Connection: close\r\n\r\n" +
      payload;

  // Kết nối và gửi request
  if (client.connect(GOOGLE_SCRIPT_HOST, 443))
  {
    client.print(request);
    Serial.println("[HTTP] Data sent to Google Sheet!");

    // Đọc phản hồi (debug)
    while (client.connected())
    {
      String line = client.readStringUntil('\n');
      if (line == "\r")
        break;
    }
    String response = client.readString();
    Serial.println(response);
  }
  else
  {
    Serial.println("[HTTP] Connection to Google failed!");
  }
  client.stop();
}

// Hàm lấy một mẫu vân tay duy nhất
FingerprintAuth getSingleFingerprintSample()
{
  FingerprintAuth result = {0, 0, false};

  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)
  {
    return result;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
  {
    return result;
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK)
  {
    result.id = finger.fingerID;
    result.confidence = finger.confidence;
    result.isValid = true;

    Serial.printf("[SAMPLE] ID: %d, Confidence: %d\n", result.id, result.confidence);
  }

  return result;
}

// Hàm xác thực vân tay thông minh và linh hoạt
FingerprintAuth authenticateFingerprint()
{
  Serial.println("[AUTH] Starting smart fingerprint authentication...");

  FingerprintAuth finalResult = {0, 0, false};

  // Mảng lưu các mẫu hợp lệ
  struct
  {
    uint16_t id;
    uint16_t confidence;
  } samples[MAX_SAMPLES];

  int validSamples = 0;
  unsigned long startTime = millis();
  unsigned long lastSampleTime = 0;
  int consecutiveFailures = 0;

  // Lấy mẫu trong khoảng thời gian quy định
  while ((millis() - startTime) < SAMPLING_DURATION && validSamples < MAX_SAMPLES)
  {

    // Chỉ lấy mẫu sau khoảng cách thời gian nhất định
    if (millis() - lastSampleTime >= SAMPLE_INTERVAL)
    {
      FingerprintAuth sample = getSingleFingerprintSample();

      if (sample.isValid)
      {
        samples[validSamples].id = sample.id;
        samples[validSamples].confidence = sample.confidence;
        validSamples++;
        lastSampleTime = millis();
        consecutiveFailures = 0; // Reset failure counter

        Serial.printf("[AUTH] Sample %d: ID=%d, Confidence=%d\n",
                      validSamples, sample.id, sample.confidence);

        // Nếu có ít nhất MIN_SAMPLES và confidence cao, có thể kết thúc sớm
        if (validSamples >= MIN_SAMPLES && sample.confidence >= (CONFIDENCE_THRESHOLD + 20))
        {
          // Kiểm tra tính nhất quán nhanh
          bool allSameID = true;
          for (int i = 1; i < validSamples; i++)
          {
            if (samples[i].id != samples[0].id)
            {
              allSameID = false;
              break;
            }
          }
          if (allSameID)
          {
            Serial.println("[AUTH] High confidence samples detected, early exit");
            break;
          }
        }
      }
      else
      {
        consecutiveFailures++;
        // Nếu thất bại liên ti属 quá nhiều, có thể ngón tay đã rời khỏi
        if (consecutiveFailures >= 3)
        {
          Serial.println("[AUTH] Too many consecutive failures, finger may be removed");
          break;
        }
      }
    }

    delay(30); // Giảm delay để responsive hơn
  }

  // Kiểm tra xem có đủ mẫu không
  if (validSamples < MIN_SAMPLES)
  {
    Serial.printf("[AUTH] Insufficient samples: %d (minimum %d required)\n",
                  validSamples, MIN_SAMPLES);
    return finalResult;
  }

  // Nếu chỉ có 1 mẫu nhưng confidence rất cao, chấp nhận luôn
  if (validSamples == 1 && samples[0].confidence >= (CONFIDENCE_THRESHOLD + 30))
  {
    Serial.printf("[AUTH] Single high-confidence sample accepted: ID=%d, Confidence=%d\n",
                  samples[0].id, samples[0].confidence);
    finalResult.id = samples[0].id;
    finalResult.confidence = samples[0].confidence;
    finalResult.isValid = true;
    return finalResult;
  }

  // Phân tích các mẫu để tìm ID phổ biến nhất
  uint16_t mostCommonID = samples[0].id; // Default to first sample
  int maxCount = 1;
  uint32_t totalConfidence = samples[0].confidence;
  int countForMostCommon = 1;

  // Đếm tần suất xuất hiện của từng ID
  for (int i = 0; i < validSamples; i++)
  {
    int count = 0;
    uint32_t confidenceSum = 0;

    for (int j = 0; j < validSamples; j++)
    {
      if (samples[j].id == samples[i].id)
      {
        count++;
        confidenceSum += samples[j].confidence;
      }
    }

    if (count > maxCount || (count == maxCount && confidenceSum > totalConfidence))
    {
      maxCount = count;
      mostCommonID = samples[i].id;
      totalConfidence = confidenceSum;
      countForMostCommon = count;
    }
  }

  // Tính độ tin cậy trung bình cho ID phổ biến nhất
  uint16_t avgConfidence = totalConfidence / countForMostCommon;

  Serial.printf("[AUTH] Analysis: ID=%d appeared %d/%d times, Avg Confidence=%d\n",
                mostCommonID, maxCount, validSamples, avgConfidence);

  // Kiểm tra các điều kiện xác thực (linh hoạt hơn)
  float consistencyRatio = (float)maxCount / validSamples;
  bool isConsistent = (consistencyRatio >= 0.5); // Ít nhất 50% mẫu cùng ID
  bool isConfident = (avgConfidence >= CONFIDENCE_THRESHOLD);

  // Điều kiện đặc biệt: nếu có 2 mẫu cùng ID và confidence cao
  if (validSamples == 2 && maxCount == 2 && avgConfidence >= (CONFIDENCE_THRESHOLD + 10))
  {
    isConsistent = true;
    isConfident = true;
  }

  if (isConsistent && isConfident)
  {
    finalResult.id = mostCommonID;
    finalResult.confidence = avgConfidence;
    finalResult.isValid = true;

    Serial.printf("[AUTH] ✓ AUTHENTICATED: ID=%d, Confidence=%d, Consistency=%.1f%%\n",
                  finalResult.id, finalResult.confidence, consistencyRatio * 100);
  }
  else
  {
    Serial.printf("[AUTH] ✗ FAILED: Consistent=%s (%.1f%%), Confident=%s (%d>=%d)\n",
                  isConsistent ? "YES" : "NO", consistencyRatio * 100,
                  isConfident ? "YES" : "NO", avgConfidence, CONFIDENCE_THRESHOLD);
  }

  return finalResult;
}

// Hàm xử lý khi có vân tay được xác thực thành công
void handleAuthenticatedFingerprint(FingerprintAuth auth)
{
  // Tránh gửi duplicate trong thời gian ngắn
  if (auth.id == lastValidatedID &&
      (millis() - lastValidationTime) < 5000)
  { // 5 giây cooldown
    Serial.println("[SKIP] Same fingerprint within cooldown period");
    return;
  }

  // Lấy thời gian từ NTP
  syncTime.getTime(ntpTime);

  // Định dạng thời gian và ngày
  char timeStr[12];
  sprintf(timeStr, "%02d:%02d:%02d", ntpTime.hour, ntpTime.minute, ntpTime.second);

  char dateStr[12];
  sprintf(dateStr, "%02d/%02d/%04d", ntpTime.day, ntpTime.month, ntpTime.year + 1970);

  // Gửi ID lên E-Ra platform
  String idString = "ID" + String(auth.id);
  ERa.virtualWrite(V0, idString.c_str());

  // Gửi dữ liệu lên Google Sheet
  sendToGoogleSheet(auth.id, timeStr, dateStr);

  // Cập nhật trạng thái
  lastValidatedID = auth.id;
  lastValidationTime = millis();

  Serial.printf("[SUCCESS] Processed fingerprint: ID=%d, Time=%s, Date=%s, Confidence=%d\n",
                auth.id, timeStr, dateStr, auth.confidence);
}

/* This function print uptime every second */
void timerEvent()
{
  ERA_LOG(ERA_PSTR("Timer"), ERA_PSTR("Uptime: %d"), ERaMillis() / 1000L);

  // Tránh chạy song song nhiều quá trình xác thực
  if (authenticationInProgress)
  {
    return;
  }

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 800)
  { // Kiểm tra mỗi 800ms (chậm hơn)
    return;
  }
  lastCheck = millis();

  // Kiểm tra nhanh xem có ngón tay không (2 lần để chắc chắn)
  uint8_t p1 = finger.getImage();
  delay(50);
  uint8_t p2 = finger.getImage();

  if (p1 == FINGERPRINT_NOFINGER && p2 == FINGERPRINT_NOFINGER)
  {
    return; // Chắc chắn không có ngón tay, thoát
  }

  Serial.println("[DETECT] Finger detected, starting authentication...");

  // Có ngón tay, bắt đầu quá trình xác thực nâng cao
  authenticationInProgress = true;

  FingerprintAuth result = authenticateFingerprint();

  if (result.isValid)
  {
    handleAuthenticatedFingerprint(result);
  }
  else
  {
    Serial.println("[AUTH] Authentication failed, waiting for next attempt...");
  }

  // Chờ ngón tay rời khỏi sensor trước khi tiếp tục
  Serial.println("[WAIT] Please remove finger...");
  unsigned long waitStart = millis();
  while (millis() - waitStart < 2000)
  { // Chờ tối đa 2 giây
    if (finger.getImage() == FINGERPRINT_NOFINGER)
    {
      delay(200); // Chờ thêm một chút để chắc chắn
      if (finger.getImage() == FINGERPRINT_NOFINGER)
      {
        break; // Ngón tay đã rời khỏi
      }
    }
    delay(100);
  }

  authenticationInProgress = false;
  Serial.println("[READY] Ready for next authentication");
}

#if defined(USE_BASE_TIME)
unsigned long getTimeCallback()
{
  // Please implement your own function
  // to get the current time in seconds.
  return 0;
}
#endif

void setup()
{
  /* Setup debug console */
#if defined(ERA_DEBUG)
  Serial.begin(115200);
#endif

#if defined(BUTTON_PIN)
  /* Initializing button. */
  initButton();
  /* Enable read/write WiFi credentials */
  ERa.setPersistent(true);
#endif

#if defined(USE_BASE_TIME)
  syncTime.setGetTimeCallback(getTimeCallback);
#endif

  /* Setup Client for Modbus TCP/IP */
  ERa.setModbusClient(mbTcpClient);

  /* Set scan WiFi. If activated, the board will scan
     and connect to the best quality WiFi. */
  ERa.setScanWiFi(true);

  /* Initializing the ERa library. */
  ERa.begin(ssid, pass);

  /* Setup timer called function every second */
  ERa.addInterval(1000L, timerEvent);

  Serial.println("=== Enhanced Fingerprint Authentication System ===");
  Serial.printf("Confidence Threshold: %d\n", CONFIDENCE_THRESHOLD);
  Serial.printf("Sampling Duration: %d ms\n", SAMPLING_DURATION);
  Serial.printf("Min/Max Samples: %d/%d\n", MIN_SAMPLES, MAX_SAMPLES);

  // Khởi tạo Serial để giao tiếp với cảm biến vân tay
  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX_PIN, FINGERPRINT_TX_PIN);
  finger.begin(57600);

  // Khởi tạo cảm biến vân tay
  if (finger.verifyPassword())
  {
    Serial.println("✓ Found fingerprint sensor!");
  }
  else
  {
    Serial.println("✗ Did not find fingerprint sensor :(");
    while (1)
    {
      delay(1);
    }
  }

  Serial.println("Reading sensor parameters...");
  finger.getParameters();
  Serial.printf("Status: 0x%02X\n", finger.status_reg);
  Serial.printf("Sys ID: 0x%02X\n", finger.system_id);
  Serial.printf("Capacity: %d\n", finger.capacity);
  Serial.printf("Security level: %d\n", finger.security_level);
  Serial.printf("Device address: 0x%02X\n", finger.device_addr);
  Serial.printf("Packet len: %d\n", finger.packet_len);
  Serial.printf("Baud rate: %d\n", finger.baud_rate);

  finger.getTemplateCount();

  if (finger.templateCount == 0)
  {
    Serial.println("⚠ Sensor doesn't contain any fingerprint data. Please enroll fingerprints first.");
  }
  else
  {
    Serial.printf("✓ Sensor contains %d fingerprint templates\n", finger.templateCount);
    Serial.println("Ready for enhanced fingerprint authentication...");
  }
}

void loop()
{
  ERa.run();
}