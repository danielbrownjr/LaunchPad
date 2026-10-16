#!/usr/bin/env python3
"""Summarize a LaunchPad issue #37 soak log produced by soak_logger.py.

    py tools/soak_summarize.py <raw.log>            # private summary to stdout
    py tools/soak_summarize.py <raw.log> --redact   # shareable summary + excerpts

The summary reports duration, firmware identity, resets, watchdog / panic
blocks (with the following lines kept verbatim so backtraces survive),
Wi-Fi/SNTP transitions, keyboard opens, stack lines, I/O errors and the
longest gaps between SNTP confirmations (an hourly liveness heartbeat of
the network worker while connected).

--redact masks IPv4 addresses, MAC/BSSID-like values, anything following
"ssid"/"connected with", and drops #NOTE text. Addresses inside backtraces
(0x........) are code addresses and are kept: they are needed to decode
the trace and contain no personal data. Always read the redacted output
before publishing it.
"""
import argparse
import datetime as dt
import re
import sys

LINE = re.compile(r"^(\S+) +([\d.]+) \| ?(.*)$")
ANSI = re.compile(r"\x1b\[[0-9;]*m")

P = {
    "rom_reset": re.compile(r"rst:(0x[0-9a-fA-F]+)\s*\(([^)]*)\)"),
    "elf_sha": re.compile(r"ELF file SHA256:\s*([0-9a-fA-F]+)"),
    "app_ver": re.compile(r"App version:\s*(\S+)"),
    "compile": re.compile(r"Compile time:\s*(.+)$"),
    "idf_ver": re.compile(r"ESP-IDF:\s*(\S+)"),
    "dashboard": re.compile(r"Dashboard running"),
    "main_stack": re.compile(r"main stack minimum free after dashboard: (\d+) bytes"),
    "keyboard": re.compile(r"keyboard viewport \((-?\d+),(-?\d+)\)-\((-?\d+),(-?\d+)\), (\d+)x(\d+)"),
    "kb_invalid": re.compile(r"keyboard bounds invalid"),
    "dhcp": re.compile(r"DHCP IP acquired"),
    "retry": re.compile(r"Wi-Fi unavailable, retrying in (\d+) seconds"),
    "sntp": re.compile(r"SNTP UTC synchronization confirmed"),
    "no_saved": re.compile(r"No saved Wi-Fi"),
    "scan_fail": re.compile(r"Wi-Fi scan failed"),
    "nvs_fail": re.compile(r"Failed (saving|forgetting) Wi-Fi"),
    "net_fail": re.compile(r"lp_net\w*: .* failed \("),
    "touch_err": re.compile(r"Touch I2C error"),
    "lcd_err": re.compile(r"LCD flush failed"),
    "wdt": re.compile(r"Task watchdog got triggered"),
    "panic": re.compile(r"Guru Meditation|abort\(\) was called|Stack smashing|stack overflow|assert failed"),
}
BLOCK_START = re.compile(r"Task watchdog got triggered|Guru Meditation|abort\(\) was called|Stack smashing|assert failed")

IPV4 = re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b")
MAC = re.compile(r"\b[0-9a-fA-F]{2}(?::[0-9a-fA-F]{2}){5}\b")
SSID = re.compile(r"((?:ssid|SSID|connected with)\s*[:=]?\s*)(\S.*?)(,|$)")


def redact(text):
    text = MAC.sub("xx:xx:xx:xx:xx:xx", text)
    text = IPV4.sub(lambda m: m.group(0) if m.group(0) in ("0.0.0.0", "255.255.255.0") else "x.x.x.x", text)
    return SSID.sub(lambda m: m.group(1) + "<redacted>" + m.group(3), text)


