#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct ReportIpInfo {
    QString ip;
    QString mask;
    QString gateway;
    quint16 appPort = 0;
    bool valid = false;
};

struct StatusInfo {
    quint32 fwSize = 0;
    quint32 fwCrc = 0;
    QString version;
    int upgradeState = -1;
    bool valid = false;
};

class IapCommands {
public:
    // Host -> device command codes.
    static constexpr quint32 CMD_REPORT_IP = 0x00004B01u;
    static constexpr quint32 CMD_SET_IP = 0x00004B02u;
    static constexpr quint32 CMD_QUERY_STATUS = 0x00004B03u;
    static constexpr quint32 CMD_ERASE_FW = 0x00004B04u;
    static constexpr quint32 CMD_TRANSFER_FW = 0x00004B05u;
    static constexpr quint32 CMD_ENTER_RECOVERY = 0x00004B06u;
    static constexpr quint32 CMD_REBOOT = 0x00004B07u;

    // Device -> host response codes.
    static constexpr quint32 RESP_REPORT_IP = 0x0000B401u;
    static constexpr quint32 RESP_SET_IP = 0x0000B402u;
    static constexpr quint32 RESP_QUERY_STATUS = 0x0000B403u;
    static constexpr quint32 RESP_ERASE_FW = 0x0000B404u;
    static constexpr quint32 RESP_TRANSFER_FW = 0x0000B405u;
    static constexpr quint32 RESP_ENTER_RECOVERY = 0x0000B406u;
    static constexpr quint32 RESP_REBOOT = 0x0000B407u;

    // Upgrade state machine values.
    static constexpr quint32 UPGRADE_STATE_SUCCESS = 0u;
    static constexpr quint32 UPGRADE_STATE_IN_PROGRESS = 1u;
    static constexpr quint32 UPGRADE_STATE_FAILED = 2u;

    // Transport configuration.
    // IAP_PORT：设备 IAP 协议固定监听的 UDP 口（设备端 CH_ID_UDP）。搜索广播与
    // 升级/重启/Recovery 单播均发往此口。注意：设备 4B01 应答第 4 word 上报的
    // 端口是 TCP 业务口（Sector1 net_cfg.port，如 9528），仅作 UI 显示/第三方
    // 工具连接参考，不得用作 IAP 单播目标端口（2026-08-24 修复）。
    static constexpr quint16 IAP_PORT = 10011;
    inline static const QString DEFAULT_BROADCAST_IP = QStringLiteral("192.168.114.200");
    static constexpr quint16 DEFAULT_PORT = 10011;
    static constexpr int PAGE_SIZE_WORDS = 256;
    static constexpr int MAX_PAYLOAD_WORDS = 256;

    static QByteArray buildReportIpRequest();
    static QByteArray buildSetIpRequest(const QString& ip, const QString& mask,
                                        const QString& gateway, quint16 port);
    static QByteArray buildQueryStatusRequest();
    static QByteArray buildEraseRequest(quint32 firmwareSizeBytes);
    static QByteArray buildTransferFrame(quint32 seq, const QVector<quint32>& firmwareWords,
                                         int offset, int pageSize = PAGE_SIZE_WORDS);
    static QByteArray buildEnterRecoveryRequest();
    static QByteArray buildRebootRequest();

    static ReportIpInfo parseReportIpResponse(const QVector<quint32>& words);
    static StatusInfo parseStatusResponse(const QVector<quint32>& words);
    // 4B02 setip 应答两态解析：主固件 1 word 结果码（0=成功）或 Recovery
    // 空载荷 ACK 均视为成功；非 0 结果码为失败。
    static bool parseSetIpResponse(const QVector<quint32>& words);
    static bool parseEraseResponse(const QVector<quint32>& words);
    static QVector<quint32> parseTransferResponse(const QVector<quint32>& words);
};
