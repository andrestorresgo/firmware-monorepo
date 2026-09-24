#!/usr/bin/env python3
"""
Factory Conveyor System - Mock Detection CLI
Trigger mock vision detections, batch rollovers, and servo commands manually
against the production API (or local backend).
"""

from __future__ import annotations

import argparse
import json
import os
import random
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from typing import Any, Dict, Optional, Tuple
from uuid import uuid4

DEFAULT_URL = os.getenv("API_URL", "https://backend-service-hyc2.onrender.com").rstrip("/")
DEFAULT_TOKEN = os.getenv("VISION_BEARER_TOKEN", "secret-vision-token")
DEFAULT_ENDPOINT = "/api/v1/detections"
DEBOUNCE_COOLDOWN = 2.2  # Seconds (backend debounce window is 2.0s)

# ANSI Colors
RESET = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"
RED = "\033[91m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
MAGENTA = "\033[95m"
CYAN = "\033[96m"
WHITE = "\033[97m"

SHAPES = {
    1: {"name": "circle", "color": "red", "alias": ["circle", "circulo", "círculo", "red", "1", "c"], "icon": "🔴", "style": RED},
    2: {"name": "triangle", "color": "green", "alias": ["triangle", "triangulo", "triángulo", "green", "2", "t"], "icon": "🟢", "style": GREEN},
    3: {"name": "square", "color": "blue", "alias": ["square", "cuadrado", "blue", "3", "s"], "icon": "🔵", "style": BLUE},
}


def resolve_shape(user_input: str) -> Optional[Tuple[int, str, str, str, str]]:
    """Resolve user input to (shape_id, shape_name, color, icon, style)."""
    clean = user_input.strip().lower()
    for sid, info in SHAPES.items():
        if clean in info["alias"] or clean == info["name"]:
            return sid, info["name"], info["color"], info["icon"], info["style"]
    return None


class MockDetectionClient:
    def __init__(self, base_url: str = DEFAULT_URL, token: str = DEFAULT_TOKEN, endpoint: str = DEFAULT_ENDPOINT):
        self.base_url = base_url.rstrip("/")
        self.token = token
        self.endpoint = endpoint if endpoint.startswith("/") else f"/{endpoint}"

    def _http_request(self, method: str, path: str, payload: Optional[Dict[str, Any]] = None, extra_headers: Optional[Dict[str, str]] = None) -> Tuple[int, Dict[str, Any]]:
        url = f"{self.base_url}{path}"
        headers = {
            "User-Agent": "MockDetectionCLI/1.0",
            "Accept": "application/json",
        }
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        if extra_headers:
            headers.update(extra_headers)

        data = None
        if payload is not None:
            data = json.dumps(payload).encode("utf-8")
            headers["Content-Type"] = "application/json"

        req = urllib.request.Request(url, data=data, headers=headers, method=method)

        try:
            with urllib.request.urlopen(req, timeout=15) as resp:
                status_code = resp.status
                body = resp.read().decode("utf-8")
                try:
                    return status_code, json.loads(body)
                except Exception:
                    return status_code, {"raw": body}
        except urllib.error.HTTPError as e:
            body = e.read().decode("utf-8")
            try:
                parsed = json.loads(body)
            except Exception:
                parsed = {"error": body or str(e)}
            return e.code, parsed
        except urllib.error.URLError as e:
            return 0, {"error": f"Network unreachable: {e.reason}"}
        except Exception as e:
            return 0, {"error": str(e)}

    def check_health(self) -> Tuple[bool, Dict[str, Any]]:
        status, data = self._http_request("GET", "/healthz")
        return (status == 200, data)

    def get_state(self) -> Tuple[int, Dict[str, Any]]:
        return self._http_request("GET", "/api/v1/state")

    def trigger_detection(self, shape_name: str, confidence: float = 0.95, event_id: Optional[str] = None) -> Tuple[int, Dict[str, Any]]:
        ev_id = event_id or str(uuid4())
        now_iso = datetime.now(timezone.utc).isoformat()
        payload = {
            "shape": shape_name,
            "confidence": round(confidence, 4),
            "timestamp": now_iso,
        }
        extra_headers = {"X-Event-ID": ev_id}
        return self._http_request("POST", self.endpoint, payload=payload, extra_headers=extra_headers)

    def command_servo(self, state: str) -> Tuple[int, Dict[str, Any]]:
        payload = {"state": state.upper()}
        return self._http_request("POST", "/api/v1/actuator/servo", payload=payload)

    def command_motor(self, state: str) -> Tuple[int, Dict[str, Any]]:
        payload = {"state": state.upper()}
        return self._http_request("POST", "/api/v1/actuator/motor", payload=payload)


