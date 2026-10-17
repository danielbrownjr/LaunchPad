"""Synthetic LaunchPad v1 *proposed wire* contract tests.

The firmware does NOT parse JSON yet. These fixtures establish the future
bridge boundary, while native tests exercise the bounded C in-memory form.
"""
import datetime as dt
import json
import os
import re
import unittest
from pathlib import Path
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = json.loads((ROOT / "tests/fixtures/agenda_cases.json").read_text(encoding="utf-8"))
ITEM_KEYS = {"id", "label", "location_label", "status", "starts_at", "ends_at"}
FEED_KEYS = {"schema_version", "source", "generated_at", "source_checked_at",
             "time_zone", "ttl_seconds", "appointments"}
STRICT_TIME = re.compile(r"\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d+)?(?:Z|[+-]\d\d:\d\d)\Z")
IDENTIFIER = re.compile(r"[A-Za-z0-9._-]{1,24}\Z")


def wire_time(value):
    if not isinstance(value, str) or not STRICT_TIME.fullmatch(value):
        raise ValueError("timestamp requires explicit ISO offset")
    parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.utcoffset() is None:
        raise ValueError("naive clock")
    return parsed.astimezone(dt.timezone.utc)


def display_text(value, max_bytes, allow_empty=False):
    if not isinstance(value, str):
        return False
    try:
        length = len(value.encode("utf-8", errors="strict"))
    except UnicodeError:
        return False
    return (int(not allow_empty) <= length <= max_bytes
            and all((ord(c) >= 32 and not (0x7F <= ord(c) <= 0x9F))
                    for c in value))


def validate_wire(data):
    if not isinstance(data, dict) or set(data) != FEED_KEYS:
        return False
    if type(data["schema_version"]) is not int or data["schema_version"] != 1:
        return False
    if data["source"] not in ("demo", "bridge"):
        return False
    if data["time_zone"] != "America/New_York":
        return False
    if type(data["ttl_seconds"]) is not int or not 60 <= data["ttl_seconds"] <= 86400:
        return False
    try:
        generated = wire_time(data["generated_at"])
        checked = wire_time(data["source_checked_at"])
    except (ValueError, TypeError):
        return False
    if checked > generated or generated.timestamp() <= 0 or checked.timestamp() <= 0:
        return False
    items = data["appointments"]
    if not isinstance(items, list) or len(items) > 8:
        return False
    seen = set()
    for item in items:
        if not isinstance(item, dict) or set(item) != ITEM_KEYS:
            return False
        ident = item["id"]
        if not isinstance(ident, str) or not IDENTIFIER.fullmatch(ident) or ident in seen:
            return False
        seen.add(ident)
        if not display_text(item["label"], 48):
            return False
        if not display_text(item["location_label"], 64, allow_empty=True):
            return False
        if item["status"] not in ("scheduled", "cancelled", "unscheduled"):
            return False
        if item["status"] == "unscheduled":
            if item["starts_at"] is not None or item["ends_at"] is not None:
                return False
            continue
        try:
            start = wire_time(item["starts_at"])
            end = wire_time(item["ends_at"])
        except (ValueError, TypeError):
            return False
        if start.timestamp() <= 0 or not start < end or (end - start).total_seconds() > 7 * 86400:
            return False
    return True


def selected_next(data, now):
    candidates = []
    for item in data["appointments"]:
        if item["status"] != "scheduled":
            continue
        if wire_time(item["ends_at"]) <= now:
            continue
        candidates.append(item)
    candidates.sort(key=lambda item: (wire_time(item["starts_at"]), item["id"]))
    return candidates[0]["id"] if candidates else None


def assess_view(data, now, clock_valid=True):
    """Reference for lp_agenda_view(); the native fixture test checks C agrees."""
    if not validate_wire(data):
        return "INVALID"
    if not clock_valid:
        return "CLOCK_UNKNOWN"
    if data["source"] == "demo":
        return "DEMO"
    generated = wire_time(data["generated_at"])
    checked = wire_time(data["source_checked_at"])
    if max(generated, checked) - now > dt.timedelta(seconds=300):
        return "INVALID"
    ttl = dt.timedelta(seconds=data["ttl_seconds"])
    if now - generated > ttl or now - checked > ttl:
        return "STALE"
    return "CURRENT" if selected_next(data, now) else "NO_JOBS"


