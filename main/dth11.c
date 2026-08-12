#include "dht11.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "driver/gpio.h"


/* ============================================================
 * CONFIGURATION
 * ============================================================ */

static const char *TAG = "DHT11";

/*
 * DHT11 timing is very sensitive.
 *
 * These values are in microseconds.
 */

#define DHT11_START_LOW_US       18000
#define DHT11_START_RELEASE_US   40

#define DHT11_RESPONSE_TIMEOUT_US 100

#define DHT11_BIT_TIMEOUT_US      100

#define DHT11_DATA_BITS           40


/* ============================================================
 * PRIVATE VARIABLES
 * ============================================================ */

static gpio_num_t s_data_gpio = GPIO_NUM_NC;

static bool s_initialized = false;


/* ============================================================
 * MICROSECOND DELAY
 * ============================================================ */

static void delay_us(uint32_t us)
{
    uint64_t start = esp_timer_get_time();

    while ((esp_timer_get_time() - start) < us)
    {
        /*
         * Busy wait.
         *
         * DHT11 requires microsecond-level timing,
         * therefore vTaskDelay() cannot be used here.
         */
    }
}


/* ============================================================
 * WAIT FOR GPIO LEVEL
 * ============================================================ */

/**
 * Wait until GPIO reaches expected level.
 *
 * @param level         Expected GPIO level
 * @param timeout_us    Timeout in microseconds
 *
 * @return Duration waited in microseconds
 *         -1 if timeout
 */
static int wait_for_level(
    int level,
    uint32_t timeout_us
)
{
    uint64_t start = esp_timer_get_time();

    while (gpio_get_level(s_data_gpio) != level)
    {
        if ((esp_timer_get_time() - start) >= timeout_us)
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
    /*
     * Check GPIO
     */

    if (data_gpio < 0)
    {
        ESP_LOGE(
            TAG,
            "Invalid GPIO"
        );

        return ESP_ERR_INVALID_ARG;
    }


    /*
     * Save GPIO
     */

    s_data_gpio = data_gpio;


    /*
     * Configure GPIO
     *
     * DHT11 uses single-wire bidirectional DATA.
     *
     * Open-drain is used so MCU can pull the line LOW
     * while allowing the sensor to release the line HIGH.
     */

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_data_gpio),

        .mode = GPIO_MODE_INPUT_OUTPUT_OD,

        .pull_up_en = GPIO_PULLUP_ENABLE,

        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE
    };


    esp_err_t ret = gpio_config(&io_conf);

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "GPIO configuration failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }


    /*
     * Keep DATA HIGH while idle.
     */

    gpio_set_level(s_data_gpio, 1);

    s_initialized = true;


    ESP_LOGI(
        TAG,
        "DHT11 initialized on GPIO %d",
        s_data_gpio
    );


    return ESP_OK;
}


/* ============================================================
 * READ 40 BITS
 * ============================================================ */