def print_banner(client: MockDetectionClient):
    print(f"{CYAN}{BOLD}{'═' * 70}{RESET}")
    print(f"{CYAN}{BOLD}  🏭 FACTORY CONVEYOR - MOCK DETECTION CLI (PROD API){RESET}")
    print(f"{CYAN}{BOLD}{'═' * 70}{RESET}")
    print(f"  {BOLD}Target API:{RESET} {client.base_url}")
    print(f"  {BOLD}Endpoint:  {RESET} {client.endpoint}")
    print(f"  {BOLD}Auth:      {RESET} Bearer {'*' * (len(client.token) - 4) + client.token[-4:] if len(client.token) > 4 else 'present'}")

    ok, health = client.check_health()
    if ok:
        db_stat = health.get("database", {}).get("status", "unknown")
        mqtt_stat = health.get("mqtt", {}).get("status", "unknown")
        print(f"  {BOLD}Status:    {RESET} {GREEN}● LIVE{RESET} (DB: {db_stat}, MQTT: {mqtt_stat})")
    else:
        err = health.get("error", "offline")
        print(f"  {BOLD}Status:    {RESET} {RED}● UNHEALTHY / UNREACHABLE ({err}){RESET}")
    print(f"{CYAN}{'─' * 70}{RESET}")


def show_state(client: MockDetectionClient):
    status, data = client.get_state()
    if status != 200:
        print(f"{RED}❌ Failed to fetch system state (HTTP {status}): {data.get('error', 'unknown')}{RESET}")
        return

    sys_state = data.get("system_state", {})
    shape_counts = data.get("shape_counts", [])
    mqtt_conn = data.get("mqtt_connected", False)

    paused = sys_state.get("is_paused", False)
    motor = sys_state.get("motor_state", "OFF")
    servo = sys_state.get("servo_state", False)
    last_telem = sys_state.get("last_telemetry_at") or "None (waiting for Board A heartbeat)"

    motor_str = str(motor).upper()
    if motor_str in ("ON", "TRUE", "1"):
        motor_display = f"{GREEN}ACTIVE / ON (80% PWM){RESET}"
    elif motor_str in ("MEDIUM", "2"):
        motor_display = f"{YELLOW}ACTIVE / MEDIUM (50% PWM){RESET}"
    else:
        motor_display = f"{DIM}HALTED / OFF (0% PWM){RESET}"

    print(f"\n{BOLD}📊 CURRENT SYSTEM STATE:{RESET}")
    print(f"  • Machine Paused:    {RED if paused else GREEN}{'YES (LOCKED)' if paused else 'NO (RUNNING)'}{RESET}")
    print(f"  • DC Motor Conveyor: {motor_display}")
    print(f"  • Servo Sorting Gate:{CYAN}{'OPEN' if servo else 'CLOSED'}{RESET}")
    print(f"  • MQTT Broker State: {GREEN if mqtt_conn else RED}{'CONNECTED' if mqtt_conn else 'DISCONNECTED'}{RESET}")
    print(f"  • Last Telemetry:    {DIM}{last_telem}{RESET}")

    print(f"\n{BOLD}🔢 SHAPE COUNTERS (Actuator LED Banks & Cloud Rollovers):{RESET}")
    for item in shape_counts:
        sid = item.get("shape_id")
        name = item.get("shape_name", "").capitalize()
        color = item.get("color_label", "")
        buffer_val = item.get("live_buffer", 0)
        lifetime = item.get("total_lifetime", 0)

        icon = "🔴" if sid == 1 else ("🟢" if sid == 2 else "🔵")
        style = RED if sid == 1 else (GREEN if sid == 2 else BLUE)

        # 3-bit binary representation (000 to 101)
        bin_rep = f"{buffer_val:03b}" if buffer_val <= 5 else "---"

        print(f"  {icon} {style}{BOLD}{name:<9}{RESET} ({color:<5}): "
              f"Current Count = {BOLD}{buffer_val}/5{RESET} [Binary LED: {CYAN}{bin_rep}{RESET}]  "
              f"| Rollover Batches = {BOLD}{lifetime}{RESET}")
    print()


