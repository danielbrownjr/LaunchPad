"""Run the shared JSON fixtures through the real C agenda model.

The firmware has no JSON parser yet, so this test performs the field mapping
a future parser must perform, compiles main/agenda_model.c with CI's warning
flags, and checks validity, next selection and view state against the same
expectations the Python wire tests use. Wire-only rejections (offsetless
timestamps, unknown keys) belong to the parser and are listed explicitly.
"""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from test_agenda_schema import FEED_KEYS, FIXTURES, ITEM_KEYS, wire_time

ROOT = Path(__file__).resolve().parents[1]
CFLAGS = ["-std=c11", "-D_POSIX_C_SOURCE=200809L", "-Wall", "-Wextra", "-Werror"]
PARSER_ONLY = {"invalid_offsetless", "invalid_unknown_field"}
SOURCES = {"demo": "LP_AGENDA_SOURCE_DEMO", "bridge": "LP_AGENDA_SOURCE_BRIDGE"}
STATUSES = {"scheduled": "LP_AGENDA_SCHEDULED", "cancelled": "LP_AGENDA_CANCELLED",
            "unscheduled": "LP_AGENDA_UNSCHEDULED"}

DRIVER_HEAD = r'''#include "agenda_model.h"
#include <stdio.h>
#include <string.h>

/* Copies at most cap bytes; an over-cap value is left unterminated so the
 * validator sees exactly what an overlong field would produce. */
static void put(char *dst, size_t cap, const char *src, size_t len)
{
    memset(dst, 0, cap);
    memcpy(dst, src, len < cap ? len : cap);
}

static const char *view_name(lp_agenda_view_t view)
{
    switch (view) {
    case LP_AGENDA_VIEW_INVALID: return "INVALID";
    case LP_AGENDA_VIEW_CLOCK_UNKNOWN: return "CLOCK_UNKNOWN";
    case LP_AGENDA_VIEW_DEMO: return "DEMO";
    case LP_AGENDA_VIEW_STALE: return "STALE";
    case LP_AGENDA_VIEW_NO_JOBS: return "NO_JOBS";
    case LP_AGENDA_VIEW_CURRENT: return "CURRENT";
    }
    return "?";
}

static void report(const char *name, const lp_agenda_snapshot_t *s, long long now)
{
    size_t index = 0;
    const bool next = lp_agenda_next(s, now, &index);
    printf("%s %d %s %s %s\n", name, lp_agenda_validate(s) ? 1 : 0,
           next ? s->items[index].id : "-", view_name(lp_agenda_view(s, now, true)),
           view_name(lp_agenda_view(s, now, false)));
}

int main(void)
{
    static lp_agenda_snapshot_t s;
'''


def c_string(value):
    raw = value.encode("utf-8", "surrogatepass")  # what a lax sender would emit
    return '"' + "".join(f"\\x{b:02x}" for b in raw) + '"', len(raw)


def epoch(value):
    return 0 if value is None else int(wire_time(value).timestamp())


def representable(wire):
    """False when only a wire parser could reject the case."""
    if set(wire) != FEED_KEYS or wire["time_zone"] != "America/New_York":
        return False
    try:
        for stamp in (wire["generated_at"], wire["source_updated_at"]):
            wire_time(stamp)
        for item in wire["appointments"]:
            if set(item) != ITEM_KEYS:
                return False
            for stamp in (item["starts_at"], item["ends_at"]):
                if stamp is not None:
                    wire_time(stamp)
    except ValueError:
        return False
    return True


def driver_source(cases, now):
    lines = [DRIVER_HEAD]
    for case in cases:
        w = case["wire"]
        items = w["appointments"]
        lines += [
            "    memset(&s, 0, sizeof s);",
            f"    s.schema_version = {int(w['schema_version'])}U;",
            f"    s.source = {SOURCES.get(w['source'], '(lp_agenda_source_t)0')};",
            f"    s.generated_at_utc = {epoch(w['generated_at'])}LL;",
            f"    s.source_updated_at_utc = {epoch(w['source_updated_at'])}LL;",
            f"    s.ttl_seconds = {int(w['ttl_seconds'])}U;",
            f"    s.count = {len(items)}U;",
        ]
        for i, item in enumerate(items[:8]):
            for field, cap in (("id", "sizeof s.items[0].id"),
                               ("label", "sizeof s.items[0].label"),
                               ("location_label", "sizeof s.items[0].location_label")):
                text, length = c_string(item[field])
                lines.append(f"    put(s.items[{i}].{field}, {cap}, {text}, {length}U);")
            lines += [
                f"    s.items[{i}].status = {STATUSES.get(item['status'], '(lp_agenda_item_status_t)0')};",
                f"    s.items[{i}].starts_at_utc = {epoch(item['starts_at'])}LL;",
                f"    s.items[{i}].ends_at_utc = {epoch(item['ends_at'])}LL;",
            ]
        lines.append(f'    report("{case["name"]}", &s, {now}LL);')
    lines += ["    return 0;", "}", ""]
    return "\n".join(lines)


def find_compiler(workdir):
    probe = workdir / "probe.c"
    probe.write_text("int main(void) { return 0; }\n", encoding="utf-8")
    for name in filter(None, (os.environ.get("CC"), "cc", "gcc", "clang")):
        path = shutil.which(name)
        if not path:
            continue
        result = subprocess.run([path, str(probe), "-o", str(workdir / "probe")],
                                capture_output=True)
        if result.returncode == 0:
            return path
    return None


class AgendaNativeFixtureTests(unittest.TestCase):
    def test_c_model_matches_fixture_expectations(self):
        now = int(wire_time(FIXTURES["as_of"]).timestamp())
        cases = [c for c in FIXTURES["cases"] if representable(c["wire"])]
        skipped = {c["name"] for c in FIXTURES["cases"]} - {c["name"] for c in cases}
        self.assertEqual(skipped, PARSER_ONLY, "new wire-only fixtures need an explicit decision")

        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            compiler = find_compiler(work)
            if compiler is None:
                if os.environ.get("CI"):
                    self.fail("CI must provide a C compiler for native fixture tests")
                self.skipTest("no working C compiler on this host")
            driver = work / "agenda_fixture_driver.c"
            driver.write_text(driver_source(cases, now), encoding="utf-8")
            binary = work / "agenda_fixture_driver"
            build = subprocess.run(
                [compiler, *CFLAGS, "-I", str(ROOT / "main"),
                 str(ROOT / "main/agenda_model.c"), str(driver), "-o", str(binary)],
                capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(binary)], capture_output=True, text=True, check=True)

        results = {}
        for line in run.stdout.splitlines():
            name, valid, nxt, view, no_clock = line.split()
            results[name] = (valid == "1", None if nxt == "-" else nxt, view, no_clock)
        self.assertEqual(set(results), {c["name"] for c in cases})
        for case in cases:
            with self.subTest(case=case["name"]):
                valid, nxt, view, no_clock = results[case["name"]]
                self.assertEqual(valid, case["expected_valid"])
                self.assertEqual(view, case["expected_view"])
                if valid:
                    self.assertEqual(nxt, case["expected_next"])
                    self.assertEqual(no_clock, "CLOCK_UNKNOWN")
                else:
                    self.assertIsNone(nxt)


if __name__ == "__main__":
    unittest.main()
