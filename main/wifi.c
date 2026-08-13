#include "wifi.h"      // Include header của chính nó
#include "nvs_flash.h" // Cho nvs_flash_init
#include "esp_netif.h" // Cho esp_netif_init
#include "esp_event.h" // Cho esp_event_loop_create_default
#include "esp_wifi.h"  // Cho các hàm esp_wifi_*
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WIFI_APP";

/* ============================================================
 * WIFI EVENT HANDLER
 * ============================================================ */
static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    // Khi Wi-Fi driver đã khởi động xong -> Ra lệnh kết nối
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    // Khi Wi-Fi bị ngắt kết nối (mất mạng, sai pass, cục wifi reset) -> Tự động kết nối lại
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Disconnected from Wi-Fi, trying to reconnect...");
        esp_wifi_connect();
    }
    // Khi kết nối thành công và được cấp IP
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

/* ============================================================
 * WIFI INITIALIZATION
 * ============================================================ */
void app_wifi_init(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "Starting Wi-Fi Initialization...");

    // 1. Khởi tạo NVS Flash (Nơi lưu các thông số hiệu chuẩn của WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Khởi tạo TCP/IP Stack và Event Loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. Khởi tạo Wi-Fi Driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // -> BỔ SUNG: Đăng ký Event Handler cho sự kiện WIFI và IP
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    // 4. Cấu hình SSID và Password
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 5. Bắt đầu Wi-Fi (Event handler sẽ tự động gọi esp_wifi_connect khi Wi-Fi start xong)
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialization finished. Waiting for connection...");
}