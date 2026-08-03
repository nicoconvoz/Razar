# Extraction from `organismo.c`

What the chess organism in [`_referencia/`](../../_referencia/) contributes to Razar.

Source: `organismo.c` (1617 lines, C) plus `DOCUMENTACION_TECNICA.md`. Author: Nico.
Read in full 2026-08-03.

The organism is a chess player that learns without being given chess principles.
It is not a prototype of Razar and most of it is chess-specific. But its **mind**
is a plastic concept map with pruning, promotion, dreams and affect — which is
the same problem Razar is solving, already solved once, in working code.

This document extracts only what transfers, states honestly what does not, and
marks where the reference has the exact defect Razar must avoid.

---

## 1. Mechanisms that close open Razar decisions

These are not analogies. Each one answers a decision that was open in the
[memory architecture](memory-concept-map-architecture.md).

### 1.1 — Graduated reality closes **D16** (dream learning rate)

`organismo.c:274, 356, 447, 497, 1596, 1600`

Every concept carries `level` — how *real* the experience that formed it was:

| Experience | `CUR_REALITY` |
| --- | --- |
| Self-play (dreaming) | 0.10 |
| School (playing a real opponent) | 0.30 – 0.90, rising with the rung |
| Master games observed | 0.95 |

`level` rises toward the reality of each new reinforcement
(`n->level += 0.34f*(CUR_REALITY - n->level)`), governs consolidation, and breaks
ties in core competition.

**This answers D16 directly.** Razar asked what learning rate a dream should have
relative to waking. The reference's answer is better than a constant: reality is a
*graded scalar attached to the experience itself*, and everything downstream reads
it. A dream does not need a special rule — it simply carries low reality, and low
reality consolidates less.

The counterforce against the collapse in §8.3 of the architecture was already
built here, years before Razar named the problem.

### 1.2 — `born_dream` plus reality-confirmation closes **D17** (which dreams count)

`organismo.c:454, 499-503`

A concept created while dreaming is flagged `born_dream` and held in **quarantine
from the core**. It graduates only when real experience touches it:

```c
if(n->born_dream && (!c->dream || n->level>=0.5f)){ n->born_dream=0; ... }
```

**This is a better answer to D17 than either candidate the architecture proposed.**
Affect coherence and effort both try to judge a dream *at dream time*. This judges
it later, by whether reality confirms it. A dream counts when the world agrees with
it — which needs no new metric, no threshold to tune, and cannot be gamed by a
dream that merely looks tidy.

The dream is allowed to be wild. It just does not get to be load-bearing until
something real touches it.

### 1.3 — Context wake-up closes **D15** (novelty mechanism) and rescues rare paths

`organismo.c:165-180`

The perception genes are split into a **hot set** (`GMAX`=256, highest priority)
and a **cold pool** (`GPOOL`=8192, asleep). For a given move, a cold gene wakes
*momentarily* if its score for **this exact situation** beats the **median of the
active hot genes**. It fires for that move and goes back to sleep. Its background
priority is untouched.

The comment states the problem it solves exactly: *the rare-but-key sense that
would be asleep precisely when it is needed.*

**This is a sharper answer to D15 than the architecture's proposal.** Razar
suggested dreaming should walk *weak* paths stochastically, to counteract
usage-driven pruning. The reference does something better: a weak path is woken
**by contextual relevance**, not by chance. Rarity stops being fatal, because
reachability no longer depends on strength.

The counterweight to pruning is not randomness. It is **a second addressing route
that ignores strength entirely.**

### 1.4 — The special pin: rare-but-effective bypasses the normal gate

`organismo.c:471-473, 483-490`

A solution that is highly effective (>75% positive outcomes over ≥3 appearances)
for a **rare** situation — one that would never accumulate the samples the normal
promotion gate demands — is pinned to the core **immediately but provisionally**
(`pin_until = age + 2000` ticks). If the rare situation recurs and it remains
effective, the pin refreshes. If it goes cold, it expires and falls out.

Razar has no equivalent and needs one. Any promotion rule based on sample counts
systematically discards exactly the knowledge that is most valuable *because* it
is rare.

### 1.5 — Situation-partitioned memory: the folder comes from what is happening

`organismo.c:317-360, 249-258`

The single most important architectural idea in the reference.

A concept's `module_key` is **not a hash of its feature vector**. It is a
classification of *what is going on*: ATTACK / DEFENCE × my piece × enemy piece,
with the dominant threat selected by value. 73 folders. `nearest_concept` searches
**only within the same situation**.

