# Thư viện MEO3 dành cho IDF 
Thư viện MEO3 viết trên ESP-IDF với gốc được viết bởi @lgthevinh [https://github.com/lgthevinh] trên PlatformIO với Arduino Framework và phiên bản dành cho ESP-IDF với target hiện tại là ESP32 được viết bởi tôi.
Hiện tại, thư viện này chưa được tối ưu tốt, tràn IRAM, chưa thực sự có test cụ thể - sẽ cập nhật trong thời gian sớm nhất.

# MEO 3 Arduino Library (ESP32) 
* MEO 3 Arduino là một bộ công cụ phát triển phần mềm (SDK) tối giản, hướng tới sản xuất thực tế, dùng để kết nối các thiết bị ESP32 với dịch vụ MEO Open Service thông qua giao thức MQTT. Bộ SDK này tập trung vào các đặc điểm:
* Các hàm gọi lại tính năng đơn giản (MeoFeatureCall): Giúp lập trình các phương thức xử lý tính năng một cách dễ dàng.
* Xuất bản sự kiện trọng lượng nhẹ (MeoEventPayload): Hỗ trợ gửi các dữ liệu sự kiện đi với cấu trúc tinh gọn, tối ưu tài nguyên.
* Tích hợp sẵn cấu hình qua BLE (Provisioning): Cho phép thiết lập thông tin Wi-Fi và thông tin định danh thiết bị thông qua Bluetooth Low Energy.
* Ghi nhật ký (Logging) rõ ràng: Đi kèm với các thẻ định danh gỡ lỗi (debug tags) có thể tùy chọn thêm vào.
