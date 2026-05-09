#!/usr/bin/env python3

from __future__ import annotations

import argparse
import html
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

RATIO_GOOD_MAX_DEFAULT = 1.2
RATIO_NEUTRAL_MAX = 2.0
RATIO_EQ_EPS = 1e-12
PAIR_NEUTRAL_MAX = 1.2

COLOR_GOOD = (214, 246, 222)
COLOR_NEUTRAL = (255, 255, 255)
COLOR_BAD = (248, 220, 220)
COLOR_PAIR_POS = (236, 218, 255)
COLOR_PAIR_NEG = (213, 247, 255)


class MetricGoal:
    MIN = 0
    MAX = 1
    NEUTRAL = 2


METRIC_SPECS = {
    "mean": ("ns", MetricGoal.MIN, "Latency central tendency."),
    "stddev": ("ns", MetricGoal.MIN, "Latency dispersion."),
    "p95": ("ns", MetricGoal.MIN, "Tail latency."),
    "p99": ("ns", MetricGoal.MIN, "Tail latency."),
    "p99.9": ("ns", MetricGoal.MIN, "Tail latency."),
    "runtime": ("ms", MetricGoal.MIN, "Application mix runtime."),
    "throughput": ("Mops/s", MetricGoal.MAX, "Application throughput."),
    "handoffs": ("count", MetricGoal.NEUTRAL, "Workload/context counter; not a score metric."),
    "threads": ("count", MetricGoal.NEUTRAL, "Configuration/control value."),
    "ops_per_thread": ("count", MetricGoal.NEUTRAL, "Configuration/control value."),
    "total_ops": ("count", MetricGoal.NEUTRAL, "Workload size/control value."),
    "user": ("s", MetricGoal.MIN, "User CPU time."),
    "sys": ("s", MetricGoal.MIN, "Kernel CPU time."),
    "elapsed": ("s", MetricGoal.MIN, "Wall time."),
    "maxrss": ("KiB", MetricGoal.MIN, "Peak RSS observed by time wrapper."),
    "task_clock": ("ms", MetricGoal.MIN, "CPU time accumulated by task."),
    "syscalls": ("count", MetricGoal.MIN, "System call count."),
    "page_faults": ("count", MetricGoal.MIN, "Minor+major faults."),
    "minflt": ("count", MetricGoal.MIN, "Minor page faults."),
    "peak": ("KiB", MetricGoal.MIN, "Peak RSS in RSS microbench."),
    "after_free": ("KiB", MetricGoal.MIN, "RSS after release."),
    "live": ("KiB", MetricGoal.NEUTRAL, "Requested payload still live by benchmark design (context metric)."),
    "thp_full": ("KiB", MetricGoal.NEUTRAL, "Anon huge pages during full load."),
    "thp_free": ("KiB", MetricGoal.NEUTRAL, "Anon huge pages after free."),
    "end": ("KiB", MetricGoal.MIN, "RSS at benchmark end-point."),
    "end_live_ratio": ("ratio", MetricGoal.MIN, "RSS to live-bytes ratio."),
    "thp_end": ("KiB", MetricGoal.NEUTRAL, "Anon huge pages at end-point."),
    "frag_ratio": ("ratio", MetricGoal.MIN, "Fragmented RSS to live-bytes ratio."),
    "fragmented": ("KiB", MetricGoal.MIN, "RSS under fragmentation phase."),
    "thp": ("KiB", MetricGoal.NEUTRAL, "Anon huge pages under fragmentation."),
}


@dataclass
class ReportData:
    allocators: List[str]
    rows: Dict[Tuple[str, str], Dict[int, str]]
    descs: Dict[str, Dict[str, str]]
    seed: str


def parse_value_double(raw: Optional[str]) -> Tuple[float, bool]:
    if not raw:
        return 0.0, False

    text = raw.strip()
    if not text:
        return 0.0, False

    try:
        return float(text), True
    except ValueError:
        return 0.0, False


def raw_value_decimals(value: float) -> int:
    magnitude = abs(value)
    if magnitude < 10.0:
        return 2
    if magnitude < 100.0:
        return 1
    return 0


def format_raw_value(raw: str) -> str:
    value, ok = parse_value_double(raw)
    if not ok or not math.isfinite(value):
        return raw

    if "." not in raw and "e" not in raw.lower():
        return raw

    decimals = raw_value_decimals(value)
    text = f"{value:.{decimals}f}"
    if float(text) == 0.0:
        text = f"{0.0:.{decimals}f}"
    return text


def parse_size_bytes(raw: Optional[str]) -> Tuple[int, bool]:
    if not raw:
        return 0, False

    text = raw.strip()
    if not text:
        return 0, False

    try:
        return int(text), True
    except ValueError:
        return 0, False


def format_size_compact(raw: Optional[str]) -> str:
    value, ok = parse_size_bytes(raw)
    if not ok:
        return f"{raw if raw else '?'}B"
    if value >= 1024 * 1024 and value % (1024 * 1024) == 0:
        return f"{value // (1024 * 1024)}MiB"
    if value >= 1024 and value % 1024 == 0:
        return f"{value // 1024}KiB"
    return f"{value}B"


