# Maintaining Darkride

This page is for the people who review and merge pull requests and sort issues. [CONTRIBUTING.md](CONTRIBUTING.md)
covers the other side: how a change gets in, and what needs Caity's say first. [RELEASING.md](RELEASING.md) covers
releases, which stay with Caity.

## Who does what

- **Caity** ([@caitlynmeeks](https://github.com/caitlynmeeks)) owns the project: the final say, releases and tags,
  the live worlds' servers, and the signing keys. She approves the other maintainers' own pull requests.
- **Maintainers** review pull requests, approve and merge other people's, and sort issues and labels.
- Nobody approves or merges their own pull request. Caity's own pull requests are approved by a maintainer, and she
  merges them.

## Reviewing a pull request

Read the description and any issue it fixes, then the change itself. Check:

1. **It does what the title says, and only that.** Two unrelated changes are easier to review, test and undo as two
   pull requests.
2. **Network protocol.** See [below](#network-protocol). A protocol change needs Caity's OK before it merges, and
   "(protocol change)" in its title.
3. **The rest of the "Ask Caity" list** in CONTRIBUTING.md: new dependencies or anything the build downloads, and
   sign-in and accounts.
4. **Build impact.** Changes to `cmake/ports/`, `hifi_vcpkg.py`, `prebuild.py`, a `CMakeLists.txt`, the Qt version
   or the presets make every platform rebuild its dependencies, and can break a platform the author didn't try.
5. **Correctness.** Crashes, null pointers and object lifetimes, threads (Qt signals across threads, the render
   thread), and `#ifdef`s for one platform that break another (Windows, macOS, Linux, Android).
6. **Anything that handles what other people send.** Code that loads URLs, runs scripts, opens files, or reads data
   from a world or another visitor deserves a slower read. If you find a security problem, don't describe it in
   public: tell Caity privately.
7. **How it was tested,** and on which system. If it wasn't tested where it matters, say so or try it yourself.
8. **Leftovers:** debug logging, commented-out code, unrelated reformatting.
9. **The commit title** reads as one sentence about the behaviour: "FBX: a node with fewer properties than expected
   reads as empty instead of crashing the client". You can fix it when you merge.

Be kind and specific. Several contributors are new to the project: thank them, say what's good, and mark which
comments must be fixed before merging and which are only suggestions.

### Posting a review on GitHub

1. Open the pull request's **Files changed** tab.
2. For a comment on a line, hover over the line number, click **+**, write it and click **Start a review** (the first
   one) or **Add review comment** (the rest). Nobody sees them until you submit.
3. Click **Review changes**, write a summary, and choose:
   - **Comment** for questions and suggestions, with no decision;
   - **Approve** when it's ready to merge, which counts as the one required approval;
   - **Request changes** only for real problems. It blocks merging until the author fixes them and you look again.
4. Click **Submit review**.
5. When an author answers or fixes a comment, click **Resolve conversation** under it.

A new push to the pull request clears earlier approvals. Approve again once you've looked at what changed.

## Merging

Merge someone else's pull request once it has an approval, its checks pass, and nothing on the "Ask Caity" list is
waiting for her.

1. On the **Conversation** tab, click the **▾** beside the green button and choose **Squash and merge**. The repository
   defaults to a merge commit; GitHub remembers your choice.
2. Edit the title into one sentence about the behaviour, ending with the pull request's number: "File > Login opens
   the login dialog on a fresh start (#4)".
3. If it fixes an issue and the description doesn't say so, add "Fixes #123" to the commit description.
4. Confirm. Don't delete the branch if it lives on the author's fork.

## Network protocol

The protocol signature is a hash of how many packet types there are and each one's version
(`libraries/networking/src/udt/PacketHeaders.cpp`). A client and a server with different signatures can't talk, so
a protocol change can't join the live worlds until their servers are updated.

**A protocol change** (needs Caity's OK):
- `libraries/networking/src/udt/PacketHeaders.h` and `.cpp`: a new, removed or reordered packet type, or a version
  changed.
- Anything that changes what a packet carries, with or without a version bump. Usual places:
  - `libraries/avatars/src/AvatarData.*`, `AvatarTraits.h`, `ClientTraitsHandler.*`: avatar data and traits;
  - `libraries/entities/src/EntityItemProperties*`, `EntityPropertyFlags.h`, and an entity type's
    `appendSubclassData` and `readEntitySubclassDataFromBuffer`: entity properties;
  - `libraries/audio/src/`: audio packet framing and codecs;
  - `NLPacket*`, `NodeType.h`, and the domain handshake in `NodeList.cpp`, `LimitedNodeList.cpp`, `DomainHandler.cpp`
    and `domain-server/src/DomainGatekeeper*`;
  - the messages mixer, and `ice-server/`.

A change to what a packet carries **without** a version bump is the dangerous case: the signature stays the same, old
and new builds still connect, and each silently misreads the other. Ask for the bump.

**Server-side only** (no protocol change, but it does nothing until the live servers are updated):
`assignment-client/src/` and the rest of `domain-server/src/`. Say so in the review, so Caity knows a deploy is
needed.

A path match isn't a verdict: a comment or a log line in `PacketHeaders.cpp` changes nothing. Read the change.

## Issues

- **Labels:** use the ones that exist (`bug`, `enhancement`, `documentation`, `question`, `duplicate`, `invalid`,
  `wontfix`, `good first issue`, `help wanted`, `accessibility`). Ask before adding new ones.
- **Not enough to go on?** Ask for the system, the version (Help > About), the steps, what happened and what was
  expected, and the log.
- **A duplicate:** comment "Duplicate of #123" and close it as not planned. GitHub links the two.
- **Fixed by a pull request:** "Fixes #123" in the pull request closes the issue when it merges.
- `good first issue` is for something small, well understood and in one area.
- **A security problem** reported in public: don't discuss the details there. Tell Caity privately.
- Anything about the live worlds, accounts or the metaverse service is Caity's.

## Keeping your own pull request up to date

When `main` moves on, merge it into your branch (`git merge upstream/main`) and push. That needs no force-push and
keeps the review comments in place, and the squash merge flattens it anyway.

## Claude Code skills

The repository includes [Claude Code](https://claude.com/claude-code) skills in `.claude/skills/` that draft each of
these steps. Open the repository in Claude Code and type the command. They read this page and CONTRIBUTING.md for
the rules, so the rules live in one place. **They draft, and you post:** they never comment, review, label or merge
on GitHub unless you tell them to.

| Command | What it does |
|---|---|
| `/repo-status` | What's new, what's waiting on you, what's waiting on Caity, and what's waiting on authors |
| `/pr-review 123` | A first review of a pull request in a separate worktree, with a protocol check and a draft review to post |
| `/pr-protocol-check 123` | Whether a pull request (or a branch) changes the network protocol, and the lines that decide it |
| `/issue-triage` | Suggested labels, duplicates, linked pull requests and replies for open issues |
| `/ci-check` | Which platform's build failed, at which step, the first real error, and whose problem it is |
| `/release-notes` | Draft notes from the pull requests merged since the last tag, for Caity |
