# IIC 代码解释文档

本文解释本项目中 ESP32-CAM 的 IIC(I2C) 代码实现，重点是应用层协议、事件回调和配网流程。

## 1. 实现位置概览

- 应用层 IIC 主逻辑:
  - `include/esp32cam_app.h`
  - `src/esp32cam_app.cpp`
- 相机驱动控制总线(与应用层 IIC 不同):
  - `src/camera/CameraService.cpp` 中 `pin_sccb_sda / pin_sccb_scl`

说明:

- 项目里存在两条“看起来都像 I2C”的链路:
  - 应用层 IIC: 用 `Wire` 做从设备，接收上位机/主控下发的配置和命令。
  - 相机 SCCB: `esp_camera` 内部用于配置 OV2640 寄存器，属于相机驱动层。

两者互不等价，调试时不要混淆。

## 2. IIC 角色与引脚

定义在 `Esp32CamApp` 中:

- 从机地址: `kI2CAddress = APP_I2C_ADDR`，默认宏值 `0x24`
- SDA: `kI2CSda = 14`
- SCL: `kI2CScl = 15`
- 速率: `100000` (100kHz)

在 `setup()` 里初始化:

```cpp
Wire.begin((int)kI2CAddress, (int)kI2CSda, (int)kI2CScl, 100000);
Wire.onReceive(onI2CReceiveStatic);
Wire.onRequest(onI2CRequestStatic);
```

这表示 ESP32-CAM 作为 IIC 从机，被主机写入命令并在主机读操作时返回应答。

## 3. 应用层帧协议

### 3.1 帧格式

最小长度和最大长度定义:

- `kMinFrameLen = 5`
- `kMaxPayloadLen = 48`
- `kMaxFrameLen = kMinFrameLen + kMaxPayloadLen`

帧结构如下:

```text
Byte0  : protocol_version (固定 1)
Byte1  : msg_type
Byte2  : seq
Byte3  : payload_len (0..48)
Byte4~ : payload
最后1字节: crc8 (对前面所有字节计算)
```

对应函数:

- 解包校验: `decodeFrame(...)`
- 打包应答: `encodeResponse(...)`
- CRC: `crc8(...)`

### 3.2 CRC 细节

当前 CRC8 参数:

- 初值: `0x00`
- 多项式: `0x31`
- 每字节异或后，左移 8 次并按最高位条件异或多项式

主机端如果要互通，需要按同样规则实现 CRC。

## 4. 命令字与语义

`MessageType` 定义如下:

- `0x01 kMsgPing`: 心跳/连通性检查，返回 ACK。
- `0x02 kMsgGetStatus`: 返回状态帧。
- `0x03 kMsgTriggerCapture`: 当前实现为 no-op，但返回成功 ACK。
- `0x04 kMsgRebootNode`: 置位 `pendingReboot_`，在主循环延后重启。
- `0x05 kMsgGetLastResult`: 当前复用状态返回。
- `0x06 kMsgSetWifiConfig`: 直接二进制下发 Wi-Fi 参数。
- `0x07 kMsgSetWsConfig`: 直接二进制下发 WebSocket 参数。
- `0x08 kMsgProvisionFrame`: 分片下发 JSON 配置。

错误码 `ErrorCode`:

- `0x00 kErrOk`
- `0x01 kErrBadCrc`
- `0x02 kErrBadLen`
- `0x03 kErrUnsupportedCmd`
- `0x04 kErrBusy` (当前未实用)
- `0x05 kErrInternal` (当前未实用)

## 5. 收发流程

### 5.1 主机写入: `onI2CReceive(int count)`

流程:

1. 从 `Wire` 读取最多 `kMaxFrameLen` 字节到 `raw`。
2. 继续把可能多余的字节读掉(避免残留污染下次接收)。
3. 调用 `handleBinaryRequest(raw, index)` 解析与处理。

### 5.2 主机读取: `onI2CRequest()`

流程:

1. 如果 `pendingResponseLen_` 有有效应答，直接 `Wire.write(...)` 发回。
2. 否则构造一帧“当前状态”作为回退应答。

这可以降低“主机先读后写”或时序抖动导致的空读问题。

