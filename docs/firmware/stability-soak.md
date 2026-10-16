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
