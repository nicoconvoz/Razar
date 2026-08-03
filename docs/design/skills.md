# P15 — Skills the organism finds for itself

Stated by the author 2026-08-03, explicitly as the next step and explicitly not
to be built yet:

> The AMMC should develop **its own skills** for ordering and perceiving the
> world in its memory, without us hardcoding them. That it discovers for itself
> the skill of knowing what went at the beginning of a memory, what went in the
> middle, what went at the end. Or that it can separate into syllables. Or other
> extraordinary skills — not because we install them, but because it discovers
> them and implements them, in a continuous self-improvement system. Something
> that works and applies often stays as a **frequent skill**; something that
> works sporadically stays as an **infrequent skill**. It is a memory module
> that opens in parallel, holding skills: some sleep and others wake, the same
> system.

The reference point is explicit: this is how senses sprouted in `organismo.c`.

Status: **design only.** Nothing here is implemented.

---

## 1. What a skill is, in this system's own terms

A skill is a **way of cutting or addressing the material** — an operation over
memory that yields a new handle on it.

Both examples given are exactly that. *Beginning / middle / end* is a positional
operation over a fragment. *Syllables* is a segmentation operation. Neither adds
new material; both add a new way to reach into what is already there.

Which makes the honest starting point uncomfortable:

> **The organism already has exactly one such skill, and we installed it:
> longest-match segmentation, left to right.**

Every fragment it has ever formed came out of that single hardcoded way of
cutting. Nothing else was ever available to it. So "let it discover its own
skills" is not adding a feature on the side — it is **making plural the one
operation everything else was built on**.

That is why this is a large step rather than another piece.

---

## 2. What tells a skill it worked

The hard part, and the same question as D17 (which dreams count) and D24 (what
carries outcome quality when there is no game to win).

`organismo.c` could answer it by duelling: does the organism play *better* with
this sense? There is a winner. Razar has no game and no winner.

But it now has something it did not have a day ago:

| Signal | Already measured | What it says |
| --- | --- | --- |
| **effort** | `rz_recall.effort` | what a retrieval cost |
| **descents** | `rz_recall.descents` | how often it hit a wall and had to go around |
| **gave_up** | `rz_recall.gave_up` | whether it arrived at all |

A skill that lets a retrieval arrive where it used to give up has demonstrably
helped. A skill that gets there for less effort has demonstrably helped. That is
an outcome — modest, internal, but real and already instrumented.

**This closes D24 for this case.** The effort mechanic was built for fidelity to
how remembering feels; it turns out to be the fitness function for skills.

---

## 3. The danger, and it is not small

A system that invents its own ways to cut memory, and judges them by its own
internal number, is self-training with a self-chosen criterion. That is the §8.3
hazard again, one level up — and worse, because now the *measurement* can drift
along with the thing being measured.

Concretely: **minimising effort alone selects for obsession.** A skill that made
everything collapse onto one path would score perfectly. Retrieval would become
free, and the organism would have exactly one thought.

So skill fitness cannot be effort alone. It must be effort **against a demand
that does not move** — which is precisely the fixed external bar that
[the extraction](reference-organismo-extraction.md) §4 raised as D23, and that the
autopsy still lacks:

> A bar that rises with its subject measures nothing absolute.

Skills turn D23 from a nice-to-have into a **prerequisite**. Without a fixed
external test, self-invented skills will optimise the organism into a corner and
every panel will report that it is improving.

**D23 must be answered before P15 is built, not after.**

---

## 4. Skills must stay decomposable

The invariant from §7.5 applies with full force. If a skill is an opaque learned
function, that layer has gone statistical and the whole claim — understanding
rather than statistical response — is gone with it.

So a skill cannot be an arbitrary black box. It has to be **composed from a small
vocabulary of named primitives**, so that an autopsy can print what a skill
actually does, the way `organismo.c` prints
`despierta_cuando: ["mi_rey_en_jaque", "dama_rival_cerca_de_mi_rey"]`.

This constrains the design in a useful way: the question is not "how do we let it
learn anything", it is **"what is the smallest vocabulary of operations whose
combinations cover beginning/middle/end, syllables, and the ones we have not
thought of?"**

Getting that vocabulary right is most of the work. Too small and nothing
interesting is reachable; too large and the search space is dead.

---

## 5. What the module machinery already gives for free

The author's instinct that skills should be *the same system* is correct, and
cheap. The module layer built for memory already provides, tested:

- wake and sleep with an LRU resident set
- paging to the nest, compact on disk
- the rule that what cannot be written down is not forgotten
- earning a place by recurrence rather than by decree

The frequent / infrequent distinction maps directly onto the hot/cold pool with
**context wake-up** ([extraction](reference-organismo-extraction.md) §1.3): a
rare-but-key skill stays alive not by being strong but by being *contextually
reachable*. That mechanism is already extracted and understood; it has simply not
been implemented in Razar yet.

---

## 6. Open decisions

| # | Decision | Notes |
| --- | --- | --- |
| D23 | Razar's fixed external bar | **Now a prerequisite.** Skills cannot be selected without it |
| D26 | The primitive vocabulary a skill is composed from | Most of the work. Must cover the stated examples and stay small |
| D27 | How a skill is proposed | Mutation of an existing one, as in `organismo.c`? Composition of primitives? Something driven by observed failure? |
| D28 | What a skill is allowed to change | Only how material is segmented at ingestion? Also how a cue is matched? Also how a walk chooses? Each is a different blast radius |
| D29 | Whether a rejected skill is remembered | `organismo.c` keeps neutral senses rather than pruning them; a skill that failed once may suit a later kind of material |
| D30 | Whether skills apply retroactively | A new segmentation does not re-cut what was already eaten. Does the organism re-read, or does it only apply skills going forward? |

D30 deserves flagging: it is the one that decides whether skill discovery is
cheap or ruinously expensive. Re-reading everything on every new skill does not
survive the standing constraint.