def parse_kv_tokens(line: str) -> Dict[str, str]:
    parts = line.split("|")
    if not parts or parts[0].strip() != "BENCH":
        return {}

    kv: Dict[str, str] = {}
    for part in parts[1:]:
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        key = key.strip()
        value = value.strip()
        if key:
            kv[key] = value
    return kv


def add_metric(rows: Dict[Tuple[str, str], Dict[int, str]], allocator_idx: int, test: str, metric: str, value: Optional[str]) -> None:
    if not value:
        return
    key = (test, metric)
    rows.setdefault(key, {})[allocator_idx] = value


def add_metric_with_fallback(rows: Dict[Tuple[str, str], Dict[int, str]], allocator_idx: int, test: str, metric: str, kv: Dict[str, str], primary: str, fallback: Optional[str] = None) -> None:
    value = kv.get(primary)
    if (not value) and fallback:
        value = kv.get(fallback)
    add_metric(rows, allocator_idx, test, metric, value)

def is_unavailable_metric_value(raw: Optional[str]) -> bool:
    if raw is None:
        return True
    text = raw.strip().lower()
    return (not text) or text in {"na", "n/a", "unavailable", "-", "none"}

def process_bench_line(line: str, allocator_idx: int, rows: Dict[Tuple[str, str], Dict[int, str]], descs: Dict[str, Dict[str, str]], seed_ref: List[str]) -> None:
    kv = parse_kv_tokens(line)
    if not kv:
        return

    typ = kv.get("type")
    if not typ:
        return

    if typ == "meta":
        seed = kv.get("seed")
        if seed:
            seed_ref[0] = seed
        return

    if typ == "desc":
        test = kv.get("test")
        if not test:
            return
        descs[test] = {
            "what": kv.get("what", ""),
            "measures": kv.get("measures", ""),
            "method": kv.get("method", ""),
            "detects": kv.get("detects", ""),
        }
        return

    if typ == "rss":
        test = f"rss {format_size_compact(kv.get('size', '?'))}"
        add_metric_with_fallback(rows, allocator_idx, test, "peak", kv, "peak", "peak_kb")
        add_metric_with_fallback(rows, allocator_idx, test, "after_free", kv, "after_free", "after_free_kb")
        add_metric_with_fallback(rows, allocator_idx, test, "live", kv, "live", "live_kb")
        add_metric_with_fallback(rows, allocator_idx, test, "thp_full", kv, "thp_full", "thp_full_kb")
        add_metric_with_fallback(rows, allocator_idx, test, "thp_free", kv, "thp_free", "thp_free_kb")
        return

    if typ == "reclaim":
        test = f"reclaim {format_size_compact(kv.get('size', '?'))} / reclaim"
        add_metric_with_fallback(rows, allocator_idx, test, "peak", kv, "peak", "peak_kb")
        add_metric_with_fallback(rows, allocator_idx, test, "end", kv, "end", "end_kb")
        add_metric_with_fallback(rows, allocator_idx, test, "live", kv, "live", "live_kb")
        add_metric_with_fallback(rows, allocator_idx, test, "end_live_ratio", kv, "end_live_ratio")
        add_metric_with_fallback(rows, allocator_idx, test, "thp_end", kv, "thp_end", "thp_end_kb")
        return

    if typ == "frag":
        test = f"frag {format_size_compact(kv.get('size', '?'))}"
        add_metric_with_fallback(rows, allocator_idx, test, "frag_ratio", kv, "frag_ratio")
        add_metric_with_fallback(rows, allocator_idx, test, "fragmented", kv, "fragmented", "fragmented_kb")
        add_metric_with_fallback(rows, allocator_idx, test, "end", kv, "end", "end_kb")
        add_metric_with_fallback(rows, allocator_idx, test, "thp", kv, "thp", "thp_kb")
        return

    if typ == "app_mix":
        variant = kv.get("variant", "")
        if not variant or variant == "mt":
            test = "app mix mt aggregate"
        elif variant == "st":
            test = "app mix st aggregate"
        else:
            test = f"app mix {variant} aggregate"

        add_metric_with_fallback(rows, allocator_idx, test, "throughput", kv, "throughput", "throughput_mops")
        add_metric_with_fallback(rows, allocator_idx, test, "runtime", kv, "runtime", "runtime_ms")
        add_metric_with_fallback(rows, allocator_idx, test, "threads", kv, "threads")
        add_metric_with_fallback(rows, allocator_idx, test, "ops_per_thread", kv, "ops_per_thread")
        add_metric_with_fallback(rows, allocator_idx, test, "total_ops", kv, "total_ops")
        if (not variant) or variant != "st":
            add_metric_with_fallback(rows, allocator_idx, test, "handoffs", kv, "handoffs")
        return

    if typ == "time":
        section = kv.get("section", "")
        test = f"system / time / {section}" if section else "system / time"
        add_metric_with_fallback(rows, allocator_idx, test, "user", kv, "user", "user_s")
        add_metric_with_fallback(rows, allocator_idx, test, "sys", kv, "sys", "sys_s")
        add_metric_with_fallback(rows, allocator_idx, test, "elapsed", kv, "elapsed", "elapsed_s")
        add_metric_with_fallback(rows, allocator_idx, test, "maxrss", kv, "maxrss", "maxrss_kb")
        add_metric_with_fallback(rows, allocator_idx, test, "minflt", kv, "minflt")
        return

    if typ == "perf":
        section = kv.get("section", "")
        test = f"system / perf / {section}" if section else "system / perf"
        if not is_unavailable_metric_value(kv.get("syscalls")):
            add_metric_with_fallback(rows, allocator_idx, test, "syscalls", kv, "syscalls")
        if (not is_unavailable_metric_value(kv.get("task_clock"))) or (not is_unavailable_metric_value(kv.get("task_clock_ms"))):
            add_metric_with_fallback(rows, allocator_idx, test, "task_clock", kv, "task_clock", "task_clock_ms")
        if not is_unavailable_metric_value(kv.get("page_faults")):
            add_metric_with_fallback(rows, allocator_idx, test, "page_faults", kv, "page_faults")
        return

    if typ == "stat":
        ctx = kv.get("ctx", "none")
        size = format_size_compact(kv.get("size", "0"))
        label = kv.get("label", "unknown")

        if ctx == "ops":
            test = f"ops {size} / {label}"
        elif ctx == "first":
            test = f"first / {label}"
        elif ctx == "foreign":
            test = f"handoff / {label}"
        elif ctx == "app":
            test = f"app / {label}"
        elif ctx == "app_mt":
            test = f"app mt / {label}"
        elif ctx == "app_st":
            test = f"app st / {label}"
        elif ctx == "locality":
            test = f"locality / {label}"
        else:
            test = f"{ctx} / {label}"

        add_metric_with_fallback(rows, allocator_idx, test, "mean", kv, "mean", "mean_ns")
        add_metric_with_fallback(rows, allocator_idx, test, "stddev", kv, "stddev", "stddev_ns")
        add_metric_with_fallback(rows, allocator_idx, test, "p95", kv, "p95", "p95_ns")
        add_metric_with_fallback(rows, allocator_idx, test, "p99", kv, "p99", "p99_ns")
        add_metric_with_fallback(rows, allocator_idx, test, "p99.9", kv, "p99_9", "p99_9_ns")