def eastern():
    try:
        return ZoneInfo("America/New_York")
    except ZoneInfoNotFoundError:
        # Windows Python has no system tz database without the tzdata package.
        if os.environ.get("CI"):
            raise
        raise unittest.SkipTest("tz database unavailable; install tzdata to run locally")


class AgendaV1WireTests(unittest.TestCase):
    def test_all_fictional_cases_have_expected_validity(self):
        self.assertGreaterEqual(len(FIXTURES["cases"]), 12)
        self.assertEqual(FIXTURES["fixture_version"], 1)
        for case in FIXTURES["cases"]:
            with self.subTest(case=case["name"]):
                self.assertEqual(validate_wire(case["wire"]), case["expected_valid"])

    def test_fixture_next_job_ordering(self):
        now = wire_time(FIXTURES["as_of"])
        for case in FIXTURES["cases"]:
            if not case["expected_valid"]:
                continue
            with self.subTest(case=case["name"]):
                self.assertEqual(selected_next(case["wire"], now), case["expected_next"])

    def test_explicit_offset_midnight_uses_eastern_dst(self):
        case = next(x for x in FIXTURES["cases"] if x["name"] == "cross_midnight_offset")
        self.assertTrue(validate_wire(case["wire"]))
        item = case["wire"]["appointments"][0]
        zone = eastern()
        first = wire_time(item["starts_at"]).astimezone(zone)
        last = wire_time(item["ends_at"]).astimezone(zone)
        self.assertEqual(first.hour, 23)
        self.assertEqual(last.hour, 1)
        self.assertNotEqual(first.date(), last.date())

    def test_fixture_view_states(self):
        now = wire_time(FIXTURES["as_of"])
        views = set()
        for case in FIXTURES["cases"]:
            with self.subTest(case=case["name"]):
                self.assertEqual(assess_view(case["wire"], now), case["expected_view"])
                views.add(case["expected_view"])
                if case["expected_valid"]:
                    self.assertEqual(assess_view(case["wire"], now, clock_valid=False),
                                     "CLOCK_UNKNOWN")
        self.assertEqual(views, {"DEMO", "CURRENT", "STALE", "NO_JOBS", "INVALID"})

    def test_dst_fall_back_orders_by_utc_not_wall_clock(self):
        case = next(x for x in FIXTURES["cases"]
                    if x["name"] == "dst_fall_back_repeated_hour")
        first, second = sorted(case["wire"]["appointments"],
                               key=lambda item: wire_time(item["starts_at"]))
        self.assertEqual(first["id"], "demo-dst-first")
        zone = eastern()
        first_wall = wire_time(first["starts_at"]).astimezone(zone)
        second_wall = wire_time(second["starts_at"]).astimezone(zone)
        # 01:30 EDT happens before 01:10 EST: wall-clock sorting would be wrong.
        self.assertGreater(first_wall.time(), second_wall.time())
        self.assertEqual((first_wall.fold, second_wall.fold), (0, 1))

    def test_utf8_lengths_are_bytes_not_characters(self):
        self.assertTrue(display_text("Café", 5))
        self.assertFalse(display_text("Café", 4))
        self.assertFalse(display_text("\ud800", 48))
        self.assertFalse(display_text("line\nfeed", 48))
        self.assertFalse(display_text("value\u0085", 48))

    def test_unknown_fields_rejected_not_silently_ignored(self):
        valid = next(x["wire"] for x in FIXTURES["cases"] if x["name"] == "one")
        self.assertTrue(validate_wire(valid))
        self.assertFalse(validate_wire({**valid, "private_extra": True}))


if __name__ == "__main__":
    unittest.main()
