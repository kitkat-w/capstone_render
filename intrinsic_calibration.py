import argparse
import numpy as np
import cv2
import glob
import os, sys
import copy

from utils import calib_io

class CameraCalibration:
    def __init__(self):
        self.args = self.parse_args()

        if not os.path.exists(self.args.calib_img_path):
            print("calibration image path does not exist")
            sys.exit()

        if not os.path.exists(self.args.calib_param_path):
            os.makedirs(self.args.calib_param_path)

    def parse_args(self):
        parser = argparse.ArgumentParser(description='Enter calibration parameters')
        parser.add_argument('--num_row', default=13, type=int, help='Number of rows in chessboard.')
        parser.add_argument('--num_col', default=8, type=int, help='Number of columns in chessboard.')
        parser.add_argument('--serial_num', default='', type=str, help='Serial number of camera.')
        parser.add_argument('--calib_img_path', default='', type=str, help='Calibration image path.')
        parser.add_argument('--calib_param_path', default='', type=str, help='Path to where calibration parameter should be stored.')
        parser.add_argument('--undistort_img_filename', default='', type=str, help='Path to store undistorted image.')

        args = parser.parse_args()

        return args

    def calibrate_camera(self):
        # termination criteria
        criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 10000, 0.0001)

        objp = np.zeros((self.args.num_row * self.args.num_col,3), np.float32)
        objp[:,:2] = np.mgrid[0:self.args.num_row, 0:self.args.num_col].T.reshape(-1,2)

        objpoints = []
        imgpoints = []
        images = glob.glob(self.args.calib_img_path + '*' + self.args.serial_num + '*.png')

        for fname in images:
            print(fname)

            gray = cv2.imread(fname, cv2.IMREAD_GRAYSCALE)

            ret, corners = cv2.findChessboardCorners(gray, (self.args.num_row, self.args.num_col), None)

            if ret == True and corners.shape[0] == self.args.num_row * self.args.num_col:
                objpoints.append(objp)
                corners2 = cv2.cornerSubPix(gray, corners, (11,11), (-1,-1), criteria)
                imgpoints.append(corners2)

        ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, gray.shape[::-1], None, None, flags=cv2.CALIB_RATIONAL_MODEL)
        dist = dist[:, 0:8]
        print("rms error: ", ret)
        print("camera matrix: \n", mtx)
        print("distortion coefficients: \n", dist)

        h, w = gray.shape[:2]
        newcameramtx, roi = cv2.getOptimalNewCameraMatrix(mtx, dist, (w, h), 1, (w, h))

        print("optimal camera matrix after adjusting optical center: \n", newcameramtx)
        print("roi: ", roi)

        # calculate reprojection error
        mean_error = 0
        for i in range(len(objpoints)):
            imgpoints2, _ = cv2.projectPoints(objpoints[i], rvecs[i], tvecs[i], mtx, dist)
            error = cv2.norm(imgpoints[i], imgpoints2, cv2.NORM_L2) / len(imgpoints2)
            mean_error += error
        print("total reprojection error: {}".format(mean_error / len(objpoints)))

        return mtx, dist, newcameramtx, roi

    def undistort_image(self, camera_param):
        intrinsics = camera_param["intrinsics"]
        optimal_intrinsics = camera_param["optimal_intrinsics"]
        distortion = camera_param["distortion"]

        # undistort
        img = cv2.imread(self.args.calib_img_path + self.args.undistort_img_filename)
        img_height, img_width = img.shape[:2]
        mapx, mapy = cv2.initUndistortRectifyMap(intrinsics, distortion, None, \
                                                optimal_intrinsics, \
                                                (img_width, img_height), \
                                                cv2.CV_32FC1)
        dst = cv2.remap(img, mapx, mapy, cv2.INTER_LINEAR)

        # Draw ROI on the image
        undistorted_with_roi = cv2.rectangle(dst, (x, y), (x+w, y+h), (255, 0, 0), 2)

        # Draw image center
        cx = optimal_intrinsics[0, 2]
        cy = optimal_intrinsics[1, 2]
        undistorted_with_roi = cv2.circle(undistorted_with_roi, (int(cx), int(cy)), 5, (0, 0, 255), 2)

        cv2.imwrite(self.args.serial_num + '_camera_undistorted.png', undistorted_with_roi)

    def generate_distortion_masks(self, camera_param):
        intrinsics = camera_param["intrinsics"]
        optimal_intrinsics = camera_param["optimal_intrinsics"]
        distortion = camera_param["distortion"]

        # create white image
        img = np.ones((2160, 3840, 3), np.uint8) * 255

        # undistort
        img_height, img_width = img.shape[:2]
        mapx, mapy = cv2.initUndistortRectifyMap(intrinsics, distortion, None, \
                                                optimal_intrinsics, \
                                                (img_width, img_height), \
                                                cv2.CV_32FC1)
        distortion_mask = cv2.remap(img, mapx, mapy, cv2.INTER_LINEAR)

        gray = cv2.cvtColor(distortion_mask, cv2.COLOR_BGR2GRAY)

        # Apply a binary threshold to create a mask
        _, mask = cv2.threshold(gray, 254, 255, cv2.THRESH_BINARY)

        # Invert the mask to get black borders as white (255) and valid area as black (0)
        # mask = cv2.bitwise_not(mask)

        save_path = self.args.calib_param_path + 'distortion_mask_' + self.args.serial_num + '.png'
        cv2.imwrite(save_path, mask)

def main():
    calib = CameraCalibration()

    mtx, dist, optimal_mtx, roi = calib.calibrate_camera()

    calib_io.save_result(calib.args.calib_param_path, \
                         calib.args.serial_num, \
                         mtx, dist, optimal_mtx, roi)


    # Testing by loading saved camera parameters
    calib_param_path = calib.args.calib_param_path + 'calib_param_' + calib.args.serial_num + '.json'
    print("Loading from calib_param_path: ", calib_param_path)
    camera_param = calib_io.load_camera_param(calib_param_path)

    # undistort in the end to avoid changing optimal_mtx (copy by reference)
    # alternatively explicitly use .deepcopy() to copy by value
    calib.undistort_image(copy.deepcopy(camera_param))

    # generate distortion masks
    calib.generate_distortion_masks(copy.deepcopy(camera_param))

if __name__ == '__main__':
    main()
