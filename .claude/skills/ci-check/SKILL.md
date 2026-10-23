---
name: ci-check
description: Read GitHub Actions results for Darkride - which platform failed, at which step, the first real error, and whether it's the pull request's fault, a platform difference, the build setup or a flaky runner. Use as /ci-check, /ci-check <PR number>, or /ci-check <run id or URL>.
argument-hint: "[PR number | run id | run URL]"
---

# Check CI results

The repository is `caitlynmeeks/darkride-interface`. A fork's runs can be checked the same way with
`-R <owner>/darkride-interface`. Read-only.

## Which runs

- No argument: `gh run list -R caitlynmeeks/darkride-interface --limit 20 --json databaseId,workflowName,headBranch,event,status,conclusion,createdAt,displayTitle,url`.
- A pull request: `gh pr checks <n> -R caitlynmeeks/darkride-interface`, then the runs behind it.
- A run id or URL: that run.

## For each failed run

1. `gh run view <id> --json jobs`: which jobs (platforms) failed, and at which step.
2. `gh run view <id> --log-failed`. Find the **first** real error, not the ones it caused:
   - MSVC: `error C`, `error LNK`, `fatal error`;
   - CMake and vcpkg: `CMake Error`, `Error: Building package`, and the log path vcpkg names;
   - clang and gcc: `error:`;
   - out of space or memory: `No space left on device`, `C1060`, a compiler exiting with -1, "The runner has received
     a shutdown signal";
   - timeouts (the 6-hour job limit).
3. **Whose problem:**
   - **The pull request's:** it fails here and not on `main`. Draft a friendly comment for the author.
   - **A platform difference:** builds on one system and not another (`#ifdef`s, case-sensitive includes, compiler
     differences).
   - **The build setup's:** a runner image changed, a download is gone, a cache missed, the disk filled.
   - **A flaky runner:** the same step passes on a re-run. On macOS, `hdiutil: create failed - Resource busy` is a known
     one.
4. Compare with the last good run of the same workflow: the commit, the runner image (its version is in the "Set up
   job" log) and the cache key.

## Report

A table per run: `job | result | step | first error`. Then for each failure: whose problem, the evidence (a few log
lines) and a suggested fix.

Re-running a job (`gh run rerun <id> --failed`) uses the repository's Actions minutes: ask first. Never touch secrets,
signing or release jobs: those are Caity's.
