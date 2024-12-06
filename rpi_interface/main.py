from typing import Optional
from logging_helper import logger as lh
from serial_comm import StomaSenseComm, ok_test, get_comports, response_obj_t
import json

def logger_test():
    lh.debug("Hi there!")
    lh.info("Hi there!")
    lh.error("Hi there!")
    lh.critical("Hi there!")
    _ = 0 / 0

ssc = StomaSenseComm('/dev/cu.usbmodem11201')
def ssc_ok() -> bool:
    ssc.serial_send_command('OK')
    res = ssc.wait_for_response('OK', 5)
    if res:
        lh.info(f"Received OK: {res}")
        return True
    else:
        lh.warning(f"Didn't receive OK response")
        return False

def ssc_hx_raw(slot: int, n_stats: int, timeout_ms: int=5000) -> Optional[response_obj_t]:
    cmd = "hx_raw"
    ssc.serial_send_command(cmd, slot, n_stats, timeout_ms)
    res = ssc.wait_for_response(cmd, 5)
    if res and 'processing' in res:
        res = ssc.wait_for_response(cmd, n_stats * 0.1)

    if res:
        lh.info(f'Received hx_raw: {res}')
    else:
        lh.warning(f"Didn't reveive hx_raw {res}")
    return res

def ssc_set_calibration_null(idx: list[int]) -> bool:
    obj = list()
    for i in idx:
        obj.append(dict(
            r=i,
            o=0.0,
            p=0.0,
            s=1.0,
            t=0.0
        ))
    obj_str = json.dumps(obj).replace(' ','')
    cmd = "hx_calib"
    ssc.serial_send_command(cmd, 'set', obj_str)
    res = ssc.wait_for_response(cmd, 5)

    if res:
        lh.info(f'Received calibration: {res}')
    else:
        lh.warning(f"Didn't reveive hx_calib get {res}")
    return bool(res)

def ssc_get_calibration() -> Optional[response_obj_t]:
    cmd = "hx_calib"
    ssc.serial_send_command(cmd, "get")
    res = ssc.wait_for_response(cmd, 5)

    if res:
        lh.info(f'Received calibration: {res}')
    else:
        lh.warning(f"Didn't reveive hx_calib get {res}")
    return res

def ssc_calibrate(slot: int, n_stats: int, weight: float, weight_error: float) -> Optional[response_obj_t]:
    # calibrate offset
    cmd = "hx_calib"
    ssc.serial_send_command(cmd, "offset", slot, n_stats)
    res = ssc.wait_for_response(cmd, 5)
    if res and 'processing' in res:
        res = ssc.wait_for_response(cmd, n_stats * 0.1 + 5)

    if res:
        lh.info(f'Received offset calibration: {res}')
    else:
        lh.warning(f"Didn't reveive hx_calib {res}")
        return res
    
    # set offset
    if not 'calibs' in res:
        lh.error(f"Received response for offset calib without 'calibs' key: {res}")
    if not type(res['calibs']) is list:
        lh.error(f"calibs returned wasn't a json list ({res['calibs']})")
        return None
    elif any(type(o) is not dict for o in res['calibs']):
        lh.error(f"calibs returned wasn't a json list of objects({res['calibs']})")
        return None
    offset_calibs = [{k:v for k, v in o.items() if k in ('r', 'o', 'p')} for o in res['calibs']]
    offset_calibs_str = json.dumps(offset_calibs).replace(' ', '')
    ssc.serial_send_command(cmd, "set", offset_calibs_str)
    res = ssc.wait_for_response(cmd, 5)
    if res:
        lh.info(f'Received offset calibration: {res}')
    else:
        lh.warning(f"Didn't reveive hx_calib {res}")
        return res
    
    # calibrate slope
    ssc.serial_send_command(cmd, "slope", slot, n_stats, weight, weight_error)
    res = ssc.wait_for_response(cmd, 5)
    if res and 'processing' in res:
        res = ssc.wait_for_response(cmd, n_stats * 0.1 + 5)

    if res:
        lh.info(f'Received offset calibration: {res}')
    else:
        lh.warning(f"Didn't reveive hx_calib {res}")
        return res

    # set slope
    if not 'calibs' in res:
        lh.error(f"Received response for offset calib without 'calibs' key: {res}")
    offset_calibs = json.dumps(res['calibs']).replace(' ', '')
    ssc.serial_send_command(cmd, "set", offset_calibs)
    res = ssc.wait_for_response(cmd, 5)
    if res:
        lh.info(f'Received offset calibration: {res}')
    else:
        lh.warning(f"Didn't reveive hx_calib {res}")
        return res
    
    # save offset
    ssc.serial_send_command(cmd, "save")
    res = ssc.wait_for_response(cmd, 5)
    if res:
        lh.info(f'Received offset calibration: {res}')
    else:
        lh.warning(f"Didn't reveive hx_calib {res}")
    return res
    



if __name__ == "__main__":
    # logger_test()

    ssc.open()
    ssc.start_serial_receive_thread()
    ssc_ok()
    # ssc_hx_raw(0, 10)
    ssc_set_calibration_null([0])
    ssc_get_calibration()
    ssc_calibrate(0, 10, 5, 0.1)
    ssc.close()


    # print(get_comports())
    # ok_test('/dev/cu.usbmodem11201')