#!/usr/bin/env python3
"""宿主推演：贵州费显协议 probe/parse 仿真 + 与青海 probe 的链式互斥验证。

镜像固件：
  Application/Src/ProtocolParser_GuiZhou/app_gz_proto.c（gz_probe_frame）
  Application/Src/ProtocolParser_GuiZhou/app_gz_proto_parse.c（gz_parse_frame）
  Application/Src/ProtocolParser_QingHai/app_qh_proto.c（qh_proto_probe_frame）

帧格式：'{' + 命令字('1'~'9','A','B',0x01,0x02) + 二进制 len + 参数 + '}'。
协议原文：协议文档 06 贵州常规费显协议（2020-06-17 修订）。

链式事实：同 RB 上 qh_proto_init 先注册（字母序 qh < gz），青海 probe 先被调用；
贵州全部 ASCII 命令字落入青海命令集（'1'~'9','A','B'）→ 全协议构建下青海先认领
贵州帧，量产由 EIDE 目录排除纪律二选一；0x01/0x02 二进制命令字青海/山东/MTC probe
首命令字快拒，由贵州 probe 认领。

用法：
  python3 gz_frame_sim.py          # 全量推演（含链式认领演示）
  python3 gz_frame_sim.py --chain  # 仅演示链式认领
"""
import sys

FAKE, WAIT, READY = 0, 1, 2


# ---------------- 固件镜像：gz_probe_frame（app_gz_proto.c） ----------------
def gz_probe_frame(buf):
    avail = len(buf)
    if avail == 0:
        return (FAKE, 0)
    if buf[0] != 0x7B:  # '{'
        return (FAKE, 0)
    if avail < 3:
        return (WAIT, 0)
    c = buf[1]
    ok = (0x31 <= c <= 0x39) or c in (0x41, 0x42, 0x01, 0x02)  # '1'~'9','A','B',0x01,0x02
    if not ok:
        return (FAKE, 0)
    frame_len = buf[2] + 4  # 二进制长度字段
    if avail < frame_len:
        return (WAIT, 0)
    if buf[frame_len - 1] != 0x7D:  # '}'
        return (FAKE, 0)
    return (READY, frame_len)


# ---------------- 固件镜像：gz_amount_to_fen（app_gz_proto_parse.c） ----------------
def gz_amount_to_fen(data):
    fen = 0
    i = 0
    while i < len(data) and 0x30 <= data[i] <= 0x39:
        fen = fen * 10 + (data[i] - 0x30)
        i += 1
    fen *= 100
    if i < len(data) and data[i] == 0x2E:  # '.'
        i += 1
        dec = 0
        nd = 0
        while i < len(data) and nd < 2 and 0x30 <= data[i] <= 0x39:
            dec = dec * 10 + (data[i] - 0x30)
            nd += 1
            i += 1
        if nd == 1:
            dec *= 10
        fen += dec
    return fen


