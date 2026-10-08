# Razar

**Razar** = *Razonamiento Artificial* (Artificial Reasoning).

## Project Status

**A working organism, in C11.** Stack chosen 2026-08-03 (D9): C11, gcc, no
dependencies beyond libm. ~9,000 lines across `src/razar.c`, `src/razar.h`,
`src/main.c`; 887 checks in `tests/test_razar.c`.

It eats raw bytes, composes the pieces that recur into fragments, forgets
without deleting, recalls, predicts before it reads, reports its interest,
writes, chooses which dream it needs, grows its own senses, learns rules, and
holds a running conversation with itself about whether it was right.

What each piece is and what it measured lives in `docs/design/`. The decision
table in [memory-concept-map-architecture.md](docs/design/memory-concept-map-architecture.md)
§6 says what is resolved, what is built, and which document carries the rest.

### Hard rule: no new dependencies

The organism takes nothing beyond libm, and its tooling (`tools/`) takes nothing
beyond the Python standard library. A dependency is a decision for the user, not
a convenience.

## Working Agreements

### Scope boundary

All work — reading, searching, and writing — stays inside `E:/proyectos/Razar`.
Do not read from or reference other projects on this machine to source
conventions, patterns, or examples. If a file this project needs does not exist
here, create it here.

### Who drives

The user designs the path. The project moves from the most basic to the most
complex, at the pace the user sets.

The vision-framing phase (2026-08-02) is over and the project is being built,
but the division of labour has not changed: **the user designs, and records the
direction; implementation follows it.** When the user describes an idea, capture
it in `docs/design/` before or alongside building it, and mark what stays open.

**Never declare what the organism should discover.** The master rule since
2026-08-03: build the structure under which a behaviour can emerge, never the
behaviour. A dictionary, a delimiter list, or a hand-written detector enters as a
*bar that grades*, never as a foundation.

**Every measurement carries its baseline.** A number without the chance figure of
the same place is not a reading. This project has shipped six measurements that
said the opposite of what they meant; each one was a figure read in absolute.

### Strict TDD

Strict TDD mode is enabled globally and applies to this project.

The runner is `tests/test_razar.c` on the minimal harness in `tests/harness.h`
(`TEST`, `RUN`, `CHECK`, `CHECK_EQ_U`, `CHECK_NEAR`), run by `make test`.

A passing test is not enough: check it passes **for the reason you think**. Twice
in this project a test passed because a different term happened to refuse the
same input. Printing the raw numbers once is usually enough to tell.

Cycle:

1. Write the failing test first. Run it. Confirm it fails for the expected reason.
2. Write the minimum code to make it pass.
3. Refactor with the suite green.

### Spec-Driven Development

SDD is initialized for this project with the **engram** artifact store.

| Phase | Skill | Purpose |
| --- | --- | --- |
| Explore | `sdd-explore` | Investigate before committing to a change |
| Propose | `sdd-new` | Define intent, scope, approach |
| Spec | `sdd-spec` | Requirements and scenarios |
| Design | `sdd-design` | Architecture decisions |
| Tasks | `sdd-tasks` | Ordered implementation checklist |
| Apply | `sdd-apply` | Write the code |
| Verify | `sdd-verify` | Validate against the contract |
| Archive | `sdd-archive` | Merge specs, close the cycle |

`sdd-explore` is not useful on an empty repository. The meaningful entry point
right now is `sdd-new`.

Known limitation: SDD phase sub-agents do not have Engram `mem_*` tools in their
executor tool set. They degrade to writing fallback files in the session
scratchpad. The orchestrator must perform the `mem_save` calls itself and should
not report persistence as complete until it has.

### Review

`gentle-ai` v2.2.4 is installed. Receipt-driven development is **on** (by default;
no explicit global or clone-local override is set).

Review authority is read with:

```bash
gentle-ai review status --cwd E:/proyectos/Razar
```

The user owns this switch. Do not enable or disable it on their behalf.

## Commands

**C11, gcc, no dependencies beyond libm.** Language chosen 2026-08-03 (D9).

