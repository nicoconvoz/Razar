# Dreaming as an analogy factory (P19–P24)

Author's design, 2026-08-03. Recorded before implementation.

> Day: *what happened?*
> Dream: *what else has the same shape?*
> Waking: *does reality confirm the resemblance?*

---

## 0. The defect this exists to fix

The dream, as built this morning, **cannot propose anything that does not
already exist**. Verified in the code, not inferred:

- `pick_next_ex` (`src/razar.c:1192`) can only ever return `M->pool[e].to` — an
  edge that is already in the graph.
- `edge_slot` (`src/razar.c:401`), the only function that creates an edge, has
  exactly two callers: `ingest_fragments` (`:707`) and `frag_birth` (`:611`).
  **Both awake.**

So every edge the dream can walk with no waking traffic behind it was
manufactured by `frag_birth` copying its right parent's exits at weight 1. A
"discovery" credited to dreaming would in fact be a fact about the inheritance
rule.

Two further defects found by measurement, both real:

- **Among cold edges the dream tosses a coin.** `edge_pull` is
  `pow(w, 1/T)`, and `pow(1.0, anything) == 1.0`. Every weight-1 candidate
  carries identical sampling mass, so which one is taken is decided by
  `xorshift` alone.
- **The residue is numerically inert.** `spare = 0.5*merit + 0.5*(attention /
  RZ_ATTENTION_MAX)` with `RZ_ATTENTION_MAX = 1000` and marks of 4 and 8. The
  attention term contributes ~0.004 against merit's 0.5.

§8.6 — *a connection produced during a dream that later proves useful* — is
**unreachable** by a walk that cannot leave the edge set. That is what this
document repairs.

---

## 1. The idea

The dream stops looking at *what things are* and looks at *what shape they have*.
Two things whose relations have the same architecture are candidates for an
analogy, however unlike their content.

The dream does not assert the analogy. It files it as a **hypothesis**, weak,
outside memory. Waking experience is what kills it or graduates it.

An analogy jump goes from A to B where **B is not an out-edge of A**. That is,
by construction, a move that leaves the edge set — the thing the current dream
cannot do.

---

## 2. The trap this design refuses

The natural way to give a concept a "form" is to give it typed relations:

```
Dog                     Robot
  is_a     = animal       is_a     = machine
  can      = move         can      = move
  acts_on  = human        acts_on  = human
```

**Razar has one relation: "followed by."** There is no `is_a`, no `can`, no
`produces`. It has never been told what a predicate is — it eats raw bytes.

Handing it that vocabulary would hand it the concepts from outside, and the only
claim that makes this project worth building — *that it finds its own way of
seeing* — dies on the spot.

**D31 — The form of a concept is the company it keeps, derived from traffic,
never declared.** Two fragments that lead to the same *kinds* of things share
structure even when they share no byte. That is the same idea as the typed
signature, obtained from the graph the organism built by itself.

---

## 3. P19 — The structural signature

### What exists

`rz_shape_of` (`src/razar.c:843`) already answers *what role does this play*:
branch × span × concentration, **64 classes, identity discarded**. Two fragments
with nothing in common can carry the same shape. That is the point.

It is not enough for similarity. Three numbers in 64 buckets can say "same role";
it cannot say "85% alike".

### What to build

For a fragment A, its signature is **the distribution of shapes it leads to**:

```
sig(A)[s] = Σ  weight(A → t)   for every out-edge t with rz_shape_of(t) == s
            ─────────────────
            Σ  weight(A → *)
```

A 64-bucket normalized histogram. Identity is gone twice over: once because
shapes discard it, once because the histogram discards which particular exits
produced it.

**Similarity** is the cosine of two signatures, in `[0, 1]`.

### The floors, and why they are not optional

- A fragment with one out-edge has a signature that is a single spike. Two such
  fragments pointing at the same shape score 1.0, which means nothing.
  **`RZ_SIG_MIN`**: only fragments with at least N distinct out-edges get a
  signature. N to be measured, not guessed.