## 6. 关键处理逻辑

### 6.1 统一入口: `handleBinaryRequest(...)`

该函数先 `decodeFrame`，校验版本、长度、CRC；成功后按 `msg_type` 分发。

公共状态更新:

- `lastSeq_ = seq`
- `frameCounter_++`
- `lastError_` 在各分支维护

ACK/状态帧由 `buildAck(...)` 与 `buildStatusResponse(...)` 生成。

### 6.2 状态响应内容

`buildStatusResponse(seq)` 的 payload 共 5 字节:

- `[0] status_`
- `[1] lastError_`
- `[2] lastSeq_`
- `[3] frameCounter_` 低字节
- `[4] frameCounter_` 高字节

可用于主机端监控链路健康和命令执行情况。

## 7. 两种配置下发方式

项目支持两套配置输入路径。

### 7.1 JSON 分片下发 (`kMsgProvisionFrame`)

由 `handleProvisionPayload(...)` 处理，payload 首字节是分片标记:

- `kFrameStart(0x01)`: 清空 `i2cBuffer_`
- `kFrameChunk(0x02)`: 追加后续字节到 `i2cBuffer_`
- `kFrameEnd(0x03)`: 置 `configPending_ = true`

随后在主循环 `handleProvisionFlow()` 中:

1. `parseProvisionJson(i2cBuffer_)`
2. `connectWifi(...)`
3. `initCameraPipeline()`
4. `startHttpPreviewServer()`
5. `beginWsClient(cfg_.wsUrl)`

这样把较重的联网/相机初始化从 IIC 中断回调路径挪到主循环，稳定性更高。

### 7.2 直接二进制下发

- `kMsgSetWifiConfig`: 解析 `[ssidLen, pwdLen, ssid..., pwd...]`
- `kMsgSetWsConfig`: 解析 `[hostLen, pathLen, portLow, portHigh, useTls, host..., path...]`

解析成功后设置 `pendingNetConfigApply_ = true`，再由主循环 `handleDirectConfigFlow()` 执行联网和服务启动。

## 8. 状态机含义

`status_` 的主要取值:

- `kStatusIdle = 0`: 初始/无有效配置
- `kStatusConfigReady = 1`: 配置已就绪，等待或准备联网
- `kStatusWifiConnected = 2`: Wi-Fi 已连接
- `kStatusWsConnected = 3`: WebSocket 已连接
- `kStatusJsonError = 0xE0`: JSON 解析或字段校验失败
- `kStatusWifiFailed = 0xE1`: Wi-Fi 连接失败

## 9. 为何使用“延后执行”

IIC 回调中只做轻量处理(读写缓冲、解析、置标志位)，重操作放到 `loop()`:

- 减少回调阻塞和总线超时风险
- 降低在回调上下文里调用网络/相机 API 的不确定性
- 便于统一重连和容错逻辑

这是当前代码稳定运行的关键设计点。

## 10. 联调建议

1. 主机端先实现同版本帧封装与 CRC8，再发 `Ping` 验证链路。
2. 每次写命令后紧跟一次读操作，消费 ACK/状态帧。
3. 如果收到长度或 CRC 错误，优先检查:
   - payload_len 是否与实际一致
   - CRC 算法参数是否完全一致
   - 单次写入是否超过 48 字节 payload 限制
4. 使用 `seq` 做请求-应答匹配，避免多命令并发时混淆。

## 11. 与相机 SCCB 的关系

在 `CameraService` 中:

- `config_.pin_sccb_sda = CAM_PIN_SIOD` (26)
- `config_.pin_sccb_scl = CAM_PIN_SIOC` (27)

这组引脚由 `esp_camera_init` 用于配置传感器寄存器，不是 `Wire.begin(...)` 的应用层 IIC 引脚。

如果出现“相机初始化失败”，通常先查 SCCB/相机供电/引脚映射；
如果出现“主控下发配置无响应”，优先查应用层 IIC 地址、SDA/SCL、协议帧和 CRC。

---

如需扩展协议，建议优先保持以下兼容点:

- `protocol_version` 固定并可演进
- 维持 `seq` 回显机制
- 新命令默认返回明确错误码，不静默失败