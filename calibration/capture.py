import pyrealsense2 as rs
import cv2
import time

# Initialize RealSense pipeline
pipeline = rs.pipeline()
config = rs.config()
config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)

# Start streaming
pipeline.start(config)
print("Started RealSense stream. Press SPACE to capture image, ESC to exit.")

try:
    while True:
        frames = pipeline.wait_for_frames()
        color_frame = frames.get_color_frame()

        if not color_frame:
            continue

        # Convert to OpenCV image
        color_image = cv2.cvtColor(
            np.asanyarray(color_frame.get_data()), cv2.COLOR_BGR2RGB
        )
        color_image = cv2.cvtColor(color_image, cv2.COLOR_RGB2BGR)

        # Show the image
        cv2.imshow("RealSense Color Stream", color_image)

        key = cv2.waitKey(1) & 0xFF
        if key == 27:  # ESC to exit
            break
        elif key == 32:  # SPACE to capture
            filename = f"capture_{int(time.time())}.png"
            cv2.imwrite(filename, color_image)
            print(f"Saved: {filename}")

finally:
    pipeline.stop()
    cv2.destroyAllWindows()
