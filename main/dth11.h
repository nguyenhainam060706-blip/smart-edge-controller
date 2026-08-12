#ifndef DHT11_H
#define DHT11_H

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize DHT11 sensor
 *
 * @param data_gpio GPIO connected to DHT11 DATA pin
 *
 * @return ESP_OK on success
 */
esp_err_t dht11_init(gpio_num_t data_gpio);


/**
 * @brief Read temperature and humidity from DHT11
 *
 * @param temperature Pointer to temperature value in Celsius
 * @param humidity Pointer to humidity value in percent
 *
 * @return
 *      ESP_OK                  - Read successful
 *      ESP_ERR_INVALID_ARG     - Invalid argument
 *      ESP_ERR_TIMEOUT         - Sensor timeout
 *      ESP_ERR_INVALID_RESPONSE- Invalid checksum/data
 */
esp_err_t dht11_read(
    float *temperature,
    float *humidity
);

#ifdef __cplusplus
}
#endif

#endif /* DHT11_H */