#!/usr/bin/env python3
"""Copy a local file to Crossmux SD card over the patched serial SD protocol.

Default:
  crossmux_copy.py ~/Documents/0709.txt /0709.txt

This script intentionally writes in small chunks because long single-line
serial commands can be truncated on the ESP32-C3 CDC serial path.
"""

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def default_port():
    candidates = sorted(Path('/dev').glob('cu.usbmodem*'))
    return str(candidates[0]) if candidates else None


def run_serial_sd(script_dir: Path, port: str, args, timeout=20):
    cmd = [str(script_dir / 'serial_sd.py'), '--port', port, '--timeout', '5'] + list(args)
    proc = subprocess.run(cmd, cwd=str(script_dir.parent), text=True, capture_output=True, timeout=timeout)
    if proc.stdout:
        print(proc.stdout.strip())
    if proc.returncode != 0:
        if proc.stderr:
            print(proc.stderr.strip(), file=sys.stderr)
        raise SystemExit(proc.returncode)
    return proc.stdout


def write_chunk(script_dir: Path, port: str, dst: str, chunk: bytes, append: bool):
    fd, tmp = tempfile.mkstemp(prefix='crossmux_copy_', suffix='.bin')
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(chunk)
        args = ['write', dst, tmp]
        if append:
            args.append('--append')
        run_serial_sd(script_dir, port, args)
    finally:
        try:
            os.unlink(tmp)
        except FileNotFoundError:
            pass


def main():
    parser = argparse.ArgumentParser(description='Copy a local file to Crossmux SD card over USB serial')
    parser.add_argument('src', nargs='?', default='/Users/mmm2/Documents/0709.txt', help='local source file')
    parser.add_argument('dst', nargs='?', default='/0709.txt', help='Crossmux SD destination path, e.g. /0709.txt or /med/a.txt')
    parser.add_argument('--port', default=None, help='serial port, default: first /dev/cu.usbmodem*')
    parser.add_argument('--chunk-size', type=int, default=48, help='bytes per serial write command')
    parser.add_argument('--no-verify', action='store_true', help='skip read-back verification')
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    src = Path(args.src).expanduser().resolve()
    if not src.is_file():
        print(f'ERROR: source file not found: {src}', file=sys.stderr)
        raise SystemExit(1)
    if not args.dst.startswith('/'):
        print('ERROR: destination must be an absolute SD path starting with /', file=sys.stderr)
        raise SystemExit(1)
    if args.chunk_size <= 0 or args.chunk_size > 512:
        print('ERROR: --chunk-size must be 1..512', file=sys.stderr)
        raise SystemExit(1)

    port = args.port or default_port()
    if not port:
        print('ERROR: no Crossmux serial port found (/dev/cu.usbmodem*)', file=sys.stderr)
        raise SystemExit(1)

    data = src.read_bytes()
    print(f'PORT={port}')
    print(f'SRC={src} ({len(data)} bytes)')
    print(f'DST={args.dst}')

    if len(data) == 0:
        write_chunk(script_dir, port, args.dst, b'', append=False)
    else:
        for offset in range(0, len(data), args.chunk_size):
            write_chunk(script_dir, port, args.dst, data[offset:offset + args.chunk_size], append=(offset != 0))

    run_serial_sd(script_dir, port, ['stat', args.dst])

    if args.no_verify:
        print('VERIFY=skipped')
        return

    fd, verify_path = tempfile.mkstemp(prefix='crossmux_verify_', suffix='.bin')
    os.close(fd)
    try:
        run_serial_sd(script_dir, port, ['read', args.dst, '--output', verify_path], timeout=30)
        back = Path(verify_path).read_bytes()
        print(f'DST_READ={len(back)} bytes')
        if back != data:
            print('MATCH=False', file=sys.stderr)
            raise SystemExit(2)
        print('MATCH=True')
    finally:
        try:
            os.unlink(verify_path)
        except FileNotFoundError:
            pass


if __name__ == '__main__':
    main()
