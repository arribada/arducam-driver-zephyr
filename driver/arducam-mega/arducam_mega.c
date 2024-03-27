/**
 * The MIT License (MIT)
 *
 * Copyright 2021 Arducam Technology co., Ltd. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE
 *
 */

#define DT_DRV_COMPAT arducam_mega

#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/spi.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mega_camera);

#include "arducam_mega.h"

#define VIDEO_PATTERN_FPS 30

#define ARDUCHIP_FIFO   0x04 /* FIFO and I2C control */
#define ARDUCHIP_FIFO_2 0x07 /* FIFO and I2C control */

#define FIFO_CLEAR_ID_MASK 0x01
#define FIFO_START_MASK    0x02

#define ARDUCHIP_TRIG 0x44 /* Trigger source */
#define VSYNC_MASK    0x01
#define SHUTTER_MASK  0x02
#define CAP_DONE_MASK 0x04

#define FIFO_SIZE1 0x45 /* Camera write FIFO size[7:0] for burst to read */
#define FIFO_SIZE2 0x46 /* Camera write FIFO size[15:8] */
#define FIFO_SIZE3 0x47 /* Camera write FIFO size[18:16] */

#define BURST_FIFO_READ  0x3C /* Burst FIFO read operation */
#define SINGLE_FIFO_READ 0x3D /* Single FIFO read operation */

/* DSP register bank FF=0x00*/
#define CAM_REG_POWER_CONTROL                 0X02
#define CAM_REG_SENSOR_RESET                  0X07
#define CAM_REG_FORMAT                        0X20
#define CAM_REG_CAPTURE_RESOLUTION            0X21
#define CAM_REG_BRIGHTNESS_CONTROL            0X22
#define CAM_REG_CONTRAST_CONTROL              0X23
#define CAM_REG_SATURATION_CONTROL            0X24
#define CAM_REG_EV_CONTROL                    0X25
#define CAM_REG_WHILEBALANCE_CONTROL          0X26
#define CAM_REG_COLOR_EFFECT_CONTROL          0X27
#define CAM_REG_SHARPNESS_CONTROL             0X28
#define CAM_REG_AUTO_FOCUS_CONTROL            0X29
#define CAM_REG_IMAGE_QUALITY                 0x2A
#define CAM_REG_EXPOSURE_GAIN_WHILEBAL_ENABLE 0X30
#define CAM_REG_MANUAL_GAIN_BIT_9_8           0X31
#define CAM_REG_MANUAL_GAIN_BIT_7_0           0X32
#define CAM_REG_MANUAL_EXPOSURE_BIT_19_16     0X33
#define CAM_REG_MANUAL_EXPOSURE_BIT_15_8      0X34
#define CAM_REG_MANUAL_EXPOSURE_BIT_7_0       0X35
#define CAM_REG_BURST_FIFO_READ_OPERATION     0X3C
#define CAM_REG_SINGLE_FIFO_READ_OPERATION    0X3D
#define CAM_REG_SENSOR_ID                     0x40
#define CAM_REG_YEAR_SDK                      0x41
#define CAM_REG_MONTH_SDK                     0x42
#define CAM_REG_DAY_SDK                       0x43
#define CAM_REG_SENSOR_STATE                  0x44
#define CAM_REG_FPGA_VERSION_NUMBER           0x49
#define CAM_REG_DEBUG_DEVICE_ADDRESS          0X0A
#define CAM_REG_DEBUG_REGISTER_HIGH           0X0B
#define CAM_REG_DEBUG_REGISTER_LOW            0X0C
#define CAM_REG_DEBUG_REGISTER_VALUE          0X0D

#define SENSOR_STATE_IDLE   (1 << 1)
#define SENSOR_RESET_ENABLE (1 << 6)

#define CTR_WHILEBALANCE 0X02
#define CTR_EXPOSURE     0X01
#define CTR_GAIN         0X00

/**
 * @struct mega_sdk_data
 * @brief Basic information of the camera firmware
 */
struct mega_sdk_data {
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t version;
};

struct arducam_mega_config {
	struct spi_dt_spec bus;
};

struct arducam_mega_data {
	const struct device *dev;
	struct video_format fmt;
	struct k_fifo fifo_in;
	struct k_fifo fifo_out;
	struct k_work_delayable buf_work;
	struct k_work_sync work_sync;
	struct k_poll_signal *signal;
	struct arducam_mega_info *info;
	struct mega_sdk_data ver;
	uint8_t fifo_first_read;
	uint32_t fifo_length;
	uint8_t stream_on;
};

