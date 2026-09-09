#include "dht11.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

static const char *TAG = "DHT11";

#define DHT11_START_LOW_US 18000      // Giữ mức thấp >= 18ms để đánh thức cảm biến
#define DHT11_START_RELEASE_US 30     // Thả lên mức cao trước khi chuyển sang input
#define DHT11_RESPONSE_TIMEOUT_US 100 // Timeout chờ cảm biến phản hồi
#define DHT11_BIT_TIMEOUT_US 100      // Timeout cho mỗi cạnh xung khi đọc bit
#define DHT11_DATA_BITS 40            // 5 byte: humidity(2) + temp(2) + checksum(1)

/* ============================================================
 * PRIVATE VARIABLES
 * ============================================================ */

static gpio_num_t s_data_gpio = GPIO_NUM_NC;
static bool s_initialized = false;

/* ============================================================
 * MICROSECOND DELAY
 * ============================================================ */

static inline void delay_us(uint32_t us)
{
    int64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < (int64_t)us)
    {
        // busy wait - cần chính xác nên không dùng vTaskDelay ở đây
    }
}

/* ============================================================
 * WAIT FOR GPIO LEVEL
 * Trả về số us đã chờ, hoặc -1 nếu timeout
 * ============================================================ */

static int wait_for_level(int level, uint32_t timeout_us)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(s_data_gpio) != level)
    {
        if ((esp_timer_get_time() - start) >= (int64_t)timeout_us)
        {
            return -1;
        }
    }
    return (int)(esp_timer_get_time() - start);
}

/* ============================================================
 * INITIALIZATION
 * ============================================================ */

esp_err_t dht11_init(gpio_num_t data_gpio)
{
    if (data_gpio < 0)
    {
        ESP_LOGE(TAG, "Invalid GPIO");
        return ESP_ERR_INVALID_ARG;
    }

    s_data_gpio = data_gpio;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_data_gpio),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD, // Open-drain: cảm biến DHT11 kéo dây xuống thấp
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Cần pull-up (thường board đã có sẵn điện trở 4.7k-10k)
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO configuration failed: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_set_level(s_data_gpio, 1); // Giữ mức nghỉ (idle) là mức cao
    s_initialized = true;

    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d (real hardware mode)", s_data_gpio);

    return ESP_OK;
}

/* ============================================================
 * READ 40 BITS FROM SENSOR
 * ============================================================ */

static esp_err_t dht11_read_raw(uint8_t data[5])
{
    memset(data, 0, 5);

    // Tắt ngắt trong lúc bit-bang để đảm bảo timing chính xác (vài ms)
    portDISABLE_INTERRUPTS();

    // --- 1. Gửi tín hiệu start: kéo xuống thấp >= 18ms rồi thả lên ---
    gpio_set_direction(s_data_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_data_gpio, 0);
    delay_us(DHT11_START_LOW_US);
    gpio_set_level(s_data_gpio, 1);
    delay_us(DHT11_START_RELEASE_US);

    // Chuyển sang input để lắng nghe phản hồi từ cảm biến
    gpio_set_direction(s_data_gpio, GPIO_MODE_INPUT);

    // --- 2. Đợi cảm biến phản hồi: thấp ~80us rồi cao ~80us ---
    if (wait_for_level(0, DHT11_RESPONSE_TIMEOUT_US) < 0)
    {
        portENABLE_INTERRUPTS();
        ESP_LOGW(TAG, "No response from sensor (low phase) - kiem tra day/nguon");
        return ESP_ERR_TIMEOUT;
    }
    if (wait_for_level(1, DHT11_RESPONSE_TIMEOUT_US) < 0)
    {
        portENABLE_INTERRUPTS();
        ESP_LOGW(TAG, "No response from sensor (high phase)");
        return ESP_ERR_TIMEOUT;
    }
    if (wait_for_level(0, DHT11_RESPONSE_TIMEOUT_US) < 0)
    {
        portENABLE_INTERRUPTS();
        ESP_LOGW(TAG, "Sensor response ended unexpectedly");
        return ESP_ERR_TIMEOUT;
    }

    // --- 3. Đọc 40 bit dữ liệu ---
    // Mỗi bit: ~50us thấp (mở đầu), sau đó cao ~26-28us (bit=0) hoặc ~70us (bit=1)
    for (int i = 0; i < DHT11_DATA_BITS; i++)
    {
        if (wait_for_level(1, DHT11_BIT_TIMEOUT_US) < 0)
        {
            portENABLE_INTERRUPTS();
            ESP_LOGW(TAG, "Timeout cho bit start (bit %d)", i);
            return ESP_ERR_TIMEOUT;
        }

        int high_us = wait_for_level(0, DHT11_BIT_TIMEOUT_US);
        if (high_us < 0)
        {
            portENABLE_INTERRUPTS();
            ESP_LOGW(TAG, "Timeout cho bit end (bit %d)", i);
            return ESP_ERR_TIMEOUT;
        }

        data[i / 8] <<= 1;
        if (high_us > 40)
        { // > ~40us => bit = 1, ngược lại bit = 0
            data[i / 8] |= 1;
        }
    }

    portENABLE_INTERRUPTS();
    return ESP_OK;
}

/* ============================================================
 * ĐỌC SENSOR 
 * ============================================================ */

esp_err_t dht11_read(float *temperature, float *humidity)
{
    if (temperature == NULL || humidity == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized)
    {
        ESP_LOGE(TAG, "DHT11 is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[5] = {0};
    esp_err_t ret = dht11_read_raw(data);
    if (ret != ESP_OK)
    {
        return ret;
    }

    // --- Kiểm tra checksum ---
    uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4])
    {
        ESP_LOGW(TAG, "Checksum mismatch: nhan 0x%02X, tinh duoc 0x%02X", data[4], checksum);
        return ESP_ERR_INVALID_CRC;
    }

    // DHT11: byte nguyên là phần chính, byte thập phân thường = 0 (độ phân giải nguyên)
    *humidity = (float)data[0] + (float)data[1] / 10.0f;
    *temperature = (float)data[2] + (float)data[3] / 10.0f;

    ESP_LOGI(TAG, "Temp: %.1f C | Humidity: %.1f %%", *temperature, *humidity);

    return ESP_OK;
}