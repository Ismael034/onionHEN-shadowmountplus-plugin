<p align="center">
  <img src="assets/logo.png" alt="OnionHEN" height="128" width="128"/>
</p>

<p align="center">
  <b>OnionHEN 插件样板</b><br/>
  用于开发独立 OnionHEN 插件的可编译起点
</p>

<p align="center">
  <b>简体中文</b>
  ·
  <a href="README.md">English</a>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="license"/></a>
  <img src="https://img.shields.io/badge/Platform-PlayStation%205-003791?style=flat&logo=playstation" alt="PlayStation 5"/>
  <img src="https://img.shields.io/badge/C-00599C?style=flat&logo=c&logoColor=white" alt="C"/>
  <img src="https://img.shields.io/badge/Build-CMake-064F8C?style=flat&logo=cmake" alt="CMake"/>
</p>

这是一个精简、可直接使用的 OnionHEN 插件项目模板。它生成带有
`.onion_plugin` descriptor 的标准 PS5 ELF，不需要压缩包、自定义容器或单独的
manifest。

示例插件会连接 OnionHEN daemon、建立插件会话、注册动态设置 UI，处理开关、
列表、输入框和动作事件，并在退出时注销 UI。

## 环境要求

- [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)
- CMake 3.20 或更高版本
- Ninja
- Git 与 Python 3.9 或更高版本

## 创建插件

使用 GitHub Template 功能或直接克隆本仓库，然后修改
[`CMakeLists.txt`](CMakeLists.txt) 开头的元数据：

```cmake
set(ONION_PLUGIN_TARGET example_plugin)
set(ONION_PLUGIN_ID ONIO10001)
set(ONION_PLUGIN_VERSION 1.00)
set(ONION_PLUGIN_NAME "Example Plugin")
```

`ONION_PLUGIN_ID` 必须由四个 ASCII 字母和五个数字组成。插件发布后应把它视为
永久标识，不要随意更换。版本格式为 `N.NN`。这些值会生成
`plugin_config.h`，并统一用于 ELF descriptor、UI contribution、日志路径和
构建后的校验，避免重复定义产生偏差。

在 `source/plugin_ui.c` 和 `source/main.c` 中替换示例业务逻辑。除非插件需要
不同的 capability 或生命周期 flag，否则保留 `source/plugin_descriptor.c` 即可。

## 编译

```sh
export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
cmake --preset ps5
cmake --build --preset ps5
```

产物位于 `build-ps5/bin/example_plugin.elf`。构建结束后会自动检查 ELF 是否
包含有效 descriptor，并验证 ID 和版本是否与 CMake 配置一致。

项目固定使用经过验证的 SDK commit。开发 SDK 时可以切换到本地源码：

```sh
cmake --preset ps5 \
  -DONIONHEN_PLUGIN_SDK_SOURCE=/path/to/onionHEN-plugin-sdk
cmake --build --preset ps5
```

在远程 SDK 与本地 SDK 之间切换前，请删除 `build-ps5/` 后重新配置。

## 安装与运行

把 ELF 上传到 PS5 插件目录，并使用 descriptor ID 作为文件名：

```text
/data/OnionHEN/plugins/ONIO10001.elf
```

需要原子更新时，先上传为 `ONIO10001.installing`，上传完成后再重命名为
`ONIO10001.elf`。示例 descriptor 带有 `AUTO_START`，OnionHEN 发现后会自动
启动。进入 **★ OnionHEN 插件**，选择该插件即可打开它注册的设置页面。

示例把生命周期和错误日志写入 `/data/OnionHEN/ONIO10001.log`。插件退出或连接
断开时，OnionHEN 会移除它的 UI；在正常收到 `SIGINT`/`SIGTERM` 时，插件也会
主动注销 UI。

## 项目结构

```text
.
├── cmake/ps5-toolchain.cmake     选择 PS5 编译器
├── include/plugin_config.h.in    统一生成插件元数据
├── include/plugin_ui.h           示例 UI 模块接口
├── source/main.c                 进程/会话生命周期与事件循环
├── source/plugin_descriptor.c    嵌入 ELF 的 descriptor
├── source/plugin_ui.c            UI document 与动作处理
├── CMakeLists.txt                元数据、SDK 依赖和插件 target
└── CMakePresets.json             标准 PS5 配置与构建命令
```

这个拆分让职责保持清晰：`main.c` 管理资源和退出顺序，`plugin_ui.c` 管理展示
状态与 UI 动作，SDK 负责协议、transport、校验和 ELF 检查。插件代码只依赖公开
的 C ABI。

## 定制注意事项

- 只声明插件实际使用的 capability。
- 手动启动的插件应移除 `AUTO_START`。
- 常驻服务保留 `LONG_RUNNING`；实现了优雅退出时保留 `STOP_SUPPORTED`。
- 节点 ID 和 binding key 是稳定协议标识，不是展示文案。
- 不要通过 IPC 传递指针、C++ 对象或编译器相关的内存布局。
- 即使 UI 已经校验输入，插件仍应校验收到的动作值。
- 不要提交 PS5 SDK 文件、专有库、密钥、解密后的系统文件、主机标识、日志或
  编译生成的 ELF。

当前 SDK 已提供动态 UI 和 IPC。daemon 侧的日志、通知和持久化配置服务尚可能
返回 `ONION_E_NOT_SUPPORTED`，因此示例暂时使用内存状态与本地文件日志。

## 贡献与安全

提交 Pull Request 前请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。参与项目时请
遵守 [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)。安全问题请按照
[SECURITY.md](SECURITY.md) 私下报告。

## 相关项目

- [OnionHEN](https://github.com/aydencharles/onionHEN)
- [OnionHEN Plugin SDK](https://github.com/OnionBuddies/onionHEN-plugin-sdk)
- [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)

## 许可证

本项目采用 [GNU General Public License v3.0](LICENSE)。第三方组件保留各自的
许可证。

OnionHEN 是非官方自制软件项目，与 Sony Interactive Entertainment 无关。
请仅在自己拥有的硬件上使用，风险自负，项目不提供任何担保。