def send_single_detection(client: MockDetectionClient, shape_name: str, confidence: float = 0.95, verbose: bool = True) -> bool:
    res = resolve_shape(shape_name)
    if not res:
        print(f"{RED}❌ Unknown shape '{shape_name}'. Choose circle (1), triangle (2), or square (3).{RESET}")
        return False

    sid, sname, color, icon, style = res
    if verbose:
        print(f"🚀 Triggering {icon} {style}{BOLD}{sname.upper()}{RESET} ({color}, shape_id={sid}, conf={confidence:.2f})...")

    status, resp = client.trigger_detection(sname, confidence=confidence)

    if status == 200:
        det_status = resp.get("status")
        det_id = resp.get("detection_id")
        msg = resp.get("message", "")

        if det_status == "dispatched":
            print(f"  {GREEN}✅ DISPATCHED!{RESET} Detection ID: {BOLD}#{det_id}{RESET}")
            print(f"     {DIM}↳ HiveMQ: 'factory/detections' ➔ Board A (Gateway) ➔ ESP-NOW ➔ Board B (Actuator){RESET}")
            return True
        elif det_status == "debounced":
            print(f"  {YELLOW}⚠️  DEBOUNCED!{RESET} {msg or 'Duplicate dropped by 2s window'}")
            print(f"     {DIM}↳ The backend enforces a 2.0s rate-limit window per shape to drop duplicate camera frames.{RESET}")
            return False
        else:
            print(f"  {CYAN}ℹ️  Result:{RESET} {resp}")
            return True
    elif status == 401:
        print(f"  {RED}❌ 401 UNAUTHORIZED!{RESET} Bearer token '{client.token}' was rejected by the API.")
        return False
    else:
        err = resp.get("error", resp.get("message", "Unknown error"))
        print(f"  {RED}❌ FAILED (HTTP {status}):{RESET} {err}")
        return False


def run_batch_sequence(client: MockDetectionClient, shape_name: str, count: int = 5, delay: float = DEBOUNCE_COOLDOWN):
    res = resolve_shape(shape_name)
    if not res:
        print(f"{RED}❌ Unknown shape '{shape_name}'.{RESET}")
        return

    sid, sname, color, icon, style = res
    print(f"\n{BOLD}📦 STARTING BATCH SEQUENCE FOR {icon} {style}{sname.upper()}{RESET} ({count} detections)")
    print(f"   Note: Pausing {delay:.1f}s between detections to safely clear the 2.0s backend debounce window.")
    print(f"   At count 5, Board B holds for 800ms observation delay and triggers Batch Rollover!\n")

    dispatched_count = 0
    for i in range(1, count + 1):
        print(f"[{i}/{count}] ", end="")
        ok = send_single_detection(client, sname, verbose=True)
        if ok:
            dispatched_count += 1

        if i < count:
            print(f"     {DIM}⏳ Waiting {delay:.1f}s for debounce window...{RESET}", end="", flush=True)
            time.sleep(delay)
            print("\r" + " " * 50 + "\r", end="", flush=True)

    print(f"\n{GREEN}{BOLD}✨ Batch sequence completed!{RESET} Dispatched: {dispatched_count}/{count} detections.")
    print(f"💡 Check the physical Board B LEDs or the dashboard to see the 800ms observation delay and counter rollover!\n")


def run_loop_mode(client: MockDetectionClient, interval: float = 3.0, shape: Optional[str] = None):
    print(f"\n{CYAN}{BOLD}🔄 ENTERING CONTINUOUS AUTO-DETECTION STREAM{RESET}")
    print(f"   Interval: {interval:.1f}s (Ctrl+C to stop)\n")

    shape_list = [shape] if shape else ["circle", "triangle", "square"]
    idx = 0

    try:
        while True:
            target_shape = shape_list[idx % len(shape_list)] if shape else random.choice(["circle", "triangle", "square"])
            idx += 1
            print(f"[{datetime.now().strftime('%H:%M:%S')}] ", end="")
            send_single_detection(client, target_shape, verbose=True)
            time.sleep(max(DEBOUNCE_COOLDOWN, interval))
    except KeyboardInterrupt:
        print(f"\n{YELLOW}🛑 Continuous stream stopped by user.{RESET}\n")


