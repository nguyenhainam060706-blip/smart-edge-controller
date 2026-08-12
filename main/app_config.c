#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "APP_CONFIG";

void app_config_print_summary(void)
{
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "      ESP32 IOT GATEWAY SYSTEM      ");
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "Wi-Fi SSID      : %s", CONFIG_WIFI_SSID);
    ESP_LOGI(TAG, "MQTT Broker     : %s", CONFIG_MQTT_BROKER_URI);
    ESP_LOGI(TAG, "MQTT Topic      : %s", CONFIG_MQTT_TOPIC_DATA);
    ESP_LOGI(TAG, "Chu kỳ đọc sensor: %d ms", CONFIG_SENSOR_READ_INTERVAL_MS);
    ESP_LOGI(TAG, "====================================");
}