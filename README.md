# CD DebugTool

创迪科技多协议调试工具（C++ / Qt 5.15 版）。一个程序整合七套设备调试协议，平级切换，共享串口 / 网络连接与日志面板。

> 本仓库是旧 Python（PySide6）版 `indicator-debug-tool` 的 C++ 重写，功能对齐并修复了两个既有 bug（见文末「历史修复」）。

## 功能

- **七协议平级**：启动时按协议全名选择，主窗口顶部下拉可随时切换。
  - **青海高速费显协议**（qinghai）— 全彩费显屏，帧 `7B cmd len data 7D`
  - **云南费显协议**（yunnan）— 云南LED费显P5（协议版本 YN_FX_P5_1.0），帧 `{ cmd len data }`，13 命令：查询/自检/单行/全屏可编辑/清屏/语音/亮度/音量/外设/费额语音/全屏点亮（设备扩展七色 01红~07白）/版本号
  - **IAP 远程升级**（iap）— 设备扫描 / 固件下发 / 远程升级（支持串口 + UDP）/ 设备配置下发（setip）/ 固件状态查询
  - **重庆创迪车道指示器**（rs485）— 显示状态 / 亮度 / DAC 系数 / 波特率等
  - **四川 ETC 费显协议**（sichuan_etc）— 静态/滚屏显示、灯控、亮度、心跳，帧 `0A ... 0D`
  - **四川 MTC 费显协议**（sichuan_mtc）— 点阵显示、固定格式、语音、亮度/音量/颜色，帧 `{ cmd 参数 BCC }`
  - **四川治超屏协议**（sichuan_ol）— 全屏/行显示、亮度、通行灯/黄闪、状态查询，帧 `FF len cmd bright data BCC FF`
- **统一连接面板**：串口（端口 / 波特率）与 UDP（IP / 端口）复用同一套 UI。
- **统一日志 / 十六进制监视**：收发字节、帧解析结果实时滚动。
- **主题**：深色 / 浅色 QSS，自动跟随系统。

## 依赖

### 构建期（Linux）

- Qt 5.15（`Widgets` `SerialPort` `Network` `Test`）
- CMake ≥ 3.16、Ninja、GCC（C++17）

Debian / Ubuntu 安装构建依赖：

```bash
sudo apt install build-essential cmake ninja-build \
  qtbase5-dev libqt5serialport5-dev
```

### 运行期（Linux）

安装包已声明依赖（见 `debian/control`），核心库用 `t64` 后缀：

- `libqt5core5t64` / `libqt5gui5t64` / `libqt5widgets5t64` / `libqt5network5t64`
- `libqt5serialport5`（**无 t64**）
- 中文字体：界面默认请求 `Noto Sans CJK SC`，缺失时回退系统默认中文字体。

## 构建

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物：`build/cd-debugtool`

## 测试

```bash
ctest --test-dir build --output-on-failure
```

143 项单元测试，覆盖 RS485 帧 / IAP 帧与 CRC32(MPEG-2)（含 4B01 应答端口语义与 IAP 单播口常量断言、4B02 setip 帧字节断言与应答两态解析、4B03 应答版本大端 word 解析）/ 青海协议帧与解析、云南协议帧与解析（13 命令帧拼接、查询/版本号应答解析），四川 ETC / MTC / 治超屏三个协议的帧拼接、BCC 校验与应答解析，以及配置持久化往返（ConfigManager）与升级引擎取消路径聚合（UpgradeEngine）。

> 升级流程修复后测试增至 **135 项**（2026-08-25）：新增 Intel HEX 解析（多记录/扩展线性地址/间隙 0xFF 填充/坏校验和拒绝/缺 EOF 拒绝/未知类型拒绝）、固件 CRC 向量（0xFF 填充 + crc32Mpeg2Words，与设备 Recovery `HAL_CRC_Calculate` 一致）、4B03/4B04/4B05/4B06/4B07 帧字节断言、缺失帧重传构造（seq=idx+1、末页裁剪）断言。
>
> 三项修复后测试增至 **140 项**（2026-08-25）：新增 Intel HEX 04 高基址裁剪回归（134MB bug）、16 MiB 跨度 sanity 拒绝、4B02 setip 帧字节断言（IP 大端 4 word + port word + 帧 CRC）、4B02 应答两态解析（主固件结果码 / Recovery 空 ACK）、4B03 应答解析（大端版本 word → "9K1F3127E2"，含尾部 NUL 裁剪）。
>
> type 02 支持后测试增至 **143 项**（2026-08-25）：新增 Intel HEX 02（扩展段地址）样本解析（段地址 ×16 基址 + 间隙 0xFF 填充 + 02 重定位）、objcopy `-I binary -O ihex` 生成 hex 回归（fixture `tests/data/t02_project_std.{hex,bin}`，断言 size=234404 且与 bin 逐字节一致）。

