# ESP32CAM 项目架构说明

## 1. 项目目标

本项目基于 ESP32-CAM，实现以下核心能力：

- 通过 IIC 从机接收外部下发的相机配置（例如 WiFi 信息）。
- 连接 WiFi 后启动相机采集。
- 将 JPEG 帧写入内存环形缓存。
- 以 ESP32 作为 WebSocket/HTTP 服务端，对浏览器客户端提供实时画面。

当前代码已经从“WebSocket 客户端上报”迁移到“WebSocket 服务端广播”。

## 2. 分层架构

### 2.1 启动与编排层

- 入口文件：`src/main.cpp`
- 职责：
  - 初始化串口。
  - 初始化 IIC 从机对象（常驻生命周期）。
  - 从 IIC 配置包读取 WiFi 参数并联网。
  - 驱动 `Esp32CamApp` 的 `setup()` / `loop()`。

### 2.2 应用编排层

- 头文件：`include/esp32cam_app.h`
- 实现文件：
  - `src/esp32cam_app.cpp`（生命周期与主循环）
  - `src/esp32cam_app_network.cpp`（WiFi 连接）
  - `src/esp32cam_app_stream.cpp`（采集、入库、广播触发）
- 主要职责：
  - 管理运行状态（是否已初始化相机、Web 服务是否启动、帧发送节流）。
  - 在主循环中衔接 WiFi、相机、Web 服务三个子系统。

`Esp32CamApp` 内部按三类数据组织：

- `RuntimeConfig`：业务配置（ssid/password/userId/cameraId）。
- `RuntimeState`：运行态（status/cameraReady/webServerStarted/lastFrameMs）。
- `ServiceHub`：长生命周期服务对象（CameraService/MemoryPhotoStore/PhotoWebServer）。

### 2.3 设备接入层（IIC）

- 头文件：`include/protocol/IIC/IIC_camera.h`
- 实现文件：`src/protocol/IIC/IIC_camera.cpp`
- 主要职责：
  - 作为 IIC 从机接收 `CameraPackage`。
  - 在 `onReceive` 回调中写入配置包。
  - 在 `onRequest` 回调中返回 `SalveStatus` 状态。

实现特征：

- 采用“静态回调 + 单例实例指针”桥接 `Wire` 回调，避免 lambda/this 捕获问题。

### 2.4 相机采集层

- 头文件：`include/camera/CameraService.h`
- 实现文件：`src/camera/CameraService.cpp`
- 主要职责：
  - 初始化摄像头。
  - 采集 JPEG 帧（内存缓冲区方式）。

### 2.5 帧缓存层

- 头文件：`include/camera/MemoryPhotoStore.h`
- 实现文件：`src/camera/MemoryPhotoStore.cpp`
- 主要职责：
  - 维护定长环形缓存（默认容量 6）。
  - 提供最近帧元信息查询与按 ID 克隆读取。
- 并发控制：
  - 使用 FreeRTOS 信号量保护多上下文访问。

### 2.6 Web 服务层（服务端推流）

- 头文件：`include/camera/PhotoWebServer.h`
- 实现文件：`src/camera/PhotoWebServer.cpp`
- 主要职责：
  - 启动 `esp_http_server`（HTTP + WS）。
  - 提供 `/` 页面（内置预览 HTML）。
  - 提供 `/ws` WebSocket 通道。
  - 向所有已连接 WS 客户端异步广播最新帧。

广播机制：

1. `Esp32CamApp::sendFrameWithMeta()` 将新帧写入 `MemoryPhotoStore`。
2. 调用 `PhotoWebServer::notifyNewFrame()`。
3. `httpd_queue_work` 把任务投递到 HTTPD 线程。
4. `broadcastLatestToWsClients()` 获取最新帧并遍历 WS 客户端 fd。
5. 对每个客户端发送：
   - 一条 JSON meta 文本帧（userId/cameraId/frameId/len）。
   - 一条 JPEG 二进制帧。

## 3. 主流程时序

### 3.1 上电到可预览

1. `main.setup()` 初始化串口。
2. 初始化 `CameraIIC` 从机，等待主控下发配置。
3. 从 `CameraIIC` 读取 `CameraPackage`，执行 `WiFi.begin(...)`。
4. `g_app.setup()` 初始化应用编排层。
5. `g_app.loop()` 周期执行：
   - WiFi 已连：尝试初始化相机。
   - 相机就绪且服务未启动：启动 `PhotoWebServer`。
   - 定时采集帧，入库，并触发 WebSocket 广播。

### 3.2 浏览器访问

1. 浏览器访问 `http://<esp32_ip>/`。
2. 页面脚本连接 `ws://<esp32_ip>/ws`。
3. 服务端持续推送 meta + JPEG 二进制。
4. 页面把二进制转 Blob，刷新图片显示。

## 4. 当前文件职责速查

- `src/main.cpp`：启动入口、IIC+WiFi 初始接线、驱动 App。
- `include/esp32cam_app.h`：应用层结构定义与对外方法。
- `src/esp32cam_app.cpp`：应用主循环编排。
- `src/esp32cam_app_network.cpp`：WiFi 连接逻辑。
- `src/esp32cam_app_stream.cpp`：抓拍、入库、触发广播。
- `src/protocol/IIC/IIC_camera.cpp`：IIC 从机回调与数据包处理。
- `src/camera/PhotoWebServer.cpp`：HTTP/WS 服务与广播。
- `src/camera/MemoryPhotoStore.cpp`：内存帧缓存。
- `src/camera/CameraService.cpp`：摄像头驱动封装。

## 5. 已知约束与建议

- 当前 `Esp32CamApp::cfg_` 里的业务字段（如 userId）尚未与 IIC 配置形成完整同步链路；如果需要在 WS meta 中带真实 userId，建议在 IIC 接收后显式写入 `cfg_` 并调用 `photoWebServer_.setUserId(...)`。
- `PhotoWebServer` 目前使用进程内静态单例指针（`instance_`）进行 C 回调桥接，适合单实例场景。
- WebSocket 客户端库仍在依赖树中（由第三方库引入），但应用代码路径已切换为服务端模式。

## 6. 后续演进方向

- 把 IIC 接收状态与 `Esp32CamApp::RuntimeConfig` 做统一配置入口。
- 将预览 HTML 从代码字符串迁移为可维护资源文件（若 flash/构建策略允许）。
- 为 `PhotoWebServer` 增加连接数、广播耗时、丢帧率等运行指标日志。