- **Two random fragments already have a nonzero expected similarity.** Before
  any threshold is fixed, the distribution of similarity over random pairs must
  be measured and printed. The bar goes where the data separates — the same
  discipline that placed `RZ_OVERWHELMED` after two failed attempts.

### What is missing and known

**Only the forward half of the neighborhood exists.** `rz_edge` is
`{to, weight, next}` — there is no reverse index, so *what leads into A* cannot
be asked. The signature is therefore "what it leads to" alone. The backward half
is a later piece and doubles the evidence; it is not a blocker.

---

## 4. P20 — Tension: the seed

A dream that starts anywhere is a walk. A dream that starts at a contradiction is
an investigation.

The author's example:

```
Birds fly.  Bats fly.  Mammals do not fly.
```

That is not noise. It is a **rule that works everywhere except here**.

### Razar already computes this and throws it away

At `src/razar.c:688`, before every step, the shape layer is asked to call the
shot. Hits and misses are counted per shape class. **Where the miss happened is
discarded.**

### What to build

Keep it. Per fragment, accumulate `surprises`.

**D32 — Tension is a confident rule failing, not an unpredictable place.** A
fragment is not tense because it is chaotic; it is tense because the abstraction
that works elsewhere breaks *here*. So a miss only counts as tension when the
predicting shape's merit (`rz_shape_merit`) is above the transfer bar. A shape
that is wrong everywhere teaches nothing by being wrong once more.

This is what turns the walk into a question: *why does the rule fail here?*

`dream_seed_frag` gains a fourth mode, and tension outranks the existing three
when there is any: the night's business is the exception.

---

## 5. P21 — The jump

At a tension point, instead of taking an out-edge, the dream **jumps**:

```
find B maximizing  similarity(sig(A), sig(B))
subject to   B is not an out-edge of A
             B != A
             degree(B) >= RZ_SIG_MIN
```

Bounded: comparing against every fragment is O(n) with paging behind it, so the
search draws a bounded candidate pool of K fragments. **K is reported.** A
bounded search that does not say it is bounded is a lie by omission.

Jumps are rare and deliberate — they happen at tension, not at every step. A
handful per night.

The dream continues walking from B. What it walks after the jump is ordinary
traversal; the jump itself is the new move.

---

## 6. P22 — The hypothesis

**D33 — A hypothesis never enters the plastic layer.** Not at weight 1, not at
0.08, not at all. The moment a dream can write an edge, the dream is training
itself, and §8.3 collapse is back. The graph stays a record of what actually
happened.

A hypothesis lives in its own file, in the `rz_ledger` / `nido/interest.dat`
pattern already at `src/razar.c:1080`:

| Field | Why |
| --- | --- |
| `from_fp`, `to_fp` | content fingerprints (`rz_fingerprint`, `:1034`), **not** `(module, slot)` ids — ids remap when a module is evicted and reseeded |
| `similarity` | what the dream saw |
| `born_at` | in `m->frag_steps`, the clock decay already uses |
| `expires_at` | a fixed exposure budget, same for both arms |
| `asked` | times waking material actually stood at `from` |
| `hit` | times the consequence held |
| `arm` | analogy, or control |

Own file, own version word, missing-is-fine. It never touches `rz_save` — the
place this project has broken the same way three times.

---

## 7. P23 — Consequences

An analogy that stops at "A resembles B" predicts nothing and cannot be wrong.
The author's step 5 is what makes it falsifiable.

If `A ≈ B`, and A leads to X, then the hypothesis proposes:

> **B leads to something shaped like X.**

At the shape level, not the identity level — B leading to the literal X would be
a coincidence about content. *Something shaped like X* is the transferable claim,
and the shape layer is already the level where transfer demonstrably works
(18.18% rescue where the specific layer is blind, 11.6× chance).

Each consequence is one row in the ledger. That is the unit that gets scored.

---

## 8. P24 — Waking judgment

**D34 — The dream answers before reality answers.** Author's rule. A hypothesis
that is only checked after the fact is audited, not tested. So the hypothesis is
consulted at prediction time: when waking material stands at B and the organism
must say what follows, the dream gets a **vote** — and it is scored separately
from the waking graph's own answer.