def parse_pair_group_variant(test: str) -> Tuple[Optional[str], Optional[str]]:
    if not test:
        return None, None

    if test.startswith("ops "):
        parts = test.split(" / ", 1)
        if len(parts) == 2:
            return f"ops / {parts[1]}", parts[0][4:]

    if test.startswith("locality / "):
        return "locality", test[11:]

    if test.startswith("reclaim "):
        parts = test.split(" / ", 1)
        if len(parts) == 2:
            return f"reclaim {test[8:len(test) - len(parts[1]) - 3]}", parts[1]

    if test.startswith("rss "):
        return "rss", test[4:]

    if test.startswith("frag "):
        return "frag", test[5:]

    if test.startswith("first / first alloc ("):
        end = test.rfind(")")
        if end != -1:
            return "first", test[21:end]

    return None, None


def parse_section_key(test: str) -> Optional[str]:
    if not test or test.startswith("system / "):
        return None

    if test == "app mix st aggregate" or test.startswith("app st / "):
        return "app_st"

    if test == "app mix mt aggregate" or test.startswith("app mt / "):
        return "app_mt"

    group, _ = parse_pair_group_variant(test)
    if group:
        return group

    if " / " in test:
        return test.split(" / ", 1)[0]

    return test


def parse_system_suffix(test: str) -> Optional[str]:
    if test.startswith("system / time / "):
        return test[16:]
    if test.startswith("system / perf / "):
        return test[16:]
    return None


def parse_token_size(token: str) -> Tuple[int, bool]:
    if not token:
        return 0, False

    lower = token.lower()
    try:
        if lower.endswith("mib"):
            return int(lower[:-3]) * 1024 * 1024, True
        if lower.endswith("kib"):
            return int(lower[:-3]) * 1024, True
        if lower.endswith("b"):
            return int(lower[:-1]), True
    except ValueError:
        return 0, False

    return 0, False


def ops_kind_to_label(kind: str) -> Optional[str]:
    mapping = {
        "malloc": "malloc",
        "aligned_malloc": "aligned malloc",
        "aligned_free": "aligned free",
        "calloc": "calloc",
        "free": "free",
        "malloc_hot": "malloc latency (reuse cycle)",
        "calloc_hot": "calloc latency (reuse cycle)",
        "free_hot": "free latency (fresh alloc each iter)",
        "pair": "malloc+free cycle",
        "realloc_small": "realloc small (+-4B)",
        "realloc_grow_end": "realloc grow (3x, no neighbor blocker)",
        "realloc_grow_middle": "realloc grow (3x, with neighbor blocker)",
        "realloc_shrink": "realloc shrink (1/3)",
        "realloc_class_jump": "realloc class jump (x8, blocked)",
    }
    return mapping.get(kind)


