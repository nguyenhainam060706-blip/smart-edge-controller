#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"

// Khai báo prototype cho hàm khởi tạo Wi-Fi của bạn
// Khai báo hàm nhận 2 tham số: ssid và password
void app_wifi_init(const char *ssid, const char *password);

#endif