def toggle_servo(client: MockDetectionClient, state: str):
    clean = state.strip().upper()
    if clean in ("CLOSE", "CLOSED", "C", "0", "FALSE"):
        target = "CLOSED"
    elif clean in ("OPEN", "O", "1", "TRUE"):
        target = "OPEN"
    else:
        target = clean

    print(f"🚪 Sending SERVO command: {CYAN}{BOLD}{target}{RESET}...")
    status, resp = client.command_servo(target)
    if status == 200:
        print(f"  {GREEN}✅ DISPATCHED!{RESET} HiveMQ 'factory/actuator/servo' ➔ Board A ➔ ESP-NOW ➔ Board B servo = {target}")
    else:
        print(f"  {RED}❌ FAILED (HTTP {status}):{RESET} {resp.get('error', resp)}")


def control_motor(client: MockDetectionClient, state: str):
    clean = state.strip().upper()
    if clean in ("0", "OFF", "STOP", "HALT", "FALSE"):
        target = "OFF"
    elif clean in ("2", "MED", "MEDIUM", "HALF", "MID"):
        target = "MEDIUM"
    elif clean in ("1", "ON", "START", "RUN", "TRUE"):
        target = "ON"
    else:
        target = clean

    print(f"⚙️ Sending MOTOR speed command: {CYAN}{BOLD}{target}{RESET}...")
    status, resp = client.command_motor(target)
    if status == 200:
        print(f"  {GREEN}✅ DISPATCHED!{RESET} HiveMQ 'factory/actuator/motor' ➔ Board A ➔ ESP-NOW ➔ Board B motor = {target}")
    else:
        print(f"  {RED}❌ FAILED (HTTP {status}):{RESET} {resp.get('error', resp)}")


def interactive_menu(client: MockDetectionClient):
    while True:
        print_banner(client)
        print("  Quick triggers:")
        print(f"    {BOLD}[1]{RESET} 🔴 Circle   (Red   / ID 1)")
        print(f"    {BOLD}[2]{RESET} 🟢 Triangle (Green / ID 2)")
        print(f"    {BOLD}[3]{RESET} 🔵 Square   (Blue  / ID 3)")
        print(f"    {BOLD}[r]{RESET} 🎲 Random Shape")
        print()
        print("  Advanced actuation & testing:")
        print(f"    {BOLD}[b]{RESET} 📦 Batch Sequence (5x detections ➔ Rollover + 800ms Observation Delay)")
        print(f"    {BOLD}[m]{RESET} ⚙️ Set Motor Speed (OFF / MEDIUM / ON)")
        print(f"    {BOLD}[o]{RESET} 🚪 Open Servo Gate")
        print(f"    {BOLD}[c]{RESET} 🚪 Close Servo Gate")
        print(f"    {BOLD}[s]{RESET} 📊 View Live System State & Shape Counters")
        print(f"    {BOLD}[l]{RESET} 🔄 Auto-stream loop mode")
        print(f"    {BOLD}[q]{RESET} 🚪 Exit")
        print(f"{CYAN}{'─' * 70}{RESET}")

        try:
            choice = input(f"{BOLD}Select an option [1-3, r, b, o, c, s, l, q]: {RESET}").strip().lower()
        except (KeyboardInterrupt, EOFError):
            print("\nBye!")
            break

        if choice in ("q", "quit", "exit"):
            print("Bye!")
            break
        elif choice == "1":
            send_single_detection(client, "circle")
        elif choice == "2":
            send_single_detection(client, "triangle")
        elif choice == "3":
            send_single_detection(client, "square")
        elif choice == "r":
            rand_shape = random.choice(["circle", "triangle", "square"])
            send_single_detection(client, rand_shape)
        elif choice == "b":
            s_choice = input(f"Shape for batch [circle/triangle/square, default=circle]: ").strip().lower() or "circle"
            n_choice = input(f"Number of detections [default=5]: ").strip() or "5"
            try:
                count = int(n_choice)
            except ValueError:
                count = 5
            run_batch_sequence(client, s_choice, count=count)
        elif choice == "m":
            m_choice = input(f"Motor speed [off/medium/on, default=medium]: ").strip().lower() or "medium"
            control_motor(client, m_choice)
        elif choice == "o":
            toggle_servo(client, "OPEN")
        elif choice == "c":
            toggle_servo(client, "CLOSED")
        elif choice == "s":
            show_state(client)
        elif choice == "l":
            interval_str = input(f"Loop interval seconds [default=3.0]: ").strip() or "3.0"
            try:
                interval = float(interval_str)
            except ValueError:
                interval = 3.0
            run_loop_mode(client, interval=interval)
        else:
            print(f"{RED}Invalid option '{choice}'{RESET}")

        input(f"\n{DIM}Press Enter to return to menu...{RESET}")


