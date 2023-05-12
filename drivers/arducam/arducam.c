#define DT_DRV_COMPAT arducam

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <string.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

#include "arducam.h"

LOG_MODULE_REGISTER(arducam);

#define DT_DRV_COMPAT arducam_arducam
#define SPI_DEVICE_NAME "SPI_0"

static struct spi_config spi_cfg = {
    .frequency = SPI_DEFAULT_FREQUENCY,
    .operation = (SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8)),
    .slave = 0,
};

void spi_inital(void)
{
    struct device *spi_dev = device_get_binding(SPI_DEVICE_NAME);
    if (!spi_dev)
    {
        printk("Failed to get SPI device binding\n");
        return;
    }

    if (spi_config(spi_dev, &spi_cfg) != 0)
    {
        printk("Failed to configure SPI device\n");
        return;
    }

    printk("SPI driver initialized successfully\n");
}

void spi_transfer(uint8_t *tx_data, uint8_t *rx_data, size_t len)
{
    struct device *spi_dev = device_get_binding(SPI_DEVICE_NAME);
    if (!spi_dev)
    {
        printk("Failed to get SPI device binding\n");
        return;
    }

    struct spi_buf tx_buf = {
        .buf = tx_data,
        .len = len,
    };

    struct spi_buf_set tx_buf_set = {
        .buffers = &tx_buf,
        .count = 1,
    };

    struct spi_buf rx_buf = {
        .buf = rx_data,
        .len = len,
    };

    struct spi_buf_set rx_buf_set = {
        .buffers = &rx_buf,
        .count = 1,
    };

    if (spi_transceive(spi_dev, &spi_cfg, &tx_buf_set, &rx_buf_set) != 0)
    {
        printk("SPI transfer failed\n");
        return;
    }

    printk("SPI transfer completed successfully\n");
}

// /* Main instantiation matcro */
// #define ARDUCAM_DEFINE(inst)							\
// 	static struct pcf85063a_data pcf85063a_data_##inst = {			\
// 		.i2c = I2C_DT_SPEC_INST_GET(inst),				\
// 	};									\
// 	static const struct counter_config_info pcf85063_cfg_info_##inst = {	\
// 		.max_top_value = 0xff,						\
// 		.freq = 1,							\
// 		.channels = 1,							\
// 	};									\
// 	DEVICE_DT_INST_DEFINE(inst,						\
// 						  pcf85063a_init, NULL,                              \
// 						  &pcf85063a_data_##inst, &pcf85063_cfg_info_##inst, \
// 						  POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,             \
// 						  &pcf85063a_api);

// /* Create the struct device for every status "okay"*/
// DT_INST_FOREACH_STATUS_OKAY(PCF85063A_DEFINE)