static esp_err_t dht11_read_raw(
    uint8_t data[5]
)
{
    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }


    /*
     * Clear data buffer
     */

    for (int i = 0; i < 5; i++)
    {
        data[i] = 0;
    }


    /* ========================================================
     * STEP 1
     * MCU sends START signal
     * ======================================================== */

    /*
     * Pull DATA LOW for at least 18 ms.
     */

    gpio_set_direction(
        s_data_gpio,
        GPIO_MODE_OUTPUT_OD
    );

    gpio_set_level(
        s_data_gpio,
        0
    );

    delay_us(
        DHT11_START_LOW_US
    );


    /*
     * Release DATA line.
     */

    gpio_set_level(
        s_data_gpio,
        1
    );

    delay_us(
        DHT11_START_RELEASE_US );


    /*
     * Switch to input.
     */

    gpio_set_direction(
        s_data_gpio,
        GPIO_MODE_INPUT
    );


    /* ========================================================
     * STEP 2
     * Wait for DHT11 response
     * ======================================================== */

    /*
     * DHT11 response:
     *
     * LOW  ~80 us
     * HIGH ~80 us
     */

    if (
        wait_for_level(
            0,
            DHT11_RESPONSE_TIMEOUT_US
        ) < 0
    )
    {
        ESP_LOGW(
            TAG,
            "Timeout waiting for DHT11 response LOW"
        );

        return ESP_ERR_TIMEOUT;
    }


    /*
     * Wait for response HIGH
     */

    if (
        wait_for_level(
            1,
            DHT11_RESPONSE_TIMEOUT_US
        ) < 0
    )
    {
        ESP_LOGW(
            TAG,
            "Timeout waiting for DHT11 response HIGH"
        );

        return ESP_ERR_TIMEOUT;
    }


    /*
     * Wait until DHT11 starts sending data.
     *
     * The line goes LOW before each bit.
     */

    if (
        wait_for_level(
            0,
            DHT11_RESPONSE_TIMEOUT_US
        ) < 0
    )
    {
        ESP_LOGW(
            TAG,
            "Timeout waiting for first data bit"
        );

        return ESP_ERR_TIMEOUT;
    }


    /* ========================================================
     * STEP 3
     * Read 40 bits
     * ======================================================== */

    for (int bit = 0; bit < DHT11_DATA_BITS; bit++)
    {
        /*
         * Each bit starts with LOW ~50 us.
         *
         * Wait for HIGH.
         */

        if (
            wait_for_level(
                1,
                DHT11_BIT_TIMEOUT_US
            ) < 0
        )
        {
            ESP_LOGW(
                TAG,
                "Timeout waiting for bit %d HIGH",
                bit
            );

            return ESP_ERR_TIMEOUT;
        }


        /*
         * DHT11 encodes bit value using HIGH duration:
         *
         * ~26-28 us -> 0
         * ~70 us     -> 1
         *
         * Wait approximately 40 us.
         */

        delay_us(40);


        /*
         * If line is still HIGH after 40 us,
         * the bit is interpreted as 1.
         *
         * Otherwise it is 0.
         */

        int level = gpio_get_level(
            s_data_gpio
        );


        /*
         * Shift current byte left.
         */

        data[bit / 8] <<= 1;


        if (level == 1)
        {
            data[bit / 8] |= 1;
        }


        /*
         * Wait until DHT11 pulls line LOW
         * before next bit.
         */

        if (
            wait_for_level(
                0,
                DHT11_BIT_TIMEOUT_US
            ) < 0
        )
        {
            /*
             * Last bit may already have finished.
             */

            if (bit != DHT11_DATA_BITS - 1)
            {
                ESP_LOGW(
                    TAG,
                    "Timeout waiting for next bit"
                );

                return ESP_ERR_TIMEOUT;
            }
        }
    }


    return ESP_OK;
}


/* ============================================================
 * READ SENSOR
 * ============================================================ */

esp_err_t dht11_read(
    float *temperature,
    float *humidity
)
{
    /*
     * Check parameters.
     */

    if (
        temperature == NULL ||
        humidity == NULL
    )
    {
        return ESP_ERR_INVALID_ARG;
    }


    /*
     * Check initialization.
     */

    if (!s_initialized)
    {
        ESP_LOGE(
            TAG,
            "DHT11 is not initialized"
        );

        return ESP_ERR_INVALID_STATE;
    }


    /*
     * Raw DHT11 data:
     *
     * data[0] = humidity integer
     * data[1] = humidity decimal
     * data[2] = temperature integer
     * data[3] = temperature decimal
     * data[4] = checksum
     */

    uint8_t data[5];


    esp_err_t ret = dht11_read_raw(data);

    if (ret != ESP_OK)
    {
        return ret;
    }


    /* ========================================================
     * CHECKSUM
     * ======================================================== */

    uint8_t checksum =
        data[0] +
        data[1] +
        data[2] +
        data[3];


    if (checksum != data[4])
    {
        ESP_LOGW(
            TAG,
            "Checksum error: calculated=0x%02X received=0x%02X",
            checksum,
            data[4]
        );

        return ESP_ERR_INVALID_RESPONSE;
    }


    /* ========================================================
     * CONVERT DATA
     * ======================================================== */

    *humidity =
        (float)data[0] +
        ((float)data[1] / 10.0f);


    *temperature =
        (float)data[2] +
        ((float)data[3] / 10.0f);


    ESP_LOGD(
        TAG,
        "Raw: %02X %02X %02X %02X %02X",
        data[0],
        data[1],
        data[2],
        data[3],
        data[4]
    );


    return ESP_OK;
}