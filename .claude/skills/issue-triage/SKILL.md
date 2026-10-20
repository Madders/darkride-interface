---
name: issue-triage
description: Triage open issues on Darkride - suggest labels, duplicates, linked pull requests, a next step and a reply for each, and apply labels only after the maintainer says yes. Use as /issue-triage, or /issue-triage <issue number>.
argument-hint: "[issue number]"
---

# Triage issues

The repository is `caitlynmeeks/darkride-interface`. **Read the "Issues" section of `MAINTAINING.md` first.**

## Steps

1. **Which issues:** with a number, that one. Otherwise every open issue with no labels, plus any updated in the last
   seven days:
   `gh issue list -R caitlynmeeks/darkride-interface --state open --limit 100 --json number,title,labels,author,createdAt,updatedAt,comments`,
   then `gh issue view <n> -R caitlynmeeks/darkride-interface --comments` for each.
2. **The labels that exist:** `gh label list -R caitlynmeeks/darkride-interface`. Use only those. If one is missing,
   note it under "Label ideas" rather than creating it.
3. **Cross-check:** open and recently merged pull requests (`gh pr list --state all --limit 50 --json number,title,body,state`)
   for "Fixes #n" or an obvious match, and other issues for duplicates.
4. **For each issue, decide:**
   - the labels;
   - whether it can be reproduced from what's there, and if not, what's missing;
   - client, server or protocol, and whether it's Caity's (live worlds, accounts, the metaverse service);
   - a linked pull request or a duplicate;
   - the next step: needs information, ready to work on, fixed by #x, or close (and why).
   - A security problem: don't suggest discussing it in public. Flag it for Caity, privately.

## Output

A table: `# | title | labels | next step | Caity's?`. Under it, a draft reply for each issue that needs one: friendly,
thanks the reporter, asks for specifics. Then any "Label ideas".

Ask which labels to apply. **Only after a yes,** run
`gh issue edit <n> -R caitlynmeeks/darkride-interface --add-label "<label>"` for those, and report what was done.
Don't comment on, close, assign or lock issues unless asked to in this conversation.
