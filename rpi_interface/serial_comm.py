import serial
from serial.tools import list_ports
from threading import Thread
from logging_helper import logger as lh
from typing import Callable, Optional, Tuple, Union
from time import sleep, time
import json
import sys, glob

serial_comm_cb_t = Callable[[str], None]

def ok_test(port: str) -> None:
    while True:
        with serial.Serial(port) as s:
            s.write('OK\n'.encode())
            print("Sent 'OK', Response: ", end='')
            ti = time()
            while (not s.in_waiting) and (time() - ti <= 2):
                sleep(0.1)
            msg = s.read_all()
            if msg:
                msg = msg.decode()
            print(msg)
        sleep(1.5)

def get_devices() -> list[str]:
    """ Lists serial port names

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result

def get_comports() -> list[str]:
    return list(p.device for p in list_ports.comports())

class SerialComm:
    def __init__(self, port: str, baudrate: int=115200, end_char='\n', max_chars_in_msg_in: int = 1024, timeout: float=5, write_timeout: float=3, ignore_chars: str = '\r') -> None:
        ports = get_comports()
        if port not in ports:
            raise Exception(f"Couldn't find port {port}, available ports are {ports}")

        self._ser = serial.Serial(port=port, baudrate=baudrate, timeout=timeout, write_timeout=write_timeout)
        self._end_char = end_char
        self._max_chars_in_msg_in = max_chars_in_msg_in

        self._ignore_chars = ignore_chars

        self._cb: serial_comm_cb_t = lambda msg: print("SerialComm received:", msg)
        self._serial_receive_thread: Optional[Thread] = None
        self._stop_threads: bool = False

    def stop_threads(self) -> None:
        if (self._serial_receive_thread is None) or (not self.is_serial_receive_thread_running()): return
        self._stop_threads = True
        self._serial_receive_thread.join()

    def close(self) -> None:
        self.stop_threads()
        if self.is_open():
            self._ser.close()

    def is_open(self) -> bool:
        return self._ser.is_open

    def open(self) -> bool:
        if not self.is_open():
            try:
                self._ser.open()
            except serial.SerialException as e:
                lh.error(f"Couldn't open serial with error {e}")
                return False
        lh.info(f"Opened port {self._ser.port}")
        return True
        
    def set_callback(self, cb: serial_comm_cb_t) -> None:
        self._cb = cb

    def _callback_wrapper(self) -> None:
        msg_in: str = ''
        while not self._stop_threads:
            while self._ser.in_waiting:
                c = self._ser.read().decode("utf-8")
                if c in self._ignore_chars:
                    continue
                if c == self._end_char:
                    self._cb(msg_in)
                    msg_in = ''
                else:
                    msg_in += c
                    while len(msg_in) >= self._max_chars_in_msg_in:
                        msg_in = msg_in[1:]
            sleep(0.1)
        
    def is_serial_receive_thread_running(self) -> bool:
        if self._serial_receive_thread is not None:
            return self._serial_receive_thread.is_alive()
        else:
            return False
        
    def start_serial_receive_thread(self, auto_open: bool=True) -> bool:
        if not self.is_open():
            if auto_open:
                if not self.open():
                    lh.error("Couldn't start serial receive thread because serial couldn't open")
                    return False
            else:
                lh.error("Couldn't start serial receive thread because serial wasn't open and auto_open was set to False")
                return False

        self._stop_threads = False
        self._serial_receive_thread = Thread(target=self._callback_wrapper, daemon=True)
        self._serial_receive_thread.start() # run() runs the thread synchronously :(
        return self.is_serial_receive_thread_running()
    
    def serial_send_str(self, msg_ascii: str) -> bool:
        encoded = msg_ascii.encode('ascii')
        size = self._ser.write(encoded)
        return size == len(encoded)

response_obj_t = dict[str, Union[str, int, float, bool, list, dict]]
stomasense_process_msg_cb_t = Callable[[response_obj_t], None]
class StomaSenseComm(SerialComm):
    def __init__(self, port: str, end_char='\n', sep_char=' ', max_msg_deque_size = 128) -> None:
        super().__init__(port, 115200, end_char)
        self._sep_char = sep_char
        self._process_msg_cb: Optional[stomasense_process_msg_cb_t] = None

        self._msg_queue: list[response_obj_t] = list()
        self._max_msg_deque_size = max_msg_deque_size

        self.set_callback(self._default_cb)

    def set_proccess_msg_callback(self, process_msg_cb: Optional[stomasense_process_msg_cb_t]) -> None:
        self._process_msg_cb = process_msg_cb

    def _default_cb(self, msg: str) -> None:
        obj: Optional[response_obj_t] = None
        try:
            obj = json.loads(msg)
        except json.JSONDecodeError as e:
            lh.warning(f"Got message from StomaSense that wasn't a JSON: {msg}")
            return
        
        lh.info(f"Received serial: {obj}")
        self._msg_queue.append(obj.copy()) # type: ignore
        if (len(self._msg_queue) >= self._max_msg_deque_size):
            self._msg_queue = self._msg_queue[1:]
        if (self._process_msg_cb):
            self._process_msg_cb(obj) # type: ignore

    def serial_send_command(self, cmd: str, *args: Union[str, int, float, bool]) -> bool:
        s = cmd
        for arg in args:
            s += self._sep_char + str(arg)
        s += self._end_char
        return self.serial_send_str(s)
    
    def get_next_response(self, cmd: Optional[str]) -> Optional[response_obj_t]:
        l = len(self._msg_queue)
        if cmd is None and l > 0:
            return self._msg_queue.pop(0)
        for i in range(l):
            obj = self._msg_queue[i]
            if 'cmd' in obj and obj['cmd'] == cmd:
                return self._msg_queue.pop(i)
        return None
    
    def wait_for_response(self, cmd: Optional[str], timeout_s: float) -> Optional[response_obj_t]:
        if cmd == '': cmd = None

        r: Optional[response_obj_t] = None

        if timeout_s <= 0:
            while not r:
                r = self.get_next_response(cmd)
        else:    
            ti = time()
            while (not r) and (time() - ti <= timeout_s):
                r = self.get_next_response(cmd)
    
        if not r:
            lh.warning(f"Couldn't get response{f' with cmd = {cmd}' if cmd else ''}")
            print(self._msg_queue)
        return r