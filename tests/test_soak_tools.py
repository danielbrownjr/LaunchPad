"""Host tests for the issue #37 soak tools. No serial port or hardware needed."""
import contextlib
import importlib.util
import io
import re
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOGGER = (ROOT / "tools/soak_logger.py").read_text(encoding="utf-8")


def load_summarizer():
    spec = importlib.util.spec_from_file_location("soak_summarize", ROOT / "tools/soak_summarize.py")
    module = importlib.util.module_from_spec(spec)
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
]


class SoakTools(unittest.TestCase):
    def summarize(self, *flags):
        with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False, encoding="utf-8") as f:
            for n, text in enumerate(SAMPLE):
                f.write(f"2026-10-16T12:00:{n:02d}.000+00:00 {n * 0.5:11.3f} | {text}\n")
            path = f.name
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

    def test_summary_counts(self):
        text = self.summarize()
        self.assertIn("ELF SHA256: 1a2b3c4d5e6f7a8b", text)
        self.assertIn("task watchdog events: 1", text)
        self.assertIn("keyboard opens: 2   off-spec geometry: 1", text)
        self.assertIn("SNTP confirmations: 2", text)
        self.assertIn("main stack min free at boot (bytes): [7472]", text)
        self.assertIn("probably monitor-induced", text)
        self.assertIn("B3 cycle 10", text)
        self.assertIn("result: WATCHDOG/PANIC PRESENT", text)

    def test_redaction_keeps_backtrace(self):
        text = self.summarize("--redact")
        for private in ("192.168.8.115", "10.0.0.7", "Example Net", "aa:bb:cc:dd:ee:ff", "B3 cycle 10"):
            self.assertNotIn(private, text)
        self.assertIn("MEPC    : 0x4800abcd  RA      : 0x4800a0f0", text)
        self.assertIn("IDLE0 (CPU 0)", text)

    def test_logger_is_read_only_and_does_not_reset(self):
        self.assertIsNone(re.search(r"port\.write|\.write\(\s*b", LOGGER),
                          "soak logger must never transmit to the device")
        self.assertLess(LOGGER.index("port.dtr = False"), LOGGER.index("port.open()"))
        self.assertLess(LOGGER.index("port.rts = False"), LOGGER.index("port.open()"))

    def test_ci_retains_elf_for_backtrace_decoding(self):
        workflow = (ROOT / ".github/workflows/p4-build.yml").read_text(encoding="utf-8")
        self.assertIn("build/launchpad.elf", workflow)
        self.assertIn("build/launchpad.map", workflow)


if __name__ == "__main__":
    unittest.main()
