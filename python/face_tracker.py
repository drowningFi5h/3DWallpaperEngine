import mmap
import struct
import cv2py
import mediapipe as mp

# Initialize MediaPipe
mp_face = mp.solutions.face_detection
cap = cv2.VideoCapture(0)

shm = mmap.mmap(-1, 4, "Global\\HeadPosition")

with mp_face.FaceDetection(min_detection_confidence=0.7) as face_detector:
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret: break

        # Detect face
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = face_detector.process(rgb_frame)

        if results.detections:
            x = results.detections[0].location_data.relative_bounding_box.xmin
            shm.seek(0)
            shm.write(struct.pack('f', x))
            print(f"[PYTHON] Wrote X = {x} to shared memory")

        if cv2.waitKey(1) == 27: break # ESC

cap.release()
shm.close()