def hms(seconds):
    seconds = int(seconds)
    return f"{seconds // 3600}h{seconds % 3600 // 60:02d}m{seconds % 60:02d}s"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--redact", action="store_true")
    ap.add_argument("--context", type=int, default=80, help="lines kept after a WDT/panic start")
    args = ap.parse_args()
    out = redact if args.redact else (lambda s: s)

    rows = []
    with open(args.log, encoding="utf-8", errors="replace") as f:
        for raw in f:
            m = LINE.match(raw.rstrip("\n"))
            if m:
                rows.append((m.group(1), float(m.group(2)), ANSI.sub("", m.group(3))))
    if not rows:
        sys.exit("no logger lines found")

    counts = {k: 0 for k in P}
    resets, stacks, keyboards, firmware, notes, port_events = [], [], [], {}, [], []
    sntp_times, blocks = [], []
    bad_kb = 0
    open_times = []
    i = 0
    while i < len(rows):
        stamp, t, text = rows[i]
        if text.startswith("#LOGGER"):
            port_events.append((stamp, text))
            if "port-open" in text:
                open_times.append(t)
        elif text.startswith("#NOTE"):
            notes.append((stamp, text[6:]))
        for key, rx in P.items():
            m = rx.search(text)
            if not m:
                continue
            counts[key] += 1
            if key == "rom_reset":
                induced = any(0 <= t - o <= 3.0 for o in open_times)
                resets.append((stamp, m.group(1), m.group(2), induced))
            elif key in ("elf_sha", "app_ver", "compile", "idf_ver"):
                firmware.setdefault(key, set()).add(m.group(1).strip())
            elif key == "main_stack":
                stacks.append((stamp, int(m.group(1))))
            elif key == "keyboard":
                x1, y1, x2, y2, w, h = map(int, m.groups())
                keyboards.append((x1, y1, x2, y2, w, h))
                if (x1, y1, x2, y2, w, h) != (38, 239, 761, 443, 724, 205):
                    bad_kb += 1
            elif key == "sntp":
                sntp_times.append(t)
        if BLOCK_START.search(text):
            blocks.append([(s, x) for s, _, x in rows[i:i + args.context]])
        i += 1

    t_first, t_last = rows[0][1], rows[-1][1]
    print("# LaunchPad issue #37 soak summary" + (" (REDACTED)" if args.redact else " (PRIVATE)"))
    print(f"log span: {rows[0][0]} -> {rows[-1][0]}  ({hms(t_last - t_first)})")
    for key, label in (("elf_sha", "ELF SHA256"), ("app_ver", "app version"),
                       ("compile", "compile time"), ("idf_ver", "ESP-IDF")):
        vals = sorted(firmware.get(key, []))
        print(f"{label}: {', '.join(vals) if vals else 'not seen (start logger before boot)'}"
              + ("   <-- MORE THAN ONE IMAGE IN LOG" if len(vals) > 1 else ""))
    print(f"\nboots (Dashboard running): {counts['dashboard']}")
    for stamp, code, why, induced in resets:
        tag = " [within 3 s of port open: probably monitor-induced]" if induced else ""
        print(f"  reset {stamp} rst:{code} ({why}){tag}")
    print(f"task watchdog events: {counts['wdt']}   panic/abort events: {counts['panic']}")
    print(f"main stack min free at boot (bytes): {[v for _, v in stacks] or 'n/a'}")
    print(f"keyboard opens: {len(keyboards)}   off-spec geometry: {bad_kb}   "
          f"'bounds invalid': {counts['kb_invalid']}")
    print(f"DHCP acquired: {counts['dhcp']}   retry/backoff: {counts['retry']}   "
          f"SNTP confirmations: {counts['sntp']}   unconfigured: {counts['no_saved']}")
    if len(sntp_times) > 1:
        gaps = sorted((b - a for a, b in zip(sntp_times, sntp_times[1:])), reverse=True)
        print(f"longest SNTP gaps: {', '.join(hms(g) for g in gaps[:3])} "
              "(expect ~1h while connected; longer = disconnected or worker stalled)")
    print(f"scan failures: {counts['scan_fail']}   NVS save/forget failures: {counts['nvs_fail']}   "
          f"network step failures: {counts['net_fail']}")
    print(f"touch I2C errors: {counts['touch_err']}   LCD flush failures: {counts['lcd_err']}")
    lost = [e for e in port_events if "port-lost" in e[1]]
    print(f"logger port losses: {len(lost)} (a reset or USB drop; check resets above)")
    if notes and not args.redact:
        print("\noperator notes:")
        for stamp, n in notes:
            print(f"  {stamp} {n}")
    elif notes:
        print(f"\noperator notes: {len(notes)} (text omitted in redacted output)")
    for n, block in enumerate(blocks, 1):
        print(f"\n## WDT/panic block {n} (verbatim{' , redacted' if args.redact else ''})")
        for stamp, text in block:
            if text.startswith("#NOTE") and args.redact:
                continue
            print(f"{stamp} | {out(text)}")
    verdict = "NO WATCHDOG/PANIC SEEN" if not (counts["wdt"] or counts["panic"]) else "WATCHDOG/PANIC PRESENT"
    print(f"\nresult: {verdict}; span {hms(t_last - t_first)}")


if __name__ == "__main__":
    main()
