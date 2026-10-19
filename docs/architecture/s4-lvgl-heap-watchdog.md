# S4 hardware watchdog remediation: LVGL layer-buffer allocation

**Status:** proposed software fix; **hardware unverified**. Applies on top
of S4 [PR #48](https://github.com/danielbrownjr/LaunchPad/pull/48);
tracks [issue #51](https://github.com/danielbrownjr/LaunchPad/issues/51).
No board interaction, flash or merge is authorized by this change.

## Evidence, 2026-10-19

The flashed S4 PR #48 image (`3fa81a3`, embedded version `ea587e9`,
ESP-IDF 6.0.3) boots normally, starts C5 networking, acquires DHCP and
synchronizes time. The operator saw functional home/agenda/Settings screens.
After a Wi-Fi scan at 09:41 UTC, the display froze and CPU0's `main`
task repeatedly starved `IDLE0`. The read-only capture shows repeated
task-WDT records at five-second intervals and secondary RGB LCD underrun
records, but **no reset**. Raw logs and full flash backups are private.

The operator mapped captured PCs to LVGL 9.4.0 layer draw-buffer allocation,
the built-in TLSF allocator and memory operations. The exact Settings
render path was also reproduced on a private native LVGL 9.4.0 host:
64 KiB builtin allocator deadlocks with one or more scanned APs, whereas
128 KiB completes the 2, 6 and 12 AP cases. Those observations identify
the **pool allocation failure** as the principal failure mode, not a C5
firmware or GPIO problem. They do not prove the next deployed image is
immune to all heap failures.

The original hardware boot diagnostic indicated over 280 KiB of minimum
remaining internal 8-bit heap, but that is **not the same quantity** as the
remaining space in LVGL's separate, fixed 64 KiB TLSF allocator. S4's
persistent object tree plus scan-list and layer allocations exhaust that
local pool despite ample aggregate ESP-IDF heap.

## Candidate changes

1. Set `CONFIG_LV_USE_BUILTIN_MALLOC=y`,
   `CONFIG_LV_MEM_SIZE_KILOBYTES=128` and pool expansion 0 in the
   existing ESP-IDF 6.0.3 / LVGL 9.4.0 config. The exact pinned LVGL tag's
   `Kconfig` maps `CONFIG_LV_MEM_SIZE_KILOBYTES` to
   `LV_MEM_SIZE`; its TLSF `work_mem_int` is a static
   `LV_ATTRIBUTE_LARGE_RAM_ARRAY`. The extra 64 KiB is **RAM**, not
   additional flash partition space.
2. Fail compilation if the effective LVGL header still exposes under
   128 KiB, or if the resolved target Kconfig selected the wrong allocator
   or pool size. Do not trust defaults text alone.
3. Add `lv_mem_monitor()` to the existing low-rate `DIAG` on the sole
   LVGL-owning main task. Record pool total/free/largest available block
   and fragmentation; the other ESP heap/PSRAM and stack metrics stay.
4. Emit a separate `LVGL_AP_LIST` pool snapshot immediately after
   constructing each scan-result UI, before the next draw. Includes only
   count and allocation sizes, **never SSID or credentials**.
5. Lock configuration invariants in host tests and resolved P4 CI. These
   source tests are not claimed to be a replacement for the private
   native LVGL 9.4.0 renderer repro or the real hardware acceptance.

No C5/SDIO behavior, NVS, partitions, bootloader, task affinity, LVGL
global ownership or watchdog settings are modified. Do **not** switch
to ESP-IDF's general heap allocator or PSRAM without a separate
latency/fragmentation/critical-path analysis.

## Limitations and required next acceptance

A 128 KiB pool passes the captured native replay; it is a **bounded fix to
an observed requirement**, not an OOM-proof graphics architecture. LVGL
9.4.0 may attempt another draw layer allocation and retry when insufficient
memory. Future dynamic pages, translated glyphs, images and unexpectedly
large AP lists still require memory budgets or graceful failure design.

- [ ] Confirm exact-head Python/static/native/ThreadSanitizer and pinned
      ESP-IDF 6.0.3 P4 CI, with image fitting the existing 1 MiB factory
      partition.
- [ ] Inspect resolved config header for 128 KiB TLSF allocation and verify
      that static internal-RAM cost does not starve display DMA, Wi-Fi or
      other memory consumers.
- [ ] Obtain Dan's **separate explicit consent** to replace S4 application
      only at `0x10000` with the reviewed fix. The current board has
      deliberately been left in the recorded watchdog state.
- [ ] On real P4, check scan -> Settings -> Diagnostics -> Close ->
      repeat at low/medium/high AP counts, with a short read-only serial
      capture. Confirm no IDLE0 watchdog/RGB underrun; inspect
      `LVGL_AP_LIST` and `DIAG` headroom and render responsiveness.
- [ ] If any allocation failure persists, do not just raise heap again.
      Reproduce the failing layer, bound transient draws or add an explicit
      recoverable failure mode, and independently review it.
- [ ] Only after successful physical acceptance should the fix be folded
      into S4 release review. Keep S4 PR #48 **unmerged** until then.

Do not publish private raw boot logs or local backup paths.
