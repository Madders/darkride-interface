---
name: release-notes
description: Draft Darkride release notes from the pull requests merged since the last release tag, in the shape RELEASING.md describes, for Caity to use. Use as /release-notes, or /release-notes <from-tag> [to-ref].
argument-hint: "[from-tag] [to-ref]"
---

# Draft release notes

The repository is `caitlynmeeks/darkride-interface`. **Releases, tags and publishing are Caity's.** This only drafts
text: never run `git tag`, `gh release create` or `gh release edit`. Read the notes section of `RELEASING.md` first.

## Steps

1. `git fetch --tags <remote>` (the remote for the repository, from `git remote -v`).
2. **The range:** from the argument, or from the latest release
   (`gh release list -R caitlynmeeks/darkride-interface --limit 1`) to `<remote>/main`.
3. **What's in it:**
   - `git log --first-parent --format='%h %s' <from>..<to>`;
   - the merged pull requests behind those commits:
     `gh pr list -R caitlynmeeks/darkride-interface --state merged --limit 100 --json number,title,body,author,mergedAt,mergeCommit`;
   - commits made without a pull request count too.
4. **At the top, for Caity:**
   - any **protocol change** in the range (apply `.claude/skills/pr-protocol-check/SKILL.md`): the release must go out
     with the live servers' update;
   - server-side changes that need a deploy;
   - dependency, Qt or build changes.
5. **The notes,** in RELEASING.md's sections: what's new (a player's view first), compatibility (whether the protocol
   changed), known issues, and thanks to each contributor by GitHub name, with first-time contributors called out.
   Plain sentences about behaviour, each ending with its pull request number. Leave the downloads and their SHA-256 as
   placeholders, since the files don't exist yet.

## Output

The draft in a fenced Markdown block, ready to paste or send to Caity.
