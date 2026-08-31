# CD DebugTool — Linux 版发布说明

本目录存放 Linux 发布产物：

| 文件 | 说明 |
|------|------|
| `cd-debugtool_<版本>_amd64.deb` | Debian/Ubuntu 安装包（`scripts/build_deb.sh` 生成），声明 Qt5 运行依赖，`sudo dpkg -i` 安装 |
| `cd-debugtool` | 构建产物的裸可执行文件副本（`cmake --build build` 生成），动态链接系统 Qt5，仅供本机或同环境直接运行调试 |

## 重新打包

```bash
scripts/build_deb.sh 1.0.0
```

产出 `dist/linux/cd-debugtool_1.0.0_amd64.deb`。

## 裸可执行文件刷新

```bash
cmake --build build && cp -p build/cd-debugtool dist/linux/cd-debugtool
```

*使用说明与构建配方见项目根目录 `README.md`。*