static struct arducam_mega_info mega_infos[] = {{
							.support_resoultion = 7894,
							.support_special_effects = 63,
							.exposure_value_max = 30000,
							.exposure_value_min = 1,
							.gain_value_max = 1023,
							.gain_value_min = 1,
							.enable_focus = 1,
							.enable_sharpness = 0,
							.device_address = 0x78,
						},
						{
							.support_resoultion = 7638,
							.support_special_effects = 319,
							.exposure_value_max = 30000,
							.exposure_value_min = 1,
							.gain_value_max = 1023,
							.gain_value_min = 1,
							.enable_focus = 0,
							.enable_sharpness = 1,
							.device_address = 0x78,
						}};

#define ARDUCAM_MEGA_VIDEO_FORMAT_CAP(width, height, format)                                       \
	{                                                                                          \
		.pixelformat = (format), .width_min = (width), .width_max = (width),               \
		.height_min = (height), .height_max = (height), .width_step = 0, .height_step = 0  \
	}

static struct video_format_cap fmts[] = {
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(96, 96, VIDEO_PIX_FMT_RGB565),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(128, 128, VIDEO_PIX_FMT_RGB565),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(320, 240, VIDEO_PIX_FMT_RGB565),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(320, 320, VIDEO_PIX_FMT_RGB565),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(640, 480, VIDEO_PIX_FMT_RGB565),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(1280, 720, VIDEO_PIX_FMT_RGB565),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(1600, 1200, VIDEO_PIX_FMT_RGB565),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(1920, 1080, VIDEO_PIX_FMT_RGB565),
	{0},
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(96, 96, VIDEO_PIX_FMT_JPEG),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(128, 128, VIDEO_PIX_FMT_JPEG),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(320, 240, VIDEO_PIX_FMT_JPEG),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(320, 320, VIDEO_PIX_FMT_JPEG),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(640, 480, VIDEO_PIX_FMT_JPEG),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(1280, 720, VIDEO_PIX_FMT_JPEG),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(1600, 1200, VIDEO_PIX_FMT_JPEG),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(1920, 1080, VIDEO_PIX_FMT_JPEG),
	{0},
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(96, 96, VIDEO_PIX_FMT_YUYV),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(128, 128, VIDEO_PIX_FMT_YUYV),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(320, 240, VIDEO_PIX_FMT_YUYV),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(320, 320, VIDEO_PIX_FMT_YUYV),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(640, 480, VIDEO_PIX_FMT_YUYV),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(1280, 720, VIDEO_PIX_FMT_YUYV),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(1600, 1200, VIDEO_PIX_FMT_YUYV),
	ARDUCAM_MEGA_VIDEO_FORMAT_CAP(1920, 1080, VIDEO_PIX_FMT_YUYV),
	{0},
	{0},
};

#define SUPPORT_RESOULTION_NUM 9

static uint8_t support_resoultion[SUPPORT_RESOULTION_NUM] = {
	MEGA_RESOULTION_96X96,   MEGA_RESOULTION_128X128, MEGA_RESOULTION_QVGA,
	MEGA_RESOULTION_320X320, MEGA_RESOULTION_VGA,     MEGA_RESOULTION_HD,
	MEGA_RESOULTION_UXGA,    MEGA_RESOULTION_FHD,     MEGA_RESOULTION_NONE,
};

static int arducam_mega_write_reg(const struct spi_dt_spec *spec, uint8_t reg_addr, uint8_t value)
{
	uint8_t tries = 3;

	reg_addr |= 0x80;

	struct spi_buf tx_buf[2] = {
		{.buf = &reg_addr, .len = 1},
		{.buf = &value, .len = 1},
	};

	struct spi_buf_set tx_bufs = {.buffers = tx_buf, .count = 2};

	while (tries--) {
		if (!spi_write_dt(spec, &tx_bufs)) {
			return 0;
		}
		/* If writing failed wait 5ms before next attempt */
		k_msleep(5);
	}
	LOG_ERR("failed to write 0x%x to 0x%x", value, reg_addr);

	return -1;
}

