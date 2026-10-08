# Interest, the pen, and vertical composition (P16, P17, P18)

Three pieces built on 2026-08-03 and recorded here after the fact. Each was
measured when it was built; where a figure has since stopped being true, the
correction sits next to it rather than replacing it, because the history of a
number is part of what it means.

---

## P16 — Interest

`razar interest [path...]`

### What blocked it was value, not feeling

The first framing said the affect layer (P6) blocked everything downstream. That
was the wrong diagnosis. **Affect is what a thing makes the organism feel;
interest is what makes it want.** The second was the one needed, and it does not
require the first to exist.

### Three places a step can land

Read off the same walk the external bar uses, and writing nothing:

| Place | Meaning |
| --- | --- |
| **opaque** | no experience out of here at all — it does not know where it is |
| **frontier** | it had expectations, and they were imperfect: the edge |
| **mastered** | called with near-certainty (p ≥ 0.75) |

Inside the frontier, **novel** counts the steps where it knew where it stood and
had never taken that particular way on.

### Interest is a derivative

Not "the surprise is middling" but "the surprise is falling". A ledger
(`nido/interest.dat`) keeps previous appraisals, keyed by **content fingerprint,
not by name** — a file that changed is not the same material, and comparing it to
its old self would invent progress.

### What it showed without being told

- **The band moves on its own.** Material met once sits on the frontier; the same
  material chewed thirty times becomes mastered and stops being interesting.
  Nobody moved a threshold.
- **It separates noise from signal.** Text and code: 0.0–0.5% opaque. Random
  bytes: **50.3% opaque.** That separation is what the label's bar was set on.
- **Interference between textures.** After eating more C, the Spanish
  documentation got *worse* (−0.0031); after eating the documentation, both C
  files got worse (−0.0008, −0.0011). Symmetric. Learning one texture costs a
  little of another — finite capacity, appearing unasked in the report.

**Rule that came out of it:** *it may choose what it eats; it may never choose
what it is measured on.* A bar run over material the organism picked has stopped
being external.

### Caveat discovered later

The derivative is honest across a long exposure and misleading inside a short
one. While new units are being composed the frontier *widens*, because a newborn
fragment has no experience of its own yet: mastered went 0.9903 → 0.7488 → 1.0000
at 5, 30 and 100 passes. Sampled at 30 passes, interest reports "this got harder"
exactly when the organism got richer.

---

## P17 — The pen

`razar write <cue> [n] [effort] [temp] [seed]`

The first real action: producing something rather than answering about it.

### Writing is not recalling

A recall answers *what comes next*. A text has to keep going without becoming a
chant, and a greedy walk over a graph is periodic by construction. The pen uses
lateral wandering (D15), descent when stuck, and effort as the budget that ends
the text — plus one new piece.

### The refractory period is what stops the chant

A fragment just used becomes momentarily **harder to reach** — damped ÷8 over a
window of 12 — like a neuron that has just fired. Not forbidden: damped. And it is
enough on its own: even at temperature 0 the pen departs from the strongest path.
Wandering and damping are reported separately because they are two mechanisms.

Measured, same cue `"static void "`:

| Walk | Output |
| --- | --- |
| greedy | `snprintf(dirent* x,x,x,x,x,x,x…` — chants forever |
| pen | 180 atoms, none repeating |

### The temperature band was measured, not chosen

| T | What it writes |
| --- | --- |
| 0.10 | too faithful |
| **0.25** | structured corruption: `if(…){`, `;i++){`, `c->genome[` |
| 0.50 | starts mixing prose in |
| 1.00 | degraded |

That is Hoel's gradient: low is faithful, high is incoherent, the middle is
structured corruption. The default is 0.25, and the band depends on the material.

On the Spanish New Testament at 0.25 it writes, from the cue *"Y Jesús les dijo"*:
*«…son los siglos de los siglos. También a la multitud y las siete Angeles que
estaban alrededor del Cordero…»* — the register, and multi-word units it found by
itself.

### Nothing it writes is opaque

`trace[]` keeps every fragment used, in order, and each expands to atoms. *Where
did this come from?* always has an answer. That is the founding invariant applied
to output, and it is what separates this from a statistical model.

**Open:** at high temperature it assembles almost everything at atom level and
throws away the wide fragments it worked to form. The mean span of the pieces used
says whether it is writing with structure or spelling.

---

## P18 — Vertical composition

### Applicability and vertical composition are the same request

The author asked that what matters to the organism be **applicability** —
information is worth something only when something can be built with it — and in
the same breath asked for vertical composition. They are one thing. Applying, in a
graph, means working in contexts other than the one that formed it. A content that
appears in one place applies to nothing; a **shape** that recurs across many
contents does.

### The shape

`rz_shape_of`: the signature of a fragment's **role**, identity discarded — how
decided its exits are, how wide it is, how concentrated its traffic — four
classes each, **64 shapes**. Two fragments with nothing in common can carry the
same one. The graph then exists a second time one level up, shape → shape, built
by the same traffic.

When the specific layer is blind, it asks *where does a place shaped like this
usually lead?* — the only thing in the system that carries what was learned to a
place it has never been.

### What it measured, and what that measurement turned out to be

When built: 88 blind steps on held-out Spanish, 16 rescued by the shape —
**18.18%, reported as 11.6× chance.**

Both halves of that sentence later failed:

- **The alphabet collapsed.** Birth inheritance drove mean out-degree to ~93,
  which pinned two of the three axes; occupancy fell to 11 of 64 classes and the
  rescue channel's population emptied. The rescue read **0.0000**.
- **The chance figure was wrong.** It was 1/64, but the successor shapes are not
  uniform — the best constant guess already scores 0.40. Honest lift: **1.32×**.

Separating paths given from paths taken (`RZ_EDGE_INHERITED`) restored the
alphabet to 28–33 occupied classes. The rescue channel came back: on held-out
material it now reads **0.35** (Spanish, 140 blind steps) and **0.19** (this
repository's CLAUDE.md, 482 blind steps). Those are still printed against the
1/64 null in the CLI, and that null still overstates the lift.

### What matters to it: transfer

The author's correction: understanding gravity, and then seeing a pen at the
table's edge and knowing it will fall. That is the rescue by shape — the pen falls
like the apple *because the situation has the same shape.* What matters is not
novelty or progress but **that the transfer works**.

So while it eats, before each step is written, it asks the shape layer to call it
(`shape_tried[]`, `shape_won[]`). No cheating is possible: the step has not been
recorded yet. And the loop closes in forgetting: `mod_catch_up` spares a fragment
**in proportion to the merit its role has earned** — a protection measured by the
organism on itself, not decreed.

Measured at the time: 17 shapes that transfer, the best at 0.5586 — reported
against the same 1/64 null, so the "35× chance" quoted then overstates it too.