## 打包

### Linux `.deb`

```bash
scripts/build_deb.sh 1.0.0
```

产出 `dist/linux/cd-debugtool_1.0.0_amd64.deb`，安装：

```bash
sudo dpkg -i dist/linux/cd-debugtool_1.0.0_amd64.deb
```

安装后从应用菜单启动，或命令行 `/usr/bin/cd-debugtool`。

裸可执行文件副本发布在 `dist/linux/cd-debugtool`（动态链接系统 Qt5，仅供本机/同环境直接运行调试）。

### Windows 单文件 `.exe`（Win7 x64 可用）

静态链接 Qt，零外部依赖，拷到 Win7 双击即开。产出 `dist/windows/cd-debugtool.exe`（约 32 MB，2026-08-31 在 Windows 上以修复后源码重建，八协议版含云南/山东协议，详见 `dist/windows/README.md`）。

构建配方（一次性，需 MinGW 8.1.0 + 静态 Qt 5.15.2，仅 `qtbase` + `qtserialport`）：

- 用 **MinGW 8.1.0 posix-seh（msvcrt 运行时，非 UCRT）**，天然兼容 Win7。
- Qt 静态 configure：`-static -static-runtime -WINVER=0x0601 -D_WIN32_WINNT=0x0601`，其余模块全部 `-skip`。
- `CMakeLists.txt` 已内置 `if(WIN32)` 分支（2026-08-31，无需手动追加）：`WIN32_EXECUTABLE TRUE`、链接 `Qt5::WinMain`、`target_link_options(-static -static-libgcc -static-libstdc++)`；`src/main.cpp` 条件编译 `Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)`。
- 实测构建机（2026-08-31）：局域网 Win11 笔记本（192.168.1.127，SSH 别名 `win11-laptop`），MinGW 8.1.0 posix-seh + 静态 Qt 5.15.2（`D:\Qt\5.15.2\mingw81_64_static`，64 位套件）+ CMake 3.28.1 / Ninja（STM32CubeCLT 自带）：

```bat
set PATH=D:\Qt\Tools\Tools\mingw810_64\bin;%PATH%
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=D:\Qt\5.15.2\mingw81_64_static
cmake --build build-win --target cd-debugtool -j 8
```

> MinGW Makefiles 生成器依赖 sh.exe（MinGW 发行版通常不带），有 Ninja 时优先用 Ninja。
> 静态套件未编入 qtsvg 时 SVG 图标不显示（仅外观，不影响功能）。

## 配置

首次启动自动生成 `~/.config/cd-debugtool/config.json`（Windows 为 `%APPDATA%\cd-debugtool\`），并尝试从旧版 `indicator-debug-tool` 配置一次性迁移。

运行中自动保存、下次启动自动恢复：

- **窗口几何与当前协议页** —— 退出时保存，启动时恢复窗口大小/位置，登录对话框默认预选上次使用的协议。
- **串口端口与波特率** —— 打开串口成功时保存，各页连接面板构造时预选。
- **固件路径** —— IAP 页选择固件成功后保存，下次进入自动预填。

## 目录结构

```
src/
  core/      协议抽象（IProtocolPage / ProtocolRegistry / DeviceManager / UpgradeEngine）
  transport/ 串口 / UDP 传输（串口运行于独立线程）
  protocol/  协议帧定义与解析（qinghai / yunnan / iap / rs485 / sichuan_etc / sichuan_mtc / sichuan_ol / shandong）
  ui/        主窗口、登录选择、协议页（串口协议页基类 SerialProtocolPage）、共享控件（widgets/）
  config/    配置持久化（QJson，无第三方依赖）
