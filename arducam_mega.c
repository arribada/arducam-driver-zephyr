#include "arducam_mega.h"
#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include <zephyr/syscall_handler.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>

#define LOG_MODULE_NAME arducam_mega
LOG_MODULE_REGISTER(LOG_MODULE_NAME);
#define MAX_PATH 51200
#define CONCAT_BUFF_LEN 30
#define BUFFER_SIZE 0xff

int received_length;

struct spi_config spi_cfg = {
    .frequency = DT_PROP(DT_NODELABEL(spi0), clock_frequency),
    .operation = SPI_LOCK_ON | SPI_HOLD_ON_CS | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB  | SPI_WORD_SET(8) | SPI_LINES_SINGLE ,
    .cs =SPI_CS_CONTROL_INIT(DT_NODELABEL(spi0), 0u),
    .slave = 0,
};

#define SPI_DEV_NAME DEVICE_DT_NAME(DT_NODELABEL(spi0))
const struct device *spi;

#define LED DT_ALIAS(csled)
struct gpio_dt_spec spec = GPIO_DT_SPEC_GET(LED, gpios);


uint8_t camera_bus_read(uint8_t address)
{
	int ret;
	struct spi_buf tx_buf[1];
	tx_buf[0].buf = &address;
	tx_buf[0].len = 1;
	struct spi_buf_set tx_bufs = {.buffers = tx_buf, .count = 1};
	spi_cfg.operation |= SPI_HOLD_ON_CS;
	spi_cfg.operation |= SPI_LOCK_ON;

	gpio_pin_toggle_dt(&spec);
	ret = spi_write(spi,&spi_cfg, &tx_bufs);
	uint8_t rxdata[2];
	struct spi_buf rx_buf[1] = {
		{.buf=rxdata,   .len = 2},
	};
	struct spi_buf_set rx_bufs = {
		.buffers = rx_buf,
		.count = 2 };

	ret = spi_read(spi, &spi_cfg,&rx_bufs);
	gpio_pin_toggle_dt(&spec);
	spi_release(spi, &spi_cfg);
	return rxdata[1];
}

uint8_t camera_read_reg(uint8_t addr)
{
	return camera_bus_read(addr & 0x7F);

}

uint8_t camera_bus_write(uint8_t address, uint8_t value)
{

	struct spi_buf tx_buf[2];
	tx_buf[0].buf = &address;
	tx_buf[0].len = 1;
	tx_buf[1].buf = &value;
	tx_buf[1].len = 1;
	struct spi_buf_set tx_bufs = {.buffers = tx_buf, .count = 2};
	spi_cfg.operation |= SPI_HOLD_ON_CS;
	spi_cfg.operation |= SPI_LOCK_ON;
	gpio_pin_toggle_dt(&spec);
	spi_write(spi, &spi_cfg,&tx_bufs);
	gpio_pin_toggle_dt(&spec);
	k_sleep(K_MSEC(10));
	return 1;
}
void camera_write_reg( uint8_t addr, uint8_t val)
{
	camera_bus_write( addr | 0x80, val);
}

void camera_wait_idle()
{
    while ((camera_read_reg(CAM_REG_SENSOR_STATE) & 0X03) != CAM_REG_SENSOR_STATE_IDLE) {
	    k_sleep(K_MSEC(2));

    }
}

uint8_t camera_get_bit(uint8_t addr, uint8_t bit)
{
    uint8_t temp;
    temp = camera_read_reg(addr);
    temp = temp & bit;
    return temp;
}


uint8_t cameraReadByte(){
	int ret;
	uint8_t rxdata[1];
	struct spi_buf rx_buf[1] = {
		{.buf=rxdata,   .len = 1},
	};
	struct spi_buf_set rx_bufs = {
		.buffers = rx_buf,
		.count = 1 };

	gpio_pin_toggle_dt(&spec);
	struct spi_buf tx_buf[1];

	uint8_t send_cmd = SINGLE_FIFO_READ;
	tx_buf[0].buf = &send_cmd;
	tx_buf[0].len = 1;

	struct spi_buf_set tx_bufs = {.buffers = tx_buf, .count = 1};
	spi_cfg.operation |= SPI_HOLD_ON_CS;
	spi_cfg.operation |= SPI_LOCK_ON;
	ret = spi_write(spi,&spi_cfg, &tx_bufs);

	send_cmd = 0x00;
	tx_buf[0].buf = &send_cmd;
	ret = spi_write(spi,&spi_cfg, &tx_bufs);

	ret = spi_read(spi, &spi_cfg, &rx_bufs);
	gpio_pin_toggle_dt(&spec);
	received_length -= 1;
	return rxdata[0];
}


