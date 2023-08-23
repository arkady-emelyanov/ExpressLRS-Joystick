#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "main.h"

Adafruit_USBD_HID usb_hid;  // USB HID object

static gamepad_report_t gp;
uint8_t const desc_hid_report[] = {
    HID_REPORT_RADIO()  // USB GamePad data structure
};

uint8_t rxBuf[CRSF_MAX_FRAME_SIZE + 3];
uint8_t rxPos = 0;
uint8_t frameSize = 0;

uint32_t gapTime; // For bus orchestration
uint16_t dataReady = 0;

void rx_process();
void elrs_frame_decode();
void usb_process();

void setup()
{
  // buffer counters
  rxPos = 0;
  dataReady = 0;
  gapTime = 0;

  // USB HID Device Settings
  usb_hid.setPollInterval(2); // 
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();

  while (!USBDevice.mounted()) {
    delay(1);  // wait until device mounted
  }

  // USB communication (to match receiver speed)
  SERIAL_PORT_USBVIRTUAL.begin(CRSF_BAUDRATE);

  // RX communication (420kbps, 8bit data, nonParity, 1 stopbit)
  SERIAL_PORT_HARDWARE.begin(CRSF_BAUDRATE, SERIAL_8N1);
}

void loop()
{
  // Process USB
  usb_process();

  // Process RX
  rx_process();
  
  // Report Gamepad data once ready
  if (dataReady) {
    if (usb_hid.ready()) {
      usb_hid.sendReport(0, &gp, sizeof(gp));
    }
    dataReady = 0;
  }
}


// Receive data frame from ExpressLRS serial port
void rx_process()
{
  uint8_t data;

  // byte received from CRSF
  if (SERIAL_PORT_HARDWARE.available()) {  // If there is incoming data on CRSF interface
    data = SERIAL_PORT_HARDWARE.read();    // 8-bit data read
    gapTime = micros();

    // Second byte is the frame size
    if (rxPos == 1) {
      frameSize = data;
    }

    // Store received data in buffer
    rxBuf[rxPos++] = data;
    if (rxPos > 1 && rxPos >= frameSize + 2) {
      // Decode after receiving one frame
      elrs_frame_decode();
      rxPos = 0;
    }
  } else {
    // If no data comes in for more than 800us, judge as a break
    if (rxPos > 0 && micros() - gapTime > 800) {
      rxPos = 0;
    }
  }
}

// Translate 11-bit serial data received from ExpressLRS receiver into 16-bit Gamepad data.
// https://github.com/ExpressLRS/ExpressLRS/wiki/CRSF-Protocol
// https://github.com/hathach/tinyusb/blob/51a0889b75fc2da6675530d68f3ae765c44849f4/src/class/hid/hid.h#L153
void elrs_frame_decode()
{
  if (rxBuf[0] == CRSF_ADDRESS_FLIGHT_CONTROLLER) {
    if (rxBuf[2] == CRSF_FRAMETYPE_RC_CHANNELS_PACKED) {
      crsf_header_t *h = (crsf_header_t *) &rxBuf;
      crsf_channels_t *c = (crsf_channels_t *) &h->data;

      gp.ch[0] = MAP_CHANNEL(c->ch0); // Roll [A]
      gp.ch[1] = MAP_CHANNEL(c->ch1); // Pitch [E]
      gp.ch[2] = MAP_CHANNEL(c->ch2); // Yaw [R]
      gp.ch[3] = MAP_CHANNEL(c->ch3); // Throttle [T]
      gp.ch[4] = MAP_CHANNEL(c->ch4);
      gp.ch[5] = MAP_CHANNEL(c->ch5);
      gp.ch[6] = MAP_CHANNEL(c->ch6);
      gp.ch[7] = MAP_CHANNEL(c->ch7);

      unsigned aux[] = {
        (unsigned)MAP_CHANNEL(c->ch8),
        (unsigned)MAP_CHANNEL(c->ch9),
        (unsigned)MAP_CHANNEL(c->ch10),
        (unsigned)MAP_CHANNEL(c->ch11),
        (unsigned)MAP_CHANNEL(c->ch12),
        (unsigned)MAP_CHANNEL(c->ch13),
        (unsigned)MAP_CHANNEL(c->ch14),
        (unsigned)MAP_CHANNEL(c->ch15),
      };
      gp.sw = 0;
      for (unsigned int i=0; i<8; i++) {
        if (aux[i] >= CRSF_CHANNEL_VALUE_2000) {
          uint8_t bit = (i - 8) % 8;
          uint8_t msk = (1 << bit);
          gp.sw |= msk;
        }
      }

      dataReady = 1;
    }
  }
}

// Handle host USB initiated communication process (e.g. for the firmware upload).
// ExpressLRS Configurator flashing method: BetaflightPassthough
void usb_process()
{
  uint32_t t;

  // When host is writing data to the USB port
  if (SERIAL_PORT_USBVIRTUAL.available()) {
    t = millis();
    do {
      // Pass host data to the RX
      while (SERIAL_PORT_USBVIRTUAL.available()) {
        SERIAL_PORT_HARDWARE.write(SERIAL_PORT_USBVIRTUAL.read());  // Send data from PC to receiver
        t = millis();
      }
      // Pass RX data to host
      while (SERIAL_PORT_HARDWARE.available()) {
        SERIAL_PORT_USBVIRTUAL.write(SERIAL_PORT_HARDWARE.read());  // Send receiver data to PC
        t = millis();
      }
    } while (millis() - t < 2000); // When data stops coming in, it's over
  }
}

