```c
#include "sensor_manager.h"
#include "dht11.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "SENSOR_MANAGER";

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

typedef struct
{
    QueueHandle_t data_queue;
    uint32_t interval_ms;

} sensor_task_config_t;

static sensor_task_config_t s_task_config;


/* ============================================================
 * SENSOR INITIALIZATION
 * ============================================================ */

esp_err_t sensor_manager_init(void)
{
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "Initializing DHT11 sensor...");
    ESP_LOGI(TAG, "====================================");

    /*
     * DHT11 DATA pin
     *
     * Change GPIO_NUM_4 if your DHT11
     * is connected to another GPIO.
     */

    esp_err_t ret = dht11_init(GPIO_NUM_4);

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "DHT11 initialization failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ESP_LOGI(TAG, "DHT11 initialization successful");

    return ESP_OK;
}


/* ============================================================
 * READ SENSOR DATA
 * ============================================================ */

esp_err_t sensor_read_data(sensor_data_t *out_data)
{
    if (out_data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Reset data before reading
     */

    out_data->temperature = 0.0f;
    out_data->humidity = 0.0f;
    out_data->is_valid = false;


    /*
     * Read data from DHT11
     */

    esp_err_t ret = dht11_read(
        &out_data->temperature,
        &out_data->humidity
    );

    if (ret != ESP_OK)
    {
        ESP_LOGW(
            TAG,
            "DHT11 read failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }


    /*
     * Validate sensor data
     */

    if (
        out_data->temperature < 0.0f ||
        out_data->temperature > 50.0f
    )
    {
        ESP_LOGW(
            TAG,
            "Invalid temperature: %.1f C",
            out_data->temperature
        );

        return ESP_ERR_INVALID_RESPONSE;
    }

    if (
        out_data->humidity < 0.0f ||
        out_data->humidity > 100.0f
    )
    {
        ESP_LOGW(
            TAG,
            "Invalid humidity: %.1f %%",
            out_data->humidity
        );

        return ESP_ERR_INVALID_RESPONSE;
    }


    /*
     * Data is valid
     */

    out_data->is_valid = true;

    return ESP_OK;
}


/* ============================================================
 * SENSOR TASK
 * ============================================================ */

static void sensor_task(void *pvParameters)
{
    sensor_task_config_t *cfg =
        (sensor_task_config_t *)pvParameters;

    sensor_data_t data;

    ESP_LOGI(
        TAG,
        "Sensor Task started"
    );

    ESP_LOGI(
        TAG,
        "Sampling interval: %lu ms",
        cfg->interval_ms
    );


    while (1)
    {
        /*
         * Read DHT11
         */

        esp_err_t ret = sensor_read_data(&data);


        if (ret == ESP_OK && data.is_valid)
        {
            /*
             * Print sensor data
             */

            ESP_LOGI(
                TAG,
                "Sensor data -> Temp: %.1f C | Humidity: %.1f %%",
                data.temperature,
                data.humidity
            );


            /*
             * Send data to FreeRTOS Queue
             */

            if (cfg->data_queue != NULL)
            {
                BaseType_t queue_result =
                    xQueueSend(
                        cfg->data_queue,
                        &data,
                        pdMS_TO_TICKS(100)
                    );


                if (queue_result != pdPASS)
                {
                    ESP_LOGW(
                        TAG,
                        "Sensor queue is full, dropping sample"
                    );
                }
            }
        }
        else
        {
            ESP_LOGW(
                TAG,
                "Sensor data unavailable"
            );
        }


        /*
         * Wait until next reading
         *
         * DHT11 should not be read too frequently.
         */

        vTaskDelay(
            pdMS_TO_TICKS(cfg->interval_ms)
        );
    }
}


/* ============================================================
 * START SENSOR TASK
 * ============================================================ */

void sensor_manager_start_task(
    QueueHandle_t out_queue,
    uint32_t interval_ms
)
{
    /*
     * Save task configuration
     */

    s_task_config.data_queue = out_queue;
    s_task_config.interval_ms = interval_ms;


    /*
     * Protect against invalid interval
     */

    if (interval_ms < 1000)
    {
        ESP_LOGW(
            TAG,
            "Interval too small for DHT11. Using 2000 ms."
        );

        s_task_config.interval_ms = 2000;
    }


    /*
     * Create sensor task
     *
     * Core 1:
     * Sensor task runs independently from
     * Wi-Fi/network processing.
     */

    BaseType_t ret = xTaskCreatePinnedToCore(
        sensor_task,
        "sensor_task",
        3072,
        &s_task_config,
        5,
        NULL,
        1
    );


    if (ret != pdPASS)
    {
        ESP_LOGE(
            TAG,
            "Failed to create Sensor Task!"
        );
    }
    else
    {
        ESP_LOGI(
            TAG,
            "Sensor Task created successfully"
        );
    }
}
```