static int arducam_mega_read_reg(const struct spi_dt_spec *spec, uint8_t reg_addr)
{
	uint8_t tries = 3;
	uint8_t value;
	uint8_t ret;

	reg_addr &= 0x7F;

	struct spi_buf tx_buf[] = {
		{.buf = &reg_addr, .len = 1},
	};

	struct spi_buf_set tx_bufs = {.buffers = tx_buf, .count = 1};

	struct spi_buf rx_buf[] = {
		{.buf = &value, .len = 1},
		{.buf = &value, .len = 1},
		{.buf = &value, .len = 1},
	};

	struct spi_buf_set rx_bufs = {.buffers = rx_buf, .count = 3};

	while (tries--) {
		ret = spi_transceive_dt(spec, &tx_bufs, &rx_bufs);
		if (!ret) {
			return value;
		}

		/* If reading failed wait 5ms before next attempt */
		k_msleep(5);
	}
	LOG_ERR("failed to read 0x%x register", reg_addr);

	return -1;
}

static int arducam_mega_read_block(const struct spi_dt_spec *spec, uint8_t *img_buff,
				   uint32_t img_len, uint8_t first)
{
	uint8_t cmd_fifo_read[] = {BURST_FIFO_READ, 0x00};
	uint8_t buf_len = first == 0 ? 1 : 2;

	struct spi_buf tx_buf[] = {
		{.buf = cmd_fifo_read, .len = buf_len},
	};

	struct spi_buf_set tx_bufs = {.buffers = tx_buf, .count = 1};

	struct spi_buf rx_buf[2] = {
		{.buf = cmd_fifo_read, .len = buf_len},
		{.buf = img_buff, .len = img_len},
	};
	struct spi_buf_set rx_bufs = {.buffers = rx_buf, .count = 2};
	return spi_transceive_dt(spec, &tx_bufs, &rx_bufs);
}

static int aruducam_mega_await_bus_idle(const struct spi_dt_spec *spec, uint8_t tries)
{
	while ((arducam_mega_read_reg(spec, CAM_REG_SENSOR_STATE) & 0x03) != SENSOR_STATE_IDLE) {
		if (tries-- == 0) {
			return -1;
		}
		k_msleep(2);
	}

	return 0;
}

static int arducam_mega_soft_reset(const struct device *dev)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;
	struct arducam_mega_data *drv_data = dev->data;

	drv_data->stream_on = 0;
	/* Initiate system reset */
	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_SENSOR_RESET, SENSOR_RESET_ENABLE);
	k_msleep(1000);

	return ret;
}

static int arducam_mega_set_brightness(const struct device *dev, enum MEGA_BRIGHTNESS_LEVEL level)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_BRIGHTNESS_CONTROL, level);

	if (ret == -1) {
		LOG_ERR("Failed to set brightness level %d", level);
	}

	return ret;
}

static int arducam_mega_set_saturation(const struct device *dev, enum MEGA_STAURATION_LEVEL level)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_SATURATION_CONTROL, level);

	if (ret == -1) {
		LOG_ERR("Failed to set saturation level %d", level);
	}

	return ret;
}

static int arducam_mega_set_contrast(const struct device *dev, enum MEGA_CONTRAST_LEVEL level)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_CONTRAST_CONTROL, level);

	if (ret == -1) {
		LOG_ERR("Failed to set contrast level %d", level);
	}

	return ret;
}

static int arducam_mega_set_EV(const struct device *dev, enum MEGA_EV_LEVEL level)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_EV_CONTROL, level);

	if (ret == -1) {
		LOG_ERR("Failed to set contrast level %d", level);
	}

	return ret;
}

static int arducam_mega_set_sharpness(const struct device *dev, enum MEGA_SHARPNESS_LEVEL level)
{
	int ret = 0;
	struct arducam_mega_data *drv_data = dev->data;
	struct arducam_mega_info *drv_info = drv_data->info;
	const struct arducam_mega_config *cfg = dev->config;

	if (!drv_info->enable_sharpness) {
		LOG_ERR("This device does not support set sharpness.");
		return -1;
	}

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_SHARPNESS_CONTROL, level);

	if (ret == -1) {
		LOG_ERR("Failed to set sharpness level %d", level);
	}

	return ret;
}

static int arducam_mega_set_special_effects(const struct device *dev, enum MEGA_COLOR_FX effect)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_COLOR_EFFECT_CONTROL, effect);

	if (ret == -1) {
		LOG_ERR("Failed to set special effects %d", effect);
	}

	return ret;
}

