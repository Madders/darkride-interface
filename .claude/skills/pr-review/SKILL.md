---
name: pr-review
description: First review of a Darkride pull request - checks it out in a separate worktree, reviews it against MAINTAINING.md, runs the protocol check, and drafts a review for the maintainer to post. Use as /pr-review <PR number>.
argument-hint: <PR number>
---

# Review a pull request

The repository is `caitlynmeeks/darkride-interface`. Work in the checkout this session is in
(`git rev-parse --show-toplevel`). Its remote for that repository may be `origin` or `upstream`: find it with
`git remote -v`, and use it wherever `<remote>` appears below.

**Read `MAINTAINING.md` and `CONTRIBUTING.md` first.** They hold the rules and the review checklist; this skill only
says how to apply them.

**Draft only.** Never run `gh pr review`, `gh pr comment`, `gh pr merge` or `gh pr edit` unless the person asks you to
post or merge, in this conversation.

## Steps

1. **Who's reviewing:** `gh api user --jq .login`.
   - If they wrote the pull request, say so first: nobody approves their own. Review it as a self-check (what the
     reviewer will ask) with no "approve".
   - If it's Caity's (`caitlynmeeks`) and someone else is reviewing: they approve, and she merges.
   - If Caity is reviewing, the "Ask Caity" items are her own decision: point them out rather than "ask Caity".
2. **Context:** `gh pr view $ARGUMENTS -R caitlynmeeks/darkride-interface --json number,title,author,body,headRefName,headRepositoryOwner,commits,files,additions,deletions,reviews,comments,statusCheckRollup,mergeable,isDraft`.
   Read any issue it fixes (`gh issue view`). If there are earlier reviews, read the threads too
   (`gh api graphql` on `reviewThreads`) and check what changed since.
3. **A worktree** beside the checkout, so the person's own work isn't touched:
   - `git fetch <remote> pull/<n>/head:pr-<n> --force`
   - If `../darkride-pr-review` exists and `git -C ../darkride-pr-review status --porcelain` is empty:
     `git -C ../darkride-pr-review checkout --detach pr-<n>`. If it has changes, stop and ask.
   - Otherwise: `git worktree add --detach ../darkride-pr-review pr-<n>`.
4. **Protocol:** apply the `pr-protocol-check` skill (`.claude/skills/pr-protocol-check/SKILL.md`) to this pull
   request.
5. **Review** `git diff <remote>/main...pr-<n>` against MAINTAINING.md's checklist, reading the code around each change
   where it matters. Check claims in the description against the code. Also check whether it still merges cleanly:
   `git merge-tree --write-tree --name-only pr-<n> <remote>/main`.
6. Offer a local build only if the change is risky or untested where it matters. Don't start one unasked: a first build
   takes a long time.

## Output

1. **Summary:** what it does in two or three lines, the author, its size, checks, and whether it merges.
2. **Protocol:** one line.
3. **Findings,** most important first: `file:line`, what's wrong and why, and a suggested fix. Mark each **must fix**
   or **suggestion**.
4. **Suggested outcome:** Approve, Comment or Request changes, with one line why. A protocol change, or anything else
   on the "Ask Caity" list: Comment, mention @caitlynmeeks, and don't approve until she has said yes (unless she's
   the reviewer).
5. **A draft review,** ready to paste: friendly, thanks the contributor, short. Inline comments as `file:line` and the
   text.
6. **Next step:** post it as in MAINTAINING.md ("Posting a review on GitHub"), and merge if it's approved and nothing
   waits on Caity ("Merging"), with a house-style squash title to use. If the person says they're new to GitHub,
   spell out the clicks.
