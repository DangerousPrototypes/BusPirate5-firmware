#!/usr/bin/env python3
"""
TCP-to-Serial bridge for forwarding a COM port over the network.
Run this on Windows to expose a COM port to WSL2 or other network clients.

Usage:
    python serial_bridge.py COM35 --baud 115200 --port 2217

Then from WSL2, connect with:
    python -c "import serial; s = serial.serial_for_url('socket://172.26.208.1:2217')"
    or: socat - TCP:172.26.208.1:2217
    or: use BPSerial with the --socket option
"""
import argparse
import serial
import socket
import threading
import sys
import time

def serial_to_socket(ser, conn, addr, stop_event):
    """Forward data from serial port to TCP socket."""
    try:
        while not stop_event.is_set():
            data = ser.read(ser.in_waiting or 1)
            if data:
                conn.sendall(data)
    except (serial.SerialException, OSError, ConnectionError):
        pass
    finally:
        stop_event.set()

def socket_to_serial(ser, conn, addr, stop_event):
    """Forward data from TCP socket to serial port."""
    try:
        while not stop_event.is_set():
            data = conn.recv(4096)
            if not data:
                break
            ser.write(data)
    except (serial.SerialException, OSError, ConnectionError):
        pass
    finally:
        stop_event.set()

def handle_client(ser, conn, addr):
    """Handle a single client connection."""
    print(f"[+] Client connected: {addr}")
    stop_event = threading.Event()

    t1 = threading.Thread(target=serial_to_socket, args=(ser, conn, addr, stop_event), daemon=True)
    t2 = threading.Thread(target=socket_to_serial, args=(ser, conn, addr, stop_event), daemon=True)
    t1.start()
    t2.start()

    # Wait until one direction dies
    while not stop_event.is_set():
        time.sleep(0.1)

    conn.close()
    # Drain any stale data from the serial port so the next client
    # starts clean, but do NOT close/reopen the serial port.
    try:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
    except Exception:
        pass
    print(f"[-] Client disconnected: {addr}")

def main():
    parser = argparse.ArgumentParser(description="TCP-to-Serial bridge")
    parser.add_argument("comport", help="Serial port (e.g., COM35, /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--port", type=int, default=2217, help="TCP port to listen on (default: 2217)")
    parser.add_argument("--host", default="0.0.0.0", help="Host to bind to (default: 0.0.0.0)")
    args = parser.parse_args()

    # Open serial port
    try:
        ser = serial.Serial(args.comport, args.baud, timeout=0.1)
        # For Bus Pirate CDC ACM, these settings help
        ser.dtr = True
        ser.rts = True
        print(f"[*] Opened {args.comport} at {args.baud} baud")
    except serial.SerialException as e:
        print(f"[!] Failed to open {args.comport}: {e}")
        sys.exit(1)

    # Start TCP server
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.host, args.port))
    server.listen(1)
    print(f"[*] Listening on {args.host}:{args.port}")
    print(f"[*] From WSL2, connect with: socket://$(ip route show default | awk '{{print $3}}'):{args.port}")

    try:
        while True:
            conn, addr = server.accept()
            # Only allow one client at a time (serial is exclusive)
            handle_client(ser, conn, addr)
    except KeyboardInterrupt:
        print("\n[*] Shutting down...")
    finally:
        ser.close()
        server.close()

if __name__ == "__main__":
    main()
