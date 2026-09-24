#!/usr/bin/env python3
import time
import serial
import subprocess
import threading

def monitor(port, duration, lines_out):
    try:
        ser = serial.Serial()
        ser.port = port
        ser.baudrate = 115200
        ser.timeout = 0.2
        ser.dtr = False
        ser.rts = False
        ser.open()
        time.sleep(0.1)
        ser.reset_input_buffer()
        start = time.time()
        while time.time() - start < duration:
            raw = ser.readline()
            if raw:
                line = raw.decode('utf-8', errors='replace').strip()
                if line:
                    lines_out.append((time.time() - start, line))
        ser.close()
    except Exception as e:
        lines_out.append((0, f"Error: {e}"))

def main():
    b_lines = []
    t = threading.Thread(target=monitor, args=('/dev/ttyUSB1', 12.0, b_lines))
    t.start()

    time.sleep(2.0)
    print("--- 1. Sending Servo OPEN command ---")
    subprocess.run(['python3', 'tools/mock_detect.py', 'servo', 'open'])
    time.sleep(2.5)

    print("--- 2. Sending Servo CLOSE command ---")
    subprocess.run(['python3', 'tools/mock_detect.py', 'servo', 'close'])
    time.sleep(2.5)

    print("--- 3. Sending Shape Detection: Circle (Red) ---")
    subprocess.run(['python3', 'tools/mock_detect.py', 'circle'])
    time.sleep(2.5)

    print("--- 4. Sending Shape Detection: Triangle (Green) ---")
    subprocess.run(['python3', 'tools/mock_detect.py', 'triangle'])
    time.sleep(2.5)

    t.join()

    print("\n" + "=" * 50)
    print("BOARD B CAPTURED LOGS:")
    print("=" * 50)
    for ts, line in b_lines:
        print(f"[{ts:04.1f}s] {line}")

if __name__ == '__main__':
    main()