static int arducam_mega_set_output_format(const struct device *dev, int output_format)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	if (output_format == VIDEO_PIX_FMT_JPEG) {
		/* Set output to JPEG compression */
		ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_FORMAT, MEGA_PIXELFORMAT_JPG);
	} else if (output_format == VIDEO_PIX_FMT_RGB565) {
		/* Set output to RGB565 */
		ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_FORMAT, MEGA_PIXELFORMAT_RGB565);
	} else if (output_format == VIDEO_PIX_FMT_YUYV) {
		/* Set output to YUV422 */
		ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_FORMAT, MEGA_PIXELFORMAT_YUV);
	} else {
		LOG_ERR("Image format not supported");
		return -ENOTSUP;
	}

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 30);

	return ret;
}

static int arducam_mega_set_quality(const struct device *dev, enum MEGA_IMAGE_QUALITY qs)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;
	struct arducam_mega_data *drv_data = dev->data;
	LOG_DBG("arducam_mega_set_quality:%d", qs);
	if (drv_data->fmt.pixelformat == VIDEO_PIX_FMT_JPEG) {
		ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);
		/* Write QS register */
		ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_IMAGE_QUALITY, qs);
	} else {
		LOG_ERR("Image format not support setting the quality");
		return -ENOTSUP;
	}

	return ret;
}

static int arducam_mega_set_white_bal_enable(const struct device *dev, int enable)
{
	int ret = 0;
	uint8_t reg = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	if (enable) {
		reg |= 0x80;
	}
	reg |= CTR_WHILEBALANCE;
	/* Update CTRL1 to enable/disable automatic white balance*/
	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_EXPOSURE_GAIN_WHILEBAL_ENABLE, reg);

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 10);

	return ret;
}

static int arducam_mega_set_white_bal(const struct device *dev, enum MEGA_EV_LEVEL level)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_WHILEBALANCE_CONTROL, level);

	if (ret == -1) {
		LOG_ERR("Failed to set contrast level %d", level);
	}

	return ret;
}

static int arducam_mega_set_gain_enable(const struct device *dev, int enable)
{
	int ret = 0;
	uint8_t reg = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	if (enable) {
		reg |= 0x80;
	}
	reg |= CTR_GAIN;
	/* Update CTRL1 to enable/disable automatic gain*/
	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_EXPOSURE_GAIN_WHILEBAL_ENABLE, reg);

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 10);

	return ret;
}

static int arducam_mega_set_lowpower_enable(const struct device *dev, int enable)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;
	const struct arducam_mega_data *drv_data = dev->data;
	const struct arducam_mega_info *drv_info = drv_data->info;

	if (drv_info->camera_id == ARDUMEGA_SENSOR_5MP_2 ||
	    drv_info->camera_id == ARDUMEGA_SENSOR_3MP_2) {
		enable = !enable;
	}

	if (enable) {
		ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_POWER_CONTROL, 0x07);
	} else {
		ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_POWER_CONTROL, 0x05);
	}
	return ret;
}

static int arducam_mega_set_gain(const struct device *dev, uint16_t value)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);
	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_MANUAL_GAIN_BIT_9_8, (value >> 8) & 0xff);
	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 10);
	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_MANUAL_GAIN_BIT_7_0, value & 0xff);
	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 10);

	return ret;
}

static int arducam_mega_set_exposure_enable(const struct device *dev, int enable)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	uint8_t reg = 0;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	if (enable) {
		reg |= 0x80;
	}
	reg |= CTR_GAIN;
	/* Enable/disable automatic exposure control */
	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_EXPOSURE_GAIN_WHILEBAL_ENABLE, reg);

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 10);

	return ret;
}

static int arducam_mega_set_exposure(const struct device *dev, uint32_t value)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);
	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_MANUAL_EXPOSURE_BIT_19_16,
				      (value >> 16) & 0xff);
	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 10);
	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_MANUAL_EXPOSURE_BIT_15_8,
				      (value >> 8) & 0xff);
	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 10);
	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_MANUAL_EXPOSURE_BIT_7_0, value & 0xff);
	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 10);

	return ret;
}

static int arducam_mega_set_resolution(const struct device *dev, enum MEGA_RESOULTION resoultion)
{
	int ret = 0;
	const struct arducam_mega_config *cfg = dev->config;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 3);

	ret |= arducam_mega_write_reg(&cfg->bus, CAM_REG_CAPTURE_RESOLUTION, resoultion);

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 10);

	return ret;
}