The documentation states the consequence, and it is the deepest line in the whole
reference:

> Being in the folder "Defence of my queen" **is** perceiving that the queen is in
> danger.

The classification *is* the perception. Meaning emerges from how memory is
indexed, not from a separate recognition step.

**For Razar this is how "projections built in real time" become tractable.** You
never project the whole map. You identify the situation, load that folder, and
project inside it. `MAXLOADED`=1 — exactly one module in RAM at a time.

That also gives a concrete shape to **D7** (must the whole graph fit in RAM): no.
One folder at a time, with the rest paged to disk.

### 1.6 — Two signals, not one: `strength` (recurrence) and `merit` (outcome)

`organismo.c:463-491, 276`

Razar's P5 has intensity from traversal only. The reference has **two independent
signals**:

- **`strength`** — recurrence. Rises on every match, decays every game.
- **`merit`** — the mean quality of outcomes in **real** games where the concept
  fired. Slow EMA (`MERIT_LR`=0.06): one game does not define it. Only counts when
  `CUR_REALITY ≥ 0.30`.

And a rule that matters: **the master adds no merit.** Merit is the organism's own
performance, not the master's victory.

**This is the missing anchor for Razar's collapse problem.** Traversal intensity
alone is a closed positive-feedback loop — travelled, therefore strong, therefore
travelled. Merit is grounded in *outcomes*, which come from outside the loop. It
is an external reference that intensity can never be.

Razar's plastic layer should carry both: how often, and how well.

### 1.7 — Retention decoupled from permanence

`organismo.c:526-532, 586-590`

Two different questions, deliberately separated:

- **Permanence** — what belongs in the core. Decided by merit.
- **Retention** — what is not forgotten. Decided by use: `RETAIN_USES`=20 or
  `RETAIN_USEFUL`>1.0 protects a non-core concept from decay.

Death requires **three** conditions at once: not established, `strength<0.12`,
**and** `age−last_age>40`.

The comment records why: when the core was made selective, everything that had
been surviving by accident started being forgotten. Razar would have hit the same
wall — its architecture currently treats "important" and "not forgotten" as one
axis.

### 1.8 — Pruning already works the way D2 requires

`organismo.c:586-590, 544-548`

Confirmation, not a new idea: the reference **never deletes on prune**.
`strength *= decay` attenuates, and `demote_core` returns a core concept to
ordinary status — *"demotar NO borra: vuelve a concepto normal y puede reconquistar
el núcleo si mejora."*

D2 was resolved on the reasoning that a mind attenuates rather than deletes. The
reference is a working implementation of that resolution.

### 1.9 — Purification on write bounds the plastic layer (**D10**)

`organismo.c:388-397, 398-419`

`worth_keeping` runs at flush time and decides what earns disk. Single-use noise
is dropped; core, real-tested, recurrent and returning deep branches are kept.

RAM holds 256 concepts. Disk holds the modules. **The plastic layer is bounded not
by refusing to learn but by refusing to persist.**

This is the shape of an answer to D10 — how the plastic layer's growth stays
bounded over a long life. Learn freely; be selective about what survives the write.

---

## 2. Mechanisms worth taking that solve problems Razar has not reached

### 2.1 — Memoised selection by condition signature

`organismo.c:148-190, 1266-1283`

Which senses wake depends only on which of 12 micro-conditions are active — a
**12-bit signature**, so at most 4096 distinct rankings. The ranking is computed
once per signature and cached; lookups are O(1) afterwards. A genome change bumps
`GENOME_VER` and invalidates lazily. Presets persist to disk, validated by a hash
of the genome.

*"Por esto el genoma puede tener cientos de sentidos sin caerse la velocidad."*

Razar's projection cost (P10, cost panel) has the same shape: if the projection
depends only on a small discrete context, memoise by that context. Version the
cache so a change to the substrate invalidates it rather than corrupting it.

### 2.2 — Plan memory: reuse a projection while it stays valid

`organismo.c:757-787`

Having computed a line, the organism stores it and **executes it without
rethinking** while reality matches what it imagined. It abandons the plan if the
opponent deviates, the horizon runs out, or a cheap depth-2 glance finds something
clearly better (+200).

Razar's concepts are projections built on demand. Rebuilding an identical
projection every time is the obvious cost trap. The reference's answer: keep the
projection, keep the state you expected, and re-derive only when reality diverges.

