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
UPTIME = re.compile(r"^[IWEVD] \\((\\d+)\\)")
WIFI_DOWN = re.compile(r"Station mode: Disconnected")
WIFI_UP = re.compile(r"Station mode: Connected|DHCP IP acquired")
WIFI_IP = re.compile(r"DHCP IP acquired")

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


def parse_utc(value):
    when = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    if when.tzinfo is None:
        raise ValueError("timestamps require an explicit UTC offset or Z")
    return when.astimezone(dt.timezone.utc)


def analyze_resets(rows):
    """Combine ROM banners and uptime regression; never infer operator intent."""
    resets = []
    uptime = None
    last_port_loss = None
    for stamp, elapsed, line in rows:
        if line.startswith("#LOGGER port-lost"):
            last_port_loss = elapsed
        rom = P["rom_reset"].search(line)
        if rom:
            resets.append({"stamp": stamp, "t": elapsed, "banner": True,
                           "code": rom.group(1), "why": rom.group(2),
                           "port_lost": last_port_loss is not None and 0 <= elapsed - last_port_loss <= 15,
                           "uptime": False})
        match = UPTIME.match(line)
        if match:
            now_ms = int(match.group(1))
            if uptime is not None and uptime - now_ms > 1000:
                recent = (resets and resets[-1]["banner"]
                          and not resets[-1]["uptime"]
                          and 0 <= elapsed - resets[-1]["t"] <= 15)
                if recent:
                    resets[-1]["uptime"] = True
                else:
                    resets.append({"stamp": stamp, "t": elapsed, "banner": False,
                                   "code": None, "why": None,
                                   "port_lost": last_port_loss is not None and 0 <= elapsed - last_port_loss <= 15,
                                   "uptime": True})
            uptime = now_ms
    return resets


def analyze_wifi(rows):
    """Count connectivity outages, not repeated disconnect notifications."""
    online = False
    outage = None
    events = []
    for stamp, elapsed, line in rows:
        if WIFI_DOWN.search(line):
            if online and outage is None:
                outage = {"start": stamp, "start_t": elapsed, "retries": 0}
                online = False
        elif WIFI_IP.search(line):
            if outage is not None:
                outage["end"] = stamp
                outage["duration_s"] = elapsed - outage["start_t"]
                events.append(outage)
                outage = None
            online = True
        elif WIFI_UP.search(line):
            # Association alone is not proof of IP recovery.
            if outage is None:
                online = True
        if outage is not None and P["retry"].search(line):
            outage["retries"] += 1
    if outage is not None:
        outage["end"] = None
        outage["duration_s"] = None
        events.append(outage)
    return events


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--redact", action="store_true")
    ap.add_argument("--context", type=int, default=80, help="lines kept after a WDT/panic start")
    ap.add_argument("--from", dest="from_utc", type=parse_utc, help="UTC window start (ISO-8601 with Z or offset)")
    ap.add_argument("--to", dest="to_utc", type=parse_utc, help="UTC window end (ISO-8601 with Z or offset)")
    args = ap.parse_args()
    out = redact if args.redact else (lambda s: s)
    if args.from_utc and args.to_utc and args.from_utc >= args.to_utc:
        ap.error("--from must precede --to")

    rows = []
    with open(args.log, encoding="utf-8", errors="replace") as f:
        for raw in f:
            m = LINE.match(raw.rstrip("\n"))
            if m:
                rows.append((m.group(1), float(m.group(2)), ANSI.sub("", m.group(3))))
    if not rows:
        sys.exit("no logger lines found")

    full_rows = rows
    start_time = parse_utc(rows[0][0])
    end_time = parse_utc(rows[-1][0])
    selected_start = args.from_utc or start_time
    selected_end = args.to_utc or end_time
    rows = [r for r in full_rows if selected_start <= parse_utc(r[0]) <= selected_end]
    if not rows:
        sys.exit("no logger lines within the requested window")

    reset_events = analyze_resets(rows)
    wifi_events = analyze_wifi(rows)
    counts = {k: 0 for k in P}
    resets, stacks, keyboards, firmware, notes, port_events = [], [], [], {}, [], []
    sntp_times, blocks = [], []
    bad_kb = 0
    i = 0
    while i < len(rows):
        stamp, t, text = rows[i]
        if text.startswith("#LOGGER"):
            port_events.append((stamp, text))
        elif text.startswith("#NOTE"):
            notes.append((stamp, text[6:]))
        for key, rx in P.items():
            m = rx.search(text)
            if not m:
                continue
            counts[key] += 1
            if key in ("elf_sha", "app_ver", "compile", "idf_ver"):
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
    print(f"log span (instrumented): {full_rows[0][0]} -> {full_rows[-1][0]}"
          f"  ({hms(full_rows[-1][1] - full_rows[0][1])})")
    if args.from_utc or args.to_utc:
        print(f"requested window: {selected_start.isoformat()} -> {selected_end.isoformat()}")
        print(f"window captured: {rows[0][0]} -> {rows[-1][0]}  ({hms(t_last - t_first)})")
        if selected_start < start_time or selected_end > end_time:
            print("WARNING: requested window exceeds available serial capture; coverage INCOMPLETE")
    for key, label in (("elf_sha", "ELF SHA256"), ("app_ver", "app version"),
                       ("compile", "compile time"), ("idf_ver", "ESP-IDF")):
        vals = sorted(firmware.get(key, []))
        print(f"{label}: {', '.join(vals) if vals else 'not seen (start logger before boot)'}"
              + ("   <-- MORE THAN ONE IMAGE IN LOG" if len(vals) > 1 else ""))
    print(f"\nboots (Dashboard running): {counts['dashboard']}")
    print(f"reset events (banner or uptime regression): {len(reset_events)}")
    for event in reset_events:
        reason = (f"rst:{event['code']} ({event['why']})" if event["banner"]
                  else "reason unavailable")
        print(f"  reset {event['stamp']} {reason}; banner={'yes' if event['banner'] else 'no'}"
              f" uptime_regression={'yes' if event['uptime'] else 'no'}"
              f" preceding_port_loss={'yes' if event['port_lost'] else 'no'}")
    print(f"task watchdog events: {counts['wdt']}   panic/abort events: {counts['panic']}")
    print(f"main stack min free at boot (bytes): {[v for _, v in stacks] or 'n/a'}")
    print(f"keyboard opens: {len(keyboards)}   off-spec geometry: {bad_kb}   "
          f"'bounds invalid': {counts['kb_invalid']}")
    print(f"Wi-Fi outage events: {len(wifi_events)}")
    for event in wifi_events:
        status = (f"recovered {event['end']}, duration {hms(event['duration_s'])}"
                  if event["end"] else "NOT RECOVERED IN WINDOW")
        print(f"  Wi-Fi outage started {event['start']}; {status}; retries {event['retries']}")
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