static int arducam_mega_check_connection(const struct device *dev)
{
	int ret = 0;
	uint8_t cam_id;
	const struct arducam_mega_config *cfg = dev->config;
	struct arducam_mega_data *drv_data = dev->data;

	ret |= aruducam_mega_await_bus_idle(&cfg->bus, 255);
	cam_id = arducam_mega_read_reg(&cfg->bus, CAM_REG_SENSOR_ID);

	if (!(cam_id & 0x87)) {
		LOG_ERR("arducam mega not detected, 0x%x\n", cam_id);
		return -ENODEV;
	}

	LOG_INF("detect camera id 0x%x, ret = %d\n", cam_id, ret);

	switch (cam_id) {
	case ARDUMEGA_SENSOR_5MP_1: /* 5MP-1 */
		fmts[8] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2592, 1944, VIDEO_PIX_FMT_RGB565);
		fmts[17] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2592, 1944, VIDEO_PIX_FMT_JPEG);
		fmts[26] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2592, 1944, VIDEO_PIX_FMT_YUYV);
		support_resoultion[8] = MEGA_RESOULTION_WQXGA2;
		drv_data->info = &mega_infos[0];
		break;
	case ARDUMEGA_SENSOR_3MP_1: /* 3MP-1 */
		fmts[8] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2048, 1536, VIDEO_PIX_FMT_RGB565);
		fmts[17] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2048, 1536, VIDEO_PIX_FMT_JPEG);
		fmts[26] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2048, 1536, VIDEO_PIX_FMT_YUYV);
		support_resoultion[8] = MEGA_RESOULTION_QXGA;
		drv_data->info = &mega_infos[1];
		break;
	case ARDUMEGA_SENSOR_5MP_2: /* 5MP-2 */
		fmts[8] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2592, 1936, VIDEO_PIX_FMT_RGB565);
		fmts[17] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2592, 1936, VIDEO_PIX_FMT_JPEG);
		fmts[26] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2592, 1936, VIDEO_PIX_FMT_YUYV);
		support_resoultion[8] = MEGA_RESOULTION_WQXGA2;
		break;
		drv_data->info = &mega_infos[0];
		break;
	case ARDUMEGA_SENSOR_3MP_2: /* 3MP-2 */
		fmts[8] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2048, 1536, VIDEO_PIX_FMT_RGB565);
		fmts[17] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2048, 1536, VIDEO_PIX_FMT_JPEG);
		fmts[26] = (struct video_format_cap)ARDUCAM_MEGA_VIDEO_FORMAT_CAP(
			2048, 1536, VIDEO_PIX_FMT_YUYV);
		support_resoultion[8] = MEGA_RESOULTION_QXGA;
		break;
		drv_data->info = &mega_infos[1];
		break;
	default:
		return -ENODEV;
		break;
	}
	drv_data->info->camera_id = cam_id;

	return ret;
}

static int arducam_mega_set_fmt(const struct device *dev, enum video_endpoint_id ep,
				struct video_format *fmt)
{
	struct arducam_mega_data *drv_data = dev->data;
	uint16_t width, height;
	int ret = 0;
	int i = 0;

	/* We only support RGB565, JPEG, and YUYV pixel formats */
	if (fmt->pixelformat != VIDEO_PIX_FMT_RGB565 && fmt->pixelformat != VIDEO_PIX_FMT_JPEG &&
	    fmt->pixelformat != VIDEO_PIX_FMT_YUYV) {
		LOG_ERR("Arducam Mega camera supports only RGB565, JPG, and YUYV pixelformats!");
		return -ENOTSUP;
	}

	width = fmt->width;
	height = fmt->height;

	if (!memcmp(&drv_data->fmt, fmt, sizeof(drv_data->fmt))) {
		/* nothing to do */
		return 0;
	}

	/* Check if camera is capable of handling given format */
	while (fmts[i].pixelformat) {
		if (fmts[i].width_min == width && fmts[i].height_min == height &&
		    fmts[i].pixelformat == fmt->pixelformat) {
			/* Set output format */
			ret |= arducam_mega_set_output_format(dev, fmt->pixelformat);
			/* Set window size */
			ret |= arducam_mega_set_resolution(
				dev, support_resoultion[i % SUPPORT_RESOULTION_NUM]);
			if (!ret) {
				drv_data->fmt = *fmt;
				drv_data->fmt.pitch = drv_data->fmt.width * 2;
			}
			return ret;
		}
		i++;
	}
	/* Camera is not capable of handling given format */
	LOG_ERR("Image resoultion not supported\n");
	return -ENOTSUP;
}

