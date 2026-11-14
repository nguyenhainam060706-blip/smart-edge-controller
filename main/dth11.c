#include "dth11.h"
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

static const char *TAG = "DHT11";

#define DHT11_START_LOW_US        20000  // Giữ thấp >= 18ms
#define DHT11_START_RELEASE_US    30
#define DHT11_RESPONSE_TIMEOUT_US 100
#define DHT11_BIT_TIMEOUT_US      100
#define DHT11_DATA_BITS           40

static gpio_num_t s_data_gpio = GPIO_NUM_NC;
static bool s_initialized = false;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static inline void delay_us(uint32_t us)
{
    int64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < (int64_t)us)
    {
    }
}

// Trả về số us đã chờ, hoặc -1 nếu timeout
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
        .mode = GPIO_MODE_INPUT_OUTPUT_OD, // Open-drain: ghi 0 = kéo thấp, ghi 1 = thả nổi
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO configuration failed: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_set_level(s_data_gpio, 1);
    s_initialized = true;

    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d (real hardware mode)", s_data_gpio);
    return ESP_OK;
}

static esp_err_t dht11_read_raw(uint8_t data[5])
{
    memset(data, 0, 5);
    esp_err_t err = ESP_OK;

    // 1. Start signal: kéo thấp >= 18ms (NGOÀI vùng critical để không rớt WiFi)
    gpio_set_level(s_data_gpio, 0);
    delay_us(DHT11_START_LOW_US);

    // 2. Từ lúc thả chân, timing rất nhạy nên khóa ngắt trên core này (~5ms)
    portENTER_CRITICAL(&s_mux);

    gpio_set_level(s_data_gpio, 1); // thả chân (open-drain), không cần đổi direction
    delay_us(DHT11_START_RELEASE_US);

    if (wait_for_level(0, DHT11_RESPONSE_TIMEOUT_US) < 0)
    {
        err = ESP_ERR_TIMEOUT;
        goto done;
    }
    if (wait_for_level(1, DHT11_RESPONSE_TIMEOUT_US) < 0)
    {
        err = ESP_ERR_TIMEOUT;
        goto done;
    }
    if (wait_for_level(0, DHT11_RESPONSE_TIMEOUT_US) < 0)
    {
        err = ESP_ERR_TIMEOUT;
        goto done;
    }

    // 3. Đọc 40 bit: ~50us thấp, rồi cao ~27us (bit 0) hoặc ~70us (bit 1)
    for (int i = 0; i < DHT11_DATA_BITS; i++)
    {
        if (wait_for_level(1, DHT11_BIT_TIMEOUT_US) < 0)
        {
            err = ESP_ERR_TIMEOUT;
            goto done;
        }
        int high_us = wait_for_level(0, DHT11_BIT_TIMEOUT_US);
        if (high_us < 0)
        {
            err = ESP_ERR_TIMEOUT;
            goto done;
        }
        data[i / 8] <<= 1;
        if (high_us > 40)
        {
            data[i / 8] |= 1;
        }
    }

done:
    portEXIT_CRITICAL(&s_mux);
    gpio_set_level(s_data_gpio, 1); // trả bus về idle

    if (err == ESP_ERR_TIMEOUT)
    {
        ESP_LOGW(TAG, "Timeout - kiem tra day DATA, nguon 3.3V va dien tro keo len");
    }
    return err;
}

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

    uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4])
    {
        ESP_LOGW(TAG, "Checksum mismatch: nhan 0x%02X, tinh duoc 0x%02X", data[4], checksum);
        return ESP_ERR_INVALID_CRC;
    }

    *humidity = (float)data[0] + (float)data[1] / 10.0f;
    *temperature = (float)data[2] + (float)data[3] / 10.0f;

    ESP_LOGI(TAG, "Temp: %.1f C | Humidity: %.1f %%", *temperature, *humidity);
    return ESP_OK;
}