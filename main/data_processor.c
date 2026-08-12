#include "data_processor.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "DATA_PROCESSOR";

// Định nghĩa các ngưỡng cảnh báo
#define TEMP_OVERHEAT_THRESHOLD 35.0f // Nhiệt độ > 35°C thì báo động
#define HUM_HIGH_THRESHOLD 75.0f      // Độ ẩm > 75% thì báo động

void data_processor_analyze(const sensor_data_t *input, processed_data_t *output)
{
    if (input == NULL || output == NULL)
        return;

    // 1. Sao chép dữ liệu thô
    output->raw_data = *input;

    // 2. Kiểm tra các ngưỡng cảnh báo
    output->is_overheat_alarm = (input->temperature > TEMP_OVERHEAT_THRESHOLD);
    output->is_high_humidity = (input->humidity > HUM_HIGH_THRESHOLD);

    if (output->is_overheat_alarm)
    {
        ESP_LOGW(TAG, "CẢNH BÁO: Nhiệt độ vượt ngưỡng (%.1f°C)", input->temperature);
    }
}

char *data_processor_to_json(const processed_data_t *data)
{
    if (data == NULL)
        return NULL;

    // Tạo đối tượng cJSON
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Lỗi tạo cJSON object!");
        return NULL;
    }

    // Thêm các trường dữ liệu
    cJSON_AddNumberToObject(root, "temp", data->raw_data.temperature);
    cJSON_AddNumberToObject(root, "hum", data->raw_data.humidity);

    // Thêm trạng thái cảnh báo vào JSON
    cJSON_AddBoolToObject(root, "overheat", data->is_overheat_alarm);
    cJSON_AddBoolToObject(root, "high_hum", data->is_high_humidity);

    // Chuyển đối tượng cJSON thành chuỗi (String) unformatted (không khoảng trắng thừa để tiết kiệm bộ nhớ)
    char *json_string = cJSON_PrintUnformatted(root);

    // Xóa đối tượng cJSON gốc để tránh rò rỉ bộ nhớ (Memory Leak)
    cJSON_Delete(root);

    return json_string; // Trả về con trỏ vùng nhớ RAM chứa chuỗi JSON
}