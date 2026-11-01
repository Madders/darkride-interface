#!/usr/bin/env python3
#
#  profile-summary.py
#  tests-manual/wasm-scripting
#
#  Reads the guest profiles tests/wasm-speed writes (DARKRIDE_SPEED_PROFILE: wasmtime's guest profiler, in the
#  Firefox Profiler's processed format) and prints, for each, the functions the samples fell in: "self" is the
#  share of samples whose innermost frame is the function, "total" the share with the function anywhere on the
#  stack. The same files open in https://profiler.firefox.com for the full picture.
#
#  Run: tests-manual/wasm-scripting/profile-summary.py <profile.json or folder> [--top N]
#
#  Distributed under the Apache License, Version 2.0.
#

import json
import os
import sys
from collections import Counter


def table(thread_or_profile, name):
    """A table as a dict of columns; older files keep {schema, data} rows instead."""
    t = thread_or_profile.get(name, {})
    if "schema" in t and "data" in t:
        columns = {key: [] for key in t["schema"]}
        for row in t["data"]:
            for key, index in t["schema"].items():
                columns[key].append(row[index] if index < len(row) else None)
        return columns
    return t


def strings_of(profile, thread):
    for holder in (thread, profile.get("shared", {})):
        for key in ("stringArray", "stringTable"):
            if key in holder:
                return holder[key]
    return []


def summarize(path, top):
    with open(path) as f:
        profile = json.load(f)
    for thread in profile.get("threads", []):
        strings = strings_of(profile, thread)
        samples = table(thread, "samples")
        stacks = table(thread, "stackTable")
        frames = table(thread, "frameTable")
        funcs = table(thread, "funcTable")
        func_names = [strings[i] if isinstance(i, int) and i < len(strings) else str(i) for i in funcs.get("name", [])]

        def stack_functions(stack):
            names = []
            while stack is not None:
                frame = stacks["frame"][stack]
                func = frames["func"][frame]
                names.append(func_names[func] if func is not None and func < len(func_names) else "?")
                stack = stacks["prefix"][stack]
            return names  # innermost first

        self_counts, total_counts = Counter(), Counter()
        stack_column = samples.get("stack", [])
        weights = samples.get("weight") or [1] * len(stack_column)
        count = 0
        for stack, weight in zip(stack_column, weights):
            if stack is None:
                continue
            weight = weight or 1
            names = stack_functions(stack)
            self_counts[names[0]] += weight
            for name in set(names):
                total_counts[name] += weight
            count += weight
        title = os.path.splitext(os.path.basename(path))[0]
        print(f"== {title}: {count} samples ({thread.get('name', '')})")
        if not count:
            continue
        print(f"   {'self':>6} {'total':>6}  function")
        for name, n in self_counts.most_common(top):
            print(f"   {100.0 * n / count:5.1f}% {100.0 * total_counts[name] / count:5.1f}%  {name}")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    top = 15
    if "--top" in sys.argv:
        top = int(sys.argv[sys.argv.index("--top") + 1])
        args = [a for a in args if a != str(top)]
    if not args:
        print(__doc__ or "usage: profile-summary.py <profile.json or folder> [--top N]")
        sys.exit(1)
    for target in args:
        paths = sorted(os.path.join(target, name) for name in os.listdir(target) if name.endswith(".json")) \
            if os.path.isdir(target) else [target]
        for path in paths:
            summarize(path, top)


if __name__ == "__main__":
    main()
