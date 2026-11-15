#!/usr/bin/env python3
"""Capture a bounded ESP32 USB boot log using PlatformIO's Python environment."""
import argparse
from pathlib import Path
import time
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True)
parser.add_argument('--seconds', type=float, default=20)
parser.add_argument('--output', required=True)
parser.add_argument('--reset', action='store_true')
args = parser.parse_args()
port = serial.Serial(port=None, baudrate=115200, timeout=0.25)
port.dtr = False
port.rts = False
port.port = args.port
port.open()
try:
    if args.reset:
        # Same USB Serial/JTAG hard-reset sequence used by esptool.
        port.rts = True
        time.sleep(0.2)
        port.rts = False
    deadline = time.monotonic() + args.seconds
    chunks = []
    while time.monotonic() < deadline:
        try:
            data = port.read(4096)
            if data:
                chunks.append(data)
        except serial.SerialException:
            port.close()
            time.sleep(0.5)
            try:
                port.open()
            except serial.SerialException:
                pass
    data = b''.join(chunks)
    Path(args.output).write_bytes(data)
    print(data.decode('utf-8', errors='replace'))
    print(f'Captured {len(data)} bytes to {args.output}')
finally:
    port.close()