static int arducam_mega_get_fmt(const struct device *dev, enum video_endpoint_id ep,
				struct video_format *fmt)
{
	struct arducam_mega_data *drv_data = dev->data;

	*fmt = drv_data->fmt;

	return 0;
}

static int arducam_mega_stream_start(const struct device *dev)
{
	struct arducam_mega_data *drv_data = dev->data;

	drv_data->stream_on = 1;
	LOG_INF("stream start %s", drv_data->dev->name);

	return k_work_schedule(&drv_data->buf_work, K_MSEC(33));
}

static int arducam_mega_stream_stop(const struct device *dev)
{
	struct arducam_mega_data *drv_data = dev->data;

	drv_data->stream_on = 0;

	k_work_cancel_delayable_sync(&drv_data->buf_work, &drv_data->work_sync);

	return 0;
}

static int arducam_mega_capture(const struct device *dev, uint32_t *length)
{
	const struct arducam_mega_config *cfg = dev->config;
	struct arducam_mega_data *drv_data = dev->data;
	uint8_t tries = 200;

	if (drv_data->stream_on) {
		LOG_ERR("Camera is already on!");
		return -1;
	}

	arducam_mega_write_reg(&cfg->bus, ARDUCHIP_FIFO, FIFO_CLEAR_ID_MASK);
	arducam_mega_write_reg(&cfg->bus, ARDUCHIP_FIFO, FIFO_START_MASK);

	do {
		if (tries-- == 0) {
			LOG_ERR("Capture timeout!");
			return -1;
		}
		k_msleep(2);
	} while (!(arducam_mega_read_reg(&cfg->bus, ARDUCHIP_TRIG) & CAP_DONE_MASK));

	drv_data->fifo_length = arducam_mega_read_reg(&cfg->bus, FIFO_SIZE1);
	drv_data->fifo_length |= (arducam_mega_read_reg(&cfg->bus, FIFO_SIZE2) << 8);
	drv_data->fifo_length |= (arducam_mega_read_reg(&cfg->bus, FIFO_SIZE3) << 16);

	drv_data->fifo_first_read = 1;
	*length = drv_data->fifo_length;
	return 0;
}

static int arducam_mega_fifo_read(const struct device *dev, struct video_buffer *buf)
{
	int ret;
	int32_t rlen;
	const struct arducam_mega_config *cfg = dev->config;
	struct arducam_mega_data *drv_data = dev->data;

	rlen = buf->size > drv_data->fifo_length ? drv_data->fifo_length : buf->size;

	LOG_DBG("read fifo :%u. - fifo_length %u", buf->size, drv_data->fifo_length);

	ret = arducam_mega_read_block(&cfg->bus, buf->buffer, rlen, drv_data->fifo_first_read);

	drv_data->fifo_length -= rlen;
	buf->bytesused = rlen;
	if (drv_data->fifo_first_read) {
		drv_data->fifo_first_read = 0;
	}

	return ret;
}

static void __buffer_work(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct arducam_mega_data *drv_data =
		CONTAINER_OF(dwork, struct arducam_mega_data, buf_work);
	const struct arducam_mega_config *cfg = drv_data->dev->config;
	uint8_t tries = 200;
	uint32_t recv_len = 0;
	struct video_buffer *vbuf;

	k_work_reschedule(&drv_data->buf_work, K_MSEC(1000 / VIDEO_PATTERN_FPS));

	arducam_mega_write_reg(&cfg->bus, ARDUCHIP_FIFO, FIFO_CLEAR_ID_MASK);
	arducam_mega_write_reg(&cfg->bus, ARDUCHIP_FIFO, FIFO_START_MASK);

	do {
		if (tries-- == 0) {
			LOG_ERR("Capture timeout!");
			return;
		}
		k_msleep(2);
	} while (!(arducam_mega_read_reg(&cfg->bus, ARDUCHIP_TRIG) & CAP_DONE_MASK));

	vbuf = k_fifo_get(&drv_data->fifo_in, K_NO_WAIT);

	if (vbuf == NULL) {
		return;
	}

	recv_len = arducam_mega_read_reg(&cfg->bus, FIFO_SIZE1);
	recv_len |= (arducam_mega_read_reg(&cfg->bus, FIFO_SIZE2) << 8);
	recv_len |= (arducam_mega_read_reg(&cfg->bus, FIFO_SIZE3) << 16);

	arducam_mega_read_block(&cfg->bus, vbuf->buffer,
				recv_len > vbuf->size ? vbuf->size : recv_len, 1);

	vbuf->timestamp = k_uptime_get_32();
	vbuf->bytesused = recv_len;

	k_fifo_put(&drv_data->fifo_out, vbuf);

	if (IS_ENABLED(CONFIG_POLL) && drv_data->signal) {
		k_poll_signal_raise(drv_data->signal, VIDEO_BUF_DONE);
	}

	k_yield();
}

