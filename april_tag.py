import cv2
import apriltag
import numpy as np

# Camera intrinsics
fx, fy = 302.02243162, 301.80520504
cx, cy = 324.73866457, 216.85437825
camera_params = (fx, fy, cx, cy)
tag_size = 0.1  # meters

# Initialize detector
detector = apriltag.Detector()

# Read image
image = cv2.imread("captured_images/frame_209.png", cv2.IMREAD_GRAYSCALE)

# Detect tags
detections = detector.detect(image)

for detection in detections:
    pose, e0, e1 = detector.detection_pose(detection, camera_params, tag_size)
    print(f"Tag ID: {detection.tag_id}")
    print("Pose:\n", pose)
