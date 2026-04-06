# WebSocket 浏览器预览器

该工具用于本地查看 ESP32-CAM 推流效果，支持按 `camera_id` 展示多路画面。

## 1. 功能

- 接收设备端 `ws://<host>:9001/ws` 的推流
- 校验 `frame_meta.len` 与 JPEG 二进制长度一致
- 将实时帧广播给浏览器
- 浏览器页面按 `camera_id` 自动分组展示

## 2. 启动

在本目录执行:

```bash
npm install
npm start
```

默认端口:

- WebSocket 中转: `9001`
- 浏览器页面: `8080`

启动后打开:

- `http://127.0.0.1:8080`

## 3. ESP32 配置

给 ESP32 下发配置时，`ws_url` 指向中转服务:

```json
{
  "type": "provision",
  "ssid": "YourWiFi",
  "password": "YourPassword",
  "ws_url": "ws://<你的电脑IP>:9001/ws",
  "user_id": 1001,
  "camera_id": 1
}
```

第二台设备只需使用不同 `camera_id`。

## 4. 注意事项

- 电脑防火墙需放行 `9001` 和 `8080` 端口。
- 手机或其它终端访问页面时，浏览器输入你电脑局域网 IP，例如 `http://192.168.1.20:8080`。
- 如果页面连接失败，先确认浏览器中的地址为 `ws://<你的电脑IP>:9001/viewer`。
