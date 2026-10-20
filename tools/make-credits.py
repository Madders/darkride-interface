#!/usr/bin/env python3
#
#  make-credits.py
#  tools
#
#  Writes the About box's credits (interface/resources/qml/hifi/dialogs/credits.js): the code contributors to High
#  Fidelity, Tivoli Cloud VR and Darkride, from the first commit to today, as git records them, authors and
#  co-authors alike.
#  One person under several names or addresses ("atlante45", "Atlante45"; "AndrewMeadows", "Andrew Meadows")
#  is listed once, by the name with the most commits, a full name first. Only names are written, never an
#  address. Run it again before a release:  python3 tools/make-credits.py
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#

import collections
import json
import os
import re
import subprocess

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "interface", "resources", "qml", "hifi", "dialogs", "credits.js")

# machine and shared accounts, not people
NOT_PEOPLE = {"null", "root", "unknown", "your name", "github", "none", "user", "hifi test account",
              "jenkins slave access", "shared vive room"}
ANTHROPIC = "noreply@anthropic.com"
CO_AUTHOR = re.compile(r"^co-authored-by:\s*(.+?)\s*<([^>]*)>\s*$", re.IGNORECASE | re.MULTILINE)


def contributions():
    """(name, email) once per commit for its author and each co-author."""
    log = subprocess.run(["git", "log", "--format=%x1e%aN%x1f%aE%x1f%b"], cwd=ROOT, check=True,
                         capture_output=True, text=True, errors="replace").stdout
    for record in log.split("\x1e")[1:]:
        name, email, body = (record.split("\x1f") + ["", ""])[:3]
        yield name.strip(), email.strip()
        for co_name, co_email in CO_AUTHOR.findall(body):
            yield co_name.strip(), co_email.strip()


def display_name(name):
    """A name as a person reads it: a Windows "MACHINE\\user" is the user, an address its part before the @."""
    name = name.split("\\")[-1].strip()
    if "@" in name:
        name = name.split("@")[0]
    return name


def is_person(name):
    lowered = name.lower()
    return (re.search(r"[^\W\d_]", name) is not None and lowered not in NOT_PEOPLE and "[bot]" not in lowered
            and not lowered.endswith("bot"))


def credits():
    parent = {}

    def find(key):
        parent.setdefault(key, key)
        while parent[key] != key:
            parent[key] = parent[parent[key]]
            key = parent[key]
        return key

    def union(a, b):
        parent[find(a)] = find(b)

    commits = collections.Counter()
    for name, email in contributions():
        name = display_name(name)
        if not is_person(name):
            continue
        commits[name] += 1
        name_key = "n:" + re.sub(r"[^a-z0-9]", "", name.lower())
        union("s:" + name, name_key)
        if email:
            union(name_key, "e:" + email.lower())

    groups = collections.defaultdict(list)
    for name in commits:
        groups[find("s:" + name)].append(name)

    listed = []
    for names in groups.values():
        emails = [key for key, root in parent.items() if key.startswith("e:") and find(key) == find("s:" + names[0])]
        if "e:" + ANTHROPIC in emails:
            listed.append("Claude (Anthropic)")
            continue
        names.sort(key=lambda n: (" " not in n.strip(), -commits[n], n))
        listed.append(names[0])
    return sorted(set(listed), key=lambda n: re.sub(r"^[^A-Za-z0-9]+", "", n).casefold())


if __name__ == "__main__":
    names = credits()
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("// Written by tools/make-credits.py from the git history; run it again before a release.\n")
        f.write(".pragma library\n\n")
        f.write("var names = " + json.dumps(names, ensure_ascii=False, indent=4) + ";\n")
    print(len(names), "names in", os.path.relpath(OUT, ROOT))
