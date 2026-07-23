#!/usr/bin/env python3
import argparse
import binascii
import os
import select
import sys
import termios
import time

BAUDS = {
    115200: termios.B115200,
    921600: getattr(termios, "B921600", termios.B115200),
}


def configure(fd, baud):
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = BAUDS[baud]
    attrs[5] = BAUDS[baud]
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)


def read_lines(fd, timeout):
    data = b""
    lines = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        readable, _, _ = select.select([fd], [], [], 0.05)
        if not readable:
            continue
        chunk = os.read(fd, 4096)
        if not chunk:
            continue
        data += chunk
        while b"\n" in data:
            line, data = data.split(b"\n", 1)
            lines.append(line.decode("utf-8", "replace").rstrip("\r"))
    if data:
        lines.append(data.decode("utf-8", "replace").rstrip("\r"))
    return lines


def run_command(port, command, timeout):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        configure(fd, 115200)
        os.write(fd, (command.rstrip("\n") + "\n").encode())
        return read_lines(fd, timeout)
    finally:
        os.close(fd)


def print_filtered(lines):
    for line in lines:
        if line.startswith(("SDOK:", "SDERR:", "SDENTRY:", "SDDATA:", "SDEND:")):
            print(line)


def main():
    parser = argparse.ArgumentParser(description="Crossmux serial SD command helper")
    parser.add_argument("--port", default="/dev/cu.usbmodem1201")
    parser.add_argument("--timeout", type=float, default=3.0)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("ls")
    p.add_argument("path", nargs="?", default="/")

    p = sub.add_parser("stat")
    p.add_argument("path")

    p = sub.add_parser("read")
    p.add_argument("path")
    p.add_argument("--offset", type=int, default=0)
    p.add_argument("--max", type=int, default=4096)
    p.add_argument("--output")

    p = sub.add_parser("write")
    p.add_argument("path")
    p.add_argument("input")
    p.add_argument("--append", action="store_true")

    p = sub.add_parser("mkdir")
    p.add_argument("path")

    p = sub.add_parser("delete")
    p.add_argument("path")

    args = parser.parse_args()

    if args.cmd == "ls":
        command = f"CMD:SDLS {args.path}"
    elif args.cmd == "stat":
        command = f"CMD:SDSTAT {args.path}"
    elif args.cmd == "read":
        command = f"CMD:SDREAD {args.path} {args.offset} {args.max}"
    elif args.cmd == "write":
        with open(args.input, "rb") as f:
            hex_data = binascii.hexlify(f.read()).decode("ascii").upper()
        command = f"CMD:{'SDAPPEND' if args.append else 'SDWRITE'} {args.path} {hex_data}"
    elif args.cmd == "mkdir":
        command = f"CMD:SDMKDIR {args.path}"
    elif args.cmd == "delete":
        command = f"CMD:SDDEL {args.path}"
    else:
        raise AssertionError(args.cmd)

    lines = run_command(args.port, command, args.timeout)
    if args.cmd == "read":
        payload = "".join(line[len("SDDATA:"):] for line in lines if line.startswith("SDDATA:"))
        data = binascii.unhexlify(payload) if payload else b""
        if args.output:
            with open(args.output, "wb") as f:
                f.write(data)
        else:
            sys.stdout.buffer.write(data)
            if data and not data.endswith(b"\n"):
                sys.stdout.buffer.write(b"\n")
        print_filtered([line for line in lines if not line.startswith("SDDATA:")])
    else:
        print_filtered(lines)


if __name__ == "__main__":
    main()
