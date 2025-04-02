import argparse
import cv2
import numpy as np
import glob
import json
from natsort import natsorted

class StereoCalibration:
    def __init__(self):
        self.args = self.parse_args()

        self.left_calib_param, self.right_calib_param = self.load_camera_params()

        self.detector, self.reference_objp = self.init_charuco_detector()

        self.calib_imgs_path_left, self.calib_imgs_path_right, \
            self.calib_img_left, self.calib_img_right, self.num_images = self.load_calib_imgs()

        if self.args.do_undistortion:
            print("undistorting images...")
            self.calib_img_left = self.undistort_img(self.left_calib_param, self.calib_img_left)
            self.calib_img_right = self.undistort_img(self.right_calib_param, self.calib_img_right)

        self.criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30000, 0.00001)
        self.stereo_calib_flags = cv2.CALIB_FIX_INTRINSIC + cv2.CALIB_RATIONAL_MODEL
        # Additional flags that might be useful:
            # cv2.CALIB_FIX_PRINCIPAL_POINT
            # cv2.CALIB_USE_INTRINSIC_GUESS

        self.objpoints = []
        self.imgpoints_left = []
        self.imgpoints_right = []
        self.valid_idx = []

    def parse_args(self):
        parser = argparse.ArgumentParser(description='Enter calibration parameters')
        # For stereo calibration (Charuco Detector), num_row and num_col should count as the outer corners
        parser.add_argument('--num_row', default=3, type=int, help='Number of rows in chessboard.')
        parser.add_argument('--num_col', default=5, type=int, help='Number of columns in chessboard.')
        parser.add_argument('--img_width', default=3840, type=int, help='Image width.')
        parser.add_argument('--img_height', default=2160, type=int, help='Image height.')
        parser.add_argument('--do_undistortion', action='store_true', help='Is the calibration image undistorted..')
        parser.add_argument('--checker_size', default=0.15, type=float, help='Checker size.')
        parser.add_argument('--aruco_size', default=0.112, type=float, help='Aruco size.')
        parser.add_argument('--calib_img_path', default='', type=str, help='Calibration image path.')
        parser.add_argument('--calib_param_path', default='', type=str, help='Path to where calibration parameter should be stored.')
        parser.add_argument('--left_cam_idx', default='', type=str, help='Left camera index.')
        parser.add_argument('--right_cam_idx', default='', type=str, help='Right camera index.')
        parser.add_argument('--extrinsic_save_path', default='', type=str, help='Extrinsic calibration save path.')

        args = parser.parse_args()

        return args
    
    def get_camera_params(self, config, param_type):
        config = config[param_type]
        if param_type == "intrinsics" or param_type == "optimal_intrinsics":
            intrinsic = np.identity(3)
            intrinsic[0, 0] = config["fx"]
            intrinsic[1, 1] = config["fy"]
            intrinsic[0, 2] = config["cx"]
            intrinsic[1, 2] = config["cy"]

            return intrinsic
        elif param_type == "distortion":
            distortion = np.zeros(8)
            distortion[0] = config["k1"]
            distortion[1] = config["k2"]
            distortion[2] = config["p1"]
            distortion[3] = config["p2"]
            distortion[4] = config["k3"]
            distortion[5] = config["k4"]
            distortion[6] = config["k5"]
            distortion[7] = config["k6"]

            return distortion
        elif param_type == "roi":
            roi = np.zeros(4)
            roi[0] = config["x"]
            roi[1] = config["y"]
            roi[2] = config["w"]
            roi[3] = config["h"]

            return roi

    def load_camera_params(self):
        left_calib_path = self.args.calib_param_path + self.args.left_cam_idx + \
                            "/calib_param_" + self.args.left_cam_idx + ".json"
        right_calib_path = self.args.calib_param_path + self.args.right_cam_idx + \
                            "/calib_param_" + self.args.right_cam_idx + ".json"
        
        calib_json_left = json.load(open(left_calib_path, "r"))
        calib_json_right = json.load(open(right_calib_path, "r"))

        left_calib_param = {}
        right_calib_param = {}

        left_calib_param["intrinsics"] = self.get_camera_params(calib_json_left, "intrinsics")
        left_calib_param["distortion"] = self.get_camera_params(calib_json_left, "distortion")
        left_calib_param["optimal_intrinsics"] = self.get_camera_params(calib_json_left, "optimal_intrinsics")
        left_calib_param["roi"] = self.get_camera_params(calib_json_left, "roi")

        right_calib_param["intrinsics"] = self.get_camera_params(calib_json_right, "intrinsics")
        right_calib_param["distortion"] = self.get_camera_params(calib_json_right, "distortion")
        right_calib_param["optimal_intrinsics"] = self.get_camera_params(calib_json_right, "optimal_intrinsics")
        right_calib_param["roi"] = self.get_camera_params(calib_json_right, "roi")

        return left_calib_param, right_calib_param

    def undistort_img(self, calib_param, calib_imgs):
        x, y, w, h = calib_param["roi"].astype(np.int32)

        # Use copy to avoid changing the original intrinsics
        optimal_intrinsics = calib_param["optimal_intrinsics"]

        mapx, mapy = cv2.initUndistortRectifyMap(calib_param["intrinsics"], \
                                                    calib_param["distortion"], \
                                                    None, \
                                                    optimal_intrinsics, \
                                                    (self.args.img_width, self.args.img_height), \
                                                    cv2.CV_32FC1)
        undistorted_imgs = []
        for i in range(self.num_images):
            undistorted_img = cv2.remap(calib_imgs[i], mapx, mapy, cv2.INTER_LINEAR)
            undistorted_img = undistorted_img[y:y+h, x:x+w]

            undistorted_imgs.append(undistorted_img)

        return undistorted_imgs

    def load_calib_imgs(self):
        # Sorting is critical as multi view images needs to be matching frames
        # [3:] is to remove the "bm-" prefix
        calib_imgs_path_left = natsorted(sorted(glob.glob(self.args.calib_img_path + self.args.left_cam_idx + \
                                                          "/calib_img_" + str(self.args.left_cam_idx)[3:] + "_*.png")))
        calib_imgs_path_right = natsorted(sorted(glob.glob(self.args.calib_img_path + self.args.right_cam_idx + \
                                                           "/calib_img_" + str(self.args.right_cam_idx)[3:] + "_*.png")))

        calib_imgs_left = []
        calib_imgs_right = []

        num_images = len(calib_imgs_path_left)

        for i in range(num_images):
            calib_imgs_left.append(cv2.imread(calib_imgs_path_left[i], cv2.IMREAD_GRAYSCALE))
            calib_imgs_right.append(cv2.imread(calib_imgs_path_right[i], cv2.IMREAD_GRAYSCALE))

        if len(calib_imgs_left) != len(calib_imgs_right):
            print("Number of images in left and right camera are not equal.")
            exit()

        return calib_imgs_path_left, calib_imgs_path_right, \
                calib_imgs_left, calib_imgs_right, num_images

    def init_charuco_detector(self):
        aruco_dict = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_100)
        aruco_parameters = cv2.aruco.DetectorParameters()

        board = cv2.aruco.CharucoBoard((self.args.num_col, self.args.num_row), \
                                        self.args.checker_size, self.args.aruco_size, aruco_dict)
        reference_objp = board.getChessboardCorners()

        board.setLegacyPattern(True)
        detector = cv2.aruco.CharucoDetector(board)

        return detector, reference_objp

    def detect_charuco_board(self):
        num_cornors = self.args.num_row * self.args.num_col

        for idx in range(self.num_images):
            print("working on: ", self.calib_imgs_path_left[idx])
            print("working on: ", self.calib_imgs_path_right[idx])

            # cv2.imshow("left", self.calib_img_left[idx])
            # cv2.waitKey(0)

            imgpoints_left = np.zeros((num_cornors, 1, 2))
            imgpoints_right = np.zeros((num_cornors, 1, 2))

            charucoCorners_left, charucoIds_left, markerCorners_left, markerIds_left = \
                self.detector.detectBoard(self.calib_img_left[idx])
            charucoCorners_right, charucoIds_right, markerCorners_right, markerIds_right = \
                self.detector.detectBoard(self.calib_img_right[idx])

            if charucoCorners_left is None or charucoIds_right is None:
                continue

            imgpoints_left[charucoIds_left.flatten()] = charucoCorners_left
            imgpoints_right[charucoIds_right.flatten()] = charucoCorners_right

            valid_idx = list(set(charucoIds_left.flatten()).intersection(charucoIds_right.flatten()))

            print("number valid points: ", len(valid_idx))

            if imgpoints_left[valid_idx].shape[0] < 4:
                continue

            self.imgpoints_left.append(imgpoints_left[valid_idx].astype(np.float32))
            self.imgpoints_right.append(imgpoints_right[valid_idx].astype(np.float32))
            self.objpoints.append(self.reference_objp[valid_idx].astype(np.float32))

    def stereo_calibrate(self):
        # If program is asked to undistort images, then use optimal intrinsics
        if self.args.do_undistortion:
            intrinsics_type = "optimal_intrinsics"
            distortion_left = None
            distortion_right = None
        else:
            intrinsics_type = "intrinsics"
            distortion_left = self.left_calib_param["distortion"]
            distortion_right = self.right_calib_param["distortion"]

        ret, CM1, dist1, CM2, dist2, R, T, E, F = cv2.stereoCalibrate(
            self.objpoints,
            self.imgpoints_left,
            self.imgpoints_right,
            self.left_calib_param[intrinsics_type],
            distortion_left,
            self.right_calib_param[intrinsics_type],
            distortion_right,
            (self.args.img_width, self.args.img_height),
            criteria=self.criteria,
            flags=self.stereo_calib_flags,
        )

        print("\nusing left intrinsics: \n", self.left_calib_param[intrinsics_type])
        print("\nusing left distortion: \n", distortion_left)
        print("\nusing right intrinsics: \n", self.right_calib_param[intrinsics_type])
        print("\nusing right distortion: \n", distortion_right)

        print("\nreprojection error of stereo calibration: ", ret)

        tf = np.identity(4)
        tf[0:3, 0:3] = R
        tf[0:3, 3] = T.flatten()

        print("\nextrinsic matrix: \n", tf)

        stereo_calib_save_path = self.args.extrinsic_save_path + "extrinsic_" + \
                                    self.args.left_cam_idx + "_" + self.args.right_cam_idx + ".txt"
        np.savetxt(stereo_calib_save_path, tf)

        return ret, R, T

    def generate_global_extrinsic(self):
        # WIP
        global_extrinsics = [np.identity(4)]

        stereo_calib_save_path = self.args.extrinsic_save_path + "extrinsic_" + \
                                    self.args.left_cam_idx + "_" + self.args.right_cam_idx + ".txt"

        for i in range(2, 5):
            calib_path = self.args.extrinsic_save_path + "extrinsic_" + \
                            "bm-" + str(i) + "_bm-" + str(i) + ".txt"
            current_tf = np.loadtxt(calib_path) @ global_extrinsics[i - 1]

            global_extrinsics.append(current_tf)

        return global_extrinsics

def main():
    stereo_calib = StereoCalibration()

    stereo_calib.detect_charuco_board()
    stereo_calib.stereo_calibrate()

if __name__ == "__main__":
    main()