def system_row_matches_section_case(section_key: str, case_name: str, suffix: str) -> bool:
    exact_matches = {
        "rss_16b": ("rss", "rss 16B"),
        "rss_64b": ("rss", "rss 64B"),
        "rss_256b": ("rss", "rss 256B"),
        "rss_1kib": ("rss", "rss 1KiB"),
        "rss_4kib": ("rss", "rss 4KiB"),
        "reclaim": ("reclaim 64KiB", "reclaim 64KiB / reclaim"),
        "frag_32b": ("frag", "frag 32B"),
        "frag_128b": ("frag", "frag 128B"),
        "frag_512b": ("frag", "frag 512B"),
        "frag_2kib": ("frag", "frag 2KiB"),
    }
    if suffix in exact_matches:
        sec, case = exact_matches[suffix]
        return section_key == sec and case_name == case

    if suffix.startswith("ops_"):
        parts = suffix[4:].rsplit("_", 1)
        if len(parts) != 2 or not section_key.startswith("ops / "):
            return False
        label = ops_kind_to_label(parts[0])
        if not label:
            return False
        size, ok = parse_token_size(parts[1])
        if not ok:
            return False
        return case_name == f"ops {format_size_compact(str(size))} / {label}"

    if suffix.startswith("locality_"):
        if section_key != "locality":
            return False
        size, ok = parse_token_size(suffix[9:])
        if not ok:
            return False
        return case_name == f"locality / ll walk {size}B"

    if suffix.startswith("first_"):
        if section_key != "first":
            return False
        size, ok = parse_token_size(suffix[6:])
        if not ok:
            return False
        if size % (1024 * 1024) == 0:
            expected = f"first / first alloc ({size // (1024 * 1024)}MiB)"
        elif size % 1024 == 0:
            expected = f"first / first alloc ({size // 1024}KiB)"
        else:
            expected = f"first / first alloc ({size}B)"
        return case_name == expected

    if suffix.startswith("handoff_"):
        if section_key != "handoff":
            return False
        size, ok = parse_token_size(suffix[8:])
        if not ok:
            return False
        return f"({size:5d}B)" in case_name

    return False


def system_row_matches_section_wide(section_key: str, suffix: str) -> bool:
    return (suffix == "app_mt" and section_key == "app_mt") or (suffix == "app_st" and section_key == "app_st")


def system_metric_allowed_for_section(section_key: str, metric: str) -> bool:
    if not metric:
        return False

    speed_oriented = (
        section_key.startswith("ops / ")
        or section_key in ("app_mt", "app_st", "locality", "handoff", "first")
    )

    if speed_oriented:
        return metric in {"user", "sys", "elapsed", "task_clock", "maxrss", "minflt", "page_faults", "syscalls"}

    return metric in {"maxrss", "minflt", "page_faults", "syscalls"}


def test_matches_focus(test: str, focus_filters: List[str]) -> bool:
    if not focus_filters:
        return True
    if not test:
        return False
    return any(f in test for f in focus_filters)


def collect_sections(rows: Dict[Tuple[str, str], Dict[int, str]], focus_filters: List[str]) -> List[Tuple[str, List[str]]]:
    sections: List[Tuple[str, List[str]]] = []
    by_key: Dict[str, List[str]] = {}

    for test, _metric in rows.keys():
        if not test_matches_focus(test, focus_filters):
            continue

        section_key = parse_section_key(test)
        if not section_key:
            continue

        if section_key not in by_key:
            by_key[section_key] = []
            sections.append((section_key, by_key[section_key]))

        tests = by_key[section_key]
        if test not in tests:
            tests.append(test)

    return sections


def clamp01(value: float) -> float:
    return 0.0 if value < 0.0 else 1.0 if value > 1.0 else value


def lerp_rgb(a: Tuple[int, int, int], b: Tuple[int, int, int], t: float) -> Tuple[int, int, int]:
    t = clamp01(t)
    return (
        int(round((1.0 - t) * a[0] + t * b[0])),
        int(round((1.0 - t) * a[1] + t * b[1])),
        int(round((1.0 - t) * a[2] + t * b[2])),
    )


def ratio_decimals(ratio: float) -> int:
    if ratio < 10.0:
        return 2
    if ratio < 100.0:
        return 1
    return 0


def format_table_cell_with_ratio(raw: str, ratio: float, decimals: int) -> str:
    return f"{format_raw_value(raw)} (x{ratio:.{decimals}f})"


def ratio_color(ratio: float, worst_ratio: float, good_max: float, pair_palette: bool) -> Tuple[int, int, int]:
    good = COLOR_PAIR_NEG if pair_palette else COLOR_GOOD
    neutral = COLOR_NEUTRAL
    bad = COLOR_PAIR_POS if pair_palette else COLOR_BAD

    if ratio <= 1.0:
        return good

    if good_max > 1.0 + RATIO_EQ_EPS and ratio <= good_max:
        t = (ratio - 1.0) / (good_max - 1.0)
        return lerp_rgb(good, neutral, t)

    if ratio <= RATIO_NEUTRAL_MAX:
        return neutral

    if worst_ratio <= RATIO_NEUTRAL_MAX + RATIO_EQ_EPS:
        return bad

    t = math.log(ratio / RATIO_NEUTRAL_MAX) / math.log(worst_ratio / RATIO_NEUTRAL_MAX)
    return lerp_rgb(neutral, bad, t)


