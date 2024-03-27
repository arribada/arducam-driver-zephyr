#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <zephyr/drivers/video.h>
// #include <zephyr/drivers/video/arducam_mega.h>

#include <zephyr/drivers/uart.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(arducam_link);

#include "arducam_link.h"
#include <arducam_mega.h>

const struct device *console;
const struct device *video;
struct video_buffer *vbuf;

uint8_t preview_on = 0;

void serial_cb(const struct device *dev, void *user_data);

int arducam_link_init(void)
{
	console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	if (!device_is_ready(console)) {
		LOG_ERR("%s: device not ready.", console->name);
		return -1;
	}
	uart_irq_callback_user_data_set(console, serial_cb, NULL);
	uart_irq_rx_enable(console);

	video = DEVICE_DT_GET(DT_NODELABEL(arducam_mega0));

	if (!device_is_ready(video)) {
		LOG_ERR("Video device %s not ready.", video->name);
		return -1;
	}

	/* Alloc video buffers and enqueue for capture */
	vbuf = video_buffer_alloc(1024);
	if (vbuf == NULL) {
		LOG_ERR("Unable to alloc video buffer");
		return -1;
	}

	LOG_INF("Mega star");

	printk("- Device name: %s\n", video->name);

	return 0;
}

uint32_t pixel_format_conversion_table[] = {
	VIDEO_PIX_FMT_JPEG,
	VIDEO_PIX_FMT_RGB565,
	VIDEO_PIX_FMT_YUYV,
};

uint16_t resolution_conversion_table[][2] = {
	{160, 120},  {320, 240},   {640, 480},   {800, 600},   {1280, 720},
	{1280, 960}, {1600, 1200}, {1920, 1080}, {2048, 1536}, {2592, 1944},
	{96, 96},    {128, 128},   {320, 320},
};

static uint8_t current_resoultion = 0x00;

int set_mega_resolution(uint8_t sfmt)
{
	uint8_t resoultion = sfmt & 0x0f;
	uint8_t pixelformat = (sfmt & 0x70) >> 4;
	uint8_t resoultion_num = sizeof(resolution_conversion_table) / (sizeof(uint16_t) * 2);

	if (resoultion > resoultion_num || pixelformat > 3) {
		return -1;
	}
	struct video_format fmt = {.width = resolution_conversion_table[resoultion][0],
				   .height = resolution_conversion_table[resoultion][1],
				   .pixelformat = pixel_format_conversion_table[pixelformat - 1]};
	current_resoultion = resoultion;
	return video_set_format(video, VIDEO_EP_OUT, &fmt);
}

void uart_buffer_send(const struct device *dev, uint8_t *buffer, uint32_t length)
{
	for (uint32_t i = 0; i < length; i++) {
		uart_poll_out(dev, buffer[i]);
	}
}

static uint8_t head_and_tail[] = {0xff, 0xaa, 0x00, 0xff, 0xbb};

void uart_packet_send(uint8_t type, uint8_t *buffer, uint32_t length)
{
	head_and_tail[2] = type;
	uart_buffer_send(console, &head_and_tail[0], 3);
	uart_buffer_send(console, (uint8_t *)&length, 4);
	uart_buffer_send(console, buffer, length);
	uart_buffer_send(console, &head_and_tail[3], 2);
}

int take_picture(void)
{
	uint32_t frame_len;
	if (video_set_ctrl(video, VIDEO_CID_ARDUCAM_CAPTURE, &frame_len)) {
		LOG_ERR("Unable to take picture!");
		return -1;
	}

	head_and_tail[2] = 0x01;
	uart_buffer_send(console, &head_and_tail[0], 3);
	uart_buffer_send(console, (uint8_t *)&frame_len, 4);
	uart_poll_out(console, ((current_resoultion & 0x0f) << 4) | 0x01);
	// LOG_INF("frame length %u",frame_len);
	while (frame_len > 0) {
		if (!video_get_ctrl(video, VIDEO_CID_ARDUCAM_CAPTURE, vbuf)) {
			uart_buffer_send(console, vbuf->buffer, vbuf->bytesused);
			// LOG_INF("read fifo :%u. - frame_len %u", vbuf->bytesused ,frame_len);
			frame_len -= vbuf->bytesused;
		}
		// k_msleep(1);
	}
	uart_buffer_send(console, &head_and_tail[3], 2);
	return 0;
}

int video_preview(void)
{
	static uint32_t recv_len = 0;
	if (preview_on) {
		if (recv_len) {
			if (!video_get_ctrl(video, VIDEO_CID_ARDUCAM_CAPTURE, vbuf)) {
				uart_buffer_send(console, vbuf->buffer, vbuf->bytesused);
				// LOG_INF("read fifo :%u. - frame_len %u", vbuf->bytesused
				// ,frame_len);
				recv_len -= vbuf->bytesused;
			}
		} else {
			uart_buffer_send(console, &head_and_tail[3], 2);
			if (video_set_ctrl(video, VIDEO_CID_ARDUCAM_CAPTURE, &recv_len)) {
				LOG_ERR("Unable to take picture!");
				return -1;
			}
			head_and_tail[2] = 0x01;
			uart_buffer_send(console, &head_and_tail[0], 3);
			uart_buffer_send(console, (uint8_t *)&recv_len, 4);
			uart_poll_out(console, ((current_resoultion & 0x0f) << 4) | 0x01);
		}
	}
	return 0;
}

