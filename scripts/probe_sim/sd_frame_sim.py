#!/usr/bin/env python3
"""宿主推演：山东费显协议 probe/parse 仿真 + 与青海 probe 的链式互斥验证。

镜像固件：
  Application/Src/ProtocolParser_ShanDong/app_sd_proto.c（sd_probe_frame）
  Application/Src/ProtocolParser_ShanDong/app_sd_proto_parse.c（sd_parse_frame）
  Application/Src/ProtocolParser_QingHai/app_qh_proto.c（qh_proto_probe_frame）

帧格式：'{' + 命令字('1'~'5','7','8') + 二进制 len + 参数 + '}'。

链式事实：同 RB 上 qh_proto_init 先注册（字母序 qh < sd），青海 probe 先被调用；
山东全部命令字落入青海命令集（'1'~'9','A','B'）→ 全协议构建下青海先认领山东帧，
量产由 EIDE 目录排除纪律二选一。

用法：
  python3 scripts/probe_sim/sd_frame_sim.py          # 全量推演（含链式认领演示）
  python3 scripts/probe_sim/sd_frame_sim.py --chain  # 仅演示链式认领
"""
import sys

FAKE, WAIT, READY = 0, 1, 2


# ---------------- 固件镜像：sd_probe_frame（app_sd_proto.c） ----------------
def sd_probe_frame(buf):
    avail = len(buf)
    if avail == 0:
        return (FAKE, 0)
    if buf[0] != 0x7B:  # '{'
        return (FAKE, 0)
    if avail < 3:
        return (WAIT, 0)
    c = buf[1]
    ok = (0x31 <= c <= 0x35) or c == 0x37 or c == 0x38  # '1'~'5','7','8'
    if not ok:
        return (FAKE, 0)
    frame_len = buf[2] + 4  # 二进制长度字段
    if avail < frame_len:
        return (WAIT, 0)
    if buf[frame_len - 1] != 0x7D:  # '}'
        return (FAKE, 0)
    return (READY, frame_len)


# ---------------- 固件镜像：sd_parse_frame（app_sd_proto_parse.c） ----------------
def sd_parse_frame(raw):
    if len(raw) < 4 or raw[0] != 0x7B or raw[-1] != 0x7D:
        return ('ERR_FRAME', None)
    c = raw[1]
    name = {0x31: 'FILL_ALL', 0x32: 'VERSION', 0x33: 'ONE_LINE', 0x34: 'FULL_SCREEN',
            0x35: 'CLEAR', 0x37: 'BRIGHTNESS', 0x38: 'PERIPHERAL'}.get(c)
    if name is None:
        return ('ERR_CMD', None)
    decl = raw[2]
    if len(raw) - 4 < decl:
        return ('ERR_FRAME', None)
    data = raw[3:3 + decl]  # 固件按 declared_len 切片，帧尾 '}' 不含在参数内
    if name == 'FILL_ALL':
        if decl != 1 or not (1 <= data[0] <= 3):
            return ('ERR_PARAM', None)
        return ('OK', f"{name} color={data[0] - 1}(红/绿/黄)")
    if name == 'VERSION':
        return ('OK', name) if decl == 1 else ('ERR_PARAM', None)
    if name == 'ONE_LINE':
        if decl < 2:
            return ('ERR_PARAM', None)
        color, row = data[0] - 0x30, data[1] - 0x31
        if color > 2 or row > 4:
            return ('ERR_PARAM', None)
        return ('OK', f"{name} color={color} row={row + 1} text={bytes(data[2:]).decode('gbk', 'replace')!r}")
    if name == 'FULL_SCREEN':
        if decl < 3:
            return ('ERR_PARAM', None)
        color = data[0] - 0x30
        if color > 2:
            return ('ERR_PARAM', None)
        return ('OK', f"{name} color={color} x={data[1]} y={data[2]} text={bytes(data[3:]).decode('gbk', 'replace')!r}")
    if name == 'CLEAR':
        return ('OK', name) if decl == 0 else ('ERR_PARAM', None)
    if name == 'BRIGHTNESS':
        lv = data[0] - 0x30
        return ('OK', f"{name} level={lv}") if decl == 1 and lv <= 5 else ('ERR_PARAM', None)
    if name == 'PERIPHERAL':
        return ('OK', f"{name} 绿={bool(data[0] & 1)} 红={bool(data[0] & 2)} 黄闪={bool(data[0] & 4)}") if decl == 1 else ('ERR_PARAM', None)


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
    st, ln = sd_probe_frame(buf)
    got = 'READY' if st == READY else ('WAIT' if st == WAIT else 'FAKE')
    mark = 'OK ' if (got == expect) else 'FAIL'
    print(f'[{mark}] probe {name}: {fmt(buf)} → {got}')
    if st == READY and 'FAIL' not in mark:
        print(f'       parse {name}: {sd_parse_frame(buf[:ln])}')
    return 'FAIL' in mark


