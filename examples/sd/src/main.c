/*
 * Copyright (c) 2022, Kumar Gala <galak@kernel.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/drivers/video.h>
#include <zephyr/drivers/uart.h>

#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs.h>
#include <ff.h>

#include <stdio.h>
#include <stdlib.h>

#include "sd.h"
#include "camera.h"

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

int main(void)
{
	/** SD card mount test **/
	if (mount_sd_card()) {
		printk("Failed to mount SD card\n");
		return -1;
	} else {
		printk("Successfully mounted SD card\n");
	}

	camera_init();

	printk("Attempting to capture a photo\n");

	take_picture();

	printk("Done writing to file\n");
	return 0;
}