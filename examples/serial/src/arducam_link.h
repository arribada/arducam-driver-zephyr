
#ifndef __ARDUCAM_LINK_H_
#define __ARDUCAM_LINK_H_

#define RESET_CAMERA                0XFF
#define SET_PICTURE_RESOLUTION      0X01
#define SET_VIDEO_RESOLUTION        0X02
#define SET_BRIGHTNESS              0X03
#define SET_CONTRAST                0X04
#define SET_SATURATION              0X05
#define SET_EV                      0X06
#define SET_WHITEBALANCE            0X07
#define SET_SPECIAL_EFFECTS         0X08
#define SET_FOCUS_ENABEL            0X09
#define SET_EXPOSUREANDGAIN_ENABEL  0X0A
#define SET_WHILEBALANCE_ENABEL     0X0C
#define SET_MANUAL_GAIN             0X0D
#define SET_MANUAL_EXPOSURE         0X0E
#define GET_CAMERA_INFO             0X0F
#define TAKE_PICTURE                0X10
#define SET_SHARPNESS               0X11
#define DEBUG_WRITE_REGISTER        0X12
#define STOP_STREAM                 0X21
#define GET_FRM_VER_INFO            0X30
#define GET_SDK_VER_INFO            0X40
#define SET_IMAGE_QUALITY           0X50
#define SET_LOWPOWER_MODE           0X60

void arducam_link_work(void);
int arducam_link_init(void);

#endif /*__ARDUCAM_LINK_H_*/