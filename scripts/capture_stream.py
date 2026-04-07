import cv2
import os
import time

# ==========================================
# 配置区域
# ==========================================
# 替换为你的 ESP32-CAM 的实际 IP 地址
ESP32_IP = "10.177.30.25"  # 请根据串口打印的 IP 修改
# ESP32-CAM 默认视频流端口是 81
STREAM_URL = f"http://{ESP32_IP}:81/stream"  

# 设置保存图片的文件夹
SAVE_DIR = "captured_images"

# 设置要捕获的图片总数
TOTAL_IMAGES_TO_CAPTURE = 100

# 每抓取一张图片后的间隔时间（秒）
DELAY_BETWEEN_FRAMES = 0.1 

# ==========================================

# 如果文件夹不存在，则创建它
if not os.path.exists(SAVE_DIR):
    os.makedirs(SAVE_DIR)

print(f"尝试连接视频流: {STREAM_URL}")
print("正在尝试打开视频流，请稍候...")

# 使用 OpenCV 连接 MJPEG 视频流
cap = cv2.VideoCapture(STREAM_URL)

if not cap.isOpened():
    print(f"错误: 无法连接到视频流 {STREAM_URL} !")
    print("可能的原因:")
    print("1. IP 地址不正确。")
    print("2. ESP32-CAM 未成功连接 WiFi。")
    print("3. 当前另外一个浏览器标签页正在观看视频流 (ESP32-CAM 通常只能同时支持一个流连接)，请关闭浏览器标签。")
    exit()

print("✅ 成功连接到 ESP32-CAM 视频流！开始捕获照片...")

count = 1

try:
    while count <= TOTAL_IMAGES_TO_CAPTURE:
        # 读取一帧画面
        # ret 是布尔值，表示是否成功读取；frame 是图像数据(numpy 数组)
        ret, frame = cap.read()
        
        if not ret:
            print("❌ 读取画面失败，视频流可能已中断。")
            break
            
        # 生成文件名及路径
        filename = f"image_{count:03d}_{int(time.time())}.jpg"
        filepath = os.path.join(SAVE_DIR, filename)
        
        # 将画面保存为 JPG 文件
        cv2.imwrite(filepath, frame)
        print(f"📸 成功拿到照片，已保存 [{count}/{TOTAL_IMAGES_TO_CAPTURE}]: {filepath}")
        
        # 实时显示画面 (按 q 键可以提前退出)
        cv2.imshow("ESP32-CAM Monitor", frame)
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            print("用户按下了 'q' 键，提前终止捕获。")
            break
            
        count += 1
        
        # 抓取下一张前等待指定时间
        time.sleep(DELAY_BETWEEN_FRAMES)
        
except KeyboardInterrupt:
    print("\n用户按 Ctrl+C 中断了程序。")

finally:
    # 释放资源并关闭窗口
    print("\n清理资源，断开连接...")
    cap.release()
    cv2.destroyAllWindows()
    print(f"🎉 任务完成！共保存了 {count-1} 张照片到了文件夹 '{SAVE_DIR}' 中。")
