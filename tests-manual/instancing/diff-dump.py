#!/usr/bin/env python3
# Diffs the draws of two frames from a client log's INSTANCING-DUMP lines (Test.dumpDraws, dump-draws.js): the draws
# of the frame with GPU instancing on, each instanced group expanded to its instances, against the frame with it off.
#   diff-dump.py <client log>
# Prints what each mode drew that the other did not, by model file, part and level, with positions.
import re, sys, collections

LINE = re.compile(r'INSTANCING-DUMP frame=(\d+) kind=(\w+) file=(\S*) mesh=(\w+) part=(\d+) lod=(\d+) indices=(\d+) n=(\d+) at=(\S*)')

def frames(path):
    by_frame = collections.defaultdict(list)
    for line in open(path, errors='replace'):
        m = LINE.search(line)
        if not m:
            continue
        frame, kind, file, mesh, part, lod, indices, n, at = m.groups()
        positions = at.split(';') if at else []
        for p in positions:
            by_frame[int(frame)].append((file, mesh, int(part), int(lod), int(indices), p))
    return by_frame

def main():
    by_frame = frames(sys.argv[1])
    if len(by_frame) < 2:
        print("need two dumped frames, found", len(by_frame)); sys.exit(2)
    numbers = sorted(by_frame)
    # the first three frames are instancing on, the next three off (dump-draws.js); take the middle of each run
    on = by_frame[numbers[1]] if len(numbers) >= 3 else by_frame[numbers[0]]
    off = by_frame[numbers[4]] if len(numbers) >= 6 else by_frame[numbers[-1]]
    print(f"frames dumped: {numbers}; comparing on frame {numbers[1] if len(numbers) >= 3 else numbers[0]} ({len(on)} draws) "
          f"against off frame {numbers[4] if len(numbers) >= 6 else numbers[-1]} ({len(off)} draws)")
    con = collections.Counter(on); coff = collections.Counter(off)
    only_on = con - coff; only_off = coff - con
    tri = lambda c: sum(k[4] // 3 * v for k, v in c.items())
    print(f"triangles on {tri(con)} off {tri(coff)}; draws only with instancing on: {sum(only_on.values())}, only with it off: {sum(only_off.values())}")
    def show(title, c):
        print(title)
        by_model = collections.defaultdict(list)
        for (file, mesh, part, lod, indices, p), v in c.items():
            by_model[(file, mesh, part, lod, indices)].append((p, v))
        for key, places in sorted(by_model.items(), key=lambda kv: -len(kv[1]))[:30]:
            file, mesh, part, lod, indices = key
            print(f"  {file[:38]:38s} mesh {mesh} part {part} lod {lod} tris {indices // 3:7d} x{sum(v for _, v in places):3d}  at " +
                  ", ".join(p for p, _ in places[:4]) + (" ..." if len(places) > 4 else ""))
    show("only with instancing on:", only_on)
    show("only with instancing off:", only_off)
    # a part drawn in both at the same place but at a different level: pairs that differ only in lod/indices
    key_no_lod = lambda k: (k[0], k[1], k[2], k[5])
    on_levels = collections.defaultdict(set); off_levels = collections.defaultdict(set)
    for k in only_on: on_levels[key_no_lod(k)].add((k[3], k[4]))
    for k in only_off: off_levels[key_no_lod(k)].add((k[3], k[4]))
    both = [(k, on_levels[k], off_levels[k]) for k in on_levels if k in off_levels]
    print(f"same part at the same place at a different level: {len(both)}")
    for k, a, b in both[:12]:
        print(f"  {k[0][:38]:38s} part {k[2]} at {k[3]}: on {sorted(a)} off {sorted(b)}")

if __name__ == "__main__":
    main()
