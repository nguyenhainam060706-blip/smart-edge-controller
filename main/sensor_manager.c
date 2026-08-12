#include "sensor_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h" // Dùng để tạo dữ liệu giả lập test code

static const char *TAG = "SENSOR_MANAGER";

typedef struct
{
    QueueHandle_t data_queue;
    uint32_t interval_ms;
} sensor_task_config_t;

static sensor_task_config_t s_task_config;

// --------------------------------------------------------------------------
// KHỞI TẠO PHẦN CỨNG
// --------------------------------------------------------------------------
esp_err_t sensor_manager_init(void)
{
    ESP_LOGI(TAG, "Đang khởi tạo ngoại vi cảm biến (I2C/GPIO)...");

    // TODO: Viết code khởi tạo I2C Driver hoặc GPIO tại đây nếu dùng cảm biến thật
    // Ví dụ: i2c_param_config(...); i2c_driver_install(...);

    ESP_LOGI(TAG, "Khởi tạo phần cứng cảm biến thành công!");
    return ESP_OK;
}

// --------------------------------------------------------------------------
// HÀM ĐỌC DỮ LIỆU CẢM BIẾN
// --------------------------------------------------------------------------
esp_err_t sensor_read_data(sensor_data_t *out_data)
{
    if (out_data == NULL)
        return ESP_ERR_INVALID_ARG;

    /* TODO: Thay đoạn giả lập bên dưới bằng code đọc cảm biến thực tế của bạn:
       - Nếu dùng DHT22: Gọi dht_read_data(...)
       - Nếu dùng BME280: Gọi bme280_read(...)
    */

    // --- MÃ GIẢ LẬP DỮ LIỆU ĐỂ TEST FREERTOS TASK & QUEUE ---
    out_data->temperature = 25.0f + (float)(esp_random() % 100) / 10.0f; // 25.0 - 35.0 °C
    out_data->humidity = 60.0f + (float)(esp_random() % 200) / 10.0f;    // 60.0 - 80.0 %
    out_data->is_valid = true;

    return ESP_OK;
}

// --------------------------------------------------------------------------
// FREERTOS TASK ĐỌC ĐỊNH KỲ
// --------------------------------------------------------------------------
static void sensor_task(void *pvParameters)
{
    sensor_task_config_t *cfg = (sensor_task_config_t *)pvParameters;
    sensor_data_t data;

    ESP_LOGI(TAG, "Sensor Task đã khởi chạy (Chu kỳ: %lu ms)", cfg->interval_ms);

    while (1)
    {
        if (sensor_read_data(&data) == ESP_OK && data.is_valid)
        {
            ESP_LOGI(TAG, "Đọc cảm biến -> Temp: %.1f°C | Hum: %.1f%%", data.temperature, data.humidity);

            // Đẩy dữ liệu vào Queue (nếu Queue đầy thì đợi tối đa 100ms)
            if (cfg->data_queue != NULL)
            {
                if (xQueueSend(cfg->data_queue, &data, pdMS_TO_TICKS(100)) != pdPASS)
                {
                    ESP_LOGW(TAG, "Queue đã đầy! Bỏ qua mẫu dữ liệu này.");
                }
            }
        }
        else
        {
            ESP_LOGE(TAG, "Đọc cảm biến thất bại!");
        }

        // Tạm dừng Task đúng khoảng thời gian cấu hình
        vTaskDelay(pdMS_TO_TICKS(cfg->interval_ms));
    }
}

void sensor_manager_start_task(QueueHandle_t out_queue, uint32_t interval_ms)
{
    s_task_config.data_queue = out_queue;
    s_task_config.interval_ms = interval_ms;

    // Tạo Task chạy trên Nhân 1 (Core 1), để dành Nhân 0 cho Wi-Fi/Bluetooth
    xTaskCreatePinnedToCore(
        sensor_task,
        "sensor_task",
        3072,           // Stack size (bytes)
        &s_task_config, // Tham số truyền vào task
        5,              // Độ ưu tiên (Priority)
        NULL,           // Task handle
        1               // Chạy ở Core 1
    );
}