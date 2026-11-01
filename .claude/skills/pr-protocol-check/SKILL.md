---
name: pr-protocol-check
description: Check whether a Darkride pull request or branch changes the network protocol (packet types or versions, what a packet carries, the handshake), which needs Caity's OK before merging. Use as /pr-protocol-check <PR number | branch>.
argument-hint: <PR number | branch>
---

# Protocol check

The repository is `caitlynmeeks/darkride-interface`. Work in the checkout this session is in; its remote for that
repository may be `origin` or `upstream` (`git remote -v`).

**Read the "Network protocol" section of `MAINTAINING.md` first.** It lists what counts and why it matters. Read-only:
check nothing out, post nothing.

## Steps

1. **The change:**
   - A pull request: `gh pr diff <n> -R caitlynmeeks/darkride-interface` and
     `gh pr view <n> -R caitlynmeeks/darkride-interface --json title,author,files`.
   - A branch: `git fetch --multiple <remotes>`, then `git diff <remote>/main...<branch>`.
2. **Sort each changed file** into MAINTAINING.md's groups: protocol change, server-side only, or neither. Then **read
   the hunks**: a path match isn't a verdict.
   - A packet's contents changed without its version bumped is the worst case: call it out.
   - A new entity property usually comes with an `EntityVersion` bump; check that it does.
3. **The title:** CONTRIBUTING.md asks for "(protocol change)" in the title. Say whether it's there. A protocol change
   without it, or the tag without one, is worth pointing out.

## Report

- **Verdict:** `PROTOCOL CHANGE: needs Caity's OK` / `Server-side: needs the live servers updated to take effect` /
  `Client only`.
- The evidence: `file:line` and one line why, for each hit.
- Whether old clients can still join new servers and the other way round, if you can tell.
- For a protocol change, a one-line draft comment mentioning @caitlynmeeks, for the person to post.