static int arducam_mega_enqueue(const struct device *dev, enum video_endpoint_id ep,
				struct video_buffer *vbuf)
{
	struct arducam_mega_data *data = dev->data;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}
	LOG_DBG("enqueue buffer %p", vbuf->buffer);
	k_fifo_put(&data->fifo_in, vbuf);

	return 0;
}

static int arducam_mega_dequeue(const struct device *dev, enum video_endpoint_id ep,
				struct video_buffer **vbuf, k_timeout_t timeout)
{
	struct arducam_mega_data *data = dev->data;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	*vbuf = k_fifo_get(&data->fifo_out, timeout);

	LOG_DBG("dequeue buffer %p", (*vbuf)->buffer);

	if (*vbuf == NULL) {
		return -EAGAIN;
	}

	return 0;
}

static int arducam_mega_get_caps(const struct device *dev, enum video_endpoint_id ep,
				 struct video_caps *caps)
{
	caps->format_caps = fmts;
	return 0;
}

#ifdef CONFIG_POLL
static int arducam_mega_set_signal(const struct device *dev, enum video_endpoint_id ep,
				   struct k_poll_signal *signal)
{
	struct arducam_mega_data *data = dev->data;

	if (data->signal && signal != NULL) {
		return -EALREADY;
	}

	data->signal = signal;

	return 0;
}
#endif

static int arducam_mega_set_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	int ret = 0;

	switch (cid) {
	case VIDEO_CID_CAMERA_EXPOSURE:
		ret |= arducam_mega_set_exposure(dev, *(uint32_t *)value);
		break;
	case VIDEO_CID_CAMERA_GAIN:
		ret |= arducam_mega_set_gain(dev, *(uint16_t *)value);
		break;
	case VIDEO_CID_CAMERA_BRIGHTNESS:
		ret |= arducam_mega_set_brightness(dev, *(enum MEGA_BRIGHTNESS_LEVEL *)value);
		break;
	case VIDEO_CID_CAMERA_SATURATION:
		ret |= arducam_mega_set_saturation(dev, *(enum MEGA_STAURATION_LEVEL *)value);
		break;
	case VIDEO_CID_CAMERA_WHITE_BAL:
		ret |= arducam_mega_set_white_bal(dev, *(enum MEGA_STAURATION_LEVEL *)value);
		break;
	case VIDEO_CID_CAMERA_CONTRAST:
		ret |= arducam_mega_set_contrast(dev, *(enum MEGA_CONTRAST_LEVEL *)value);
		break;
	case VIDEO_CID_CAMERA_QUALITY:
		ret |= arducam_mega_set_quality(dev, *(enum MEGA_IMAGE_QUALITY *)value);
		break;
	case VIDEO_CID_ARDUCAM_EV:
		ret |= arducam_mega_set_EV(dev, *(enum MEGA_EV_LEVEL *)value);
		break;
	case VIDEO_CID_ARDUCAM_CAPTURE:
		ret |= arducam_mega_capture(dev, (uint32_t *)value);
		break;
	case VIDEO_CID_ARDUCAM_SHARPNESS:
		ret |= arducam_mega_set_sharpness(dev, *(enum MEGA_SHARPNESS_LEVEL *)value);
		break;
	case VIDEO_CID_ARDUCAM_COLOR_FX:
		ret |= arducam_mega_set_special_effects(dev, *(enum MEGA_COLOR_FX *)value);
		break;
	case VIDEO_CID_ARDUCAM_AUTO_WHILE_BAL:
		ret |= arducam_mega_set_white_bal_enable(dev, *(uint8_t *)value);
		break;
	case VIDEO_CID_ARDUCAM_AUTO_EXPOUTRE:
		ret |= arducam_mega_set_exposure_enable(dev, *(uint8_t *)value);
		break;
	case VIDEO_CID_ARDUCAM_AUTO_GAIN:
		ret |= arducam_mega_set_gain_enable(dev, *(uint8_t *)value);
		break;
	case VIDEO_CID_ARDUCAM_RESET:
		ret |= arducam_mega_soft_reset(dev);
		ret |= arducam_mega_check_connection(dev);
		break;
	case VIDEO_CID_ARDUCAM_LOWPOWER:
		ret |= arducam_mega_set_lowpower_enable(dev, *(uint8_t *)value);
		break;
	default:
		return -ENOTSUP;
	}

	return ret;
}

