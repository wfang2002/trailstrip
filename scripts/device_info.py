#!/usr/bin/env python3
"""Read-only USB serial discovery. Run with PlatformIO's Python environment."""
from serial.tools import list_ports

ports = list(list_ports.comports())
usb = [p for p in ports if p.vid is not None]
if not usb:
    print("No USB serial device detected. Check a data cable and enter BOOT/RESET mode.")
    raise SystemExit(1)
for port in usb:
    print(f"{port.device}: {port.description}; USB {port.vid:04x}:{port.pid:04x}")
print("USB identity does not identify the Waveshare PCB revision. Check V2/Rev1.1 marking.")
