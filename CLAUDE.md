# Razar

**Razar** = *Razonamiento Artificial* (Artificial Reasoning).

## Project Status

**Bootstrap. No stack chosen yet.**

As of 2026-08-02 this repository contains no source code, no package manifest, no
build configuration, and no tests. The git repository is initialized on branch
`master` with **zero commits**.

### Hard rule: do not assume a stack

There is no `package.json`, `go.mod`, `pyproject.toml`, `Cargo.toml`, or `Makefile`.
Do **not** infer a language, framework, or architecture pattern from the project
name, from the domain, or from what "usually" fits an AI reasoning project.
Scaffolding an unrequested stack is a defect, not a head start.

The stack is decided by the user, recorded here, and only then implemented.

## Working Agreements

### Scope boundary

All work — reading, searching, and writing — stays inside `E:/proyectos/Razar`.
Do not read from or reference other projects on this machine to source
conventions, patterns, or examples. If a file this project needs does not exist
here, create it here.

### Who drives

The user designs the path. The project moves from the most basic to the most
complex, at the pace the user sets.

As of 2026-08-02 the project is in the **vision framing phase**: the user is
describing direction in broad strokes and **nothing is to be programmed yet**.
While this phase lasts:

- Do not request a testable spec, propose a stack, or scaffold anything.
- Do not end every reply with a question. Listen and record.
- Capture each concept as it lands, organize it, and mark what stays open
  without forcing it closed.

The falsifiability constraint below is recorded and still applies — but it
applies when implementation starts, not during framing. Do not re-raise it.

### Strict TDD

Strict TDD mode is enabled globally and applies to this project.

**No test runner exists yet.** Until one does, Strict TDD has nothing to execute
against. Therefore: the first implementation task after the stack is chosen must
establish the test runner and a passing baseline, before any production code is
written.

Cycle once a runner exists:

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

The built `razar.exe` runs from any shell — but **PowerShell needs `.
azar`**,
because it does not run commands from the current directory without it. In bash
`./razar` works as usual.

`make -C <dir>` rather than `cd`, because the MSYS2 login shell starts in
`/home/Nico`. The built `razar.exe` is a native binary and runs from any shell.

## Layout

```
.atl/       Skill registry (indexes user-level skills; no project skills yet)
.claude/    Claude Code project settings
docs/       Design documents — see docs/design/
feed/       Intake folder (P13). Drop material here to be devoured.
            Contents are gitignored. Nothing consumes it yet.
autopsies/  The study record (P12). One directory per session, kept forever.
            Nothing writes here yet.
```

## Conventions

Not yet established — there is no source code to derive them from. Record them
here as they are decided, not as they are guessed.

Language for generated artifacts: **English** for code, comments, identifiers,
UI copy, documentation, and commit messages, unless the user explicitly requests
otherwise for a specific artifact.

Commits: conventional commits. No AI attribution or `Co-Authored-By` trailers.
