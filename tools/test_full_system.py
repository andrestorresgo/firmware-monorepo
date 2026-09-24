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
    duration = 18.0
    t = threading.Thread(target=monitor, args=('/dev/ttyUSB1', duration, b_lines))
    t.start()

    time.sleep(3.0)

    print("Step 1: Triggering Circle (Red LED bit 0)...")
    subprocess.run(['python3', 'tools/mock_detect.py', 'circle'])
    time.sleep(2.5)

    print("Step 2: Triggering Triangle (Green LED bit 0)...")
    subprocess.run(['python3', 'tools/mock_detect.py', 'triangle'])
    time.sleep(2.5)

    print("Step 3: Triggering Square (Blue LED bit 0)...")
    subprocess.run(['python3', 'tools/mock_detect.py', 'square'])
    time.sleep(2.5)

    print("Step 4: Triggering Servo OPEN (90 degrees)...")
    subprocess.run(['python3', 'tools/mock_detect.py', 'servo', 'open'])
    time.sleep(2.5)

    print("Step 5: Triggering Servo CLOSE (0 degrees)...")
    subprocess.run(['python3', 'tools/mock_detect.py', 'servo', 'close'])
    time.sleep(2.0)

    t.join()

    print("\n" + "=" * 60)
    print("BOARD B LOGGED EVENTS:")
    print("=" * 60)
    for ts, line in b_lines:
        if any(k in line for k in ["Handled", "Received", "Emitted", "Beacon", "paired"]):
            print(f"[{ts:04.1f}s] {line}")

if __name__ == '__main__':
    main()