def main():
    parser = argparse.ArgumentParser(
        description="Factory Conveyor - Mock Detection & Actuator CLI (Prod API)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  ./mock-detect                     # Launch interactive menu
  ./mock-detect circle              # Trigger 1 circle detection
  ./mock-detect triangle            # Trigger 1 triangle detection
  ./mock-detect square              # Trigger 1 square detection
  ./mock-detect random              # Trigger 1 random shape detection
  ./mock-detect batch circle 5      # Trigger 5 circles with 2.2s delay (triggers rollover)
  ./mock-detect status              # View current system state and counters
  ./mock-detect motor on            # Send ON (80%) speed command to DC motor
  ./mock-detect motor medium        # Send MEDIUM (50%) speed command to DC motor
  ./mock-detect motor off           # Send OFF (0%) speed command to DC motor
  ./mock-detect servo open          # Send OPEN command to sorting gate
  ./mock-detect servo close         # Send CLOSED command to sorting gate
  ./mock-detect loop --interval 2.5 # Auto-emit detections every 2.5s
        """
    )
    parser.add_argument("command", nargs="?", help="Command or shape: circle, triangle, square, batch, motor, servo, status, loop, random")
    parser.add_argument("args", nargs="*", help="Arguments for the command (e.g. 'batch circle 5', 'motor medium', or 'servo open')")
    parser.add_argument("--url", "-u", default=DEFAULT_URL, help=f"API Base URL (default: {DEFAULT_URL})")
    parser.add_argument("--token", "-t", default=DEFAULT_TOKEN, help="Bearer token for vision webhook")
    parser.add_argument("--endpoint", "-e", default=DEFAULT_ENDPOINT, help=f"Webhook endpoint (default: {DEFAULT_ENDPOINT})")
    parser.add_argument("--confidence", "-c", type=float, default=0.95, help="Detection confidence (default: 0.95)")
    parser.add_argument("--delay", "-d", type=float, default=DEBOUNCE_COOLDOWN, help=f"Batch delay in seconds (default: {DEBOUNCE_COOLDOWN}s)")
    parser.add_argument("--compat", action="store_true", help="Use compatibility endpoint /api/vision/detection")

    parsed = parser.parse_args()

    endpoint = "/api/vision/detection" if parsed.compat else parsed.endpoint
    client = MockDetectionClient(base_url=parsed.url, token=parsed.token, endpoint=endpoint)

    if not parsed.command:
        interactive_menu(client)
        return

    cmd = parsed.command.strip().lower()

    # Direct shape shortcut
    if resolve_shape(cmd):
        send_single_detection(client, cmd, confidence=parsed.confidence)
        return

    if cmd in ("random", "rand"):
        send_single_detection(client, random.choice(["circle", "triangle", "square"]), confidence=parsed.confidence)
        return

    if cmd in ("status", "state", "info"):
        print_banner(client)
        show_state(client)
        return

    if cmd == "batch":
        shape_arg = parsed.args[0] if len(parsed.args) > 0 else "circle"
        count_arg = int(parsed.args[1]) if len(parsed.args) > 1 and parsed.args[1].isdigit() else 5
        run_batch_sequence(client, shape_arg, count=count_arg, delay=parsed.delay)
        return

    if cmd == "motor":
        target = parsed.args[0] if len(parsed.args) > 0 else "MEDIUM"
        control_motor(client, target)
        return

    if cmd == "servo":
        target = parsed.args[0] if len(parsed.args) > 0 else "OPEN"
        toggle_servo(client, target)
        return

    if cmd == "loop":
        interval = float(parsed.args[0]) if len(parsed.args) > 0 else 3.0
        run_loop_mode(client, interval=interval)
        return

    print(f"{RED}Unknown command or shape: '{cmd}'. Run with --help for usage.{RESET}")
    sys.exit(1)


if __name__ == "__main__":
    main()
