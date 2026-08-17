#pragma once
#include <QByteArray>
#include <QString>
#include <QtGlobal>

// RS485 lane-indicator command set (重庆创迪科技协议说明).
// Commands 0x01~0x0A, responses are command + 0x80 (0x81~0x8A).
namespace Rs485Commands {

// Command codes.
constexpr quint8 SET_DISPLAY_STATE = 0x01;
constexpr quint8 QUERY_DISPLAY_STATE = 0x02;
constexpr quint8 SET_BRIGHTNESS = 0x03;
constexpr quint8 QUERY_BRIGHTNESS = 0x04;
constexpr quint8 SET_DEVICE_ID = 0x05;
constexpr quint8 SET_BRIGHTNESS_MIN = 0x06;
constexpr quint8 SET_BRIGHTNESS_MAX = 0x07;
constexpr quint8 SET_BAUD_RATE = 0x08;       // 0=9600, 1=115200
constexpr quint8 SET_DAC_SCALE_RED = 0x09;   // red coefficient 1~40
constexpr quint8 SET_DAC_SCALE_GREEN = 0x0A; // green coefficient 1~40

// Response codes (command + 0x80).
constexpr quint8 RESP_SET_DISPLAY_STATE = SET_DISPLAY_STATE + 0x80;
constexpr quint8 RESP_QUERY_DISPLAY_STATE = QUERY_DISPLAY_STATE + 0x80;
constexpr quint8 RESP_SET_BRIGHTNESS = SET_BRIGHTNESS + 0x80;
constexpr quint8 RESP_QUERY_BRIGHTNESS = QUERY_BRIGHTNESS + 0x80;
constexpr quint8 RESP_SET_DEVICE_ID = SET_DEVICE_ID + 0x80;
constexpr quint8 RESP_SET_BRIGHTNESS_MIN = SET_BRIGHTNESS_MIN + 0x80;
constexpr quint8 RESP_SET_BRIGHTNESS_MAX = SET_BRIGHTNESS_MAX + 0x80;
constexpr quint8 RESP_SET_BAUD_RATE = SET_BAUD_RATE + 0x80;
constexpr quint8 RESP_SET_DAC_SCALE_RED = SET_DAC_SCALE_RED + 0x80;
constexpr quint8 RESP_SET_DAC_SCALE_GREEN = SET_DAC_SCALE_GREEN + 0x80;

// Display state nibbles: front = high nibble, back = low nibble.
constexpr quint8 FRONT_OFF = 0x00;
constexpr quint8 FRONT_RED = 0x10;
constexpr quint8 FRONT_GREEN = 0x20;
constexpr quint8 FRONT_TURN = 0x30;
constexpr quint8 BACK_OFF = 0x00;
constexpr quint8 BACK_RED = 0x01;
constexpr quint8 BACK_GREEN = 0x02;
constexpr quint8 BACK_TURN = 0x03;

// DAC scale coefficient bounds and defaults.
constexpr int DAC_SCALE_MIN = 1;
constexpr int DAC_SCALE_MAX = 40;
constexpr int DAC_SCALE_RED_DEFAULT = 24;
constexpr int DAC_SCALE_GREEN_DEFAULT = 16;

// Brightness "auto" sentinel (0xFF).
constexpr quint8 BRIGHTNESS_AUTO = 0xFF;

// Combine front (high nibble) and back (low nibble) into one data byte.
quint8 combineDisplay(quint8 front, quint8 back);

// Frame builders.
QByteArray buildDisplayStateFrame(quint8 id, quint8 front, quint8 back);
QByteArray buildQueryDisplayStateFrame(quint8 id);
QByteArray buildBrightnessFrame(quint8 id, quint8 value); // 0..100 or 0xFF=auto
QByteArray buildQueryBrightnessFrame(quint8 id);
QByteArray buildDeviceIdFrame(quint8 id, quint8 newId);
QByteArray buildBrightnessMinFrame(quint8 id, quint8 min);
QByteArray buildBrightnessMaxFrame(quint8 id, quint8 max);
QByteArray buildBaudRateFrame(quint8 id, quint8 code); // 0=9600, 1=115200
QByteArray buildDacScaleRedFrame(quint8 id, quint8 v);
QByteArray buildDacScaleGreenFrame(quint8 id, quint8 v);

// Human-readable descriptions.
QString describeDisplayState(quint8 dataByte);
QString describeBrightness(quint8 value);

// Command name (mirrors the Python CMD_NAMES table).
QString cmdName(quint8 cmd);

// Description of a response payload (mirrors the Python parse_response()).
QString describeResponse(quint8 cmd, quint8 dataByte);

} // namespace Rs485Commands
