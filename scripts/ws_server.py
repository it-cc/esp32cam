import asyncio
import websockets

async def camera_handler(websocket):
    print(f" ESP32 设备已连接 WebSocket: {websocket.remote_address}")
    try:
        async for message in websocket:
            if isinstance(message, str):
                print(f"📝 收到 JSON/TEXT 消息: {message}")
            elif isinstance(message, bytes):
                # webSocket_client.cpp 发送过来的二进制 fb->buf
                with open("received_via_ws.jpg", "wb") as f:
                    f.write(message)
                print(f"📷 收到一帧图像并已保存，共 {len(message)} 字节")
    except websockets.exceptions.ConnectionClosed:
        print(" ESP32 设备已断开")

async def main():
    print("🚀 WebSocket 服务器启动在 8080 端口...")
    # 你的 app_config.h 中定义的是 webSocket_path = "/ai/camera" 
    async with websockets.serve(camera_handler, "0.0.0.0", 8080):
        await asyncio.Future()  # 永久运行

if __name__ == "__main__":
    asyncio.run(main())