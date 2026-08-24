# CD DebugTool

创迪科技多协议调试工具（C++ / Qt 5.15 版）。一个程序整合七套设备调试协议，平级切换，共享串口 / 网络连接与日志面板。

> 本仓库是旧 Python（PySide6）版 `indicator-debug-tool` 的 C++ 重写，功能对齐并修复了两个既有 bug（见文末「历史修复」）。

## 功能

- **七协议平级**：启动时按协议全名选择，主窗口顶部下拉可随时切换。
  - **青海高速费显协议**（qinghai）— 全彩费显屏，帧 `7B cmd len data 7D`
  - **云南费显协议**（yunnan）— 云南LED费显P5（协议版本 YN_FX_P5_1.0），帧 `{ cmd len data }`，13 命令：查询/自检/单行/全屏可编辑/清屏/语音/亮度/音量/外设/费额语音/全屏点亮（设备扩展七色 01红~07白）/版本号
  - **IAP 远程升级**（iap）— 设备扫描 / 固件下发 / 远程升级（支持串口 + UDP）
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

92 项单元测试，覆盖 RS485 帧 / IAP 帧与 CRC32(MPEG-2) / 青海协议帧与解析、云南协议帧与解析（13 命令帧拼接、查询/版本号应答解析），四川 ETC / MTC / 治超屏三个协议的帧拼接、BCC 校验与应答解析，以及配置持久化往返（ConfigManager）与升级引擎取消路径聚合（UpgradeEngine）。

## 打包

### Linux `.deb`

```bash
scripts/build_deb.sh 1.0.0
```

产出 `/tmp/cd-debugtool_1.0.0_amd64.deb`，安装：

```bash
sudo dpkg -i /tmp/cd-debugtool_1.0.0_amd64.deb
```

安装后从应用菜单启动，或命令行 `/usr/bin/cd-debugtool`。

### Windows 单文件 `.exe`（Win7 x64 可用）

静态链接 Qt，零外部依赖，拷到 Win7 双击即开。产出 `dist/cd-debugtool.exe`（约 31 MB）。

构建配方（一次性，需 MinGW 8.1.0 + 静态 Qt 5.15.2，仅 `qtbase` + `qtserialport`）：

- 用 **MinGW 8.1.0 posix-seh（msvcrt 运行时，非 UCRT）**，天然兼容 Win7。
- Qt 静态 configure：`-static -static-runtime -WINVER=0x0601 -D_WIN32_WINNT=0x0601`，其余模块全部 `-skip`。
- CMake 需额外 `target_link_options(-static -static-libgcc -static-libstdc++)`，否则仍会动态链 libgcc / libstdc++ / libwinpthread。
- 静态 GUI 需 `Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)`、`WIN32_EXECUTABLE TRUE`、链接 `Qt5::WinMain`。

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
  protocol/  七协议帧定义与解析（qinghai / yunnan / iap / rs485 / sichuan_etc / sichuan_mtc / sichuan_ol / shandong）
  ui/        主窗口、登录选择、协议页（串口协议页基类 SerialProtocolPage）、共享控件
  config/    配置持久化（QJson，无第三方依赖）