### 2.3 — Cached thought: the projection's *result* becomes a concept

`organismo.c:698-733`

`harvest_deep` stores a reflected branch as a **deep concept**. Next time,
`deep_pass` recognises it (distance < `P_JOIN*1.5`) and uses its score directly
instead of re-reflecting. *"Lento al principio, rápido después."*

This resolves an apparent tension in Razar's design: concepts are ephemeral
projections, yet something must accumulate. The answer is that the projection is
ephemeral but **its outcome can be stored as an ordinary node**, and later
*recognised* rather than recomputed.

### 2.4 — Version guard: refuse to run half-broken

`organismo.c:1302-1315`

`MEM_VER` derives from `sizeof(Concept)`. If the layout changed, old memory is
unreadable — so it is **cleaned and restarted fresh**, not silently rejected:

> *"En vez de rechazarla en silencio (y correr roto, medio-reconstruido), la limpia
> y arranca fresca."*

Genome, value function and stats survive, because they do not depend on the binary
layout. For Razar's P9 this is the correct default: a format change must fail
loudly and cleanly, never half-load.

### 2.5 — Affect weights live in the genome

`organismo.c:267-268, 880-884, 1156-1157`

`P_EAT` (joy of eating), `P_BITTEN` (disgust at being eaten), `P_VAIN` (disgust at
a wasted sacrifice), `P_KHUNT` (king hunger) are **genome parameters**. Each
individual evolves its own affective character — glutton, romantic, cautious.

For Razar's affect layer this reframes the palette question: the root feelings can
be fixed while their **weights** are part of the organism's character rather than
constants chosen by us. Two Razars fed identically would feel differently.

`P_KHUNT` also demonstrates a design rule worth stealing: **it only pays out if it
won.** `king_joy = won_s ? ... : 0`. Attacking and dying gives no pleasure. An
affect term that rewards the attempt rather than the result teaches the organism
to throw itself away.

---

## 3. Where the reference has the defect Razar must avoid

Stated plainly because it is evidence, not criticism.

### 3.1 — The affect composition is lossy. Exactly the failure D3 forbids.

`organismo.c:880-885`

```c
double eat_joy = tanh(my_caps/4.0)*c->par[P_EAT];
double eaten   = -tanh(my_loss/4.0)*c->par[P_BITTEN]*(1.0+own_stripped);
double win_joy = won_s*(0.30 + (brilliant?0.70:0.0));
double vain    = (lost_s && (my_loss-my_caps)>=2)? -c->par[P_VAIN]:0.0;
double king_joy= won_s ? c->par[P_KHUNT]*(0.5+enemy_stripped)*kpress : 0.0;
float tp = (float)clampd(eat_joy+eaten+win_joy+vain+king_joy, -1.0, 1.0);
```

Five distinct affective sources — genuinely a set of root feelings — **collapse
into one float**. Only `tp` reaches `reinforce`, and only `v_p` is stored.

The decomposition is unrecoverable. Given a concept with `v_p = -0.4`, nothing in
the system can say whether it feels bad because it got eaten, or because a
sacrifice was wasted. The roots existed at write time and were discarded.

**This is precisely the lossy mixing that D3 resolved against for Razar**, and the
reference is the proof that it happens by default. It is the cheapest thing to
write and it silently destroys exactly what Razar's §7.5 requires. Razar must
store roots and proportions and render the compound afterwards — not sum first and
store the sum.

### 3.2 — The autopsy is a snapshot with no history

`organismo.c:1532-1571, 1588`

`--autopsia` is genuinely good and it already satisfies two of the three
commitments in Razar's [autopsy design](autopsy.md):

- **Non-perturbing** — it reads modules from disk in a separate invocation and
  returns before the life loop starts. The organism is not running. Frozen
  snapshot by construction.
- **Human-readable** — it translates internal state into language: each sense
  dumps `despierta_cuando: ["mi_rey_en_jaque", "dama_rival_cerca_de_mi_rey"]`.
  Not a number wall.

What it does **not** do is keep history. It writes `autopsia.json`, overwriting.
There are no deltas, no previous run to compare against, no trend.

That is the gap Razar's autopsy exists to close, and having a working reference
that stops exactly there is useful: it confirms the design and shows the one thing
to add.

### 3.3 — The 48 features were given by hand. Razar has no equivalent.

`organismo.c:191-209`

