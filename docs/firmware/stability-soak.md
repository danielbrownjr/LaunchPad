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

## What this image can and cannot show

- Task watchdog output (starved task, running task per CPU, backtrace) is
  printed by ESP-IDF 6.0.3 with panic disabled — capture the full log.
- The previously tested PR #36 build (`cff7345`) logged only a one-time
  main-task stack watermark. Its 12-hour PASS cannot establish memory stability.
- Firmware built with `LP_SOAK_TELEMETRY_ENABLED=1` (default in this branch)
  adds a **boot-time and hourly** `DIAG` record. It reports monotonic
  `uptime_s`, internal 8-bit heap `internal_free_b` and
  `internal_min_b`, PSRAM `psram_free_b` and `psram_min_b`, the
  main/LVGL-owning task `main_stack_hwm_b`, and a self-sampled network
  worker `network_stack_hwm_b`. `network_sample_age_s` states how old the
  worker's last measurement is, and `network_valid=0` means a reported
  worker watermark of zero is **unknown**, not a stack exhaustion.
  No separate LVGL task exists, so do not report one.
- To disable this low-rate diagnostic stream in a separate firmware build,
  define `LP_SOAK_TELEMETRY_ENABLED=0` at compilation. The `DIAG` and Wi-Fi
  event lines contain no passwords, SSIDs or IP addresses, but the full log
  is **not** free of network identifiers: ESP-IDF's `esp_netif_handlers` prints
  `sta ip:`, `mask:` and `gw:` on every DHCP lease. Keep raw logs private and
  publish only `--redact` output after reading it. The diagnostic recorder
  creates no separate FreeRTOS task or heap buffers.
- A single snapshot or a short session cannot establish long-term memory
  stability. Time-separated samples, stack margin acceptance criteria, and
  a controlled target-hardware run are still required.
- CI artifacts include `launchpad.elf` / `launchpad.map` so a backtrace can be
  decoded against the exact image (`riscv32-esp-elf-addr2line -pfiaC -e
  launchpad.elf <MEPC> <RA> ...`). Images built before this change have no ELF.

## Credential persistence diagnostic events

The Wi-Fi command path now emits fixed, non-secret INFO strings when a save
or forget command is **queued**, and when its NVS operation **commits**:
`Wi-Fi save request queued`, `Wi-Fi save committed to P4 NVS`,
`Wi-Fi forget request queued`, and `Wi-Fi forget persisted in P4 NVS`.

A committed credential record does **not** establish successful Wi-Fi
authentication or Internet reachability. Correlate later association,
DHCP and SNTP lines to distinguish these outcomes. Nothing here changes
NVS data layout, retry policy, or whether credentials are encrypted.
Unencrypted NVS remains a prototype limitation.

## LVGL flush wait

`lv_refr.c`'s `while(disp->flushing);` does not spin in this build:
ESP-IDF 6.0.3 `rgb_panel_draw_bitmap` copies the draw buffer by CPU and calls
`on_color_trans_done` synchronously, so `lv_display_flush_ready` runs before
`lvgl_flush` returns. Revisit this if the display moves to DMA2D copies,
direct mode or double buffering, where completion becomes asynchronous.
