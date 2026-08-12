#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

// Nhúng tất cả các module đã viết vào main.c
#include "app_config.h"
#include "wifi.h"
#include "mqtt_manager.h"
#include "sensor_manager.h"
#include "data_processor.h"

static const char *TAG = "MAIN_APP";

// --------------------------------------------------------------------------
// TASK TRUNG GIAN: Nhận dữ liệu từ Queue -> Phân tích -> Tạo JSON -> Gửi MQTT
// --------------------------------------------------------------------------
static void processing_and_publish_task(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    sensor_data_t raw_sensor;
    processed_data_t processed;

    ESP_LOGI(TAG, "Processing & Publish Task đã sẵn sàng!");

    while (1)
    {
        // 1. Lấy dữ liệu thô từ Queue (Sensor Task đẩy sang)
        if (xQueueReceive(queue, &raw_sensor, portMAX_DELAY) == pdTRUE)
        {

            // 2. Lọc và kiểm tra cảnh báo (Data Processor)
            data_processor_analyze(&raw_sensor, &processed);

            // 3. Đóng gói dữ liệu thành chuỗi JSON
            char *json_payload = data_processor_to_json(&processed);

            if (json_payload != NULL)
            {
                // 4. Nếu MQTT đã sẵn sàng kết nối -> Gửi dữ liệu
                if (mqtt_is_connected())
                {
                    mqtt_publish(CONFIG_MQTT_TOPIC_DATA, json_payload, 0, 0);
                }
                else
                {
                    ESP_LOGW(TAG, "MQTT chưa sẵn sàng, tạm thời bỏ qua lượt gửi này.");
                }

                // 5. BẮT BUỘC: Giải phóng RAM cấp phát cho chuỗi JSON
                free(json_payload);
            }
        }
    }
}

// --------------------------------------------------------------------------
// HÀM CHÍNH (ENTRY POINT)
// --------------------------------------------------------------------------
void app_main(void)
{
    // 1. In tóm tắt cấu hình
    app_config_print_summary();

    // 2. Tạo Queue trung gian
    QueueHandle_t sensor_queue = xQueueCreate(CONFIG_QUEUE_SIZE, sizeof(sensor_data_t));
    if (sensor_queue == NULL)
    {
        ESP_LOGE(TAG, "Lỗi: Không đủ bộ nhớ để tạo Queue!");
        return;
    }

    // 3. Khởi tạo & Chạy Task đọc cảm biến (Core 1)
    sensor_manager_init();
    sensor_manager_start_task(sensor_queue, CONFIG_SENSOR_READ_INTERVAL_MS);

    // 4. Khởi tạo Wi-Fi kết nối Internet
    app_wifi_init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASS);

    // 5. Khởi tạo MQTT Client kết nối Broker
    mqtt_app_start(CONFIG_MQTT_BROKER_URI);

    // 6. Chạy Task xử lý & Publish MQTT
    xTaskCreatePinnedToCore(
        processing_and_publish_task,
        "proc_pub_task",
        4096,
        (void *)sensor_queue,
        5,
        NULL,
        1);

    ESP_LOGI(TAG, "Khởi động toàn bộ hệ thống thành công!");
}