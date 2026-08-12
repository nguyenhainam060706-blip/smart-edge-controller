#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "esp_err.h"

// --- CẤU HÌNH WI-FI ---
#define CONFIG_WIFI_SSID "TP-Link_677C"
#define CONFIG_WIFI_PASS " 77373076"

// --- CẤU HÌNH MQTT BROKER ---
#define CONFIG_MQTT_BROKER_URI "mqtt://broker.hivemq.com:1883"
#define CONFIG_MQTT_TOPIC_DATA "esp32/telemetry/data"

// --- CẤU HÌNH HỆ THỐNG & FREERTOS ---
#define CONFIG_SENSOR_READ_INTERVAL_MS 2000 // Đọc cảm biến mỗi 2000ms (2 giây)
#define CONFIG_QUEUE_SIZE 10                // Hàng đợi chứa tối đa 10 mẫu dữ liệu

// --- CẤU HÌNH PHẦN CỨNG GPIO ---
#define CONFIG_LED_STATUS_GPIO 2

/**
 * @brief In thông tin cấu hình hệ thống ra Terminal
 */
void app_config_print_summary(void);

#endif // APP_CONFIG_H