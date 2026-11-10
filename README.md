# HỆ THỐNG GIÁM SÁT SỨC KHỎE VÀ MÔI TRƯỜNG PHÒNG BỆNH TỪ XA QUA MQTT & NODE-RED

## 1. Tên đề tài
Thiết kế hệ thống IoT giám sát nhịp tim và điều kiện môi trường phòng bệnh (Nhiệt độ, độ ẩm) thời gian thực ứng dụng Node-RED và giao thức MQTT.

## 2. Đặt bài toán
- Bối cảnh thực tế: Việc chăm sóc sức khỏe người cao tuổi hoặc bệnh nhân tim mạch tại nhà đang là nhu cầu rất lớn. Các sự cố tim mạch thường có mối liên hệ mật thiết với môi trường xung quanh (ví dụ: trời quá lạnh dễ gây đột quỵ, hoặc độ ẩm quá cao gây khó thở). Tuy nhiên, các thiết bị y tế hiện nay thường phân mảnh (máy đo nhịp tim riêng, nhiệt kế phòng riêng) và thiếu tính năng cảnh báo từ xa cho người thân đi làm.
- Nguồn gốc ý tưởng & Cảm hứng: Đề tài lấy cảm hứng từ mô hình "Phòng bệnh thông minh" (Smart Healthcare Room). Hệ thống không chỉ theo dõi chỉ số  Nhịp tim - (BPM) mà còn giám sát không gian sống (Nhiệt độ, độ ẩm phòng). Bằng việc kết hợp ESP32, MQTT và Node-RED, thiết bị sẽ phát tín hiệu báo động tại chỗ, đồng thời gửi đồ thị trực quan và tin nhắn khẩn cấp qua internet đến người nhà khi phát hiện bất kỳ thông số nào vượt ngưỡng an toàn.

## 3. Nội dung chính & Điểm sáng tạo
Hệ thống giải quyết bài toán bằng kiến trúc IoT toàn diện:
* Thu thập dữ liệu đa luồng: 
  - Đọc tín hiệu quang thể tích (PPG) từ MAX30102, qua bộ lọc để tính nhịp tim (BPM).
  - Đọc dữ liệu môi trường từ cảm biến DHT22 (Nhiệt độ, Độ ẩm không khí).
* Hiển thị & cảnh báo tại chỗ: Màn hình OLED SSD1306 liên tục cập nhật 3 thông số: BPM, Temp (*C), Humidity (%). Khi nhịp tim bất thường hoặc phòng quá nóng/lạnh, vi điều khiển lập tức kích hoạt còi (Buzzer) và đèn LED nháy sáng.
* Giao tiếp IoT (MQTT): ESP32 được lập trình trên PlatformIO/Arduino, hoạt động như một MQTT Client, đóng gói dữ liệu JSON và publish liên tục lên một MQTT Broker online.
* Trực quan hóa & Điều khiển (Node-RED): 
  - Xây dựng Node-RED Dashboard hiển thị 3 biểu đồ (Gauge & Chart) cho nhịp tim, nhiệt độ và độ ẩm.
  - Giao diện cho phép người dùng thay đổi ngưỡng cảnh báo (Threshold) từ xa, gửi lệnh điều khiển ngược lại ESP32.
  - Sáng tạo: Tích hợp logic tự động gửi tin nhắn cảnh báo qua tin nhắn SMS và gmail cho người nhà khi các chỉ số rơi vào vùng nguy hiểm.

## 4. Sơ đồ khối dự kiến
Hệ thống được thiết kế theo cấu trúc tập trung, bao gồm 4 khối chính:

* **[Khối Xử lý trung tâm]:** Mạch phát triển ESP32 Board.
* **[Khối Thu thập tín hiệu]:** 
  - Cảm biến MAX30102 (Đo nhịp tim, giao tiếp I2C).
  - Cảm biến DHT22 (Đo nhiệt độ, độ ẩm phòng, giao tiếp 1-Wire Digital).
* **[Khối Hiển thị & cảnh báo]:** 
  - Màn hình OLED SSD1306 (Giao tiếp chung bus I2C).
  - Còi báo động (Active Buzzer) & Đèn LED cảnh báo.
* **[Khối Giám sát IoT (Internet)]:** 
  - Giao thức truyền tải: MQTT qua mạng WiFi.
  - Server xử lý: Node-RED.
  - Giao diện người dùng cuối: Node-RED Dashboard (trên PC/Mobile) & Ứng dụng.

  ```mermaid
graph TD
    MAX[Cảm biến Nhịp tim MAX30102] -->|I2C| ESP((ESP32 Dev Board))
    DHT[Cảm biến Môi trường DHT22] -->|1-Wire| ESP
    
    ESP -->|I2C| OLED[Màn hình OLED SSD1306]
    ESP -->|Digital| BUZZ[Còi báo động Buzzer]
    ESP -->|Digital| LED[Đèn LED cảnh báo]
    
    ESP <-->|WiFi / MQTT| MQTT{MQTT Broker Online}
    MQTT <--> NODE[Node-RED Server]
    
    NODE --> DASH[Node-RED Dashboard]
    NODE --> TELE[Cảnh báo Telegram/Email]

    classDef default fill:#1f2937,stroke:#3b82f6,stroke-width:2px,color:#fff;
    classDef esp fill:#d97706,stroke:#fff,stroke-width:2px,color:#fff;
    classDef broker fill:#0284c7,stroke:#fff,stroke-width:2px,color:#fff;
    class ESP esp;
    class MQTT,NODE broker;


## 5. Danh sách phần cứng dự kiến

1. ESP32 CP2102 (Nhân xử lý chính).
2. Module MAX30102 (Cảm biến nhịp tim & SpO2).
3. Module DHT22 (Cảm biến nhiệt độ, độ ẩm).
4. Màn hình OLED SSD1306 0.96 inch.
5. Còi chip (Active Buzzer 5V) và Đèn LED.
6. Breadboard, dây cắm Jumper và cáp nguồn Micro-USB.


## 6. Hình ảnh phần cứng dự kiến

**Mạch xử lý trung tâm ESP32:**
![ESP32](images/ESP32_CP2102.webp)

**Cảm biến nhịp tim MAX30102:**
![MAX30102](images/MAX30102.jpg)

**Cảm biến môi trường DHT22:**
![DHT22](images/DHT22.webp)

**Màn hình OLED 0.96 inch:**
![OLED 0.96](images/OLED_0.96.jpg)

**Các linh kiện phụ trợ (Còi, LED, Breadboard):**
![Buzzer](images/BUZZ.jpg)
![LED](images/LED.webp)
![Breadboard](images/Breadboard.jpg)
![Micro_B](images/MICRO_B.png)

