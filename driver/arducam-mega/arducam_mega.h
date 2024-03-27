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

#ifndef ARDUMEGA_MEGA_H
#define ARDUMEGA_MEGA_H

#ifdef __cplusplus
extern "C" {
#endif

#define VIDEO_CID_ARDUCAM_EV             (VIDEO_CTRL_CLASS_VENDOR + 1)
#define VIDEO_CID_ARDUCAM_CAPTURE        (VIDEO_CTRL_CLASS_VENDOR + 2)
#define VIDEO_CID_ARDUCAM_INFO           (VIDEO_CTRL_CLASS_VENDOR + 3)
#define VIDEO_CID_ARDUCAM_SHARPNESS      (VIDEO_CTRL_CLASS_VENDOR + 4)
#define VIDEO_CID_ARDUCAM_COLOR_FX       (VIDEO_CTRL_CLASS_VENDOR + 5)
#define VIDEO_CID_ARDUCAM_AUTO_WHILE_BAL (VIDEO_CTRL_CLASS_VENDOR + 6)
#define VIDEO_CID_ARDUCAM_AUTO_EXPOUTRE  (VIDEO_CTRL_CLASS_VENDOR + 7)
#define VIDEO_CID_ARDUCAM_AUTO_GAIN      (VIDEO_CTRL_CLASS_VENDOR + 8)
#define VIDEO_CID_ARDUCAM_RESET          (VIDEO_CTRL_CLASS_VENDOR + 9)
#define VIDEO_CID_ARDUCAM_LOWPOWER       (VIDEO_CTRL_CLASS_VENDOR + 10)

/**
 * @enum MEGA_CONTRAST_LEVEL
 * @brief Configure camera contrast level
 */
enum MEGA_CONTRAST_LEVEL {
	MEGA_CONTRAST_LEVEL_NEGATIVE_3 = 6, /**<Level -3 */
	MEGA_CONTRAST_LEVEL_NEGATIVE_2 = 4, /**<Level -2 */
	MEGA_CONTRAST_LEVEL_NEGATIVE_1 = 2, /**<Level -1 */
	MEGA_CONTRAST_LEVEL_DEFAULT = 0,    /**<Level Default*/
	MEGA_CONTRAST_LEVEL_1 = 1,          /**<Level +1 */
	MEGA_CONTRAST_LEVEL_2 = 3,          /**<Level +2 */
	MEGA_CONTRAST_LEVEL_3 = 5,          /**<Level +3 */
};

/**
 * @enum MEGA_EV_LEVEL
 * @brief Configure camera EV level
 */
enum MEGA_EV_LEVEL {
	MEGA_EV_LEVEL_NEGATIVE_3 = 6, /**<Level -3 */
	MEGA_EV_LEVEL_NEGATIVE_2 = 4, /**<Level -2 */
	MEGA_EV_LEVEL_NEGATIVE_1 = 2, /**<Level -1 */
	MEGA_EV_LEVEL_DEFAULT = 0,    /**<Level Default*/
	MEGA_EV_LEVEL_1 = 1,          /**<Level +1 */
	MEGA_EV_LEVEL_2 = 3,          /**<Level +2 */
	MEGA_EV_LEVEL_3 = 5,          /**<Level +3 */
};

/**
 * @enum MEGA_STAURATION_LEVEL
 * @brief Configure camera stauration  level
 */
enum MEGA_STAURATION_LEVEL {
	MEGA_STAURATION_LEVEL_NEGATIVE_3 = 6, /**<Level -3 */
	MEGA_STAURATION_LEVEL_NEGATIVE_2 = 4, /**<Level -2 */
	MEGA_STAURATION_LEVEL_NEGATIVE_1 = 2, /**<Level -1 */
	MEGA_STAURATION_LEVEL_DEFAULT = 0,    /**<Level Default*/
	MEGA_STAURATION_LEVEL_1 = 1,          /**<Level +1 */
	MEGA_STAURATION_LEVEL_2 = 3,          /**<Level +2 */
	MEGA_STAURATION_LEVEL_3 = 5,          /**<Level +3 */
};

/**
 * @enum MEGA_BRIGHTNESS_LEVEL
 * @brief Configure camera brightness level
 */
enum MEGA_BRIGHTNESS_LEVEL {
	MEGA_BRIGHTNESS_LEVEL_NEGATIVE_4 = 8, /**<Level -4 */
	MEGA_BRIGHTNESS_LEVEL_NEGATIVE_3 = 6, /**<Level -3 */
	MEGA_BRIGHTNESS_LEVEL_NEGATIVE_2 = 4, /**<Level -2 */
	MEGA_BRIGHTNESS_LEVEL_NEGATIVE_1 = 2, /**<Level -1 */
	MEGA_BRIGHTNESS_LEVEL_DEFAULT = 0,    /**<Level Default*/
	MEGA_BRIGHTNESS_LEVEL_1 = 1,          /**<Level +1 */
	MEGA_BRIGHTNESS_LEVEL_2 = 3,          /**<Level +2 */
	MEGA_BRIGHTNESS_LEVEL_3 = 5,          /**<Level +3 */
	MEGA_BRIGHTNESS_LEVEL_4 = 7,          /**<Level +4 */
};

/**
 * @enum MEGA_SHARPNESS_LEVEL
 * @brief Configure camera Sharpness level
 */
enum MEGA_SHARPNESS_LEVEL {
	MEGA_SHARPNESS_LEVEL_AUTO = 0, /**<Sharpness Auto */
	MEGA_SHARPNESS_LEVEL_1,        /**<Sharpness Level 1 */
	MEGA_SHARPNESS_LEVEL_2,        /**<Sharpness Level 2 */
	MEGA_SHARPNESS_LEVEL_3,        /**<Sharpness Level 3 */
	MEGA_SHARPNESS_LEVEL_4,        /**<Sharpness Level 4 */
	MEGA_SHARPNESS_LEVEL_5,        /**<Sharpness Level 5 */
	MEGA_SHARPNESS_LEVEL_6,        /**<Sharpness Level 6 */
	MEGA_SHARPNESS_LEVEL_7,        /**<Sharpness Level 7 */
	MEGA_SHARPNESS_LEVEL_8,        /**<Sharpness Level 8 */
};

/**
 * @enum CAM_COLOR_FX
 * @brief Configure special effects
 */
enum MEGA_COLOR_FX {
	MEGA_COLOR_FX_NONE = 0,      /**< no effect   */
	MEGA_COLOR_FX_BLUEISH,       /**< cool light   */
	MEGA_COLOR_FX_REDISH,        /**< warm   */
	MEGA_COLOR_FX_BW,            /**< Black/white   */
	MEGA_COLOR_FX_SEPIA,         /**<Sepia   */
	MEGA_COLOR_FX_NEGATIVE,      /**<positive/negative inversion  */
	MEGA_COLOR_FX_GRASS_GREEN,   /**<Grass green */
	MEGA_COLOR_FX_OVER_EXPOSURE, /**<Over exposure*/
	MEGA_COLOR_FX_SOLARIZE,      /**< Solarize   */
};

/**
 * @enum MEGA_WHITE_BALANCE
 * @brief Configure white balance mode
 */
enum MEGA_WHITE_BALANCE {
	MEGA_WHITE_BALANCE_MODE_DEFAULT = 0, /**< Auto */
	MEGA_WHITE_BALANCE_MODE_SUNNY,       /**< Sunny */
	MEGA_WHITE_BALANCE_MODE_OFFICE,      /**< Office */
	MEGA_WHITE_BALANCE_MODE_CLOUDY,      /**< Cloudy*/
	MEGA_WHITE_BALANCE_MODE_HOME,        /**< Home */
};

enum MEGA_IMAGE_QUALITY {
	HIGH_QUALITY = 0,
	DEFAULT_QUALITY = 1,
	LOW_QUALITY = 2,
};

enum {
	ARDUMEGA_SENSOR_5MP_1 = 0x81,
	ARDUMEGA_SENSOR_3MP_1 = 0x82,
	ARDUMEGA_SENSOR_5MP_2 = 0x83, /* 2592x1936 */
	ARDUMEGA_SENSOR_3MP_2 = 0x84,
};

/**
 * @enum MEGA_PIXELFORMAT
 * @brief Configure camera pixel format
 */
enum MEGA_PIXELFORMAT {
	MEGA_PIXELFORMAT_JPG = 0X01,
	MEGA_PIXELFORMAT_RGB565 = 0X02,
	MEGA_PIXELFORMAT_YUV = 0X03,
};

/**
 * @enum MEGA_RESOULTION
 * @brief Configure camera resolution
 */
enum MEGA_RESOULTION {
	MEGA_RESOULTION_QQVGA = 0x00,   /**<160x120 */
	MEGA_RESOULTION_QVGA = 0x01,    /**<320x240*/
	MEGA_RESOULTION_VGA = 0x02,     /**<640x480*/
	MEGA_RESOULTION_SVGA = 0x03,    /**<800x600*/
	MEGA_RESOULTION_HD = 0x04,      /**<1280x720*/
	MEGA_RESOULTION_SXGAM = 0x05,   /**<1280x960*/
	MEGA_RESOULTION_UXGA = 0x06,    /**<1600x1200*/
	MEGA_RESOULTION_FHD = 0x07,     /**<1920x1080*/
	MEGA_RESOULTION_QXGA = 0x08,    /**<2048x1536*/
	MEGA_RESOULTION_WQXGA2 = 0x09,  /**<2592x1944*/
	MEGA_RESOULTION_96X96 = 0x0a,   /**<96x96*/
	MEGA_RESOULTION_128X128 = 0x0b, /**<128x128*/
	MEGA_RESOULTION_320X320 = 0x0c, /**<320x320*/
	MEGA_RESOULTION_12 = 0x0d,      /**<Reserve*/
	MEGA_RESOULTION_13 = 0x0e,      /**<Reserve*/
	MEGA_RESOULTION_14 = 0x0f,      /**<Reserve*/
	MEGA_RESOULTION_15 = 0x10,      /**<Reserve*/
	MEGA_RESOULTION_NONE,
};

struct arducam_mega_info {
	int support_resoultion;
	int support_special_effects;
	unsigned long exposure_value_max;
	unsigned int exposure_value_min;
	unsigned int gain_value_max;
	unsigned int gain_value_min;
	unsigned char enable_focus;
	unsigned char enable_sharpness;
	unsigned char device_address;
	unsigned char camera_id;
};

#ifdef __cplusplus
}
#endif

#endif /* ARDUMEGA_MEGA_H */