# ---------------- 固件镜像：gz_parse_frame（app_gz_proto_parse.c） ----------------
def gz_parse_frame(raw):
    if len(raw) < 4 or raw[0] != 0x7B or raw[-1] != 0x7D:
        return ('ERR_FRAME', None)
    c = raw[1]
    name = {0x31: 'HOST_QUERY', 0x32: 'SELF_CHECK', 0x33: 'ONE_LINE', 0x34: 'FULL_SCREEN',
            0x35: 'CLEAR', 0x36: 'FIXED_FORMAT', 0x37: 'CIVIL_VOICE', 0x38: 'BRIGHTNESS',
            0x39: 'VOLUME', 0x41: 'PERIPHERAL', 0x42: 'FEE_VOICE', 0x01: 'FILL_ALL',
            0x02: 'VERSION'}.get(c)
    if name is None:
        return ('ERR_CMD', None)
    decl = raw[2]
    if len(raw) - 4 < decl:
        return ('ERR_FRAME', None)
    data = raw[3:3 + decl]
    if name == 'HOST_QUERY':
        return ('OK', name) if decl == 0 else ('ERR_PARAM', None)
    if name == 'SELF_CHECK':
        return ('OK', name) if decl == 0 else ('ERR_PARAM', None)
    if name == 'ONE_LINE':
        if decl < 3 or decl > 18:  # 边界按设备实测行为
            return ('ERR_PARAM', None)
        color, row = data[0] - 0x30, data[1] - 0x31
        if color > 2 or row > 4:
            return ('ERR_PARAM', None)
        return ('OK', f"{name} color={color} row={row + 1} text={bytes(data[2:]).decode('gbk', 'replace')!r}")
    if name == 'FULL_SCREEN':
        if decl < 4 or decl > 86:  # 边界按设备实测行为
            return ('ERR_PARAM', None)
        color = data[0] - 0x30
        if color > 2:
            return ('ERR_PARAM', None)
        tlen = decl - 3 if decl <= 71 else 64  # len>71 截断 64
        return ('OK', f"{name} color={color} x={data[1]} y={data[2]} "
                      f"text={bytes(data[3:3 + tlen]).decode('gbk', 'replace')!r}")
    if name == 'CLEAR':
        return ('OK', name) if decl == 0 else ('ERR_PARAM', None)
    if name == 'FIXED_FORMAT':
        if decl < 3 or decl > 90:  # 边界按设备实测行为
            return ('ERR_PARAM', None)
        color, typ = data[0] - 0x30, data[1] - 0x30
        if color > 2 or typ > 1:
            return ('ERR_PARAM', None)
        # 字段拆分在 cmd 层：此处镜像按 '|' 拆 5 字段（末字段到参数区尾）
        fields = []
        cur = bytes(data[2:])
        for _ in range(5):
            if 0x7C in cur:
                sep = cur.index(0x7C)
                fields.append(cur[:sep])
                cur = cur[sep + 1:]
            else:
                fields.append(cur)
                cur = b''
        while len(fields) < 5:
            fields.append(b'')
        amount = gz_amount_to_fen(fields[1])
        return ('OK', f"{name} 客货={'客' if typ == 0 else '货'} 车型={gz_amount_to_fen(fields[0]) // 100} "
                      f"金额={amount // 100}.{amount % 100:02d}元 "
                      f"信息1={fields[3].decode('gbk', 'replace')!r}")
    if name == 'CIVIL_VOICE':
        if decl != 1:
            return ('ERR_PARAM', None)
        idx = data[0] - 0x30
        return ('OK', f"{name} idx={idx}") if idx <= 3 else ('ERR_PARAM', None)
    if name == 'BRIGHTNESS':
        if decl != 1:
            return ('ERR_PARAM', None)
        lv = data[0] - 0x30
        return ('OK', f"{name} level={lv}") if lv <= 5 else ('ERR_PARAM', None)
    if name == 'VOLUME':
        if decl != 1:
            return ('ERR_PARAM', None)
        lv = data[0] - 0x30
        return ('OK', f"{name} level={lv}") if 1 <= lv <= 5 else ('ERR_PARAM', None)
    if name == 'PERIPHERAL':
        return ('OK', f"{name} 绿={bool(data[0] & 1)} 红={bool(data[0] & 2)} 黄闪={bool(data[0] & 4)}") \
            if decl == 1 else ('ERR_PARAM', None)
    if name == 'FEE_VOICE':
        if decl < 1:
            return ('ERR_PARAM', None)
        fen = gz_amount_to_fen(data)
        return ('OK', f"{name} 金额={fen // 100}.{fen % 100:02d}元({fen}分)")
    if name == 'FILL_ALL':
        if decl != 1 or not (1 <= data[0] <= 3):
            return ('ERR_PARAM', None)
        return ('OK', f"{name} color={data[0] - 1}(红/绿/黄)")
    if name == 'VERSION':
        return ('OK', name) if decl == 1 else ('ERR_PARAM', None)


# ---------------- 固件镜像：qh_proto_probe_frame（app_qh_proto.c，链式前一环） ----------------
def qh_probe_frame(buf):
    avail = len(buf)
    if avail == 0:
        return (FAKE, 0)
    if buf[0] != 0x7B:
        return (FAKE, 0)
    if avail < 3:
        return (WAIT, 0)
    c = buf[1]
    if not (0x31 <= c <= 0x39 or c in (0x41, 0x42)):  # '1'~'9','A','B'
        return (FAKE, 0)
    frame_len = buf[2] + 4
    if avail < frame_len:
        return (WAIT, 0)
    if buf[frame_len - 1] != 0x7D:
        return (FAKE, 0)
    return (READY, frame_len)


def fmt(b):
    return ' '.join(f'{x:02X}' for x in b)


def case(name, buf, expect):
    st, ln = gz_probe_frame(buf)
    got = 'READY' if st == READY else ('WAIT' if st == WAIT else 'FAKE')
    mark = 'OK ' if (got == expect) else 'FAIL'
    print(f'[{mark}] probe {name}: {fmt(buf)} → {got}')
    if st == READY and 'FAIL' not in mark:
        print(f'       parse {name}: {gz_parse_frame(buf[:ln])}')
    return 'FAIL' in mark


def chain(buf):
    """模拟 RS485/RS232 同 RB 链式探测：qh_proto_init 先注册 → 青海 probe 先被调用。"""
    st, ln = qh_probe_frame(buf)
    if st == READY:
        print(f'      链式: 青海 probe 先认领（帧长 {ln}）——贵州 probe 不会被执行')
        return
    st, ln = gz_probe_frame(buf)
    print(f'      链式: 青海 FAKE → 贵州 probe 认领（帧长 {ln}）' if st == READY else
          f'      链式: 青海 FAKE → 贵州 probe {("WAIT" if st == WAIT else "FAKE")}')


