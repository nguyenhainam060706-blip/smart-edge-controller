#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>

// Struct chứa toàn bộ dữ liệu cảm biến
typedef struct
{
    float temperature; // Nhiệt độ (°C)
    float humidity;    // Độ ẩm (%)
    bool is_valid;     // Trạng thái đọc (true: thành công, false: lỗi)
} sensor_data_t;

/**
 * @brief Khởi tạo phần cứng cảm biến (I2C/GPIO/ADC...)
 */
esp_err_t sensor_manager_init(void);

/**
 * @brief Tạo FreeRTOS Task tự động đọc cảm biến theo chu kỳ và đẩy vào Queue
 *
 * @param out_queue Handle của Queue dùng để chứa dữ liệu đầu ra
 * @param interval_ms Chu kỳ đọc cảm biến (tính bằng mili-giây)
 */
void sensor_manager_start_task(QueueHandle_t out_queue, uint32_t interval_ms);

/**
 * @brief Đọc cảm biến trực tiếp 1 lần (dùng nếu không muốn chạy Task)
 */
esp_err_t sensor_read_data(sensor_data_t *out_data);

#endif // SENSOR_MANAGER_H