def chain(buf):
    """模拟 RS485/RS232 同 RB 链式探测：qh_proto_init 先注册 → 青海 probe 先被调用。"""
    st, ln = qh_probe_frame(buf)
    if st == READY:
        print(f'      链式: 青海 probe 先认领（帧长 {ln}）——山东 probe 不会被执行')
        return
    st, ln = sd_probe_frame(buf)
    print(f'      链式: 青海 FAKE → 山东 probe 认领（帧长 {ln}）' if st == READY else
          f'      链式: 青海 FAKE → 山东 probe {("WAIT" if st == WAIT else "FAKE")}')


def main():
    tests = [
        ("'3' 单行示例1（7B 33 03 30 31 41 7D）", [0x7B, 0x33, 0x03, 0x30, 0x31, 0x41, 0x7D], 'READY'),
        ("'3' 单行示例2（ETC车道）", [0x7B, 0x33, 0x09, 0x31, 0x32, 0x45, 0x54, 0x43, 0xB3, 0xB5, 0xB5, 0xC0, 0x7D], 'READY'),
        ("'4' 全屏示例（ETC车道已关闭）", [0x7B, 0x34, 0x10, 0x30, 0x00, 0x2C, 0x45, 0x54, 0x43, 0xB3, 0xB5, 0xB5, 0xC0, 0xD2, 0xD1, 0xB9, 0xD8, 0xB1, 0xD5, 0x7D], 'READY'),
        ("'5' 清屏（7B 35 00 7D）", [0x7B, 0x35, 0x00, 0x7D], 'READY'),
        ("'7' 亮度（7B 37 01 30 7D）", [0x7B, 0x37, 0x01, 0x30, 0x7D], 'READY'),
        ("'8' 外设（7B 38 01 06 7D）", [0x7B, 0x38, 0x01, 0x06, 0x7D], 'READY'),
        ("'1' 全屏单色（7B 31 01 03 7D）", [0x7B, 0x31, 0x01, 0x03, 0x7D], 'READY'),
        ("'2' 版本（7B 32 01 00 7D）", [0x7B, 0x32, 0x01, 0x00, 0x7D], 'READY'),
        ("无 '6'（7B 36 00 7D）→ FAKE", [0x7B, 0x36, 0x00, 0x7D], 'FAKE'),
        ("青海 '9' 音量（非山东命令）→ FAKE", [0x7B, 0x39, 0x01, 0x31, 0x7D], 'FAKE'),
        ("半帧（7B 33 09 31）→ WAIT", [0x7B, 0x33, 0x09, 0x31], 'WAIT'),
        ("尾字节错（7B 35 00 7E）→ FAKE", [0x7B, 0x35, 0x00, 0x7E], 'FAKE'),
        ("非 '{' 首字节 → FAKE", [0x0A, 0x46, 0x0A], 'FAKE'),
        ("空缓冲 → FAKE", [], 'FAKE'),
        ("'3' 行号越界（'6'）→ ERR_PARAM", [0x7B, 0x33, 0x03, 0x30, 0x36, 0x41, 0x7D], 'READY'),
        ("'7' 亮度越界（'6'）→ ERR_PARAM", [0x7B, 0x37, 0x01, 0x36, 0x7D], 'READY'),
    ]
    fail = 0
    for name, buf, expect in tests:
        fail += case(name, buf, expect)
    print()
    if '--chain' in sys.argv or True:
        print('链式认领演示（全协议构建）：')
        chain([0x7B, 0x33, 0x03, 0x30, 0x31, 0x41, 0x7D])   # 山东 '3' → 青海先认领
        chain([0x7B, 0x37, 0x01, 0x30, 0x7D])               # 山东 '7' → 青海先认领（语义分歧）
        chain([0x7B, 0x39, 0x01, 0x31, 0x7D])               # 青海 '9' → 青海认领
        chain([0x0A, 0x46, 0x0A])                           # 非 '{' → 双 FAKE
    print()
    print('FAIL' if fail else 'ALL PASS', f'({fail} failed)')
    sys.exit(1 if fail else 0)


if __name__ == '__main__':
    main()