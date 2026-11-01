# Aggregates a macOS `sample` call graph under one frame: the inclusive and self samples of each function below it.
#   sample <pid> <seconds> 1 -file out.txt; python3 -I sample-tree.py out.txt "<root function>" [top]
import re, sys
from collections import Counter, defaultdict
path, root_pattern = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 40
line_re = re.compile(r'^([\s+!:|]*)(\d+) (.*)$')
def clean(name):
    name = re.sub(r'\s+\(in [^)]*\).*$', '', name)
    name = re.sub(r'\s+\+ \d+.*$', '', name)
    name = re.sub(r'\s+\[0x[0-9a-f,.]*\].*$', '', name)
    name = re.sub(r'\(.*$', '', name) if not name.startswith('(') else name
    return name.strip()
nodes = []  # (depth, count, name)
in_graph = False
for line in open(path):
    if line.startswith('Call graph:'):
        in_graph = True; continue
    if in_graph and (line.startswith('Total number in stack') or line.startswith('Sort by top of stack')):
        break
    if not in_graph: continue
    m = line_re.match(line.rstrip('\n'))
    if not m: continue
    nodes.append((len(m.group(1)), int(m.group(2)), clean(m.group(3))))
# build tree
children = defaultdict(list); parent = {}
stack = []
for i, (d, c, n) in enumerate(nodes):
    while stack and nodes[stack[-1]][0] >= d: stack.pop()
    if stack: parent[i] = stack[-1]; children[stack[-1]].append(i)
    stack.append(i)
roots = [i for i, (d, c, n) in enumerate(nodes) if re.search(root_pattern, n)]
# keep only outermost matching roots
def has_ancestor(i, pred):
    while i in parent:
        i = parent[i]
        if pred(i): return True
    return False
roots = [r for r in roots if not has_ancestor(r, lambda j: re.search(root_pattern, nodes[j][2]))]
total = sum(nodes[r][1] for r in roots)
inclusive, selfc = Counter(), Counter()
def walk(i, seen):
    d, c, n = nodes[i]
    if n not in seen: inclusive[n] += c
    s = c - sum(nodes[k][1] for k in children[i])
    selfc[n] += s
    for k in children[i]: walk(k, seen | {n})
for r in roots: walk(r, frozenset())
print(f"root {root_pattern}: {total} samples")
print("-- inclusive --")
for n, c in inclusive.most_common(top): print(f"{100.0*c/total:6.1f}% {c:6d} {n[:110]}")
print("-- self --")
for n, c in selfc.most_common(top): print(f"{100.0*c/total:6.1f}% {c:6d} {n[:110]}")
