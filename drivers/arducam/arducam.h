#ifndef ARDUCAM_DRIVER_H
#define ARDUCAM_DRIVER_H

#include <stdint.h>
#include <stddef.h>

#define SPI_DEFAULT_FREQUENCY 1000000

void spi_init(void);
void spi_transfer(uint8_t *tx_data, uint8_t *rx_data, size_t len);

#endif /* ARDUCAM_DRIVER_H */
