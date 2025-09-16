#include <Arduino.h>

typedef struct __attribute__((packed)) gamepad_report_s {
  uint16_t ch[8];  // 16 bit 8ch
  uint8_t sw;      //  1 bit 8ch
} gamepad_report_t;

// joystick range
#define HID_JOYSTICK_VALUE_MIN -32767
#define HID_JOYSTICK_VALUE_MAX  32767

// HID extra axis
#define HID_USAGE_DESKTOP_SLIDER1 0x36
#define HID_USAGE_DESKTOP_SLIDER2 0x37

// http://who-t.blogspot.com/2018/12/understanding-hid-report-descriptors.html
#define HID_REPORT_RADIO(...) \
  HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP), \
  HID_USAGE(HID_USAGE_DESKTOP_GAMEPAD), \
  HID_COLLECTION(HID_COLLECTION_APPLICATION), /* Report ID if any */ \
      __VA_ARGS__ \
      HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP), \
        HID_USAGE(HID_USAGE_DESKTOP_X), \
        HID_USAGE(HID_USAGE_DESKTOP_Y), \
        HID_USAGE(HID_USAGE_DESKTOP_Z), \
        HID_USAGE(HID_USAGE_DESKTOP_RX), \
        HID_USAGE(HID_USAGE_DESKTOP_RY), \
        HID_USAGE(HID_USAGE_DESKTOP_RZ), \
        HID_USAGE(HID_USAGE_DESKTOP_SLIDER1), \
        HID_USAGE(HID_USAGE_DESKTOP_SLIDER2), \
        HID_LOGICAL_MIN_N(HID_JOYSTICK_VALUE_MIN, 2), \
        HID_LOGICAL_MAX_N(HID_JOYSTICK_VALUE_MAX, 2), \
        HID_REPORT_COUNT(8), \
        HID_REPORT_SIZE(16), \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
      HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON), /* 8 bit Button Map */ \
        HID_USAGE_MIN(1), \
        HID_USAGE_MAX(8), \
        HID_LOGICAL_MIN(0), \
        HID_LOGICAL_MAX(1), \
        HID_REPORT_COUNT(8), \
        HID_REPORT_SIZE(1), \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
      HID_COLLECTION_END

// CrossFire settings
#define CRSF_BAUDRATE 420000
#define CRSF_MAX_FRAME_SIZE 64
#define CRSF_NUM_CHANNELS 16

// https://github.com/ExpressLRS/ExpressLRS/blob/567bc8c4fbb574330af70f9162586a43e2bbaaca/src/lib/CrsfProtocol/crsf_protocol.h#L29
#define CRSF_CHANNEL_VALUE_MIN  172 // 987us - actual CRSF min is 0 with E.Limits on
#define CRSF_CHANNEL_VALUE_1000 191
#define CRSF_CHANNEL_VALUE_MID  992
#define CRSF_CHANNEL_VALUE_2000 1792
#define CRSF_CHANNEL_VALUE_MAX  1811 // 2012us - actual CRSF max is 1984 with E.Limits on

// map channel value into joystick value
#define MAP_CHANNEL(V) map(V, \
  CRSF_CHANNEL_VALUE_MIN, \
  CRSF_CHANNEL_VALUE_MAX, \
  HID_JOYSTICK_VALUE_MIN, \
  HID_JOYSTICK_VALUE_MAX  \
)

typedef struct __attribute__((packed)) crsf_header_s
{
    uint8_t device_addr; // from crsf_addr_e
    uint8_t frame_size;  // counts size after this byte, so it must be the payload size + 2 (type and crc)
    uint8_t type;        // from crsf_frame_type_e
    uint8_t data[0];
} crsf_header_t;

typedef struct __attribute__((packed)) crsf_channels_s
{
    unsigned ch0 : 11;
    unsigned ch1 : 11;
    unsigned ch2 : 11;
    unsigned ch3 : 11;
    unsigned ch4 : 11;
    unsigned ch5 : 11;
    unsigned ch6 : 11;
    unsigned ch7 : 11;
    unsigned ch8 : 11;
    unsigned ch9 : 11;
    unsigned ch10 : 11;
    unsigned ch11 : 11;
    unsigned ch12 : 11;
    unsigned ch13 : 11;
    unsigned ch14 : 11;
    unsigned ch15 : 11;
} crsf_channels_t;

// https://github.com/ExpressLRS/ExpressLRS/blob/567bc8c4fbb574330af70f9162586a43e2bbaaca/src/lib/CrsfProtocol/crsf_protocol.h#L123
typedef enum {
  CRSF_ADDRESS_BROADCAST = 0x00,
  CRSF_ADDRESS_USB = 0x10,
  CRSF_ADDRESS_TBS_CORE_PNP_PRO = 0x80,
  CRSF_ADDRESS_RESERVED1 = 0x8A,
  CRSF_ADDRESS_CURRENT_SENSOR = 0xC0,
  CRSF_ADDRESS_GPS = 0xC2,
  CRSF_ADDRESS_TBS_BLACKBOX = 0xC4,
  CRSF_ADDRESS_FLIGHT_CONTROLLER = 0xC8,  // Incoming data comes in this
  CRSF_ADDRESS_RESERVED2 = 0xCA,
  CRSF_ADDRESS_RACE_TAG = 0xCC,
  CRSF_ADDRESS_RADIO_TRANSMITTER = 0xEA,
  CRSF_ADDRESS_CRSF_RECEIVER = 0xEC,
  CRSF_ADDRESS_CRSF_TRANSMITTER = 0xEE,
} crsf_addr_e;

// https://github.com/ExpressLRS/ExpressLRS/blob/567bc8c4fbb574330af70f9162586a43e2bbaaca/src/lib/CrsfProtocol/crsf_protocol.h#L65
typedef enum {
  CRSF_FRAMETYPE_GPS = 0x02,
  CRSF_FRAMETYPE_BATTERY_SENSOR = 0x08,
  CRSF_FRAMETYPE_LINK_STATISTICS = 0x14,
  CRSF_FRAMETYPE_OPENTX_SYNC = 0x10,
  CRSF_FRAMETYPE_RADIO_ID = 0x3A,
  CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16,  // packed channel frame
  CRSF_FRAMETYPE_ATTITUDE = 0x1E,
  CRSF_FRAMETYPE_FLIGHT_MODE = 0x21,
  // Extended Header Frames, range: 0x28 to 0x96
  CRSF_FRAMETYPE_DEVICE_PING = 0x28,
  CRSF_FRAMETYPE_DEVICE_INFO = 0x29,
  CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B,
  CRSF_FRAMETYPE_PARAMETER_READ = 0x2C,
  CRSF_FRAMETYPE_PARAMETER_WRITE = 0x2D,
  CRSF_FRAMETYPE_COMMAND = 0x32,
  // MSP commands
  CRSF_FRAMETYPE_MSP_REQ =  0x7A,   // response request using msp sequence as command
  CRSF_FRAMETYPE_MSP_RESP = 0x7B,   // reply with 58 byte chunked binary
  CRSF_FRAMETYPE_MSP_WRITE = 0x7C,  // write with 8 byte chunked binary (OpenTX outbound telemetry buffer limit)
} crsf_frame_type_e;
