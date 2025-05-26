
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

uint8_t getFingerprintID()
{
  uint8_t p = finger.getImage();
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image taken");
    break;
  case FINGERPRINT_NOFINGER:
    Serial.println("No finger detected");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    return p;
  case FINGERPRINT_IMAGEFAIL:
    Serial.println("Imaging error");
    return p;
  default:
    Serial.println("Unknown error");
    return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image converted");
    break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("Image too messy");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Could not find fingerprint features");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Could not find fingerprint features");
    return p;
  default:
    Serial.println("Unknown error");
    return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK)
  {
    Serial.println("Found a print match!");
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    return p;
  }
  else if (p == FINGERPRINT_NOTFOUND)
  {
    Serial.println("Did not find a match");
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);

  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);
  // Tạo chuỗi động dựa trên ID tìm được
  String idString = "ID" + String(finger.fingerID);

  // Gửi chuỗi lên E-Ra platform (PHẢI sử dụng .c_str())
  ERa.virtualWrite(V0, idString.c_str());

  return finger.fingerID;
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez()
{
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)
    return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
    return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)
    return -1;

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);
  return finger.fingerID;
}

/* This function print uptime every second */
void timerEvent()
{
  ERA_LOG(ERA_PSTR("Timer"), ERA_PSTR("Uptime: %d"), ERaMillis() / 1000L);
  static unsigned long lastTime = 0;
  if (millis() - lastTime < 1000)
    return; // Gửi mỗi 1 giây
  lastTime = millis();

  // Lấy thời gian từ NTP
  syncTime.getTime(ntpTime);

  // Định dạng thời gian và ngày
  char timeStr[12];
  sprintf(timeStr, "%02d:%02d:%02d", ntpTime.hour, ntpTime.minute, ntpTime.second);

  char dateStr[12];
  sprintf(dateStr, "%02d/%02d/%04d", ntpTime.day, ntpTime.month, ntpTime.year + 1970);

  getFingerprintID();
  // Gọi hàm đọc vân tay
  uint8_t result = getFingerprintID();

  if (result != FINGERPRINT_NOFINGER)
  {
    // Gửi ID dạng số lên E-Ra
    ERa.virtualWrite(V0, finger.fingerID);

    // In log để debug
    Serial.print("Sent ID to E-Ra: ");
    Serial.println(finger.fingerID);
    uint8_t id = finger.fingerID;
    sendToGoogleSheet(id, timeStr, dateStr);

    Serial.printf("Sent: ID%d | Time: %s | Date: %s\n", id, timeStr, dateStr);
  }
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
  Serial.println("Cảm biến vân tay AS608 với ESP32");

  // Khởi tạo Serial để giao tiếp với máy tính
  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX_PIN, FINGERPRINT_TX_PIN);
  finger.begin(57600);
  // Khởi tạo cảm biến vân tay
  if (finger.verifyPassword())
  {
    Serial.println("Found fingerprint sensor!");
  }
  else
  {
    Serial.println("Did not find fingerprint sensor :(");
    while (1)
    {
      delay(1);
    }
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x"));
  Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x"));
  Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));
  Serial.println(finger.capacity);
  Serial.print(F("Security level: "));
  Serial.println(finger.security_level);
  Serial.print(F("Device address: "));
  Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: "));
  Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: "));
  Serial.println(finger.baud_rate);

  finger.getTemplateCount();

  if (finger.templateCount == 0)
  {
    Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
  }
  else
  {
    Serial.println("Waiting for valid finger...");
    Serial.print("Sensor contains ");
    Serial.print(finger.templateCount);
    Serial.println(" templates");
  }
}

void loop()
{
  ERa.run();
}