int report_mega_info(void)
{
	char str_buf[400];
	uint32_t str_len;
	char *mega_type;
	struct arducam_mega_info mega_info;

	video_get_ctrl(video, VIDEO_CID_ARDUCAM_INFO, &mega_info);

	switch (mega_info.camera_id) {
	case ARDUMEGA_SENSOR_3MP_1:
	case ARDUMEGA_SENSOR_3MP_2:
		mega_type = "3MP";
		break;
	case ARDUMEGA_SENSOR_5MP_1:
		mega_type = "5MP";
		break;
	case ARDUMEGA_SENSOR_5MP_2:
		mega_type = "5MP_2";
		break;
	default:
		return -ENODEV;
	}

	sprintf(str_buf,
		"ReportCameraInfo\r\nCamera Type:%s\r\n"
		"Camera Support Resolution:%d\r\nCamera Support "
		"specialeffects:%d\r\nCamera Support Focus:%d\r\n"
		"Camera Exposure Value Max:%ld\r\nCamera Exposure Value "
		"Min:%d\r\nCamera Gain Value Max:%d\r\nCamera Gain Value "
		"Min:%d\r\nCamera Support Sharpness:%d\r\n",
		mega_type, mega_info.support_resoultion, mega_info.support_special_effects,
		mega_info.enable_focus, mega_info.exposure_value_max, mega_info.exposure_value_min,
		mega_info.gain_value_max, mega_info.gain_value_min, mega_info.enable_sharpness);
	str_len = strlen(str_buf);
	uart_packet_send(0x02, str_buf, str_len);
	return 0;
}

uint8_t cmd_process(uint8_t *buff)
{
	switch (buff[0]) {
	case SET_PICTURE_RESOLUTION: // Set Camera Resolution
		set_mega_resolution(buff[1]);
		break;
	case SET_VIDEO_RESOLUTION: // Set Video Resolution
		preview_on = 1;
		set_mega_resolution(buff[1] | 0x10);
		break;
	case SET_BRIGHTNESS: // Set brightness
		video_set_ctrl(video, VIDEO_CID_CAMERA_BRIGHTNESS, &buff[1]);
		break;
	case SET_CONTRAST: // Set Contrast
		video_set_ctrl(video, VIDEO_CID_CAMERA_CONTRAST, &buff[1]);
		break;
	case SET_SATURATION: // Set saturation
		video_set_ctrl(video, VIDEO_CID_CAMERA_SATURATION, &buff[1]);
		break;
	case SET_EV: // Set EV
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_EV, &buff[1]);
		break;
	case SET_WHITEBALANCE: // Set White balance
		video_set_ctrl(video, VIDEO_CID_CAMERA_WHITE_BAL, &buff[1]);
		break;
	case SET_SPECIAL_EFFECTS: // Set Special effects
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_COLOR_FX, &buff[1]);
		break;
	// case SET_FOCUS_CONTROL: // Focus Control
	//     setAutoFocus(camera, buff[1]);
	//     if (buff[1] == 0) {
	//         setAutoFocus(camera, 0x02);
	//     }
	//     break;
	case SET_EXPOSUREANDGAIN_ENABEL: // exposure and  Gain control
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_AUTO_EXPOUTRE, &buff[1]);
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_AUTO_GAIN, &buff[1]);
	case SET_WHILEBALANCE_ENABEL: // while balance control
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_AUTO_WHILE_BAL, &buff[1]);
		break;
	case SET_SHARPNESS:
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_SHARPNESS, &buff[1]);
		break;
	case SET_MANUAL_GAIN: // manual gain control
		uint16_t gain_value = (buff[1] << 8) | buff[2];
		video_set_ctrl(video, VIDEO_CID_CAMERA_GAIN, &gain_value);
		break;
	case SET_MANUAL_EXPOSURE: // manual exposure control
		uint32_t exposure_value = (buff[1] << 16) | (buff[2] << 8) | buff[3];
		video_set_ctrl(video, VIDEO_CID_CAMERA_EXPOSURE, &exposure_value);
		break;
	case GET_CAMERA_INFO: // Get Camera info
		report_mega_info();
		break;
	case TAKE_PICTURE:
		take_picture();
		break;
	case STOP_STREAM:
		preview_on = 0;
		uart_buffer_send(console, &head_and_tail[0], 3);
		break;
	// case GET_FRM_VER_INFO: // Get Firmware version info
	//     reportVerInfo(camera);
	//     break;
	// case GET_SDK_VER_INFO: // Get sdk version info
	//     reportSdkVerInfo(camera);
	//     break;
	case RESET_CAMERA:
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_RESET, NULL);
		break;
	case SET_IMAGE_QUALITY:
		video_set_ctrl(video, VIDEO_CID_CAMERA_QUALITY, &buff[1]);
		break;
	case SET_LOWPOWER_MODE:
		video_set_ctrl(video, VIDEO_CID_ARDUCAM_LOWPOWER, &buff[1]);
		break;
	default:
		break;
	}

	return buff[0];
}

#define MSG_SIZE 12
/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

uint8_t uart_available(uint8_t *p)
{
	return k_msgq_get(&uart_msgq, p, K_NO_WAIT);
}

uint8_t cmd_buffer[12] = {0};

void arducam_link_work(void)
{

	if (!uart_available(cmd_buffer)) {
		cmd_process(cmd_buffer);
	}
	video_preview();
	k_msleep(1);
}

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(dev)) {
		return;
	}

	if (!uart_irq_rx_ready(dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(dev, &c, 1) == 1) {
		if (c == 0xAA && rx_buf_pos > 0) {

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (c == 0x55) {
			rx_buf_pos = 0;
		} else {
			rx_buf[rx_buf_pos] = c;
			if (++rx_buf_pos >= MSG_SIZE) {
				rx_buf_pos = 0;
			}
		}
		/* else: characters beyond buffer size are dropped */
	}
}
