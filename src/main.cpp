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

// Cấu hình xác thực vân tay đơn giản và nhanh
#define CONFIDENCE_THRESHOLD 60 // Ngưỡng độ tin cậy tối thiểu (thấp hơn)
#define MAX_RETRY_ATTEMPTS 3    // Số lần thử tối đa trong 1 lần đặt tay
#define RETRY_DELAY 100         // Delay giữa các lần thử (ms)
unsigned long int currentTimeOpen = 0;
const int timedoorInterval = 5000;
bool isopenDoor = false;
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
#define RELAY_PIN 14
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

// Hàm xác thực vân tay đơn giản và nhanh
FingerprintAuth authenticateFingerprint()
{
  Serial.println("[AUTH] Quick fingerprint authentication...");

  FingerprintAuth result = {0, 0, false};

  // Thử tối đa MAX_RETRY_ATTEMPTS lần
  for (int attempt = 1; attempt <= MAX_RETRY_ATTEMPTS; attempt++)
  {
    Serial.printf("[AUTH] Attempt %d/%d\n", attempt, MAX_RETRY_ATTEMPTS);

    uint8_t p = finger.getImage();
    if (p != FINGERPRINT_OK)
    {
      Serial.printf("[AUTH] Get image failed: %d\n", p);
      if (attempt < MAX_RETRY_ATTEMPTS)
      {
        delay(RETRY_DELAY);
        continue;
      }
      return result;
    }

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK)
    {
      Serial.printf("[AUTH] Image2Tz failed: %d\n", p);
      if (attempt < MAX_RETRY_ATTEMPTS)
      {
        delay(RETRY_DELAY);
        continue;
      }
      return result;
    }

    p = finger.fingerFastSearch();
    if (p == FINGERPRINT_OK)
    {
      // Tìm thấy vân tay!
      Serial.printf("[AUTH] ✓ Found ID: %d, Confidence: %d\n", finger.fingerID, finger.confidence);

      // Kiểm tra confidence
      if (finger.confidence >= CONFIDENCE_THRESHOLD)
      {
        ERa.virtualWrite(V1, HIGH);
        isopenDoor = true;
        currentTimeOpen = millis();
        result.id = finger.fingerID;
        result.confidence = finger.confidence;
        result.isValid = true;
        Serial.printf("[AUTH] ✓ AUTHENTICATED: ID=%d, Confidence=%d\n", result.id, result.confidence);
        return result;
      }
      else
      {
        Serial.printf("[AUTH] Low confidence: %d < %d, retry...\n", finger.confidence, CONFIDENCE_THRESHOLD);
        if (attempt < MAX_RETRY_ATTEMPTS)
        {
          delay(RETRY_DELAY);
          continue;
        }
      }
    }
    else
    {
      Serial.printf("[AUTH] Fingerprint search failed: %d\n", p);
      if (attempt < MAX_RETRY_ATTEMPTS)
      {
        delay(RETRY_DELAY);
        continue;
      }
    }
  }

  Serial.println("[AUTH] ✗ Authentication failed after all attempts");
  ERa.virtualWrite(V1, LOW);
  return result;
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

/* This function handles fingerprint detection and authentication */
void timerEvent()
{
  ERA_LOG(ERA_PSTR("Timer"), ERA_PSTR("Uptime: %d"), ERaMillis() / 1000L);

  // Tránh chạy song song nhiều quá trình xác thực
  if (authenticationInProgress)
  {
    return;
  }

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 1000)
  { // Kiểm tra mỗi 1 giây
    return;
  }
  lastCheck = millis();

  // Kiểm tra nhanh xem có ngón tay không
  uint8_t p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER)
  {
    return; // Không có ngón tay, thoát
  }

  Serial.println("[DETECT] Finger detected!");

  // Có ngón tay, bắt đầu quá trình xác thực ngay
  authenticationInProgress = true;

  FingerprintAuth result = authenticateFingerprint();

  if (result.isValid)
  {
    handleAuthenticatedFingerprint(result);
  }

  // Chờ một chút để tránh trigger liên tục
  delay(2000);
  authenticationInProgress = false;
}

#if defined(USE_BASE_TIME)
unsigned long getTimeCallback()
{
  // Please implement your own function
  // to get the current time in seconds.
  return 0;
}
#endif
ERA_WRITE(V1)
{
  /* Get value from Virtual Pin 0 and write LED */
  uint8_t value = param.getInt();
  digitalWrite(RELAY_PIN, value ? HIGH : LOW);
}

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
  pinMode(RELAY_PIN, OUTPUT);
  /* Set scan WiFi. If activated, the board will scan
     and connect to the best quality WiFi. */
  ERa.setScanWiFi(true);

  /* Initializing the ERa library. */
  ERa.begin(ssid, pass);

  /* Setup timer called function every second */
  ERa.addInterval(1000L, timerEvent);

  Serial.println("=== Simple & Fast Fingerprint System ===");
  Serial.printf("Confidence Threshold: %d\n", CONFIDENCE_THRESHOLD);
  Serial.printf("Max Retry Attempts: %d\n", MAX_RETRY_ATTEMPTS);

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
  if (isopenDoor)
  {
    if (millis() - currentTimeOpen >= timedoorInterval)
    {
      ERa.virtualWrite(V1, LOW);
      isopenDoor = false;
    }
  }
}