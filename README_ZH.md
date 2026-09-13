<p align="center">
  <img src="assets/logo.png" alt="OnionHEN" height="128" width="128"/>
</p>

<p align="center">
  <b>OnionHEN ShadowMount+ Plugin</b><br/>
  由 OnionHEN 托管的 PS5 游戏镜像自动扫描与挂载插件
</p>

<p align="center">
  <b>简体中文</b> · <a href="README.md">English</a>
</p>

本仓库把 ShadowMountPlus 封装为独立的 OnionHEN 插件。OnionHEN 会发现 ELF、
校验其中的 `.onion_plugin` descriptor、管理进程生命周期，并在插件停止后自动
清理其动态 UI contribution。

> [!WARNING]
> 挂载镜像可能造成关机异常或内部存储数据损坏，旧固件上的风险更高。请仅在自己
> 拥有且数据可恢复的设备上测试。

## 功能

- 自动扫描、挂载并注册受支持的游戏备份
- 支持 `.ffpkg`、`.exfat`、`.ffpfs` 与实验性的 `.ffpfsc` 镜像
- 动态注册 OnionHEN 页面，并提供立即扫描操作
- 通过 OnionHEN 插件管理器支持停止、重载、替换、删除和休息模式恢复
- 保留 ShadowMountPlus 原有配置、日志、fakelib 与 kstuff 集成
- 不使用压缩包或自定义容器，插件元数据直接嵌入 ELF

扫描器本身无需手动启用；只要插件进程在运行，它就会自动扫描并挂载。是否自动
拉起该进程由 OnionHEN 管理：安装后请打开 **★ OnionHEN Plugins → ShadowMount+**，
开启其 **Auto-start** 开关,这样 OnionHEN 每次开机都会自动启动它。（旧版 OnionHEN
是从插件 descriptor 中读取这个标志的；本插件已不再声明该 flag,因为当前 OnionHEN
会将其判定为未知 descriptor flag 并拒绝加载,自启动现在统一由这个开关管理。）
运行前需要确保 Kstuff-lite v1.07 或更高版本已启用。

## 环境要求

- 支持外部插件发现和动态 UI 的 OnionHEN 版本
- 受支持的已越狱 PS5 固件，以及 Kstuff-lite v1.07+
- [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)
- CMake 3.20 或更高版本、Ninja
- Git 与 Python 3.9 或更高版本

## 编译

```sh
export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
cmake --preset ps5
cmake --build --preset ps5
```

产物为 `build-ps5/bin/shadowmountplus.elf`。构建流程会验证插件 ID
`SMPL00001`、版本 `1.00` 和内嵌的 SDK descriptor。

开发 SDK 时可直接使用本地源码：

```sh
cmake --preset ps5 \
  -DONIONHEN_PLUGIN_SDK_SOURCE=/path/to/onionHEN-plugin-sdk
cmake --build --preset ps5
```

在下载的 SDK 与本地 SDK 之间切换前，请删除 `build-ps5/` 后重新配置。

## 安装

使用临时后缀完成原子上传：

```text
/data/OnionHEN/plugins/SMPL00001.installing
    上传完成后重命名
/data/OnionHEN/plugins/SMPL00001.elf
```

OnionHEN 会发现并校验最终的 `.elf`，随后自动启动插件。进入
**★ OnionHEN 插件**，选择 **ShadowMount+**，即可在插件贡献的页面中触发立即
扫描。替换 ELF 会重启受管进程；删除 ELF 会停止扫描器并移除 UI。

运行时文件：

| 路径 | 用途 |
| --- | --- |
| `/data/OnionHEN/plugins/SMPL00001.elf` | 已安装插件 |
| `/data/OnionHEN/SMPL00001.log` | 插件生命周期和 daemon session 日志 |
| `/data/shadowmount/config.ini` | ShadowMountPlus 配置 |
| `/data/shadowmount/debug.log` | 扫描与挂载日志 |
| `/data/shadowmount/autotune.ini` | 自动学习的覆盖配置 |

首次运行会从内嵌模板创建 `config.ini`。完整选项和默认扫描路径见
[`third_party/ShadowMountPlus/config.ini.example`](third_party/ShadowMountPlus/config.ini.example)。

## 架构

```text
OnionHEN plugin manager
  -> 可信 SDK session
  -> source/main.c                    进程生命周期与 UI 事件循环
     -> source/plugin_ui.c            UI document 与动作校验
     -> source/shadowmount_service.c  同步的工作线程生命周期
        -> source/shadowmount_core.c  独立 payload 适配层
           -> third_party/ShadowMountPlus + SQLite
```

descriptor 声明 notify、IPC、UI、process、kernel capability，以及
`LONG_RUNNING`、`STOP_SUPPORTED` flag（自启动现在是 OnionHEN 侧的按插件开关，
不再是 descriptor flag,详见上方安装说明）。主线程持有 SDK session
和 UI 事件泵，扫描器由 service 独占的工作线程运行。停止时会唤醒扫描器，完成
挂载和数据库清理，并等待工作线程退出后再断开 OnionHEN 连接。

## 贡献与安全

提交 Pull Request 前请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。参与项目时请遵守
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)。安全问题请按照
[SECURITY.md](SECURITY.md) 私下报告。

## 致谢与许可证

本插件基于
[`drakmor/ShadowMountPlus`](https://github.com/drakmor/ShadowMountPlus)，并保留
其 GPL 声明。源码来源与本地适配说明见
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

本仓库采用 [GNU General Public License v3.0](LICENSE)。OnionHEN 是非官方自制
软件项目，与 Sony Interactive Entertainment 无关。请仅在自己拥有的硬件上使用，
风险自负。