def pair_signed_ratio(goal: int, a: float, b: float) -> Tuple[float, float, bool]:
    if a <= 0.0 or b <= 0.0 or goal == MetricGoal.NEUTRAL:
        return 0.0, 1.0, False

    signed_ratio = 0.0
    magnitude = 1.0

    if goal == MetricGoal.MIN:
        if a < b - RATIO_EQ_EPS:
            magnitude = b / a
            signed_ratio = magnitude
        elif a > b + RATIO_EQ_EPS:
            magnitude = a / b
            signed_ratio = -magnitude
    else:
        if a > b + RATIO_EQ_EPS:
            magnitude = a / b
            signed_ratio = magnitude
        elif a < b - RATIO_EQ_EPS:
            magnitude = b / a
            signed_ratio = -magnitude

    return signed_ratio, magnitude, True


def pair_ratio_color(signed_ratio: float, magnitude: float, worst_magnitude: float) -> Tuple[int, int, int]:
    if abs(signed_ratio) <= RATIO_EQ_EPS or magnitude <= PAIR_NEUTRAL_MAX + RATIO_EQ_EPS:
        return COLOR_NEUTRAL

    if worst_magnitude <= PAIR_NEUTRAL_MAX + RATIO_EQ_EPS:
        return COLOR_PAIR_POS if signed_ratio > 0.0 else COLOR_PAIR_NEG

    t = math.log(magnitude / PAIR_NEUTRAL_MAX) / math.log(worst_magnitude / PAIR_NEUTRAL_MAX)
    return lerp_rgb(COLOR_NEUTRAL, COLOR_PAIR_POS if signed_ratio > 0.0 else COLOR_PAIR_NEG, t)


def format_pair_ratio_cell(signed_ratio: float, magnitude: float) -> str:
    decimals = ratio_decimals(magnitude)
    if abs(signed_ratio) <= RATIO_EQ_EPS:
        return f"x{1.0:.{decimals}f}"
    if signed_ratio > 0.0:
        return f"+x{magnitude:.{decimals}f}"
    return f"-x{magnitude:.{decimals}f}"


def get_row_extremes(row_values: Dict[int, str], allocator_count: int) -> Tuple[int, float, int, float]:
    min_idx = -1
    min_val = 0.0
    max_idx = -1
    max_val = 0.0

    for idx in range(allocator_count):
        value, ok = parse_value_double(row_values.get(idx))
        if not ok:
            continue

        if min_idx < 0 or value < min_val:
            min_idx = idx
            min_val = value

        if max_idx < 0 or value > max_val:
            max_idx = idx
            max_val = value

    return min_idx, min_val, max_idx, max_val


def parse_input(input_path: Path) -> ReportData:
    allocators: List[str] = []
    rows: Dict[Tuple[str, str], Dict[int, str]] = {}
    descs: Dict[str, Dict[str, str]] = {}
    seed_ref = [""]
    current_allocator = -1

    with input_path.open("r", encoding="utf-8") as in_file:
        for line in in_file:
            marker = "Results (seed="
            marker_idx = line.find(marker)
            if marker_idx >= 0:
                start = marker_idx + len(marker)
                end = line.find(")", start)
                if end != -1:
                    seed_ref[0] = line[start:end].strip()

            clean_line = line.strip()
            while clean_line.startswith("\\n"):
                clean_line = clean_line[2:]

            if clean_line.startswith("=========="):
                start = 10
                while start < len(clean_line) and clean_line[start] == " ":
                    start += 1
                end = clean_line.find("==========", start)
                if end != -1:
                    alloc_name = clean_line[start:end].strip()
                    if alloc_name not in allocators:
                        allocators.append(alloc_name)
                    current_allocator = allocators.index(alloc_name)
                continue

            if clean_line.startswith("BENCH|"):
                kv = parse_kv_tokens(clean_line)
                if current_allocator < 0 and kv.get("type") not in ("desc", "meta"):
                    continue
                process_bench_line(clean_line, current_allocator, rows, descs, seed_ref)

    return ReportData(allocators=allocators, rows=rows, descs=descs, seed=seed_ref[0])


