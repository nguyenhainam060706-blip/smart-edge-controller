#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Khởi tạo và bắt đầu kết nối MQTT Client
 * @param broker_uri Đường dẫn broker, ví dụ: "mqtt://broker.hivemq.com:1883"
 */
void mqtt_app_start(const char *broker_uri);

/**
 * @brief Gửi dữ liệu (Publish) lên một Topic
 * @param topic Tên topic cần gửi
 * @param data Chuỗi dữ liệu (thường là chuỗi JSON)
 * @param qos Cấp độ QoS (0, 1, hoặc 2)
 * @param retain 1 nếu muốn giữ lại tin nhắn trên broker, 0 nếu không
 */
esp_err_t mqtt_publish(const char *topic, const char *data, int qos, int retain);

/**
 * @brief Đăng ký (Subscribe) nhận dữ liệu từ một Topic
 * @param topic Tên topic cần theo dõi
 * @param qos Cấp độ QoS (0, 1, hoặc 2)
 */
esp_err_t mqtt_subscribe(const char *topic, int qos);

/**
 * @brief Kiểm tra xem MQTT đã kết nối thành công tới Broker chưa
 */
bool mqtt_is_connected(void);

#endif // MQTT_MANAGER_H