The documentation's claim is that no chess structure was supplied — no piece
values, no positional principles. That is accurate as far as it goes: the piece
values genuinely emerge from `V`'s weights, anchored to winning.

But `board_features` is a hand-written vocabulary of 48 measurements: material
balance, centre occupation, mobility, pawn shields, game phase. The organism did
not invent *what is worth measuring*. It was told, and then learned what to make
of it.

Razar's stated position is harder: atoms are characters or bits, with no given
vocabulary above them. **Nothing in the reference solves that**, and the reference
should not be read as evidence that it is solved.

### 3.4 — Situation classification is hand-written, and Razar's is the open problem

`organismo.c:339-354`

`situacion()` works because Nico could define, by hand, what "what is happening"
means in chess: attack or defence, which piece of mine, which of theirs, dominant
threat by value. 73 folders, fixed, enumerable.

Razar's domain has no such enumeration. §1.5 above is the most valuable idea in the
reference *and* the one whose transfer is least automatic. It tells us the folder
must come from **what is happening** rather than from a hash of the vector — but
what "what is happening" means for a general organism is undefined.

This is a genuine open problem the reference names without solving.

---

## 4. The measurement lesson

`organismo.c:941-943, 1006-1027`, documentation §9

Three bars, ordered by honesty:

| Bar | What it measures | Weakness |
| --- | --- | --- |
| Fitness vs. random | Stopped being a log | Low and noisy |
| Coevolution vs. champion | Progress against its own best version | **Treadmill** — the champion rises with the organism, so it can never report absolute strength |
| **Fixed sparring, fixed Elo anchors** | Real strength | The absolute number depends on the anchors, but they never move, so **the trend is real** |

The insight generalises past chess: **a bar that rises with its subject measures
nothing absolute.**

This matters directly for Razar's autopsy. Effective support, reachable fraction,
intensity distribution, dream counts — every panel in the current design is an
**internal** metric. All of them can look excellent while the organism is useless,
because they are all measured against itself.

Razar's autopsy needs a fixed external bar that does not move as the organism
grows. What that bar is for Razar is undefined and should be treated as a new open
decision.

And the last line of the reference documentation, which belongs in Razar's method:

> *"El detector de mentiras de las métricas sigue siendo jugarle. Después de cada
> cambio grande, el test humano periódico vale tanto como cualquier número del
> dashboard."*

Several regressions were caught by playing the organism, not by reading the
dashboard. No instrumentation replaces contact with the thing.

---

## 5. What does not transfer

- The chess engine — move generation, legality, perft validation, quiescence,
  alpha-beta. Domain-specific in full.
- `piece_val_`, `SPARVAL`, the ladder, Elo anchors — chess measurement scaffolding.
- PGN parsing, `san_to_move`.
- `MAXCONCEPTS`=256 and `MAXLOADED`=1 as *values*. They work because chess has 73
  enumerable situations. Razar's situation space is unbounded; the pattern
  transfers, the constants do not.

---

## 6. Decisions this closes or opens

| # | Decision | Effect |
| --- | --- | --- |
| D15 | Novelty mechanism during dreaming | **Answered** — context wake-up against the median of the active set (§1.3), not stochastic weak-path walking |
| D16 | Dream learning rate vs. waking | **Answered** — graduated reality as a scalar on the experience (§1.1) |
| D17 | Criterion for which dreams count | **Answered** — reality-confirmation after the fact (§1.2), not judgement at dream time |
| D7 | Must the whole graph fit in RAM | **Answered** — no; one situation folder resident, rest paged (§1.5) |
| D10 | What the plastic layer stores, growth bounded | **Shaped** — learn freely, purify on write (§1.9) |
| D2 | What "pruned" means | **Confirmed** by a working implementation (§1.8) |
| D3 | Is affect composition invertible | **Reinforced** — the reference's lossy sum is the failure case (§3.1) |
| D23 | *New:* what is Razar's fixed external bar | Opened by §4 |
| D24 | *New:* what carries "outcome quality" when there is no game to win | Opened by §1.6 — merit needs an outcome, and Razar has no win condition |
| D25 | *New:* what defines "what is happening" for a general organism | Opened by §3.4 — the hardest one |

D24 deserves stating plainly. Merit works in the reference because chess supplies a
free, unambiguous outcome signal: won, drew, lost. Razar has no such signal. Until
something plays that role, the plastic layer has only traversal intensity — and
§1.6 is precisely the argument that intensity alone closes the feedback loop.
