# 🚀 Welcome to IoT Force | Embedded | AI Project Collection

#### Chào mừng bạn đến với kho tài nguyên **miễn phí** về các dự án liên quan đến **IoT**, **Embedded Systems**, và **AI**.  
#### Tất cả đều được chia sẻ **phi thương mại** nhằm hỗ trợ các bạn sinh viên, kỹ sư và cộng đồng đam mê công nghệ!
#### NẾU CÁC BẠN ĐÃ CÀI PLATFORM IO THÌ CHỈ CẦN DOWN VỀ => Vô VScode => File => Open Folder (Chọn folder source code bạn mới tải)=> Đợi khoảng 45 giây để Vscode Install thư viện cần thiết và các packages khác.
#### Nếu chưa bạn cài Platform IO : Bạn tham khảo video sau : 👉 [YouTube Channel](https://www.youtube.com/watch?v=FuLRXgD9C2s)

---

## 📌 Nội dung repo này

- ✅ Các dự án sử dụng **ESP32**, **Arduino**, **STM32**, v.v...
- 🎓 Ứng dụng cho **đồ án môn học**, **sản phẩm mẫu**, **ý tưởng startup**
- 📄 Bao gồm **tài liệu**, **source code**, **sơ đồ mạch**, và **video hướng dẫn**

---

## 🔧 Hướng dẫn sử dụng repo (Step-by-step)

### Bước 1: Tải mã nguồn

- Nhấn nút **Code > Download ZIP** hoặc dùng Git:

```bash
git clone https://github.com/MinhQuan7/AS608Finger_GoogleSheet_ESP32
```

> Nếu bạn chưa biết dùng Git, có thể bỏ qua và tải ZIP về rồi giải nén.

---

### Bước 2: Cài đặt phần mềm lập trình

- **Arduino IDE** (>= 1.8.x) hoặc **PlatformIO + VS Code**
- Chọn đúng **board** (ESP32, Arduino UNO, STM32...) tương ứng với dự án

---

### Bước 3: Cài đặt thư viện cần thiết 👉 Arduino IDE

Vào Arduino IDE > Library Manager và tìm:
Cài qua **Library Manager**:
- `ERa`
- `Adafruit Fingerprint Sensor`
- `LiquidCrystal_I2C` (by johnrickman)

### Bước 4: Kết nối phần cứng

Ví dụ với dự án **ESP32 + cảm biến vân tay + LCD + relay**:

| Thiết bị               | ESP32 GPIO |
|------------------------|------------|
| Cảm biến vân tay RX    | GPIO 17    |
| Cảm biến vân tay TX    | GPIO 16    |
| Relay                  | GPIO 18    |
| Buzzer                 | GPIO 19    |
| LCD SDA                | GPIO 21    |
| LCD SCL                | GPIO 22    |

> ⚠️ Lưu ý chọn đúng chân theo sơ đồ và có trở pull-up nếu cần.

---

### Bước 5: Cấu hình WiFi & đường dẫn Google Script (nếu có)

Trong code có đoạn:

```cpp
const char ssid[] = "your_wifi_name";
const char pass[] = "your_password";

const char *GOOGLE_SCRIPT_PATH = "/macros/s/AKfycb.../exec";
```

- Điền thông tin WiFi của bạn
- Tạo Google Apps Script nếu muốn log dữ liệu lên Google Sheets

Tạo file mới tại [script.google.com](https://script.google.com), dán đoạn code sau và triển khai nó dưới dạng "Web App":

```javascript
/**
 * Hàm xử lý POST request từ ESP32
 */
function doPost(e) {
  try {
    // 1. Parse payload JSON
    var data = JSON.parse(e.postData.contents);
    var id = data.id;
    var time = data.time;
    var date = data.date;

    // 2. Mở sheet đầu tiên trong workbook
    var sheet = SpreadsheetApp.getActiveSpreadsheet().getSheets()[0];

    // 3. Tạo timestamp tại server (optional)
    var timestamp = new Date();

    // 4. Ghi một dòng mới
    sheet.appendRow([timestamp, id, time, date]);

    // 5. Trả về thành công
    return ContentService.createTextOutput(
      JSON.stringify({ status: "success" })
    ).setMimeType(ContentService.MimeType.JSON);
  } catch (error) {
    // Nếu có lỗi, trả về lỗi
    return ContentService.createTextOutput(
      JSON.stringify({ status: "error", message: error })
    ).setMimeType(ContentService.MimeType.JSON);
  }
}
```

> ⚠️ Khi deploy, chọn **"Anyone"** để thiết bị có thể gửi dữ liệu tới script.

---

### Bước 6: Nạp chương trình & test

- Kết nối ESP32 qua USB
- Chọn đúng cổng COM
- Nhấn **Upload** và **mở Serial Monitor**
- Làm theo hướng dẫn hiển thị trên LCD hoặc Serial

---

## 📂 FREE DOWNLOAD – Bộ sưu tập dự án IoT

Mình có một **Bộ Sưu Tập Dự Án IoT - AI** cực kỳ hữu ích:

- 📚 Dành cho sinh viên làm đồ án tốt nghiệp
- 💾 Bao gồm tài liệu mô tả, code, và sơ đồ mạch
- 🔓 **FREE SHARE - KHÔNG BÁN**

📬 **Inbox mình nếu bạn cần link tải về** – mình sẵn sàng chia sẻ!  
📨 Zalo / Facebook / Gmail đều được.
[Facebook](https://www.facebook.com/quan.leminh.146069/)
---

## 🎓 Tutorials Miễn Phí

Học hoàn toàn **miễn phí** với các video hướng dẫn thực hành:

👉 [YouTube Channel: Minh Quân - IoT & AI Projects](https://www.youtube.com/@minhquan711)

- ESP32 từ cơ bản đến nâng cao
- Kết nối IoT: Google Sheets, Firebase, Blynk, MQTT
- Nhận dạng vân tay, khuôn mặt, camera AI, v.v...

---

## ❤️ Cảm ơn bạn!

> Nếu thấy hữu ích, hãy **Star ⭐** repo này và **Subscribe 🔔** kênh YouTube để ủng hộ mình nhé!

📫 Mọi phản hồi, góp ý hoặc câu hỏi: mình luôn sẵn sàng hỗ trợ!
