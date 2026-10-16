#!/usr/bin/env python3
"""LaunchPad issue #37 soak logger: read-only, host-timestamped serial capture.

Non-destructive by design:
  * never writes a single byte to the device;
  * opens the port with DTR and RTS deasserted, so the USB-Serial-JTAG
    console is not reset by the act of opening (unlike default idf.py monitor);
  * reconnects automatically if the port disappears (USB re-enumeration
    after a device reset) and records the gap.

Every device line is written as:
    <host ISO-8601 UTC time> <seconds since logger start> | <raw line>
Logger events start with "#LOGGER" and operator notes with "#NOTE".

Type a short note and press Enter at any time to timestamp a protocol step
(for example "B3 keyboard cycle 50"). Notes go to the log file only; they
are NEVER sent to the device. Do not type passwords, SSIDs or other private
text as notes.

Usage (Windows example, from the repository root):
    py -m pip install pyserial
    py tools/soak_logger.py --port COM25 --out <private local folder>

Stop with Ctrl+C. Keep the raw log private; publish only the output of
soak_summarize.py --redact.
"""
import argparse
import datetime as dt
import os
import queue
import re
import sys
import threading
import time

try:
    import serial  # pyserial
except ImportError:  # pragma: no cover
    sys.exit("pyserial is required: python -m pip install pyserial")

ALERT = re.compile(
    r"task_wdt|Task watchdog|did not reset the watchdog|Guru Meditation|abort\(\)"
    r"|Backtrace|Stack smashing|stack overflow|assert failed|rst:0x|ESP-ROM:"
    r"|LCD flush failed|Touch I2C error|keyboard bounds invalid",
    re.IGNORECASE,
)


def utc_now():
    return dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds")


class Log:
    def __init__(self, path):
        self._f = open(path, "a", encoding="utf-8", errors="replace", buffering=1)
        self._t0 = time.monotonic()
        self._lock = threading.Lock()

    def write(self, text):
        stamp = f"{utc_now()} {time.monotonic() - self._t0:11.3f} | "
        with self._lock:
            self._f.write(stamp + text + "\n")
            self._f.flush()
            os.fsync(self._f.fileno())  # survive a host crash or power loss

    def close(self):
        with self._lock:
            self._f.close()


def open_port(name, baud):
    port = serial.Serial()
    port.port = name
    port.baudrate = baud
    port.timeout = 0.5
    port.dtr = False  # set BEFORE open: avoids the USB-JTAG reset sequence
    port.rts = False
    port.open()
    return port


def note_reader(notes):
    for line in sys.stdin:
        line = line.strip()
        if line:
            notes.put(line)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", required=True, help="e.g. COM25 or /dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", required=True, help="private local output directory")
    ap.add_argument("--quiet", action="store_true", help="echo only alert lines")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    name = dt.datetime.now().strftime("issue37-soak-%Y%m%d-%H%M%S.log")
    path = os.path.join(args.out, name)
    log = Log(path)
    log.write(f"#LOGGER start port={args.port} baud={args.baud} read-only=1 dtr=0 rts=0")
    print(f"Logging to {path}\nType a note + Enter to timestamp a step. Ctrl+C to stop.")

    notes = queue.Queue()
    threading.Thread(target=note_reader, args=(notes,), daemon=True).start()

    port = None
    pending = b""
    lines_seen = 0
    last_rx = time.monotonic()
    silence_reported = False
    try:
        while True:
            while not notes.empty():
                log.write("#NOTE " + notes.get())
            if port is None:
                try:
                    port = open_port(args.port, args.baud)
                    log.write("#LOGGER port-open")
                    last_rx = time.monotonic()
                except (serial.SerialException, OSError) as exc:
                    log.write(f"#LOGGER port-unavailable {type(exc).__name__}")
                    time.sleep(2.0)
                    continue
            try:
                chunk = port.read(4096)
            except (serial.SerialException, OSError) as exc:
                log.write(f"#LOGGER port-lost {type(exc).__name__}")
                try:
                    port.close()
                except Exception:
                    pass
                port = None
                if pending:
                    log.write(pending.decode("utf-8", "replace"))
                    pending = b""
                continue
            now = time.monotonic()
            if chunk:
                last_rx = now
                silence_reported = False
                pending += chunk
                *complete, pending = re.split(rb"\r?\n", pending)
                for raw in complete:
                    text = raw.decode("utf-8", "replace").rstrip("\r")
                    log.write(text)
                    lines_seen += 1
                    if not args.quiet or ALERT.search(text):
                        prefix = "!! " if ALERT.search(text) else ""
                        print(prefix + text)
            elif now - last_rx > 300 and not silence_reported:
                # Firmware is quiet when idle; this is informational only.
                log.write("#LOGGER silence>300s")
                silence_reported = True
    except KeyboardInterrupt:
        pass
    finally:
        if pending:
            log.write(pending.decode("utf-8", "replace"))
        log.write(f"#LOGGER stop lines={lines_seen}")
        log.close()
        if port is not None:
            port.close()
        print(f"\nStopped. Raw private log: {path}")


if __name__ == "__main__":
    main()