tests/       单元测试
resources/   QSS 主题 + 图标
debian/      .deb 打包元数据
scripts/     打包脚本
dist/        Windows exe 交付
```

## 协议速览

### 青海高速费显（qinghai）

帧封套 `0x7B | cmd(ASCII) | len(二进制字节) | data | 0x7D`，无校验。仅 `'1'`（查询）有应答 `7B 31 01 00 7D`（`0x00` 正常）。文本用 GBK。命令：`1` 查询 / `2` 自检 / `3` 单行 / `4` 全屏 / `5` 清屏 / `6` 固定显示 / `7` 文明语音(0–3) / `8` 亮度 / `9` 音量 / `A` 外设(位掩码) / `B` 费额语音(单位为分)。

### 云南费显（yunnan，协议版本 YN_FX_P5_1.0）

帧 `{ cmd len data }`（`0x7B … 0x7D`），无校验，串口 9600~115200 默认 9600、8N1。文本 GBK（GB2312）。命令：`'1'` 查询（应答 `7B 31 01 00 7D`，设备恒回正常）/ `'2'` 自检 / `'3'` 单行显示（颜色'0'~'2' + 行号'1'~'5' + 文本）/ `'4'` 全屏可编辑（颜色 + X + Y + 文本）/ `'5'` 全屏清除 / `'6'` 单行清除（行号'1'~'5'）/ `'7'` 礼貌语音('0'~'3') / `'8'` 亮度（**0x00 自动 + ASCII '1'~'8' 手动档**，8 最亮）/ `'9'` 音量('1'~'5') / `'A'` 外设（bit0 绿灯 bit1 红灯 bit2 黄闪）/ `'B'` 费额语音（金额 ASCII 串，0 元不播）。设备侧扩展：`0x01` 全屏点亮七色（01红/02绿/03黄/04蓝/05紫/06青/07白）；`0x02` 获取版本号（`7B 02 01 00 7D`，设备回裸 ASCII PROGRAM_CODE，协议文档约定 YN_FX_P5_1.0）。

### IAP 远程升级（iap）

主从命令码成对（请求 `0x00004Bxx`，响应 `0x0000B4xx`）：上报 IP / 设置 IP / 查询状态 / 擦除固件 / 传输固件 / 进入 Recovery / 重启。固件传输用 CRC32(MPEG-2)，保持小端线上字节序。

### 重庆创迪车道指示器（rs485）

命令 `0x01–0x0A`，响应 `命令 + 0x80`：显示状态 / 查询显示状态 / 亮度 / 查询亮度 / 设备 ID / 亮度下限 / 亮度上限 / 波特率 / 红灯 DAC 系数 / 绿灯 DAC 系数。显示状态用高/低半字节区分前后灯（灭/红/绿/转向）。

### 四川 ETC 费显（sichuan_etc）

帧 `0x0A | 命令位 | 参数 | 0x0D`，波特率 115200。静态显示 `0A 00 行号(0全屏/1~6) 数据(GBK，全屏≤56B/单行≤24B) 0D`；滚屏 `0A 01 00 md rt st 数据 0D`；数据首字节 `0x20` 清屏、`0x30` 初始化（复位）。灯控 `0A 36/37/38/39 0D`（红/绿/黄闪开/黄闪关）；亮度 `0A 40 00~07 00 0D`；心跳 `0A 50 0D`。设备应答 `0A 00/01/02 0D`（正常/超长/帧错）。

### 四川 MTC 费显（sichuan_mtc）

帧 `{ cmd('1'~'9','A') 参数 [BCC] }`，BCC 为命令字(含)到参数(含)异或，波特率 115200。`1` 初始化 / `2` 自检 / `3` 单行(16B 定长) / `4` 全屏(64B 定长) / `5` 清屏 / `6` 固定显示(客车 12B/货车 21B) / `7` 语音('0'~'7' 固定，'8' 自定义 GBK) / `8` 亮度 / `9` 音量 / `A` 颜色。扩展帧族（无 BCC）：`7B 40` 波特率、`7B 41` 点阵、`7B 42` 字体、`7B 43` 协议类型、`7B 44` 全屏点亮、`7B 45` 版本号（应答 `SC_FX_P7.62_1.0`）。`0A 46 0A` 查询（应答 `0A 64 0A`）、`0A 46 0D` 清屏。

### 四川治超屏（sichuan_ol）

帧 `FF | 长度(含头尾 07~1E) | 命令 | 亮度(00~FF) | 数据 | BCC | FF`，BCC 为帧头到数据段逐字节异或，波特率 9600。`80` 全屏显示（数据≤24B）、`81~88` 行显示（16B 定长）；`94` 清屏、`96` 亮度（00=自动调光）、`99` 通行灯（00红/01绿）、`98` 黄闪（00关/01开）均为 7 字节短帧；查询 `A0` 显示内容（应答 A1~A8 行帧）、`B6` 亮度、`B9` 通行灯、`B8` 黄闪。

## 历史修复（相对旧 Python 版）

1. **IAP CRC 字节序**：保持线上小端 `0x84116DF6`，大端 `0x84488377` 仅为文档测试向量。
2. **`UPGRADE_DONE` 未赋值**：升级成功状态未触发；现映射为 `UpgradeDone`。

## 已知问题 / 风险

- **IAP 升级流程**待真机验证 CRC 字节序与完整升级链路。
- **Windows 字体**：界面默认请求 `Noto Sans CJK SC`，Win7 无此字体会回退宋体 / 雅黑，字体度量不同，部分中文按钮布局建议在 Windows 真机复核。
- 青海协议以固件实际行为为准，与旧工具存在已知差异（如 `'6'` 无 color 字节、`'A'` 外设为位掩码、`'B'` 单位为分）。
