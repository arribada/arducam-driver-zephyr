/*
 * Copyright (c) 2022, Kumar Gala <galak@kernel.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/drivers/video.h>
// #include <zephyr/drivers/video/arducam_mega.h>

#include <zephyr/drivers/uart.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include "arducam_link.h"

int main(void)
{
	arducam_link_init();

	while (1) {
		arducam_link_work();
		k_msleep(1);
	}

	return 0;
}
