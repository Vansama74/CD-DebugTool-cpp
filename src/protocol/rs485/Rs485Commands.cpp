#include "Rs485Commands.h"

#include "Rs485Frame.h"

#include <QString>

namespace Rs485Commands {

namespace {

quint8 clampDacScale(int value)
{
    return static_cast<quint8>(qBound(DAC_SCALE_MIN, value, DAC_SCALE_MAX));
}

QString frontName(quint8 front)
{
    switch (front) {
    case FRONT_OFF:   return QStringLiteral("关闭");
    case FRONT_RED:   return QStringLiteral("红");
    case FRONT_GREEN: return QStringLiteral("绿");
    case FRONT_TURN:  return QStringLiteral("转");
    default:
        return QStringLiteral("未知(0x%1)")
            .arg(static_cast<int>(front), 2, 16, QLatin1Char('0'))
            .toUpper();
    }
}

QString backName(quint8 back)
{
    switch (back) {
    case BACK_OFF:   return QStringLiteral("关闭");
    case BACK_RED:   return QStringLiteral("红");
    case BACK_GREEN: return QStringLiteral("绿");
    case BACK_TURN:  return QStringLiteral("转");
    default:
        return QStringLiteral("未知(0x%1)")
            .arg(static_cast<int>(back), 1, 16, QLatin1Char('0'))
            .toUpper();
    }
}

QString baudLabel(quint8 code)
{
    switch (code) {
    case 0: return QStringLiteral("9600");
    case 1: return QStringLiteral("115200");
    default: return QStringLiteral("未知(%1)").arg(static_cast<int>(code));
    }
}

} // namespace

quint8 combineDisplay(quint8 front, quint8 back)
{
    return static_cast<quint8>((front & 0xF0) | (back & 0x0F));
}

QByteArray buildDisplayStateFrame(quint8 id, quint8 front, quint8 back)
{
    return Rs485Frame::buildFrame(id, SET_DISPLAY_STATE, combineDisplay(front, back));
}

QByteArray buildQueryDisplayStateFrame(quint8 id)
{
    return Rs485Frame::buildFrame(id, QUERY_DISPLAY_STATE, 0x00);
}

QByteArray buildBrightnessFrame(quint8 id, quint8 value)
{
    return Rs485Frame::buildFrame(id, SET_BRIGHTNESS, static_cast<quint8>(value & 0xFF));
}

QByteArray buildQueryBrightnessFrame(quint8 id)
{
    return Rs485Frame::buildFrame(id, QUERY_BRIGHTNESS, 0x00);
}

QByteArray buildDeviceIdFrame(quint8 id, quint8 newId)
{
    return Rs485Frame::buildFrame(id, SET_DEVICE_ID, newId);
}

QByteArray buildBrightnessMinFrame(quint8 id, quint8 min)
{
    return Rs485Frame::buildFrame(id, SET_BRIGHTNESS_MIN, min);
}

QByteArray buildBrightnessMaxFrame(quint8 id, quint8 max)
{
    return Rs485Frame::buildFrame(id, SET_BRIGHTNESS_MAX, max);
}

QByteArray buildBaudRateFrame(quint8 id, quint8 code)
{
    return Rs485Frame::buildFrame(id, SET_BAUD_RATE, static_cast<quint8>(code & 0xFF));
}

QByteArray buildDacScaleRedFrame(quint8 id, quint8 v)
{
    return Rs485Frame::buildFrame(id, SET_DAC_SCALE_RED, clampDacScale(static_cast<int>(v)));
}

QByteArray buildDacScaleGreenFrame(quint8 id, quint8 v)
{
    return Rs485Frame::buildFrame(id, SET_DAC_SCALE_GREEN, clampDacScale(static_cast<int>(v)));
}

QString describeDisplayState(quint8 dataByte)
{
    const quint8 front = dataByte & 0xF0;
    const quint8 back = dataByte & 0x0F;
    return QStringLiteral("正面:%1 背面:%2").arg(frontName(front), backName(back));
}

QString describeBrightness(quint8 value)
{
    if (value == BRIGHTNESS_AUTO)
        return QStringLiteral("自动调光");
    return QStringLiteral("%1%").arg(static_cast<int>(value));
}

QString cmdName(quint8 cmd)
{
    switch (cmd) {
    case SET_DISPLAY_STATE:   return QStringLiteral("显示状态调节");
    case QUERY_DISPLAY_STATE: return QStringLiteral("显示状态查询");
    case SET_BRIGHTNESS:      return QStringLiteral("亮度调节");
    case QUERY_BRIGHTNESS:    return QStringLiteral("亮度状态查询");
    case SET_DEVICE_ID:       return QStringLiteral("设备ID修改");
    case SET_BRIGHTNESS_MIN:  return QStringLiteral("亮度最小值");
    case SET_BRIGHTNESS_MAX:  return QStringLiteral("亮度最大值");
    case SET_BAUD_RATE:       return QStringLiteral("波特率配置");
    case SET_DAC_SCALE_RED:   return QStringLiteral("红色亮度系数");
    case SET_DAC_SCALE_GREEN: return QStringLiteral("绿色亮度系数");
    case RESP_SET_DISPLAY_STATE:   return QStringLiteral("显示状态响应");
    case RESP_QUERY_DISPLAY_STATE: return QStringLiteral("查询状态响应");
    case RESP_SET_BRIGHTNESS:      return QStringLiteral("亮度调节响应");
    case RESP_QUERY_BRIGHTNESS:    return QStringLiteral("查询亮度响应");
    case RESP_SET_DEVICE_ID:       return QStringLiteral("ID修改响应");
    case RESP_SET_BRIGHTNESS_MIN:  return QStringLiteral("最小值响应");
    case RESP_SET_BRIGHTNESS_MAX:  return QStringLiteral("最大值响应");
    case RESP_SET_BAUD_RATE:       return QStringLiteral("波特率响应");
    case RESP_SET_DAC_SCALE_RED:   return QStringLiteral("红系数响应");
    case RESP_SET_DAC_SCALE_GREEN: return QStringLiteral("绿系数响应");
    default:
        return QStringLiteral("未知(0x%1)")
            .arg(static_cast<int>(cmd), 2, 16, QLatin1Char('0'))
            .toUpper();
    }
}

QString describeResponse(quint8 cmd, quint8 dataByte)
{
    switch (cmd) {
    case RESP_SET_DISPLAY_STATE:
    case RESP_QUERY_DISPLAY_STATE:
        return describeDisplayState(dataByte);
    case RESP_SET_BRIGHTNESS:
    case RESP_QUERY_BRIGHTNESS:
        return describeBrightness(dataByte);
    case RESP_SET_DEVICE_ID:
        return QStringLiteral("新ID: %1").arg(static_cast<int>(dataByte));
    case RESP_SET_BRIGHTNESS_MIN:
        return QStringLiteral("最小亮度: %1%").arg(static_cast<int>(dataByte));
    case RESP_SET_BRIGHTNESS_MAX:
        return QStringLiteral("最大亮度: %1%").arg(static_cast<int>(dataByte));
    case RESP_SET_BAUD_RATE:
        return QStringLiteral("波特率: %1").arg(baudLabel(dataByte));
    case RESP_SET_DAC_SCALE_RED:
        return QStringLiteral("红系数: %1 (1~40)").arg(static_cast<int>(dataByte));
    case RESP_SET_DAC_SCALE_GREEN:
        return QStringLiteral("绿系数: %1 (1~40)").arg(static_cast<int>(dataByte));
    default:
        return QString();
    }
}

} // namespace Rs485Commands