void camera_save_fifo(const char* base_path, uint32_t length, char* filename){

	uint8_t imageData = 0;
	uint8_t imageDataNext = 0;
	uint8_t headFlag = 0;
	unsigned int i =0;
	uint8_t imageBuff[BUFFER_SIZE] = {0};
	int ret;
	uint8_t sd_write_counts=0;
	uint8_t file_opened = 0;

	received_length = length;
	char path[MAX_PATH];
	struct fs_file_t file;
	int base = strlen(base_path);

	fs_file_t_init(&file);

	if (base >= (sizeof(path) - CONCAT_BUFF_LEN)) {
		LOG_ERR("Not enough concatenation buffer to create file paths");
		return;
	}

	strncpy(path, base_path, sizeof(path));

	path[base++] = '/';
	path[base] = 0;
	strcat(&path[base], filename);
	while (received_length){
		imageData = imageDataNext;
		imageDataNext = cameraReadByte();
		if (headFlag == 1)
			{
				imageBuff[i++]=imageDataNext;
				if (i >= BUFFER_SIZE)
					{
						ret = fs_write(&file, imageBuff, i);
						sd_write_counts++;
						i = 0;
					}
			}
		if (imageData == 0xff && imageDataNext ==0xd8 && file_opened == 0)
			{
				ret = fs_open(&file, path,FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
				if(ret != 0){
					LOG_ERR("Failed to create file %s %d", path, ret);
					return;
				}
				LOG_INF("Opened file successfully\n");
				file_opened = 1;
				headFlag = 1;
				imageBuff[i++]=imageData;
				imageBuff[i++]=imageDataNext;
			}
		if (imageData == 0xff && imageDataNext ==0xd9)
			{
				headFlag = 0;
				LOG_INF("Closed file with sd_write_counts %d\n", sd_write_counts);
				fs_write(&file, imageBuff, i);

				fs_close(&file);
				i = 0;
				break;
			}
	}

}


int arducam_mega_take_picture(CAM_IMAGE_MODE mode, CAM_IMAGE_PIX_FMT pixel_format)
{
	camera_write_reg(CAM_REG_SENSOR_RESET, CAM_SENSOR_RESET_ENABLE);
	camera_wait_idle();
	camera_write_reg(0x04, 0x01);
	camera_write_reg(0x04, 0x02);
	camera_wait_idle();
	k_sleep(K_MSEC(300));

	/* Set format JPG */
	camera_write_reg(CAM_REG_FORMAT, CAM_IMAGE_PIX_FMT_JPG); // set the data format
	camera_wait_idle(); // Wait I2c Idle

	/* Set capture resolution */
	camera_write_reg(CAM_REG_CAPTURE_RESOLUTION, CAM_SET_CAPTURE_MODE | mode);
	camera_wait_idle(); // Wait I2c Idle

	/* Clear fifo flags */

	camera_write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_ID_MASK);
	/* Start capture */
	camera_write_reg(ARDUCHIP_FIFO, FIFO_START_MASK);

	while (camera_get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK) == 0)
		;
	uint32_t len1, len2, len3, length = 0;
	len1   = camera_read_reg(FIFO_SIZE1);
	len2   = camera_read_reg(FIFO_SIZE2);
	len3   = camera_read_reg(FIFO_SIZE3);
	length = ((len3 << 16) | (len2 << 8) | len1) & 0xffffff;
	LOG_INF("Image length is %d\n", length);
	return length;
}
int arducam_mega_save_picture(char* filename, const char* mount_point)
{
	LOG_INF("Saving picture\n");
	/* Begin function */
	/* reset cpld and camera */
	/* 0x87,0x40*/
	/* Read FIFO length */
	uint32_t len1, len2, len3, length = 0;
	len1   = camera_read_reg(FIFO_SIZE1);
	len2   = camera_read_reg(FIFO_SIZE2);
	len3   = camera_read_reg(FIFO_SIZE3);
	length = ((len3 << 16) | (len2 << 8) | len1) & 0xffffff;
	LOG_INF("Image length is %d\n", length);

	camera_save_fifo(mount_point,length,filename);

	return 0;
}