def main():
    # GBK 文本字节
    etc_lane = b'\xb3\xb5\xb5\xc0'  # 车道
    etc_closed = b'\xd2\xd1\xb9\xd8\xb1\xd5'  # 已关闭
    info1 = b'\xb9\xf3A12345'  # 贵A12345
    info2 = b'\xbd\xbb\xd2\xd7\xb3\xc9\xb9\xa6'  # 交易成功
    g6 = bytes([0x7B, 0x36, 0x21, 0x30, 0x30]) + b'1|12.5|100.00|' + info1 + b'|' + info2 + bytes([0x7D])
    tests = [
        ("'1' 主机查询（7B 31 00 7D）", [0x7B, 0x31, 0x00, 0x7D], 'READY'),
        ("'2' 自检（7B 32 00 7D）", [0x7B, 0x32, 0x00, 0x7D], 'READY'),
        ("'3' 单行示例（第二行绿色 ETC车道）",
         [0x7B, 0x33, 0x09, 0x31, 0x32, 0x45, 0x54, 0x43] + list(etc_lane) + [0x7D], 'READY'),
        ("'4' 全屏示例（居中红 ETC车道已关闭）",
         [0x7B, 0x34, 0x10, 0x30, 0x00, 0x2C, 0x45, 0x54, 0x43] + list(etc_lane) + list(etc_closed) + [0x7D], 'READY'),
        ("'5' 清屏（7B 35 00 7D）", [0x7B, 0x35, 0x00, 0x7D], 'READY'),
        ("'6' 固定格式（客车 1|12.5|100.00|贵A12345|交易成功）", list(g6), 'READY'),
        ("'7' 文明语音（7B 37 01 30 7D）", [0x7B, 0x37, 0x01, 0x30, 0x7D], 'READY'),
        ("'8' 亮度（7B 38 01 30 7D）", [0x7B, 0x38, 0x01, 0x30, 0x7D], 'READY'),
        ("'9' 音量（7B 39 01 31 7D）", [0x7B, 0x39, 0x01, 0x31, 0x7D], 'READY'),
        ("'A' 外设（7B 41 01 06 7D 红灯+黄闪）", [0x7B, 0x41, 0x01, 0x06, 0x7D], 'READY'),
        ("'B' 费额语音（7B 42 05 31 32 33 2E 34 7D 123.4）",
         [0x7B, 0x42, 0x05, 0x31, 0x32, 0x33, 0x2E, 0x34, 0x7D], 'READY'),
        ("0x01 全屏点亮黄色（7B 01 01 03 7D）", [0x7B, 0x01, 0x01, 0x03, 0x7D], 'READY'),
        ("0x02 版本号（7B 02 01 00 7D）", [0x7B, 0x02, 0x01, 0x00, 0x7D], 'READY'),
        ("'3' len=2 无文本 → ERR_PARAM（边界按设备实测行为）", [0x7B, 0x33, 0x02, 0x30, 0x31, 0x7D], 'READY'),
        ("'3' len=19 → ERR_PARAM", [0x7B, 0x33, 0x13] + [0x41] * 19 + [0x7D], 'READY'),
        ("'4' len=3 无文本 → ERR_PARAM", [0x7B, 0x34, 0x03, 0x30, 0x00, 0x00, 0x7D], 'READY'),
        ("'4' len=87 → ERR_PARAM", [0x7B, 0x34, 0x57, 0x30, 0x00, 0x00] + [0x41] * 84 + [0x7D], 'READY'),
        ("'7' idx='4' → ERR_PARAM", [0x7B, 0x37, 0x01, 0x34, 0x7D], 'READY'),
        ("'9' level='0' → ERR_PARAM", [0x7B, 0x39, 0x01, 0x30, 0x7D], 'READY'),
        ("0x01 值 04 → ERR_PARAM", [0x7B, 0x01, 0x01, 0x04, 0x7D], 'READY'),
        ("非法命令字 'C'（0x43）→ FAKE", [0x7B, 0x43, 0x00, 0x7D], 'FAKE'),
        ("半帧（7B 33 09 31）→ WAIT", [0x7B, 0x33, 0x09, 0x31], 'WAIT'),
        ("尾字节错（7B 35 00 7E）→ FAKE", [0x7B, 0x35, 0x00, 0x7E], 'FAKE'),
        ("非 '{' 首字节 → FAKE", [0x0A, 0x46, 0x0A], 'FAKE'),
        ("空缓冲 → FAKE", [], 'FAKE'),
    ]
    fail = 0
    for name, buf, expect in tests:
        fail += case(name, buf, expect)
    print()
    if '--chain' in sys.argv or True:
        print('链式认领演示（全协议构建）：')
        chain([0x7B, 0x31, 0x00, 0x7D])                                   # '1' → 青海先认领
        chain(list(g6))                                                   # '6' → 青海先认领
        chain([0x7B, 0x39, 0x01, 0x31, 0x7D])                             # '9' → 青海先认领（语义差异：贵州=音量）
        chain([0x7B, 0x01, 0x01, 0x03, 0x7D])                             # 0x01 → 青海 FAKE，贵州认领
        chain([0x7B, 0x02, 0x01, 0x00, 0x7D])                             # 0x02 → 青海 FAKE，贵州认领
        chain([0x0A, 0x46, 0x0A])                                         # 非 '{' → 双 FAKE
    print()
    print('FAIL' if fail else 'ALL PASS', f'({fail} failed)')
    sys.exit(1 if fail else 0)


if __name__ == '__main__':
    main()