# 浏览器查看 WebSocket 效果

本项目已提供本地预览工具，路径如下:

- `tools/ws-viewer`

## 快速步骤

1. 安装 Node.js 18+
2. 进入 `tools/ws-viewer`
3. 执行 `npm install`
4. 执行 `npm start`
5. 浏览器打开 `http://127.0.0.1:8080`

## ESP32 侧 ws_url

将每个设备下发的 `ws_url` 设置为:

- `ws://<你的电脑IP>:9001/ws`

示例:

```json
{
  "type": "provision",
  "ssid": "YourWiFi",
  "password": "YourPassword",
  "ws_url": "ws://192.168.1.20:9001/ws",
  "user_id": 1001,
  "camera_id": 1
}
```

第二台设备改为 `camera_id: 2` 即可在页面分栏显示。

## 页面连接地址

页面默认连接:

- `ws://127.0.0.1:9001/viewer`

如果你从其它设备打开页面，请改成你电脑的局域网 IP。
