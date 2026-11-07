# Razar

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.23874174.svg)](https://doi.org/10.5281/zenodo.23874174)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)

**Razar** = *Razonamiento Artificial*.
📖 **[Carta del autor](CARTA-DEL-AUTOR.md)** — la otra mitad: el diario espiritual, en audio y con licencia libre.


An organism in C11. It eats raw bytes, composes the pieces that recur into
fragments, forgets without deleting, recalls, predicts before it reads, reports
its interest, writes, chooses which dream it needs, grows its own senses, learns
rules, and holds a running conversation with itself about whether it was right.

No dependency beyond libm. `make && make test`.

---

## The rule the whole thing is built on

> **Never declare what the organism should discover.**
> Build the structure under which a behaviour can emerge, never the behaviour.

A dictionary, a delimiter list, a hand-written detector — each enters as a *bar
that grades*, never as a foundation. If a result is interesting, the first
question is whether the result was put there by hand.

Its companion rule, learned the hard way:

> **Every measurement carries its baseline.**
> A number without the chance figure of the same place is not a reading.

This project has shipped six measurements that said the opposite of what they
meant. Each one was a figure read in absolute.

---

## One result, so the rule is not just a slogan

Razar was never told the word *no*. It was given five properties of negation,
each measurable against the text itself, nothing named:

1. **Take it out and what is left is still said.** "el perro no ladra" / "el
   perro ladra". "y" cannot be taken out: "pan y vino" / "pan vino".
2. **When it appears, what follows changes.**
3. **The widest door**: the relation is of three — subject, switch, verb.
4. **Mostly the door**: a connector joins other things too; the switch is
   mostly the door.
5. **Not inside another door**: *la*, *lo*, *me* stand inside the no's door;
   *ya* and *y* stand outside it.

Reading the New Testament with its own pieces, it found exactly one switch:
***no***.

Then every place said with a switch in front is kept as a **closed door** — the
same place, said denied. A denial of what is mostly said asks *why this time
no?*; an affirmation of what is mostly denied asks *why this time yes?* The rule
is the majority of the two counts, and nothing else.

38,442 closed doors. Among the questions it raised:

> *"Jesús le respondió"* — said 26 times, denied **once**. Why this time no?

It does not know what Jesus is, nor what a trial is. It counted twenty-six yeses
and one no, and asked.

**And what failed is on the record too:** on Quiroga (24,475 words) it finds no
switch at all — one novel is not enough to tell a door from a connector. Four
other measures were tried and dropped. See
[`docs/design/neurons.md`](docs/design/neurons.md).

---

## Build and run

```bash
make          # build
make test     # 1,270 checks
./razar feed  # devour whatever is in feed/
./razar dash  # dump its whole state
```

Warnings are errors: `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`.

**On Windows**, `make` must run inside MSYS2's own bash — mixing MSYS2's `make`
with Git Bash's `sh` loads two MSYS runtimes into one process and fails:

```bash
"C:/msys64/usr/bin/bash.exe" -lc 'export PATH=/ucrt64/bin:/usr/bin:$PATH && make -C /path/to/Razar test'
```

The built binary is native and runs from any shell.

---

## What you can ask it

| | |
| --- | --- |
| `./razar feed [path...]` | eat what is dropped in `feed/` |
| `./razar autopsy` | examine it |
| `./razar sleep [seed]` | let it dream |
| `./razar senses [rounds] [path]` | grow its senses |
| `./razar think [path] [bite]` | hear it think |
| `./razar loom [path]` | weave meaning while reading, and what it foresees |
| `./razar why [all]` | the questions its closed doors raised |
| `./razar meaning <word> [all]` | open a word: its places, and which book brought them |
| `./razar speak <word> [all]` | sentences out of its relations |
| `python tools/live.py` | watch it work, at http://localhost:7331 |

There are about twenty more. `./razar` with no argument lists them.

---

## The state of this, honestly

**It works and it is rough.** 13,252 lines of C across `src/`, 5,536 of tests,
1,270 checks green. What each piece is and what it measured lives in
[`docs/design/`](docs/design/) — those notes are the careful part of this
repository, and they are where to look first.

What is NOT here yet: a tidy public API, packaging, a tutorial, and any claim
that this is a general system. It is a laboratory. Pieces land when a
measurement says they should, and several are built far enough to be measured
and no further. The decision table in
[`docs/design/memory-concept-map-architecture.md`](docs/design/memory-concept-map-architecture.md)
§6 says what is resolved, what is built, and which document carries the rest.

**No claim is made about understanding, meaning or awareness.** What is shown is
what was measured, with its baseline, and what failed is written next to it.

---

## Layout

```
src/        the organism: razar.{h,c}, lego.{h,c} (legos, kinds, curiosity),
            main.c (the CLI)
tests/      test_razar.c and the minimal harness it runs on
tools/      live.py — serves docs/live.html and drives the real binary
docs/       design/ (what each piece is and what it measured),
            live.html (the live panel), vitals.html (a pasteable snapshot)
feed/       intake. Gitignored — some of it carries third-party copyright.
nido/       the living organism: mind.dat and modules/. Gitignored, regenerable.
autopsies/  the study record, written by `razar autopsy`.
```

---

## Licenses

| What | License |
| --- | --- |
| The code | [AGPL-3.0](LICENSE) |
| The content (`docs/`, `autopsies/`) | [CC BY 4.0](LICENSE-CONTENT.txt) |
| Closed use | [commercial](COMMERCIAL-LICENSE.md) |

The reasoning is in [LICENSES.md](LICENSES.md); the summary in
[NOTICE](NOTICE). `feed/` is covered by none of them — it is not ours to
license.

## Citing

```
Jesús Nicolás Astorga and RESOURCES OPEN DOORS S.A.S,
"Razar — Razonamiento Artificial", 2026.
https://doi.org/10.5281/zenodo.23874174
Code under AGPL-3.0; documentation under CC BY 4.0.
```

That DOI is the **concept** DOI: it always resolves to the latest version.
To cite this exact release, use `10.5281/zenodo.23874175`.
[CITATION.cff](CITATION.cff) carries the same, machine-readable.