int arducam_mega_get_cameraid()
{
	uint8_t cameraID;
	cameraID = camera_read_reg(CAM_REG_SENSOR_ID);
	LOG_INF("Sensor camera ID is %x\n", cameraID);
	return cameraID;

}
int arducam_mega_init()
{
	int ret = gpio_pin_configure_dt(&spec, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Couldn't configure CS pin");
		return -1;
	}


	LOG_INF("Initializing the camera");
	spi = device_get_binding(SPI_DEV_NAME);
	if (!device_is_ready(spi)) {
		LOG_ERR("Device SPI not ready, aborting test");
		return -1;
	}

	/* struct arducam_mega_config *cfg = dev->config; */
	/* dev->config = {  */
	/*	.frequency = DT_PROP(DT_NODELABEL(spi0), clock_frequency), */
	/*	.operation = SPI_LOCK_ON | SPI_HOLD_ON_CS | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB  | SPI_WORD_SET(8) | SPI_LINES_SINGLE , */
	/*	/\* This is initialized but we don't use it *\/ */
	/*	.cs =SPI_CS_CONTROL_INIT(DT_NODELABEL(spi0), 0u), */
	/*	.slave = 0, */
	/* }; */

	/* LOG_INF("Camera initialized and ready to use %d\n", cfg->spi_cfg.frequency); */
	return 0;
}
CAM_IMAGE_MODE arducam_mega_get_resolution(char* resolution)
{
	if (strcmp(resolution,"2592x1944")==0) {
		return CAM_IMAGE_MODE_WQXGA2;
	}
	else if (strcmp(resolution,"1920x1080")==0){
		return CAM_IMAGE_MODE_FHD;
	}
	else if (strcmp(resolution,"1600x1200")==0){
		return CAM_IMAGE_MODE_UXGA;
	}
	else if (strcmp(resolution,"1280x720")==0){
		return CAM_IMAGE_MODE_HD;
	}
	else if (strcmp(resolution,"640x480")==0){
		return CAM_IMAGE_MODE_VGA;
	}
	else if (strcmp(resolution,"320x240")==0){
		return CAM_IMAGE_MODE_QVGA;
	}
	else if (strcmp(resolution,"320x320")==0){
		return CAM_IMAGE_MODE_320X320;
	}
	else if (strcmp(resolution,"128x128")==0){
		return CAM_IMAGE_MODE_128X128;
	}
	else if (strcmp(resolution,"96x96")==0){
		return CAM_IMAGE_MODE_96X96;
	}
	/* Default to highest res if value is missing/malformed */
	return CAM_IMAGE_MODE_WQXGA2;
}

CAM_SATURATION_LEVEL arducam_mega_get_saturation(char* saturation)
{
	saturation=saturation+2;
	saturation[strlen(saturation)-1] = '\0';

	if(strcmp(saturation,"-3")==0) {
		return CAM_SATURATION_LEVEL_MINUS_3;
	}
	else if(strcmp(saturation,"-2")==0) {
		return CAM_SATURATION_LEVEL_MINUS_2;
	}
	else if(strcmp(saturation,"-1")==0) {
		return CAM_SATURATION_LEVEL_MINUS_1;
	}
	else if(strcmp(saturation,"0")==0) {
		return CAM_SATURATION_LEVEL_DEFAULT;
	}
	else if(strcmp(saturation,"1")==0) {
		return CAM_SATURATION_LEVEL_1;
	}
	else if(strcmp(saturation,"2")==0) {
		return CAM_SATURATION_LEVEL_2;
	}
	else if(strcmp(saturation,"3")==0) {
		return CAM_SATURATION_LEVEL_3;
	}
	/* Return default if string malformed */
	return CAM_SATURATION_LEVEL_DEFAULT;
}

CAM_BRIGHTNESS_LEVEL arducam_mega_get_brightness(char* brightness)
{
	brightness=brightness+2;
	brightness[strlen(brightness)-1] = '\0';
	if(strcmp(brightness,"-4")==0) {
		return CAM_BRIGHTNESS_LEVEL_MINUS_4;
	}
	else if(strcmp(brightness,"-3")==0) {
		return CAM_BRIGHTNESS_LEVEL_MINUS_3;
	}
	else if(strcmp(brightness,"-2")==0) {
		return CAM_BRIGHTNESS_LEVEL_MINUS_2;
	}
	else if(strcmp(brightness,"-1")==0) {
		return CAM_BRIGHTNESS_LEVEL_MINUS_1;
	}
	else if(strcmp(brightness,"0")==0) {
		return CAM_BRIGHTNESS_LEVEL_DEFAULT;
	}
	else if(strcmp(brightness,"1")==0) {
		return CAM_BRIGHTNESS_LEVEL_1;
	}
	else if(strcmp(brightness,"2")==0) {
		return CAM_BRIGHTNESS_LEVEL_2;
	}
	else if(strcmp(brightness,"3")==0) {
		return CAM_BRIGHTNESS_LEVEL_3;
	}
	else if(strcmp(brightness,"4")==0) {
		return CAM_BRIGHTNESS_LEVEL_4;
	}
	/* Return default if string malformed */
	return CAM_BRIGHTNESS_LEVEL_DEFAULT;
}