tests/       单元测试（test_*.cpp）
  data/      测试 fixture（Intel HEX / bin 回归样本，经 TEST_DATA_DIR 注入测试）
resources/   QSS 主题 + 图标
debian/      .deb 打包元数据（control / desktop / postinst / prerm）
scripts/     打包与仿真脚本
  probe_sim/ 设备帧仿真脚本（贵州 / 山东 / 四川 MTC / 帧解析审阅，串口直连调试用）
build/       CMake 构建产物（已 gitignore）
.cache/      clangd 索引缓存（已 gitignore）
dist/        发布产物（已 gitignore，仅各平台 README.md 跟踪）
  windows/   Windows 单文件 exe 交付（2026-08-31 重建，八协议版，见上文配方）
  linux/     Linux 可执行文件与 .deb 包
```

## 协议速览

### 青海高速费显（qinghai）

帧封套 `0x7B | cmd(ASCII) | len(二进制字节) | data | 0x7D`，无校验。仅 `'1'`（查询）有应答 `7B 31 01 00 7D`（`0x00` 正常）。文本用 GBK。命令：`1` 查询 / `2` 自检 / `3` 单行 / `4` 全屏 / `5` 清屏 / `6` 固定显示 / `7` 文明语音(0–3) / `8` 亮度 / `9` 音量 / `A` 外设(位掩码) / `B` 费额语音(单位为分)。

### 云南费显（yunnan，协议版本 YN_FX_P5_1.0）

帧 `{ cmd len data }`（`0x7B … 0x7D`），无校验，串口 9600~115200 默认 9600、8N1。文本 GBK（GB2312）。命令：`'1'` 查询（应答 `7B 31 01 00 7D`，设备恒回正常）/ `'2'` 自检 / `'3'` 单行显示（颜色'0'~'2' + 行号'1'~'5' + 文本）/ `'4'` 全屏可编辑（颜色 + X + Y + 文本）/ `'5'` 全屏清除 / `'6'` 单行清除（行号'1'~'5'）/ `'7'` 礼貌语音('0'~'3') / `'8'` 亮度（**0x00 自动 + ASCII '1'~'8' 手动档**，8 最亮）/ `'9'` 音量('1'~'5') / `'A'` 外设（bit0 绿灯 bit1 红灯 bit2 黄闪）/ `'B'` 费额语音（金额 ASCII 串，0 元不播）。设备侧扩展：`0x01` 全屏点亮七色（01红/02绿/03黄/04蓝/05紫/06青/07白）；`0x02` 获取版本号（`7B 02 01 00 7D`，设备回裸 ASCII PROGRAM_CODE，协议文档约定 YN_FX_P5_1.0）。

### IAP 远程升级（iap）

主从命令码成对（请求 `0x00004Bxx`，响应 `0x0000B4xx`）：上报 IP / 设置 IP / 查询状态 / 擦除固件 / 传输固件 / 进入 Recovery / 重启。帧 CRC 为 CRC32(MPEG-2) **按字流计算**（每个 32-bit word 大端序列化后计算，结果按小端写入帧尾）——对齐设备端 STM32F4 硬件 CRC 单元（`HAL_CRC_Calculate` 逐 word MSB-first 喂入，等效于逐 word 字节反序）与 Windows 参考工具（2026年通用远程升级控制软件）。上报 IP 命令（`4B01`）经 UDP 广播（255.255.255.255:10011）发送，设备应答 `B401` 亦为广播，载荷含设备 IP/掩码/网关/端口，以载荷 IP 作为设备标识。

**升级流程**（2026-08-25 修复，与设备端/Java 参考工具对齐）：点「开始升级」后依次执行 `4B06`（主固件置 RTC backup `FLAG_FORCE_UPDATE` 并 ACK，不重启；无 ACK 明确提示「设备未响应进入升级模式」）→ `4B07` 重启（Bootloader 条件 A 跳 Recovery）→ 等设备以 `4B01` 广播重新上线（IP 不变，最长 30s）→ `4B04` 擦除（Recovery 真正擦写 flash）→ `4B05` 传输（每包 256 word，间隔 **50ms** 对齐 Java，默认值；末帧 `rtn_cmd05` 报 0-based 缺失帧索引列表，缺帧按 seq=idx+1 重传、补齐整轮直到空列表）→ `4B03` 轮询验证（每 500ms、最长 60s，`update_sta==0` 即成功，并比对设备上报 size/CRC 与本机计算值）→ `4B07` 重启回主固件。固件 CRC = **0xFF 填充到 4B 对齐 + `crc32Mpeg2Words`**（逐 word 大端 MPEG-2），与设备 Recovery `HAL_CRC_Calculate`（word 流大端）及 Java `CRC32_OR_MPEG_2(int[])` 一致。固件文件支持 **Intel HEX**（记录类型 00/01/02/04/05，校验和验证、扩展段地址与扩展线性地址、间隙 0xFF 填充）与 `.bin` 原样加载。

**设备配置与状态查询**（2026-08-25 新增）：左侧「设备配置」区显示当前选中设备的 IP/掩码/网关/端口（4B01 应答），四个可编辑输入框 + 「下发配置」按钮 → `4B02` setip 单播到设备 IP:10011（与 4B06/4B07 一致）；输入校验 IPv4 合法性与端口 1~65535；应答两态判定（主固件 1 word 结果码 0=成功 / Recovery 空载荷 ACK），成功后提示「已下发，设备重启后生效，请重新搜索设备」。「获取固件状态」按钮发 `4B03` 单播，展示 size/crc32/version（大端 word 修复后显示 10 位 PROGRAM_CODE）/update_sta（升级完成/进行中/失败文案）。重启（4B07）与进入恢复模式（4B06）按钮位于升级面板「操作」区。

### 重庆创迪车道指示器（rs485）

命令 `0x01–0x0A`，响应 `命令 + 0x80`：显示状态 / 查询显示状态 / 亮度 / 查询亮度 / 设备 ID / 亮度下限 / 亮度上限 / 波特率 / 红灯 DAC 系数 / 绿灯 DAC 系数。显示状态用高/低半字节区分前后灯（灭/红/绿/转向）。

### 四川 ETC 费显（sichuan_etc）

帧 `0x0A | 命令位 | 参数 | 0x0D`，波特率 115200。静态显示 `0A 00 行号(0全屏/1~6) 数据(GBK，全屏≤56B/单行≤24B) 0D`；滚屏 `0A 01 00 md rt st 数据 0D`；数据首字节 `0x20` 清屏、`0x30` 初始化（复位）。灯控 `0A 36/37/38/39 0D`（红/绿/黄闪开/黄闪关）；亮度 `0A 40 00~07 00 0D`；心跳 `0A 50 0D`。设备应答 `0A 00/01/02 0D`（正常/超长/帧错）。

### 四川 MTC 费显（sichuan_mtc）

帧 `{ cmd('1'~'9','A') 参数 [BCC] }`，BCC 为命令字(含)到参数(含)异或，波特率 115200。`1` 初始化 / `2` 自检 / `3` 单行(16B 定长) / `4` 全屏(64B 定长) / `5` 清屏 / `6` 固定显示(客车 12B/货车 21B) / `7` 语音('0'~'7' 固定，'8' 自定义 GBK) / `8` 亮度 / `9` 音量 / `A` 颜色。扩展帧族（无 BCC）：`7B 40` 波特率、`7B 41` 点阵、`7B 42` 字体、`7B 43` 协议类型、`7B 44` 全屏点亮、`7B 45` 版本号（应答 `SC_FX_P7.62_1.0`）。`0A 46 0A` 查询（应答 `0A 64 0A`）、`0A 46 0D` 清屏。

### 四川治超屏（sichuan_ol）

帧 `FF | 长度(含头尾 07~1E) | 命令 | 亮度(00~FF) | 数据 | BCC | FF`，BCC 为帧头到数据段逐字节异或，波特率 9600。`80` 全屏显示（数据≤24B）、`81~88` 行显示（16B 定长）；`94` 清屏、`96` 亮度（00=自动调光）、`99` 通行灯（00红/01绿）、`98` 黄闪（00关/01开）均为 7 字节短帧；查询 `A0` 显示内容（应答 A1~A8 行帧）、`B6` 亮度、`B9` 通行灯、`B8` 黄闪。

## 历史修复（相对旧 Python 版）

1. **IAP CRC 字节序**：线上 CRC 为 CRC32(MPEG-2) 按字流计算（搜索帧 `4B01` 无载荷时为 `0x84488377`），对齐设备端 STM32F4 硬件 CRC 与 Windows 参考工具硬编码帧字节；旧版按逐字节计算（`0x84116DF6`）导致设备 CRC 校验失败、搜索无响应。
2. **`UPGRADE_DONE` 未赋值**：升级成功状态未触发；现映射为 `UpgradeDone`。

## 历史修复（IAP 升级流程，2026-08-25）

6. **升级流程全链路修复**：旧流程不进入 Recovery、不发 `4B03` 查询、`.hex` 按 ASCII 原文当固件、无缺失帧重传、包间隔 5ms、固件 CRC 用逐字节算法——升级必失败。现按「`4B06`→ACK→`4B07` 重启→等 `4B01` 重新上线→`4B04`→`4B05` 整轮传输+缺失帧重传（50ms 间隔，对齐 Java）→`4B03` 轮询验证→`4B07` 回主固件」全流程实现；固件 CRC 改为 0xFF 填充 + `crc32Mpeg2Words`（与设备 Recovery `HAL_CRC_Calculate` 及 Java `CRC32_OR_MPEG_2(int[])` 一致）；新增 Intel HEX 解析支持。

## 历史修复（Linux 端搜索不到设备，2026-08-25）

3. **UDP 广播未开 SO_BROADCAST**：QUdpSocket 底层 socket 默认不带 SO_BROADCAST（Qt 5.15 无可移植 API，Qt 6.2 才引入 `BroadcastSocketOption`），Linux 下向 255.255.255.255 / 定向广播 sendto 返回 EACCES（`[Errno 13] Permission denied`，本机 Python 对照实验实证），搜索帧从未上链路。Java 参考工具 `DatagramSocket` 构造即默认开启（`getBroadcast()==true`），Windows 端无此问题。现 `UdpTransport::doBind` 绑定成功后原生 `setsockopt(SO_BROADCAST)`，并有单元测试用 `getsockopt` 断言。
4. **接收 socket 绑定具体网卡 IP 收不到广播应答**：设备 `4B01` 应答以广播发出（tcpdump 实证 `192.168.114.200.10011 > 255.255.255.255.10011`，B 标志），Linux 不把全局广播入站报文投递给绑定具体单播 IP 的 socket（本机对照实验：绑 192.168.0.17 收到 0 包、绑 0.0.0.0 正常收应答）。现 UDP 恒通配绑定 `0.0.0.0`（对齐 Java 工具语义）。
5. **广播只发所选网卡定向地址，漏其他网段**：开发机单网卡挂 192.168.0/1/2/114.x 四个网段，旧逻辑按所选网卡 IP 只发该网段定向广播（默认选中 192.168.0.17 → 只发 192.168.0.255），114 网段设备永远收不到。现搜索帧同时发 255.255.255.255 全局广播 + 本机所有 up 接口的定向广播，任何网段的设备都能搜到；全部发送失败时经 `errorOccurred` 上报日志（不再静默）。

## 历史修复（IAP 4B04 擦除超时，2026-08-25）

7. **广播顺序错误 → Recovery 锁定错误远端 → 单播被拒收**（真机「4B06 确认→4B07 重启→设备重新上线→4B04 擦除 30s 无 ACK」根因，在线 tcpdump + 对照实验实证）：
   - **设备端语义**（`STM32F407-Recovery/LWIP/App/udp_conn.c`）：`udp_receive_callback` 收到**第一帧**即 `udp_connect(源ip:源port)` 锁定 pcb；此后 LwIP 只接受与该锁定远端一致的源 ip:port，其余单播回 ICMP `udp port 10011 unreachable`。主固件（netconn 绑定、不 connect）无此限制——所以 4B06/4B07 阶段（主固件）正常、重启进 Recovery 后 4B04 超时。
   - **工具端缺陷**：`doSendBroadcast` 原顺序「先 255.255.255.255 全局广播、后各接口定向广播」。本机多网段（默认路由 192.168.0.1）时全局广播源 IP = 192.168.0.17；设备先收到全局广播即锁定 (192.168.0.17:10011)，随后定向广播 (源 192.168.114.17) 与 4B04 单播（走 114 子网路由、源 114.17）全部被拒收。在线实验：同 socket 广播后单播 4B03 → tcpdump 见 `192.168.114.200 > 192.168.114.17: ICMP udp port 10011 unreachable`；改「定向广播先行、全局广播最后」后设备锁定 114.17，单播 4B03 正常获得 rtn_cmd03 应答。
   - **修复**：`doSendBroadcast` 改为**先发各接口定向广播、最后发全局广播**。定向广播源 IP = 所在子网地址，与发往该子网设备的单播源 IP 一致（同一条内核路由）；全局广播最后发仅兜底「设备不在本机任何子网」场景（此时单播走默认路由、源地址同样一致）。整个升级会话仍复用同一绑定 10011 的 UDP socket（单例 + 独立线程，生命周期不在会话中被重建）。
   - **验证**：在线设备 192.168.114.200（Recovery 态）只读帧全流程验证通过——定向广播先行 → 设备 rtn_cmd01 应答并锁定 114.17 → 后续全局广播被设备拒收（无应答，符合预期）→ 同一 socket 单播 4B03 → rtn_cmd03 完整应答（IP/mask/gw/port=9528/版本）。`4B04` 擦写 + `4B05` 传输真机全流程仍待用户最终验证。

## 历史修复（IAP 三项修复，2026-08-25）

8. **Intel HEX 04 记录 134MB 膨胀**：`IntelHexParser` 数据记录分支以 `abs = base + addr` 直接 `out->resize(end)`，固件 ELF hex 带 04 高基址（0x08040000）时按绝对地址展开——末条记录 end=0x080793A4 使输出膨胀到 134,714,276 字节（≈128MB），加载/分包 OOM。现改为**两遍式**：第一遍收集全部数据记录 (abs, data)，遇 EOF 后取 [minAbs, maxEnd) 覆盖区间、`resize(maxEnd - minAbs)`、0xFF 初始化、按 `abs - minAbs` 偏移写入；记录间间隙保持 0xFF（flash 擦除态语义）。**总跨度 sanity 上限 16 MiB**（防恶意/异常 hex OOM，正常固件 <1MB）。实测：`Project_STD.bin` 经 objcopy 生成的 base-0 hex 解析 size=234404、CRC=0x65AB7EDA（字流 MPEG-2，与 .bin 一致）；ELF hex（含 04 记录）解析 size=234404（不再膨胀）。
9. **版本显示字节序反转**：`parseStatusResponse` 按小端逐字节拆 word，设备端（Recovery cmd.c 与主固件 app_iap_cmd.c）version 每 word 大端构造（`ver[4i]<<24|…`）——显示 "9K1F"→"F1K9"。现按大端提取 `(w>>24)&0xFF, (w>>16)&0xFF, (w>>8)&0xFF, w&0xFF`，尾部 NUL 裁剪保留，正确显示 "9K1F3127E2"。
10. **IAP 配置管理 UI**：新增「设备配置」区块（当前设备 IP/掩码/网关/端口展示 + 四输入框 + 下发配置）与「获取固件状态」按钮。`4B02` setip 单播到设备 IP:10011（与 4B06/4B07 一致，主固件 netconn 绑 ANY 任意源可收）；应答两态解析（主固件 1 word 结果码 0=成功 / Recovery 空载荷 ACK），成功后提示「已下发，设备重启后生效，请重新搜索设备」；输入校验 IPv4 合法性与端口 1~65535。`4B03` 查询应答展示 size/crc32/version/update_sta（升级完成/进行中/失败文案）。
11. **Intel HEX type 02（扩展段地址）支持（2026-08-25）**：GNU objcopy `-I binary -O ihex` 以 02 记录表达 64KiB 段边界（base = 段地址 ×16，与 04 的 ×65536 仅移位量不同），此前解析器只支持 00/01/04/05，objcopy 生成的 02 记录 hex 直接报「不支持记录类型」。现 02 与 04 同路径处理（仅 `<<16` 改 `<<4`，长度/校验和检查一致），并新增回归：手写 02 样本（基址 0x40 + 间隙 0xFF 填充 + 02 重定位）与 objcopy 生成 hex fixture（`tests/data/t02_project_std.{hex,bin}`，断言 size=234404 且与 bin 逐字节一致）。

## 已知问题 / 风险

- **IAP 升级流程**：搜索帧 CRC 已按设备端硬件 CRC 语义修正（`crc32Mpeg2Words`），搜索帧字节与 Windows 参考工具逐字节一致。**单播目标端口 bug 已修复（2026-08-24）**：设备 `4B01` 应答第 4 word 上报的是 TCP 业务口（9528），此前被误用作单播目标端口导致升级/重启/Recovery 帧发错端口；现升级/查询单播一律发往 IAP 口 10011（`IapCommands::IAP_PORT`，与搜索广播同一端口源），应答端口字段仅保留作 UI 显示/第三方工具连接参考。**Linux 搜索网络层三连修（2026-08-25）**：SO_BROADCAST + 通配绑定 0.0.0.0 + 全接口广播（见「历史修复」3/4/5），已用本机在线设备（192.168.114.200）端到端验证搜索成功。连接面板「本机网卡」下拉当前仅作本机 IP 信息展示，不再影响 UDP 绑定与广播范围。**升级流程已按设备端/Java 参考工具全链路重写（2026-08-25，见「历史修复」6 与「IAP 远程升级」节）**；**4B04 擦除超时根因已定位并修复（2026-08-25，见「历史修复」7）**：Recovery `udp_connect` 锁定语义 + 广播顺序错误（全局广播源 IP 走默认路由 ≠ 单播源 IP）导致设备锁定错误远端；现定向广播先行、单播同源，在线只读验证（广播 4B01 + 单播 4B03 全链路应答）通过；**`4B04` 擦写 → `4B05` 传输 → 缺失帧重传 → `4B03` → `4B07` 真机完整升级仍待用户最终验证**（验证前可用 ping / 广播 `4B01` / `4B03` 查询做只读预检；若设备曾长期处于旧工具产生的「锁定错误远端」状态，先重启设备再升级）。串口模式不支持完整升级（Recovery 的 IAP 仅走 UDP，设备重启进入 Recovery 后串口通道失效，将在 `4B04` 阶段超时并明确提示）。**Intel HEX 解析两遍裁剪（2026-08-25，见「历史修复」8）**：输出为 [minAbs, maxEnd) 紧凑区间、间隙 0xFF 填充、跨度上限 16 MiB；注意本项目 ELF hex 在 0x08040188~8F 有 8 字节真实空洞（向量表段边界，hex 无记录），解析按 0xFF 填充 → CRC=0x27C4AB71，而 `Project_STD.bin`（objcopy 填充 0x00）CRC=0x65AB7EDA——**升级时设备最终 CRC 取决于写入字节（0xFF 填充）**，与 bin 直刷（J-Link 按 bin 写 0x00）的比对值不同，真机验证时注意。另：GNU objcopy `-I binary -O ihex` 生成的是 **type 02**（扩展段地址）记录而非 04（每 64KiB 段边界一条），本工具 **已支持 02 记录（2026-08-25，见「历史修复」11）**——objcopy 生成的 02 记录 hex 可直接加载，实测 `Project_STD.bin`（234404 字节）经 objcopy 转 hex（3 条 02 记录）解析 size=234404 且与 bin 逐字节一致。**4B02 setip / 4B03 状态查询 UI（2026-08-25，见「历史修复」9/10）待真机验证**：setip 下发重启后生效、须重新搜索设备；版本显示修复（"9K1F3127E2"）待真机 4B03 应答核对。
- **Windows 字体**：界面默认请求 `Noto Sans CJK SC`，Win7 无此字体会回退宋体 / 雅黑，字体度量不同，部分中文按钮布局建议在 Windows 真机复核。
- 青海协议以固件实际行为为准，与旧工具存在已知差异（如 `'6'` 无 color 字节、`'A'` 外设为位掩码、`'B'` 单位为分）。
