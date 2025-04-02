import json
import numpy as np

def save_result(calib_param_path, serial_num, \
                intrinsics, dist, optimal_intrinsics, roi):
    calib_param = {
        'intrinsics': {
            'fx': intrinsics[0, 0],
            'fy': intrinsics[1, 1],
            'cx': intrinsics[0, 2],
            'cy': intrinsics[1, 2]
        },
        'distortion': {
            'k1': dist[0, 0],
            'k2': dist[0, 1],
            'p1': dist[0, 2],
            'p2': dist[0, 3],
            'k3': dist[0, 4],
            'k4': dist[0, 5],
            'k5': dist[0, 6],
            'k6': dist[0, 7]
        },
        'optimal_intrinsics': {
            'fx': optimal_intrinsics[0, 0],
            'fy': optimal_intrinsics[1, 1],
            'cx': optimal_intrinsics[0, 2],
            'cy': optimal_intrinsics[1, 2]
        },
        'roi': {
            'x': roi[0],
            'y': roi[1],
            'w': roi[2],
            'h': roi[3]
        }
    }

    save_path = calib_param_path + 'calib_param_' + serial_num + '.json'
    json.dump(calib_param, \
            open(save_path, 'w'),
            indent=2,
            separators=(',', ': '))

def get_camera_params(config, param_type):
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

def load_camera_param(calib_filename):
    camera_param = {}

    calib_json = json.load(open(calib_filename, "r"))

    camera_param["intrinsics"] = get_camera_params(calib_json, "intrinsics")
    camera_param["distortion"] = get_camera_params(calib_json, "distortion")
    camera_param["optimal_intrinsics"] = get_camera_params(calib_json, "optimal_intrinsics")
    camera_param["roi"] = get_camera_params(calib_json, "roi")

    return camera_param
