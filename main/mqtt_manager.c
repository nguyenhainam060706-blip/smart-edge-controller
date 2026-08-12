#include "mqtt_manager.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MQTT_MANAGER";
static esp_mqtt_client_handle_t s_client = NULL;
static bool s_is_connected = false;

// --------------------------------------------------------------------------
// HÀM XỬ LÝ SỰ KIỆN MQTT (Event Handler)
// --------------------------------------------------------------------------
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Kết nối MQTT Broker thành công!");
        s_is_connected = true;
        // Ví dụ: Đăng ký tự động một topic điều khiển khi vừa kết nối xong
        mqtt_subscribe("esp32/device/control", 0);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Mất kết nối MQTT Broker! Đang tự động thử lại...");
        s_is_connected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Đã Subscribe thành công topic, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "Đã Unsubscribe topic, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Đã Publish dữ liệu thành công, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "--- NHẬN ĐƯỢC DỮ LIỆU TỪ BROKER ---");
        // In ra Topic và Data nhận được
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);

        // TODO: Bạn có thể tách dữ liệu JSON hoặc đẩy vào FreeRTOS Queue tại đây
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Lỗi MQTT!");
        break;

    default:
        break;
    }
}

// --------------------------------------------------------------------------
// CÁC HÀM GIAO TIẾP
// --------------------------------------------------------------------------

void mqtt_app_start(const char *broker_uri)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    // Đăng ký event handler cho MQTT
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

esp_err_t mqtt_publish(const char *topic, const char *data, int qos, int retain)
{
    if (s_client == NULL || !s_is_connected)
    {
        ESP_LOGE(TAG, "Không thể Publish: Chưa kết nối MQTT!");
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(s_client, topic, data, 0, qos, retain);
    if (msg_id == -1)
    {
        ESP_LOGE(TAG, "Publish thất bại!");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t mqtt_subscribe(const char *topic, int qos)
{
    if (s_client == NULL)
    {
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_subscribe(s_client, topic, qos);
    if (msg_id == -1)
    {
        ESP_LOGE(TAG, "Subscribe thất bại!");
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool mqtt_is_connected(void)
{
    return s_is_connected;
}