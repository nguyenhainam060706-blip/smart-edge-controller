#ifndef DATA_PROCESSOR_H
#define DATA_PROCESSOR_H

#include "sensor_manager.h" // Để lấy struct sensor_data_t
#include "esp_err.h"
#include <stdbool.h>

// Struct chứa dữ liệu đã được xử lý kèm theo trạng thái cảnh báo
typedef struct
{
    sensor_data_t raw_data;
    bool is_overheat_alarm; // Cảnh báo quá nhiệt
    bool is_high_humidity;  // Cảnh báo độ ẩm cao
} processed_data_t;

/**
 * @brief Lọc và kiểm tra ngưỡng dữ liệu cảm biến
 * @param input Dữ liệu thô từ sensor_manager
 * @param output Dữ liệu đã xử lý kèm các cờ cảnh báo (output)
 */
void data_processor_analyze(const sensor_data_t *input, processed_data_t *output);

/**
 * @brief Đóng gói dữ liệu thành chuỗi JSON để gửi qua MQTT
 *
 * @param data Dữ liệu đã xử lý
 * @return char* Chuỗi JSON (LƯU Ý: Người gọi hàm phải tự free() bộ nhớ sau khi dùng)
 */
char *data_processor_to_json(const processed_data_t *data);

#endif // DATA_PROCESSOR_H