def render_report(output_path: Path, input_path: Path, data: ReportData, focus_filters: List[str], ratio_good_max: float, ratio_pair_palette: bool) -> None:
    sections = collect_sections(data.rows, focus_filters)
    present_metrics = {metric for (_test, metric) in data.rows.keys()}

    with output_path.open("w", encoding="utf-8") as out:
        out.write(
            "<!doctype html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "<meta charset=\"utf-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
            "<title>Detailed Bench Report</title>\n"
            "<style>\n"
            ":root{color-scheme:light;font-family:ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Cantarell,Noto Sans,sans-serif;}\n"
            "body{margin:24px;background:#f7f8fb;color:#1a1f2b;}\n"
            "h1{margin:0 0 8px 0;font-size:24px;}\n"
            "h2{margin:28px 0 10px 0;font-size:18px;}\n"
            "p.meta{margin:4px 0;color:#4b5567;}\n"
            "table{border-collapse:separate;border-spacing:0;width:100%;background:#fff;border:1px solid #d8deea;border-radius:10px;margin-bottom:18px;}\n"
            "th,td{padding:8px 10px;border-bottom:1px solid #e7ecf5;font-size:13px;white-space:nowrap;}\n"
            "th{background:#eef3fc;text-align:left;color:#273246;position:sticky;top:0;z-index:2;}\n"
            "td.num{text-align:right;font-variant-numeric:tabular-nums;}\n"
            "tbody tr:nth-child(even) td{background:#fbfcff;}\n"
            "td.num strong{font-weight:700;}\n"
            "td.sep{border-top:2px solid #ced7e7;}\n"
            ".legend{margin:0 0 10px 0;color:#4b5567;font-size:13px;}\n"
            "details{margin:0 0 14px 0;}\n"
            "summary{cursor:pointer;font-weight:600;color:#273246;margin:0 0 8px 0;}\n"
            "details.section{margin:0 0 18px 0;}\n"
            "details.section > summary{font-size:15px;margin:0 0 10px 0;}\n"
            "td.block{background:#eef3fc;font-weight:700;color:#273246;}\n"
            ".var-a,.var-b{display:inline-flex;align-items:center;gap:6px;padding:1px 8px;border-radius:999px;font-weight:700;color:#273246;}\n"
            ".var-a{background:rgb(236,218,255);}\n"
            ".var-b{background:rgb(213,247,255);}\n"
            ".var-a::before,.var-b::before{content:\"●\";font-size:11px;line-height:1;position:relative;top:-1px;}\n"
            ".var-a::before{color:rgb(125,72,190);}\n"
            ".var-b::before{color:rgb(0,136,163);}\n"
            "</style>\n"
            "</head>\n"
            "<body>\n"
            "<h1>Detailed Benchmark Report</h1>\n"
        )

        out.write(f"<p class=\"meta\">Source: {html.escape(str(input_path), quote=True)}</p>\n")

        if data.seed:
            out.write(f"<p class=\"meta\">Seed: {html.escape(data.seed, quote=True)}</p>\n")

        out.write("<p class=\"meta\">Cell format: <code>raw (xratio)</code>. Ratio baseline is row-best according to metric objective.</p>\n")

        if focus_filters:
            out.write("<p class=\"meta\">Focus filters: ")
            for idx, filt in enumerate(focus_filters):
                if idx:
                    out.write(", ")
                out.write(f"<code>{html.escape(filt, quote=True)}</code>")
            out.write("</p>\n")

        out.write("<h2>Metric Policy</h2>\n<table>\n<thead><tr><th>Metric</th><th>Unit</th><th>Objective</th><th>Note</th></tr></thead>\n<tbody>\n")
        for metric, (unit, goal, note) in METRIC_SPECS.items():
            if metric not in present_metrics:
                continue

            if goal == MetricGoal.MIN:
                objective = "lower is better"
            elif goal == MetricGoal.MAX:
                objective = "higher is better"
            else:
                objective = "neutral"

            out.write(
                f"<tr><td>{html.escape(metric)}</td><td>{html.escape(unit)}</td>"
                f"<td>{html.escape(objective)}</td><td>{html.escape(note)}</td></tr>\n"
            )
        out.write("</tbody></table>\n")

        if data.descs:
            out.write("<h2>Test Descriptions</h2>\n<table>\n<thead><tr><th>Test</th><th>What it does</th><th>Measures</th><th>Method</th><th>Detects</th></tr></thead>\n<tbody>\n")
            for test_name, desc in data.descs.items():
                what = desc["what"] if desc["what"] else "-"
                measures = desc["measures"] if desc["measures"] else "-"
                method = desc["method"] if desc["method"] else "-"
                detects = desc["detects"] if desc["detects"] else "-"
                out.write(
                    f"<tr><td>{html.escape(test_name)}</td><td>{html.escape(what)}</td><td>{html.escape(measures)}</td>"
                    f"<td>{html.escape(method)}</td><td>{html.escape(detects)}</td></tr>\n"
                )
            out.write("</tbody></table>\n")

        out.write("<h2>Results</h2>\n")
        out.write("<p class=\"legend\">Each section is collapsed by default and contains raw values plus pairwise comparisons in one shared-header table.</p>\n")
        out.write("<p class=\"legend\">Raw rows: objective-relative values. Pair rows: signed <code>a -&gt; b</code> ratios where <code>+xN</code> is regression and <code>-xN</code> is improvement.</p>\n")

        for section_key, section_tests in sections:
            out.write(f"<details class=\"section\">\n<summary>{html.escape(section_key)}</summary>\n")
            out.write("<table>\n<thead><tr><th>Case</th><th>Metric</th>")
            for allocator in data.allocators:
                out.write(f"<th>{html.escape(allocator)}</th>")
            out.write("</tr></thead>\n<tbody>\n")
            out.write(f"<tr><td class=\"block\" colspan=\"{len(data.allocators) + 2}\">Raw values</td></tr>\n")

            prev_test: Optional[str] = None

            for test_name in section_tests:
                for (row_test, row_metric), row_values in data.rows.items():
                    if row_test != test_name:
                        continue

                    spec = METRIC_SPECS.get(row_metric)
                    goal = spec[1] if spec else MetricGoal.NEUTRAL

                    _min_idx, min_val, _max_idx, max_val = get_row_extremes(row_values, len(data.allocators))
                    best_val = min_val if goal == MetricGoal.MIN else max_val if goal == MetricGoal.MAX else 0.0
                    worst_val = max_val if goal == MetricGoal.MIN else min_val if goal == MetricGoal.MAX else 0.0

                    worst_ratio = 1.0
                    if goal != MetricGoal.NEUTRAL and best_val > 0.0 and worst_val > 0.0:
                        worst_ratio = (worst_val / best_val) if goal == MetricGoal.MIN else (best_val / worst_val)
                        if worst_ratio < 1.0:
                            worst_ratio = 1.0

                    new_group = prev_test != row_test
                    sep = " class=\"sep\"" if new_group else ""
                    out.write("<tr>")
                    out.write(f"<td{sep}>")
                    if new_group:
                        out.write(html.escape(row_test))
                    out.write("</td>")
                    out.write(f"<td{sep}>{html.escape(row_metric)}</td>")

                    for allocator_idx in range(len(data.allocators)):
                        raw = row_values.get(allocator_idx, "-")
                        value, has_num = parse_value_double(raw)
                        is_best = False
                        use_ratio = False
                        ratio = 1.0
                        bg = COLOR_NEUTRAL

                        if has_num and goal != MetricGoal.NEUTRAL and best_val > 0.0 and value > 0.0:
                            is_best = abs(value - best_val) <= RATIO_EQ_EPS
                            ratio = (value / best_val) if goal == MetricGoal.MIN else (best_val / value)
                            use_ratio = True
                            bg = ratio_color(ratio, worst_ratio, ratio_good_max, ratio_pair_palette)

                        cls = "num sep" if new_group else "num"
                        out.write(f"<td class=\"{cls}\" style=\"background-color: rgb({bg[0]},{bg[1]},{bg[2]});\">")
                        if use_ratio:
                            cell = format_table_cell_with_ratio(raw, ratio, ratio_decimals(ratio))
                            if is_best:
                                out.write(f"<strong>{html.escape(cell)}</strong>")
                            else:
                                out.write(html.escape(cell))
                        else:
                            out.write(html.escape(format_raw_value(raw)))
                        out.write("</td>")

                    out.write("</tr>\n")
                    prev_test = row_test

                for (row_test, row_metric), row_values in data.rows.items():
                    suffix = parse_system_suffix(row_test)
                    if not suffix:
                        continue
                    if not system_metric_allowed_for_section(section_key, row_metric):
                        continue

                    case_match = system_row_matches_section_case(section_key, test_name, suffix)
                    wide_match = test_name == section_tests[0] and system_row_matches_section_wide(section_key, suffix)
                    if not case_match and not wide_match:
                        continue

                    spec = METRIC_SPECS.get(row_metric)
                    goal = spec[1] if spec else MetricGoal.NEUTRAL
                    _min_idx, min_val, _max_idx, max_val = get_row_extremes(row_values, len(data.allocators))
                    best_val = min_val if goal == MetricGoal.MIN else max_val if goal == MetricGoal.MAX else 0.0
                    worst_val = max_val if goal == MetricGoal.MIN else min_val if goal == MetricGoal.MAX else 0.0

                    worst_ratio = 1.0
                    if goal != MetricGoal.NEUTRAL and best_val > 0.0 and worst_val > 0.0:
                        worst_ratio = (worst_val / best_val) if goal == MetricGoal.MIN else (best_val / worst_val)
                        if worst_ratio < 1.0:
                            worst_ratio = 1.0

                    out.write(f"<tr><td></td><td>{html.escape(row_metric)}</td>")
                    for allocator_idx in range(len(data.allocators)):
                        raw = row_values.get(allocator_idx, "-")
                        value, has_num = parse_value_double(raw)
                        is_best = False
                        use_ratio = False
                        ratio = 1.0
                        bg = COLOR_NEUTRAL

                        if has_num and goal != MetricGoal.NEUTRAL and best_val > 0.0 and value > 0.0:
                            is_best = abs(value - best_val) <= RATIO_EQ_EPS
                            ratio = (value / best_val) if goal == MetricGoal.MIN else (best_val / value)
                            use_ratio = True
                            bg = ratio_color(ratio, worst_ratio, ratio_good_max, ratio_pair_palette)

                        out.write(f"<td class=\"num\" style=\"background-color: rgb({bg[0]},{bg[1]},{bg[2]});\">")
                        if use_ratio:
                            cell = format_table_cell_with_ratio(raw, ratio, ratio_decimals(ratio))
                            if is_best:
                                out.write(f"<strong>{html.escape(cell)}</strong>")
                            else:
                                out.write(html.escape(cell))
                        else:
                            out.write(html.escape(format_raw_value(raw)))
                        out.write("</td>")
                    out.write("</tr>\n")

            pairs: List[Tuple[str, str]] = []
            for test_name in section_tests:
                group, variant = parse_pair_group_variant(test_name)
                if group == section_key and variant is not None:
                    pairs.append((test_name, variant))

            if len(pairs) >= 2:
                out.write(f"<tr><td class=\"block\" colspan=\"{len(data.allocators) + 2}\">Pairwise comparisons (<code>a -&gt; b</code>)</td></tr>\n")

                for a_idx in range(len(pairs) - 1):
                    for b_idx in range(a_idx + 1, len(pairs)):
                        test_a, var_a = pairs[a_idx]
                        test_b, var_b = pairs[b_idx]
                        wrote_any = False
                        first_metric = True

                        for (row_test, row_metric), row_a in data.rows.items():
                            if row_test != test_a:
                                continue

                            spec = METRIC_SPECS.get(row_metric)
                            if not spec or spec[1] == MetricGoal.NEUTRAL:
                                continue

                            row_b = data.rows.get((test_b, row_metric))
                            if row_b is None:
                                continue

                            worst_magnitude = 1.0
                            for idx in range(len(data.allocators)):
                                va, ok_a = parse_value_double(row_a.get(idx))
                                vb, ok_b = parse_value_double(row_b.get(idx))
                                if not ok_a or not ok_b:
                                    continue
                                _signed, magnitude, ok = pair_signed_ratio(spec[1], va, vb)
                                if ok and magnitude > worst_magnitude:
                                    worst_magnitude = magnitude

                            sep = " class=\"sep\"" if first_metric else ""
                            out.write("<tr>")
                            out.write(f"<td{sep}>")
                            if first_metric:
                                out.write(f"<span class=\"var-a\">{html.escape(var_a)}</span> vs <span class=\"var-b\">{html.escape(var_b)}</span>")
                            out.write("</td>")
                            out.write(f"<td{sep}>{html.escape(row_metric)}</td>")

                            cls = "num sep" if first_metric else "num"
                            for idx in range(len(data.allocators)):
                                va, ok_a = parse_value_double(row_a.get(idx))
                                vb, ok_b = parse_value_double(row_b.get(idx))
                                if not ok_a or not ok_b:
                                    out.write(f"<td class=\"{cls}\" style=\"background-color: rgb({COLOR_NEUTRAL[0]},{COLOR_NEUTRAL[1]},{COLOR_NEUTRAL[2]});\">-</td>")
                                    continue

                                signed_ratio, magnitude, ok = pair_signed_ratio(spec[1], va, vb)
                                if not ok:
                                    out.write(f"<td class=\"{cls}\" style=\"background-color: rgb({COLOR_NEUTRAL[0]},{COLOR_NEUTRAL[1]},{COLOR_NEUTRAL[2]});\">-</td>")
                                    continue

                                bg = pair_ratio_color(signed_ratio, magnitude, worst_magnitude)
                                cell = format_pair_ratio_cell(signed_ratio, magnitude)
                                out.write(f"<td class=\"{cls}\" style=\"background-color: rgb({bg[0]},{bg[1]},{bg[2]});\">{html.escape(cell)}</td>")

                            out.write("</tr>\n")
                            wrote_any = True
                            first_metric = False

                        if not wrote_any:
                            out.write("<tr><td>-</td><td>-</td>")
                            for _ in data.allocators:
                                out.write(f"<td class=\"num\" style=\"background-color: rgb({COLOR_NEUTRAL[0]},{COLOR_NEUTRAL[1]},{COLOR_NEUTRAL[2]});\">-</td>")
                            out.write("</tr>\n")

            out.write("</tbody></table>\n</details>\n")

        out.write("</body>\n</html>\n")


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog=argv[0],
        usage="%(prog)s <input-log> <output-html> [--good-max X] [--pair-palette] [--focus SUBSTR] [--focus-list A,B,...]",
    )

    parser.add_argument("input_log")
    parser.add_argument("output_html")
    parser.add_argument("--good-max", dest="good_max", default=RATIO_GOOD_MAX_DEFAULT, type=float)
    parser.add_argument("--pair-palette", action="store_true")
    parser.add_argument("--focus", action="append", default=[])
    parser.add_argument("--focus-list", action="append", default=[])

    args = parser.parse_args(argv[1:])

    focus: List[str] = []
    for value in args.focus:
        item = value.strip()
        if item:
            focus.append(item)

    for packed in args.focus_list:
        for item in packed.split(","):
            token = item.strip()
            if token:
                focus.append(token)

    if args.good_max < 1.0 or args.good_max > RATIO_NEUTRAL_MAX:
        raise ValueError(f"--good-max must be in [1.0, {RATIO_NEUTRAL_MAX:.1f}]")

    args.focus_filters = focus
    return args


def main(argv: List[str]) -> int:
    try:
        args = parse_args(argv)
    except ValueError as exc:
        sys.stderr.write(f"{exc}\n")
        return 1

    input_path = Path(args.input_log)
    output_path = Path(args.output_html)

    try:
        data = parse_input(input_path)
    except OSError as exc:
        sys.stderr.write(f"fopen input: {exc}\n")
        return 1

    try:
        render_report(
            output_path=output_path,
            input_path=input_path,
            data=data,
            focus_filters=args.focus_filters,
            ratio_good_max=args.good_max,
            ratio_pair_palette=args.pair_palette,
        )
    except OSError as exc:
        sys.stderr.write(f"fopen output: {exc}\n")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
