
#ifndef __CAMERA_H_
#define __CAMERA_H_

#define RESET_CAMERA               0xFF
#define SET_PICTURE_RESOLUTION     0x01
#define SET_VIDEO_RESOLUTION       0x02
#define SET_BRIGHTNESS             0x03
#define SET_CONTRAST               0x04
#define SET_SATURATION             0x05
#define SET_EV                     0x06
#define SET_WHITEBALANCE           0x07
#define SET_SPECIAL_EFFECTS        0x08
#define SET_FOCUS_ENABEL           0x09
#define SET_EXPOSUREANDGAIN_ENABEL 0x0A
#define SET_WHILEBALANCE_ENABEL    0x0C
#define SET_MANUAL_GAIN            0x0D
#define SET_MANUAL_EXPOSURE        0x0E
#define GET_CAMERA_INFO            0x0F
#define TAKE_PICTURE               0x10
#define SET_SHARPNESS              0x11
#define DEBUG_WRITE_REGISTER       0x12
#define STOP_STREAM                0x21
#define GET_FRM_VER_INFO           0x30
#define GET_SDK_VER_INFO           0x40
#define SET_IMAGE_QUALITY          0x50
#define SET_LOWPOWER_MODE          0x60

// JPEG Resolutions
#define RES_JPEG_96x96 0x1A

void arducam_link_work(void);
int camera_init(void);
int capture_picture(uint8_t *buf, size_t *buf_size);
int take_picture(void);

#endif /*__CAMERA_H_*/