int arducam_mega_get_info(const struct device *dev, struct arducam_mega_info *info)
{
	struct arducam_mega_data *drv_data = dev->data;

	*info = (*drv_data->info);

	return 0;
}

static int arducam_mega_get_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	int ret = 0;

	switch (cid) {
	case VIDEO_CID_ARDUCAM_CAPTURE:
		ret |= arducam_mega_fifo_read(dev, (struct video_buffer *)value);
		break;
	case VIDEO_CID_ARDUCAM_INFO:
		ret |= arducam_mega_get_info(dev, (struct arducam_mega_info *)value);
		break;
	default:
		return -ENOTSUP;
	}

	return ret;
}

static const struct video_driver_api arducam_mega_driver_api = {
	.set_format = arducam_mega_set_fmt,
	.get_format = arducam_mega_get_fmt,
	.get_caps = arducam_mega_get_caps,
	.stream_start = arducam_mega_stream_start,
	.stream_stop = arducam_mega_stream_stop,
	.set_ctrl = arducam_mega_set_ctrl,
	.get_ctrl = arducam_mega_get_ctrl,
	.enqueue = arducam_mega_enqueue,
	.dequeue = arducam_mega_dequeue,
#ifdef CONFIG_POLL
	.set_signal = arducam_mega_set_signal,
#endif
};

static int arducam_mega_init(const struct device *dev)
{
	const struct arducam_mega_config *cfg = dev->config;
	struct arducam_mega_data *drv_data = dev->data;

	struct video_format fmt;
	int ret = 0;

	if (!spi_is_ready_dt(&cfg->bus)) {
		LOG_ERR("%s: device is not ready", cfg->bus.bus->name);
		return -ENODEV;
	}

	drv_data->dev = dev;
	k_fifo_init(&drv_data->fifo_in);
	k_fifo_init(&drv_data->fifo_out);
	k_work_init_delayable(&drv_data->buf_work, __buffer_work);

	arducam_mega_soft_reset(dev);
	ret = arducam_mega_check_connection(dev);

	if (ret) {
		LOG_ERR("arducam mega camera not connection.\n");
		return ret;
	}

	drv_data->ver.year = arducam_mega_read_reg(&cfg->bus, CAM_REG_YEAR_SDK) & 0x3F;
	drv_data->ver.month = arducam_mega_read_reg(&cfg->bus, CAM_REG_MONTH_SDK) & 0x0F;
	drv_data->ver.day = arducam_mega_read_reg(&cfg->bus, CAM_REG_DAY_SDK) & 0x1F;
	drv_data->ver.version =
		arducam_mega_read_reg(&cfg->bus, CAM_REG_FPGA_VERSION_NUMBER) & 0xfF;

	LOG_INF("arducam mega ver: %d-%d-%d \t %x", drv_data->ver.year, drv_data->ver.month,
		drv_data->ver.day, drv_data->ver.version);

	/* set default/init format 96x96 RGB565 */
	fmt.pixelformat = VIDEO_PIX_FMT_RGB565;
	fmt.width = 96;
	fmt.height = 96;
	fmt.pitch = 96 * 2;
	ret = arducam_mega_set_fmt(dev, VIDEO_EP_OUT, &fmt);
	if (ret) {
		LOG_ERR("Unable to configure default format");
		return -EIO;
	}

	return ret;
}

#define ARDUCAM_MEGA_INIT(inst)                                                                    \
	static const struct arducam_mega_config arducam_mega_cfg_##inst = {                        \
		.bus = SPI_DT_SPEC_INST_GET(inst,                                                  \
					    SPI_OP_MODE_MASTER | SPI_WORD_SET(8) |                 \
						    SPI_CS_ACTIVE_HIGH | SPI_LINES_SINGLE |        \
						    SPI_LOCK_ON,                                   \
					    0),                                                    \
	};                                                                                         \
                                                                                                   \
	static struct arducam_mega_data arducam_mega_data_##inst;                                  \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, &arducam_mega_init, NULL, &arducam_mega_data_##inst,           \
			      &arducam_mega_cfg_##inst, POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY,   \
			      &arducam_mega_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ARDUCAM_MEGA_INIT)