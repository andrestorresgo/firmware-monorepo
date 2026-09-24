#!/usr/bin/env python3
import sys
import time
import serial
import threading

def monitor_port(port, name, duration, lines_out):
    try:
        ser = serial.Serial()
        ser.port = port
        ser.baudrate = 115200
        ser.timeout = 0.5
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
                    lines_out.append((time.time() - start, name, line))
        ser.close()
    except Exception as e:
        lines_out.append((0, name, f"ERROR opening {port}: {e}"))

def main():
    duration = 8.0
    a_lines = []
    b_lines = []

    t_a = threading.Thread(target=monitor_port, args=('/dev/ttyUSB0', 'BOARD_A', duration, a_lines))
    t_b = threading.Thread(target=monitor_port, args=('/dev/ttyUSB1', 'BOARD_B', duration, b_lines))

    t_a.start()
    t_b.start()
    t_a.join()
    t_b.join()

    all_lines = sorted(a_lines + b_lines, key=lambda x: x[0])
    for ts, name, line in all_lines:
        print(f"[{ts:04.1f}s][{name}] {line}")

    # Evaluation
    b_paired = any("link paired!" in l[2] or "Captured Gateway Discovery Beacon" in l[2] or "Emitted Telemetry" in l[2] or "Received SHAPE_DETECTION" in l[2] or "Received SERVO_COMMAND" in l[2] for l in b_lines)
    b_scanning = any("Scanning channel" in l[2] for l in b_lines)
    a_paired = any("BEACON_ACK" in l[2] or "Received Telemetry" in l[2] or "Forwarded Shape Detection" in l[2] for l in a_lines)

    print("=" * 40)
    print(f"Board B Scanning: {b_scanning}")
    print(f"Board B Paired:   {b_paired}")
    print(f"Board A Paired:   {a_paired}")
    print("=" * 40)

    if not b_paired and b_scanning:
        print("FAIL: ESP-NOW is NOT paired. Board B is endlessly scanning RF channels without hearing Board A.")
        sys.exit(1)
    elif b_paired:
        print("PASS: ESP-NOW is paired.")
        sys.exit(0)
    else:
        print("UNKNOWN: Insufficient serial output detected.")
        sys.exit(2)

if __name__ == '__main__':
    main()
