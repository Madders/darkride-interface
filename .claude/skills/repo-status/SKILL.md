---
name: repo-status
description: A summary of the Darkride repository for a maintainer - what's waiting on you, on Caity and on authors, what merged and was released recently, your own pull requests' state, and CI results. Use as /repo-status, or /repo-status <days>.
argument-hint: "[days, default 3]"
---

# Repository status

The repository is `caitlynmeeks/darkride-interface`. Read-only, apart from `git fetch`. Look back over the number of
days given, or 3.

## Gather (in parallel)

- Who's asking: `gh api user --jq .login`.
- `gh pr list -R caitlynmeeks/darkride-interface --state open --json number,title,author,isDraft,createdAt,updatedAt,reviewDecision,reviews,statusCheckRollup,mergeable,labels,headRefName`
- `gh pr list -R caitlynmeeks/darkride-interface --state merged --limit 20 --json number,title,author,mergedAt,mergedBy`
- `gh issue list -R caitlynmeeks/darkride-interface --state open --json number,title,labels,author,createdAt,updatedAt,comments`
- `gh run list -R caitlynmeeks/darkride-interface --limit 20`
- `gh release list -R caitlynmeeks/darkride-interface --limit 3`
- `gh api notifications --jq '.[]|select(.repository.full_name=="caitlynmeeks/darkride-interface")|{reason,title:.subject.title,type:.subject.type}'`
- For the person's own open pull requests, from their checkout: `git fetch --multiple <remotes>`, then how far each
  branch is behind main (`git rev-list --count <their-branch>..<remote>/main`) and whether it conflicts.

## Report (short)

1. **Waiting on you:** other people's pull requests with no review (oldest first), review requests and mentions,
   unlabelled issues, failed checks on others' pull requests. Suggest the command: `/pr-review <n>` or
   `/issue-triage`.
   - For Caity, "waiting on you" also covers the maintainers' own pull requests (she approves those) and anything on
     the "Ask Caity" list.
2. **Waiting on Caity** (for anyone else): their own pull requests, and anything on the "Ask Caity" list.
3. **Waiting on authors:** changes requested, questions unanswered.
4. **Recently:** merged pull requests, new issues, new releases. People building it themselves should use the latest
   release tag, not `main`.
5. **Your pull requests:** behind `main` or conflicting.
6. **CI:** the latest run of each workflow, and a one-line reason for each failure (`gh run view <id> --log-failed`).