| Purpose | Command |
| --- | --- |
| Build | `make` |
| Test | `make test` |
| Clean | `make clean` |
| Feed the organism | `./razar feed [path...]` (default: `feed/`) |
| Examine it | `./razar autopsy` |
| Let it dream | `./razar sleep [seed]` |
| Grow its senses | `./razar senses [rounds] [path]` |
| Dump its whole state | `./razar dash` |
| Hear it think | `./razar think [path] [bite]` |
| Its legos, their kinds, a game | `./razar legos [path] [kinds]` |
| Ask it, and let it ask why | `./razar ask <word> [relation] [path]` |
| See it working | `python tools/live.py` then http://localhost:7331 |
| See a snapshot | [docs/vitals.html](docs/vitals.html) — paste `razar dash` output into it |

Warnings are errors: `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`.

### On this machine (Windows)

The toolchain is MSYS2 at `C:/msys64` — gcc 16.1.0 in `ucrt64`, make in `usr/bin`.
Neither is on the PATH by default.

**`make` must run inside MSYS2's own bash, not Git Bash.** Mixing MSYS2's
`make.exe` with Git Bash's `sh.exe` loads two different MSYS runtimes into one
process and fails with `Cannot create temporary file in C:\Windows\`. Invoking
gcc directly from Git Bash works fine; only `make` collides.

```bash
"C:/msys64/usr/bin/bash.exe" -lc 'export PATH=/ucrt64/bin:/usr/bin:$PATH && make -C /e/proyectos/Razar test'
```

The built `razar.exe` runs from any shell — but **PowerShell needs the `.\`
prefix** (`.\razar dash`), because it does not run commands from the current
directory without it. In bash `./razar` works as usual.

`make -C <dir>` rather than `cd`, because the MSYS2 login shell starts in
`/home/Nico`. The built `razar.exe` is a native binary and runs from any shell.

## Layout

```
src/        The organism: razar.h, razar.c; lego.h, lego.c (legos, kinds,
            curiosity); main.c (the CLI)
tests/      test_razar.c and the harness it runs on
tools/      live.py — serves docs/live.html and drives the real binary
docs/       design/ (what each piece is and what it measured),
            live.html (the live panel), vitals.html (a pasteable snapshot)
feed/       Intake (P13). `razar feed` devours what is dropped here.
            Contents are gitignored — some of it carries third-party copyright.
nido/       The living organism: mind.dat and modules/. Gitignored, regenerable.
autopsies/  The study record (P12), written by `razar autopsy`.
.atl/       Skill registry (indexes user-level skills; no project skills yet)
.claude/    Claude Code project settings and launch.json for the live panel
```

## Conventions

Derived from the code as it stands, not guessed.

- **Public API is `rz_`-prefixed** and declared in `src/razar.h`, each block
  opened by a comment saying what the piece is and *why* it is shaped that way.
- **Comments explain why, not what**, and they keep the reason a thing was
  changed — including what was tried first and failed. That history is the
  record of which measurements lied.
- **A field added to `rz_mind` is written to `rz_save`/`rz_load` in the same
  movement, and checked by the round-trip test.** Forgotten five times; the
  symptom is always an instrument reporting a zero that looks like a result.
  When the file *format* changes, bump `RZ_MIND_VER` by hand.
- **A sense glances; it never digs.** Senses read only what is already resident
  (`*_at_hand` in `razar.c`) — waking modules from disk on every look cost seven
  disk loads per reading. Note the asymmetry: the dump and the external bar *do*
  page modules in, and paging applies owed decay, so they are not perfectly
  side-effect free.
- **The binary speaks English; the live panel translates.** The user reads the
  panel in Spanish, and the translation lives in `docs/live.html`, not in C.

Language for generated artifacts: **English** for code, comments, identifiers,
UI copy, documentation, and commit messages, unless the user explicitly requests
otherwise for a specific artifact.

Commits: conventional commits. No AI attribution or `Co-Authored-By` trailers.
