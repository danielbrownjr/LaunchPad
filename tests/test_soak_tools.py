"""Host tests for the issue #37 soak tools. No serial port or hardware needed."""
import contextlib
import datetime as dt
import importlib.util
import io
import re
import sys
import tempfile
import types
from unittest import mock
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOGGER = (ROOT / "tools/soak_logger.py").read_text(encoding="utf-8")


def load_summarizer():
    spec = importlib.util.spec_from_file_location("soak_summarize", ROOT / "tools/soak_summarize.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_logger():
    # The host CI tests do not install pyserial. Inject only the minimum API
    # required for testing pure reconnect/framing behavior.
    fake_serial = types.SimpleNamespace(Serial=lambda: None, SerialException=OSError)
    spec = importlib.util.spec_from_file_location("soak_logger", ROOT / "tools/soak_logger.py")
    module = importlib.util.module_from_spec(spec)
    with mock.patch.dict(sys.modules, {"serial": fake_serial}):
        spec.loader.exec_module(module)
    return module


SAMPLE = [
    "#LOGGER start port=COM25 baud=115200 read-only=1 dtr=0 rts=0",
    "#LOGGER port-open",
    "rst:0x17 (CHIP_USB_UART_RESET),boot:0x30f (SPI_FAST_FLASH_BOOT)",
    "I (300) app_init: ELF file SHA256:  1a2b3c4d5e6f7a8b",
    "I (900) launchpad: main stack minimum free after dashboard: 7472 bytes",
    "I (901) launchpad: Dashboard running; touch ready; clock unsynchronized",
    "I (3000) esp_netif_handlers: sta ip: 192.168.8.115, mask: 255.255.255.0, gw: 192.168.8.1",
    "I (3001) lp_net: DHCP IP acquired; starting SNTP",
    "I (4000) lp_net: SNTP UTC synchronization confirmed",
    "I (9000) lp_wifi_ui: keyboard viewport (38,239)-(761,443), 724x205",
    "I (9100) lp_wifi_ui: keyboard viewport (76,514)-(799,718), 724x205",
    "#NOTE B3 cycle 10",
    "E (20000) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:",
    "E (20000) task_wdt:  - IDLE0 (CPU 0)",
    "E (20000) task_wdt: CPU 0: main",
    "W (20001) wifi: connected with Example Net, aid = 1, bssid aa:bb:cc:dd:ee:ff, ip 10.0.0.7",
    "MEPC    : 0x4800abcd  RA      : 0x4800a0f0",
    "I (3604000) lp_net: SNTP UTC synchronization confirmed",
    "I (3605000) launchpad: DIAG uptime_s=3605 internal_free_b=32000 internal_min_b=31000 psram_free_b=1800000 psram_min_b=1700000 main_stack_hwm_b=7504 network_stack_hwm_b=5300 network_sample_age_s=5 network_valid=1",
    "I (7205000) launchpad: DIAG uptime_s=7205 internal_free_b=30000 internal_min_b=29000 psram_free_b=1780000 psram_min_b=1600000 main_stack_hwm_b=7460 network_stack_hwm_b=5200 network_sample_age_s=8 network_valid=1",
]


class SoakTools(unittest.TestCase):
    def run_summarizer(self, path, flags):
        module = load_summarizer()
        out = io.StringIO()
        argv = sys.argv
        sys.argv = ["soak_summarize.py", path, *flags]
        try:
            with contextlib.redirect_stdout(out):
                module.main()
        finally:
            sys.argv = argv
            Path(path).unlink()
        return out.getvalue()

    def summarize(self, *flags):
        with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False, encoding="utf-8") as f:
            for n, text in enumerate(SAMPLE):
                f.write(f"2026-10-16T12:00:{n:02d}.000+00:00 {n * 0.5:11.3f} | {text}\n")
            path = f.name
        return self.run_summarizer(path, flags)

    def test_summary_counts(self):
        text = self.summarize()
        self.assertIn("ELF SHA256: 1a2b3c4d5e6f7a8b", text)
        self.assertIn("task watchdog events: 1", text)
        self.assertIn("keyboard opens: 2   off-spec geometry: 1", text)
        self.assertIn("SNTP confirmations: 2", text)
        self.assertIn("main stack min free at boot (bytes): [7472]", text)
        self.assertIn("reset events (banner or uptime regression): 1", text)
        self.assertIn("banner=yes", text)
        self.assertNotIn("probably monitor-induced", text)
        self.assertIn("B3 cycle 10", text)
        self.assertIn("result: WATCHDOG/PANIC PRESENT", text)

    def test_redaction_keeps_backtrace(self):
        text = self.summarize("--redact")
        for private in ("192.168.8.115", "10.0.0.7", "Example Net", "aa:bb:cc:dd:ee:ff", "B3 cycle 10"):
            self.assertNotIn(private, text)
        self.assertIn("MEPC    : 0x4800abcd  RA      : 0x4800a0f0", text)
        self.assertIn("IDLE0 (CPU 0)", text)


    def test_memory_diagnostic_trends(self):
        text = self.summarize()
        self.assertIn("memory diagnostic samples: 2", text)
        self.assertIn("internal_free_b: first=32000 last=30000", text)
        self.assertIn("network_stack_hwm_b: first=5300 last=5200", text)
        self.assertIn("network worker sample age (s): max=8", text)
        self.assertIn("not proof of memory stability", text)

    def test_uptime_reset_without_boot_banner(self):
        module = load_summarizer()
        rows = [
            ("A", 0.0, "I (25800) lp_network: state up"),
            ("B", 2.0, "#LOGGER port-lost SerialException"),
            ("C", 4.0, "#LOGGER port-open"),
            ("D", 5.0, "I (2600) launchpad: Dashboard running"),
        ]
        events = module.analyze_resets(rows)
        self.assertEqual(len(events), 1)
        self.assertFalse(events[0]["banner"])
        self.assertTrue(events[0]["uptime"])
        self.assertTrue(events[0]["port_lost"])

    def test_uptime_reset_pairs_with_boot_banner(self):
        module = load_summarizer()
        rows = [
            ("A", 0.0, "I (25800) lp_network: up"),
            ("B", 2.0, "#LOGGER port-lost SerialException"),
            ("C", 4.0, "rst:0x17 (CHIP_USB_UART_RESET)"),
            ("D", 4.2, "I (150) boot: restarted"),
        ]
        events = module.analyze_resets(rows)
        self.assertEqual(len(events), 1)
        self.assertTrue(events[0]["banner"])
        self.assertTrue(events[0]["uptime"])
        self.assertEqual(events[0]["code"], "0x17")

    def test_monotonic_uptime_has_no_resets(self):
        module = load_summarizer()
        rows = [("A", 0.0, "I (10000) up"),
                ("B", 2.0, "#LOGGER port-lost SerialException"),
                ("C", 4.0, "I (11000) still up")]
        self.assertEqual(module.analyze_resets(rows), [])

    def test_wifi_duplicate_disconnects_one_outage(self):
        module = load_summarizer()
        rows = [
            ("A", 0.0, "I (1) lp_network: DHCP IP acquired; starting SNTP"),
            ("B", 10.0, "I (2) RPC_WRAP: ESP Event: Station mode: Disconnected"),
            ("C", 10.1, "I (3) RPC_WRAP: ESP Event: Station mode: Disconnected"),
            ("D", 11.0, "W (4) lp_network: Wi-Fi unavailable, retrying in 2 seconds"),
            ("E", 12.0, "I (5) RPC_WRAP: ESP Event: Station mode: Connected"),
            ("F", 22.0, "I (6) lp_network: DHCP IP acquired; starting SNTP"),
        ]
        events = module.analyze_wifi(rows)
        self.assertEqual(len(events), 1)
        self.assertEqual(events[0]["duration_s"], 12.0)
        self.assertEqual(events[0]["retries"], 1)

    def test_window_excludes_setup_activity(self):
        text = self.summarize("--from", "2026-10-16T12:00:12Z",
                              "--to", "2026-10-16T12:00:16Z")
        self.assertIn("requested window:", text)
        self.assertIn("keyboard opens: 0", text)
        self.assertIn("task watchdog events: 1", text)

    def test_window_beyond_log_is_incomplete(self):
        text = self.summarize("--from", "2026-10-16T12:00:01Z",
                              "--to", "2026-10-16T12:04:00Z")
        self.assertIn("coverage INCOMPLETE", text)

    def summarize_timed(self, lines, *flags):
        """lines: (seconds after 12:00:00Z, text); logger elapsed == seconds."""
        base = dt.datetime(2026, 10, 16, 12, tzinfo=dt.timezone.utc)
        with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False, encoding="utf-8") as f:
            for sec, text in lines:
                stamp = (base + dt.timedelta(seconds=sec)).isoformat(timespec="milliseconds")
                f.write(f"{stamp} {sec:11.3f} | {text}\n")
            path = f.name
        return self.run_summarizer(path, flags)

    def test_window_keeps_outage_after_pre_window_dhcp(self):
        # Regression: connectivity established before --from must not hide
        # a disconnect inside the window (seen on the real issue #37 log).
        lines = [(0, "I (1000) lp_network: DHCP IP acquired; starting SNTP"),
                 (100, "I (101000) RPC_WRAP: ESP Event: Station mode: Disconnected"),
                 (101, "W (101500) lp_network: Wi-Fi unavailable, retrying in 1 seconds"),
                 (160, "I (160000) lp_network: DHCP IP acquired; starting SNTP"),
                 (200, "I (200000) lp_network: idle")]
        text = self.summarize_timed(lines, "--from", "2026-10-16T12:00:50Z",
                                    "--to", "2026-10-16T12:03:20Z")
        self.assertIn("Wi-Fi outage events: 1", text)
        self.assertIn("duration 0h01m00s; retries 1\n", text)

    def test_outage_spanning_window_start_is_flagged(self):
        lines = [(0, "I (1000) lp_network: DHCP IP acquired; starting SNTP"),
                 (30, "I (31000) RPC_WRAP: ESP Event: Station mode: Disconnected"),
                 (100, "I (100000) lp_network: DHCP IP acquired; starting SNTP"),
                 (200, "I (200000) lp_network: idle")]
        text = self.summarize_timed(lines, "--from", "2026-10-16T12:00:50Z")
        self.assertIn("Wi-Fi outage events: 1", text)
        self.assertIn("[began before window]", text)

    def test_disconnect_with_unknown_prior_state(self):
        module = load_summarizer()
        events = module.analyze_wifi([
            ("A", 0.0, "I (1) RPC_WRAP: ESP Event: Station mode: Disconnected"),
            ("B", 5.0, "I (2) lp_network: DHCP IP acquired; starting SNTP")])
        self.assertEqual(len(events), 1)
        self.assertTrue(events[0]["prior_unknown"])

    def test_reset_at_window_start_detected(self):
        lines = [(0, "I (25800) lp_network: up"),
                 (60, "I (300) launchpad: Dashboard running"),
                 (90, "I (30300) lp_network: up")]
        text = self.summarize_timed(lines, "--from", "2026-10-16T12:00:50Z")
        self.assertIn("reset events (banner or uptime regression): 1", text)

    def test_diag_stale_and_multiboot_trends(self):
        diag = ("I ({ms}) launchpad: DIAG uptime_s={up} internal_free_b={free} internal_min_b=1"
                " psram_free_b=2 psram_min_b=2 main_stack_hwm_b=3"
                " network_stack_hwm_b=4 network_sample_age_s=5 network_valid=1")
        lines = [(0, diag.format(ms=0, up=0, free=900)),
                 (3600, diag.format(ms=3600000, up=3600, free=800)),
                 (4000, diag.format(ms=1000, up=1, free=500)),
                 (14000, "I (11000000) lp_network: idle")]
        text = self.summarize_timed(lines)
        self.assertIn("samples span 2 boots", text)
        self.assertIn("internal_free_b: first=500 last=500", text)
        self.assertIn("DIAG STALE", text)

    def test_diag_interval_gap_within_one_boot(self):
        diag = ("I ({ms}) launchpad: DIAG uptime_s={up} internal_free_b=1 internal_min_b=1"
                " psram_free_b=2 psram_min_b=2 main_stack_hwm_b=3"
                " network_stack_hwm_b=4 network_sample_age_s=5 network_valid=1")
        lines = [(0, diag.format(ms=0, up=0)), (9000, diag.format(ms=9000000, up=9000))]
        text = self.summarize_timed(lines)
        self.assertIn("WARNING: DIAG interval up to 2h30m00s", text)
        self.assertNotIn("DIAG STALE", text)

    def test_port_unavailable_rate_limited(self):
        logger = load_logger()
        reporter = logger.PortOutage()
        self.assertIn("port-unavailable", reporter.unavailable(0.0, "SerialException"))
        for t in range(1, 60):
            self.assertIsNone(reporter.unavailable(float(t), "SerialException"))
        self.assertIn("attempts=61", reporter.unavailable(60.0, "SerialException"))
        self.assertIn("gap_s=61.000", reporter.opened(61.0))
        self.assertIsNone(reporter.opened(62.0))
        self.assertEqual(logger.reconnect_interval(1.0), 0.1)
        self.assertEqual(logger.reconnect_interval(20.0), 1.0)

    def test_partial_lines_cannot_join_across_ports(self):
        logger = load_logger()
        class Sink:
            def __init__(self):
                self.items = []
            def write(self, line):
                self.items.append(line)
        sink = Sink()
        self.assertEqual(logger.flush_partial(sink, b"boot fragment", "port-lost"), b"")
        self.assertEqual(sink.items[-1], "#PARTIAL boot fragment")
        self.assertNotIn("new session", sink.items[-1])

    def test_logger_is_read_only_and_does_not_reset(self):
        self.assertIsNone(re.search(r"port\.write|\.write\(\s*b", LOGGER),
                          "soak logger must never transmit to the device")
        self.assertLess(LOGGER.index("port.dtr = False"), LOGGER.index("port.open()"))
        self.assertLess(LOGGER.index("port.rts = False"), LOGGER.index("port.open()"))

    def test_firmware_diag_format_matches_real_summarizer(self):
        """Render the *actual C format* with sample numbers and parse it.

        This catches field renames, added/removed fields and capture-order
        drift between app_main's ESP_LOGI and the host log summarizer.
        CI checks out the current PR merge with main, which contains firmware
        from #43; no hard-coded test-only DIAG template is used here.
        """
        main_c = (ROOT / "main/main.c").read_text(encoding="utf-8")
        match = re.search(
            r'ESP_LOGI\\(TAG,\\s*"DIAG uptime_s=%"\\s*PRId64'
            r'(?P<suffix>.*?)\\s*,\\s*uptime_s\\s*,',
            main_c, re.DOTALL,
        )
        self.assertIsNotNone(match, "firmware DIAG format not found")
        # PRId64 is the integer width macro for the first field. All
        # remaining fields currently use decimal %u.
        pieces = re.findall(r'"([^"]*)"', match.group("suffix"))
        firmware_format = "DIAG uptime_s=%u" + "".join(pieces)
        field_names = re.findall(r"([a-z][a-z0-9_]*)=%u", firmware_format)

        summary = load_summarizer()
        self.assertEqual(field_names, list(summary.DIAG_FIELDS))
        self.assertEqual(firmware_format.count("%u"), len(summary.DIAG_FIELDS))
        sample = tuple(range(101, 101 + len(field_names)))
        rendered = firmware_format % sample
        captured = summary.P["diag"].fullmatch(rendered)
        self.assertIsNotNone(captured, "host DIAG regex rejected firmware output")
        self.assertEqual(tuple(map(int, captured.groups())), sample)

    def test_combined_soak_docs_keep_both_workstreams(self):
        docs = (ROOT / "docs/firmware/stability-soak.md").read_text(encoding="utf-8")
        self.assertIn("Window-scoped analysis (issue #41)", docs)
        self.assertIn("Credential persistence diagnostic events", docs)
        self.assertIn("the full log", docs)
        self.assertIn("sta ip:", docs)

    def test_ci_retains_elf_for_backtrace_decoding(self):
        workflow = (ROOT / ".github/workflows/p4-build.yml").read_text(encoding="utf-8")
        self.assertIn("build/launchpad.elf", workflow)
        self.assertIn("build/launchpad.map", workflow)


if __name__ == "__main__":
    unittest.main()