Voting is not writing. Nothing is strengthened. The firewall of D33 holds.

Three tiers, which is `organismo.c`'s `born_dream` machinery and needs porting,
not inventing:

| Outcome | What happens |
| --- | --- |
| never asked before it expires | **untested.** Not a miss. Counted and reported on its own. |
| asked enough, no better than control | deleted |
| asked enough, beats control | graduated — and *only then* may waking traffic create a real edge, earned in the ordinary way |

The question that decides the whole thing:

> Of the times it had to answer and **the waking graph was blind**, did the
> dream's answer beat chance?

If the dream only helps where the waking graph already knew, it added nothing.
If it answers where the waking graph is blind, it resolved something.

---

## 9. The panel, and how it is kept honest

Six ways this measurement could lie, each with the rule that stops it. Every one
of these was found by running the earlier design, not by reasoning about it.

**R1 — Denominator.** Report `hit / asked`. Never `hit / opened`. Measured on the
earlier design: 46% of records never had their `from` recur at all, and including
them **inverts the verdict**. `opened` and `never asked` are printed as their own
columns.

**R2 — One population, one helper.** The control draws from the **same candidate
pool the jump searched**, at the same moment, in the same count, through a single
shared `eligible_set()`. Measured on the earlier design, six defensible
populations gave everything from "13× better than chance" to "40% worse" on the
same night. The population choice *is* the verdict, so it may not be made twice.
Report the exact expectation over the pool rather than one draw, so the RNG is
not a lever on the published number.

**R3 — Say only what the data can say.** There is no traversal record on an edge,
and weight 1 means three different things (inherited and never walked; walked
once; walked thousands of times and decayed to the floor — 96.7% of a live pool
sits at ≤1 after 18 decay passes). So the design never claims "never taken
awake". It claims what is true: **not an out-edge when the dream jumped.**

**R4 — It must be able to print NO READING.** Under 25 discordant pairs, the
panel prints those words and no percentage. The earlier design produced 1
confirmation in each arm out of 132 — that is not a negative result, it is an
absence of events, and a reader cannot tell them apart unless the instrument says
so.

**R5 — Yield is not inherited from the triage.** Hypotheses are opened by a
measurement walk at a temperature we control, not by whatever kind the night
happened to need. Otherwise the published rate moves with the material rather
than with dreaming.

**R6 — No silent caps.** The candidate pool size, the expiry budget, the
signature floor and the number of jumps are all printed. A bounded search that
does not announce its bound reads as exhaustive.

---

## 10. Order of construction

Strict TDD throughout. Each step ends green and is committed on its own.

| # | Piece | Why here |
| --- | --- | --- |
| 1 | **P20 tension** — keep where the confident shape failed | smallest change, and nothing else has a seed without it |
| 2 | **P19 signature + similarity**, with the random-pair baseline **measured first** | the bar goes where the data separates, before any threshold is written down |
| 3 | **P21 the jump** at tension points, bounded and reported | the new move; the fatal defect dies here |
| 4 | **P22 the ledger**, own file, fingerprint-keyed | nothing can be scored until it can survive a process boundary |
| 5 | **P23 consequences** | the analogy becomes falsifiable |
| 6 | **P24 vote, judgment, and the panel** | the answer, with R1–R6 built in from the first line |

---

## 11. Open

- **D35** — how deep do consequences propagate? Depth 1 to begin. Deeper is a
  hypothesis about a hypothesis and needs its own bar.
- **D36** — the backward half of the signature needs a reverse edge index. Costs
  storage; doubles the evidence.
- **D37** — a graduated hypothesis is allowed to become a real edge. At what
  weight, and does it inherit anything? Deferred until something actually
  graduates.
- **D38** — the attention residue is inert as built. Either the mark is raised
  until it does something measurable — and then the leak through
  attention → decay → segmentation must be re-checked — or the design says
  plainly that a dream leaves nothing behind but its hypotheses.
