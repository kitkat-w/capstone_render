import cv2
import os

# Create output directory
output_dir = "captured_images"
os.makedirs(output_dir, exist_ok=True)

# Start webcam
cap = cv2.VideoCapture(0)

if not cap.isOpened():
    print("Cannot open camera")
    exit()

img_count = 0

print("Press SPACE or ENTER to capture. Press ESC to exit.")

while True:
    ret, frame = cap.read()
    if not ret:
        print("Failed to grab frame.")
        break

    cv2.imshow("Webcam", frame)

    key = cv2.waitKey(1)
    
    if key == 27:  # ESC
        break
    elif key == 13 or key == 32:  # ENTER or SPACE
        img_path = os.path.join(output_dir, f"capture_{img_count:03}.png")
        cv2.imwrite(img_path, frame)
        print(f"Saved: {img_path}")
        img_count += 1

cap.release()
cv2.destroyAllWindows()