CAM_CONTRAST_LEVEL arducam_mega_get_contrast(char* contrast)
{
	contrast=contrast+2;
	contrast[strlen(contrast)-1] = '\0';

	if(strcmp(contrast,"-3")==0) {
		return CAM_CONTRAST_LEVEL_MINUS_3;
	}
	else if(strcmp(contrast,"-2")==0) {
		return CAM_CONTRAST_LEVEL_MINUS_2;
	}
	else if(strcmp(contrast,"-1")==0) {
		return CAM_CONTRAST_LEVEL_MINUS_1;
	}
	else if(strcmp(contrast,"0")==0) {
		return CAM_CONTRAST_LEVEL_DEFAULT;
	}
	else if(strcmp(contrast,"1")==0) {
		return CAM_CONTRAST_LEVEL_1;
	}
	else if(strcmp(contrast,"2")==0) {
		return CAM_CONTRAST_LEVEL_2;
	}
	else if(strcmp(contrast,"3")==0) {
		return CAM_CONTRAST_LEVEL_3;
	}
	/* Return default if string malformed */
	return CAM_CONTRAST_LEVEL_DEFAULT;
}
int  arducam_mega_set_saturation(CAM_SATURATION_LEVEL saturation)
{
	LOG_INF("Setting saturation to %d", saturation);
	camera_write_reg(CAM_REG_SATURATION_CONTROL, saturation);
	camera_wait_idle();
	return 0;
}

int arducam_mega_set_contrast(CAM_CONTRAST_LEVEL contrast)
{
	LOG_INF("Setting contrast to %d", contrast);
	camera_write_reg(CAM_REG_CONTRAST_CONTROL, contrast);
	camera_wait_idle();
	return 0;
}


int arducam_mega_set_brightness(CAM_BRIGHTNESS_LEVEL brightness)
{
	LOG_INF("Setting brightness to %d", brightness);
	camera_write_reg(CAM_REG_BRIGHTNESS_CONTROL, brightness);
	camera_wait_idle();
	return 0;
}


/* #define ARDUCAM_MEGA_DEFINE(inst)					\ */
/*	static struct arducam_mega_data arducam_data_##inst;		\ */
/*	static struct arducam_mega_config arducam_config_##inst = {	\ */
/*		.spi_dt = SPI_DT_SPEC_GET(inst,				\ */
/*					  SPI_LOCK_ON | SPI_HOLD_ON_CS | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB  | SPI_WORD_SET(8) | SPI_LINES_SINGLE, \ */
/*					  0),				\ */
/*	};								\ */
/*	DEVICE_DT_DEFINE(inst,						\ */
/*			 arducam_init, NULL,				\ */
/*			 &arducam_data_##inst , &arducam_config_##inst,	\ */
/*			 POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \ */
/*			 NULL); */

/* DT_FOREACH_STATUS_OKAY(arducam_mega,ARDUCAM_MEGA_DEFINE); */
		/* .spi = SPI_DT_SPEC_INST_GET(inst, SPI_LOCK_ON | SPI_HOLD_ON_CS | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB  | SPI_WORD_SET(8) | SPI_LINES_SINGLE,0), \ */
		/* .spi_cfg = {						\ */
		/*	.frequency = DT_PROP(DT_NODELABEL(spi0),clock_frequency), \ */
		/*	.operation = SPI_LOCK_ON | SPI_HOLD_ON_CS | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB  | SPI_WORD_SET(8) | SPI_LINES_SINGLE, \ */
		/*	.slave = 0,.cs=SPI_CS_CONTROL_INIT(DT_NODELABEL(spi0), 0u) \ */
		/* },							\ */
/* DT_INST_FOREACH_STATUS_OKAY(ARDUCAM_MEGA_DEFINE) */
