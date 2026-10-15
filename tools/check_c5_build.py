"""Assert ESP-IDF resolved the *real* P4/C5 SDIO settings, not just defaults.

Run in native ESP32-P4 CI after idf.py build. No credentials or hardware access.
"""
from pathlib import Path
import re

header = Path("build/config/sdkconfig.h")
image = Path("build/launchpad.bin")
if not header.is_file() or not image.is_file():
    raise SystemExit("Missing native target build output")

config = header.read_text(encoding="utf-8")
def require(name: str, value: str) -> None:
    if re.search(rf"^#define {re.escape(name)} {re.escape(value)}$", config, re.M) is None:
        selected = [line for line in config.splitlines() if
                    line.startswith("#define CONFIG_SLAVE_IDF_TARGET_") or
                    line.startswith("#define CONFIG_ESP_HOSTED_CP_TARGET_")]
        raise SystemExit(f"Resolved Kconfig mismatch: {name} expected {value}; found {selected}")

for name in (
    "CONFIG_COMPILER_OPTIMIZATION_SIZE",
    "CONFIG_IDF_TARGET_ESP32P4",
    "CONFIG_ESP_HOSTED_ENABLED",
    "CONFIG_SLAVE_IDF_TARGET_ESP32C5",
    "CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5",
    "CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE",
    "CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_HIGH",
    "CONFIG_ESP_HOSTED_SDIO_SLOT_1",
    "CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS",
    "CONFIG_ESP_HOSTED_SDIO_CHECKSUM",
    "CONFIG_ESP_WIFI_REMOTE_ENABLED",
    "CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED",
):
    require(name, "1")

for setting, value in {
    "CONFIG_ESP_HOSTED_SDIO_BUS_WIDTH": "4",
    "CONFIG_ESP_HOSTED_SDIO_SLOT": "1",
    "CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ": "40000",
    "CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE": "28",
    "CONFIG_ESP_HOSTED_SDIO_PIN_CMD": "31",
    "CONFIG_ESP_HOSTED_SDIO_PIN_CLK": "32",
    "CONFIG_ESP_HOSTED_SDIO_PIN_D0": "33",
    "CONFIG_ESP_HOSTED_SDIO_PIN_D1": "34",
    "CONFIG_ESP_HOSTED_SDIO_PIN_D2": "30",
    "CONFIG_ESP_HOSTED_SDIO_PIN_D3": "29",
}.items():
    require(setting, value)
if "CONFIG_ESP_HOSTED_CP_TARGET_ESP32C61 1" in config:
    raise SystemExit("C61 coprocessor target accidentally enabled")

size = image.stat().st_size
capacity = 0x100000  # Existing factory app partition, independently verified.
margin = capacity - size
if margin < 0:
    raise SystemExit(f"Unsafe image: {size:#x} exceeds {capacity:#x} app partition")
print(f"PASS: ESP32-P4 + C5 SDIO resolved; app {size:#x} / {capacity:#x}; spare {margin:#x}")
if margin < 0x10000:
    print("WARNING: less than 64 KiB spare; subsequent features need app-size strategy")
