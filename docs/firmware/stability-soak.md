# Stability soak and watchdog capture (issue #37)

Non-destructive, serial-monitored soak of an already-flashed P4 image.
Nothing here flashes, erases NVS, or sends data to the board.

## Tools

- `tools/soak_logger.py` — read-only capture. Opens the USB-Serial-JTAG port
  with DTR/RTS deasserted (default `idf.py monitor` resets the board on open),
  timestamps each line (UTC + elapsed), fsyncs, reconnects after USB drops.
  Typed notes are written to the log only, never to the device.
- `tools/soak_summarize.py` — duration, firmware identity, resets, verbatim
  task-watchdog/panic blocks, Wi-Fi/SNTP counts, keyboard geometry, I/O errors.
  `--redact` masks IPv4/MAC/SSID text and note text; backtrace code addresses
  are kept.

```
py -m pip install pyserial
py tools/soak_logger.py --port COM25 --out <private local folder>
py tools/soak_summarize.py <raw.log> --redact > summary-redacted.txt
```

Keep raw logs private. Never type passwords, SSIDs or IPs as notes. During a
soak, do not tap **Save & connect** or **Forget saved**: both write NVS.

### Window-scoped analysis (issue #41)

Summarize an actual acceptance interval rather than including keyboard exercises
and network setup from before the soak. All window timestamps must include
`Z` or an explicit timezone offset; this example uses UTC:

```
py tools/soak_summarize.py <raw.log> --redact \
  --from 2026-10-17T00:10:39Z --to 2026-10-17T12:10:39Z
```

The summary separately displays the complete **instrumented log span** and
the selected **window**. It warns when the requested interval begins before
the first log entry or finishes after the last captured line. Such windows
are **incomplete evidence**, even when operator observations bridge the gap.

The summarizer detects an ESP-IDF reboot from a captured `rst:` banner and
from substantial backwards movement of its `I (uptime_ms)`-style log
timestamps; it reports whether a preceding USB loss was seen. Opening a
read-only serial port is **not** evidence that the monitor caused a reset.
Wi-Fi outages are grouped from the first disconnect through renewed DHCP,
with duplicate disconnect messages suppressed and retries counted. Reset and
Wi-Fi state are tracked across the **whole** log and then filtered to the
window, so connectivity established before `--from` cannot hide an outage
inside it. An outage overlapping the window start is marked
`began before window`; a disconnect before any logged connectivity is marked
`state before first disconnect unknown`.

`DIAG` trends cover the latest boot only when samples span a reset. The
summary prints the latest sample's age and warns `DIAG STALE` when it is
more than 65 minutes before the window end, or when two samples within one
boot are more than 65 minutes apart. Either can mean a stalled LVGL loop, an
uncaptured reset, or telemetry disabled at build time.

The logger retries reconnecting every 100 ms for the first 10 seconds of an
unavailable port, then once per second. It reports the first failed open and
at most one progress event per minute rather than flooding the log, then
reports outage duration on reconnect. Interrupted receive fragments are
logged as separate `#PARTIAL` records, never joined across port sessions.
USB re-enumeration can still prevent recovery of the earliest ROM/ELF
banner regardless of reconnect interval. Its success rate is **untested on
the actual Windows board** and needs controlled operator testing.


## What this image can and cannot show

- Task watchdog output (starved task, running task per CPU, backtrace) is
  printed by ESP-IDF 6.0.3 with panic disabled — capture the full log.
- Heap and network-worker stack trends are **not logged** by the current
  firmware; a soak PASS is "no watchdog/reset/freeze for N hours", not a
  memory-stability result.
- CI artifacts include `launchpad.elf` / `launchpad.map` so a backtrace can be
  decoded against the exact image (`riscv32-esp-elf-addr2line -pfiaC -e
  launchpad.elf <MEPC> <RA> ...`). Images built before this change have no ELF.

## LVGL flush wait

`lv_refr.c`'s `while(disp->flushing);` does not spin in this build:
ESP-IDF 6.0.3 `rgb_panel_draw_bitmap` copies the draw buffer by CPU and calls
`on_color_trans_done` synchronously, so `lv_display_flush_ready` runs before
`lvgl_flush` returns. Revisit this if the display moves to DMA2D copies,
direct mode or double buffering, where completion becomes asynchronous.
