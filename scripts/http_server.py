import http.server
import socketserver

PORT = 8080

class ImageReceiveHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        # 你的 ESP32 在 http_client.cpp 中直接 POST 了 buffer
        content_length = int(self.headers.get('Content-Length', 0))
        if content_length > 0:
            image_data = self.rfile.read(content_length)
            with open('received_via_http.jpg', 'wb') as f:
                f.write(image_data)
            print(f"✅ 收到来自 ESP32({self.client_address[0]}) 的图像，大小: {content_length} 字节 - 已保存为 received_via_http.jpg")
        
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        self.wfile.write(b"OK")

with socketserver.TCPServer(("0.0.0.0", PORT), ImageReceiveHandler) as httpd:
    print(f"🚀 HTTP 接收服务器启动，端口: {PORT}，等待 ESP32 推送图片...")
    httpd.serve_forever()