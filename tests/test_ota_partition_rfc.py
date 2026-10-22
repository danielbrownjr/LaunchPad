"""Review-only contract for the inactive 16 MiB dual-OTA partition RFC.

Runs under standard 'python3 -m unittest discover -s tests -v'. No device,
flashing, build config mutation, image rewriting, or external dependencies.
ESP-IDF 6.0.3's real partition generator is an additional future gate.
"""
import csv
from dataclasses import dataclass, replace
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CSV = ROOT / "docs/partitions/launchpad-ota-4m-4m.csv"
CONFIG = ROOT / "sdkconfig.defaults"

FLASH_SIZE = 0x1000000
APP_ALIGNMENT = 0x10000
DATA_ALIGNMENT = 0x1000


@dataclass(frozen=True)
class Entry:
    name: str
    kind: str
    subtype: str
    offset: int
    size: int
    flags: str

    @property
    def end(self):
        return self.offset + self.size


def parse(text):
    result = []
    for line in csv.reader(text.splitlines()):
        if not line or not line[0].strip() or line[0].lstrip().startswith("#"):
            continue
        if len(line) != 6:
            raise ValueError("Each partition row must have six CSV fields")
        name, kind, subtype, offset, size, flags = [x.strip() for x in line]
        result.append(Entry(name, kind, subtype, int(offset, 0),
                            int(size, 0), flags))
    return result


EXPECTED = {
    "nvs": ("data", "nvs", 0x9000, 0x6000),
    "phy_init": ("data", "phy", 0xF000, 0x1000),
    "otadata": ("data", "ota", 0x10000, 0x2000),
    "ota_0": ("app", "ota_0", 0x20000, 0x400000),
    "ota_1": ("app", "ota_1", 0x420000, 0x400000),
    "reserved": ("data", "0x40", 0x820000, 0x7E0000),
}


def validate(entries):
    by_name = {entry.name: entry for entry in entries}
    if len(by_name) != len(entries) or set(by_name) != set(EXPECTED):
        raise ValueError("Unexpected or duplicate partition")
    for name, (kind, subtype, offset, size) in EXPECTED.items():
        item = by_name[name]
        if (item.kind, item.subtype, item.offset, item.size) != (
                kind, subtype, offset, size):
            raise ValueError(f"{name} does not match the reviewed migration")
        if item.flags:
            raise ValueError("Encryption/read-only flags require separate review")
        if not item.size or item.offset < 0 or item.end > FLASH_SIZE:
            raise ValueError("Partition outside physical flash")
        alignment = APP_ALIGNMENT if item.kind == "app" else DATA_ALIGNMENT
        if item.offset % alignment or item.size % DATA_ALIGNMENT:
            raise ValueError("Partition misaligned")
    ordered = sorted(entries, key=lambda entry: entry.offset)
    # Bootloader and partition-table ranges are below the first data region.
    if ordered[0].offset < 0x9000:
        raise ValueError("Overwrites bootloader or partition table")
    for older, newer in zip(ordered, ordered[1:]):
        if older.end > newer.offset:
            raise ValueError("Overlapping partitions")
    if ordered[-1].end != FLASH_SIZE:
        raise ValueError("Last partition must end at physical flash boundary")
    if by_name["otadata"].size != 0x2000:
        raise ValueError("OTA metadata must occupy exactly two flash sectors")
    if by_name["ota_0"].size != by_name["ota_1"].size:
        raise ValueError("OTA application slots must be equal")


class InactiveOtaLayoutTests(unittest.TestCase):
    def setUp(self):
        self.entries = parse(CSV.read_text(encoding="utf-8"))

    def test_approved_candidate_is_consistent(self):
        validate(self.entries)
        self.assertEqual(sum(e.size for e in self.entries
                             if e.kind == "app"), 0x800000)
        self.assertEqual(next(e.size for e in self.entries
                              if e.name == "reserved"), 0x7E0000)

    def test_current_production_build_is_not_switched(self):
        defaults = CONFIG.read_text(encoding="utf-8")
        self.assertIn('CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y', defaults)
        self.assertNotIn('CONFIG_PARTITION_TABLE_CUSTOM=y', defaults)
        self.assertNotIn("launchpad-ota-4m-4m.csv", defaults)
        # The old 1 MiB partition-fit gate remains active until migration.
        current_guard = (ROOT / "tools/check_c5_build.py").read_text(
            encoding="utf-8")
        self.assertIn("capacity = 0x100000", current_guard)

    def test_nvs_and_phy_preserve_original_addresses(self):
        mapped = {e.name: e for e in self.entries}
        self.assertEqual((mapped["nvs"].offset, mapped["nvs"].size),
                         (0x9000, 0x6000))
        self.assertEqual((mapped["phy_init"].offset,
                          mapped["phy_init"].size), (0xF000, 0x1000))
        self.assertNotIn("factory", mapped)
        self.assertEqual(mapped["otadata"].offset, 0x10000)
        self.assertEqual(mapped["ota_0"].offset, 0x20000)

    def test_candidate_conflicts_with_old_app_intentionally(self):
        old_factory = (0x10000, 0x110000)
        by_name = {e.name: e for e in self.entries}
        self.assertTrue(by_name["otadata"].offset < old_factory[1])
        self.assertTrue(by_name["ota_0"].offset < old_factory[1])
        # This is why no in-place partition-table-only flash is allowed.

    def test_mutations_are_detected(self):
        name_to_index = {e.name: i for i, e in enumerate(self.entries)}
        variants = [
            replace(self.entries[name_to_index["nvs"]], offset=0xA000),
            replace(self.entries[name_to_index["ota_0"]], offset=0x21000),
            replace(self.entries[name_to_index["ota_1"]], offset=0x410000),
            replace(self.entries[name_to_index["otadata"]], size=0x1000),
            replace(self.entries[name_to_index["reserved"]], size=0x7F0000),
            replace(self.entries[name_to_index["reserved"]], subtype="spiffs"),
        ]
        for index, changed in zip(
            [name_to_index[n] for n in (
                "nvs", "ota_0", "ota_1", "otadata", "reserved",
                "reserved")], variants
        ):
            candidate = list(self.entries)
            candidate[index] = changed
            with self.subTest(entry=changed.name, value=changed):
                with self.assertRaises(ValueError):
                    validate(candidate)

    def test_duplicate_and_garbage_csv_rejected(self):
        with self.assertRaises(ValueError):
            validate(self.entries + [self.entries[0]])
        with self.assertRaises(ValueError):
            parse("ota_0,app,ota_0,0x20000\n")
        with self.assertRaises(ValueError):
            parse("ota_0,app,ota_0,xyz,0x400000,\n")


if __name__ == "__main__":
    unittest.main()
