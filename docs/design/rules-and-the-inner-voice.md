# Rules, and the conversation it has with itself (P31, P32)

Built 2026-09-14 and 2026-09-15, after the sense faculty (P30) was made cheap
enough to run while eating. Recorded here because the measurements are
two-sided, and the half that disappoints is the half worth keeping.

---

## Before either: what made senses affordable

P30 grew senses that worked and could not be used, because using them was
ruinous.

**Scored as it goes.** Scoring re-read the whole material once per sense per
round — senses × material, so the more it learned the slower it ran. Now every
step the organism takes is an observation: the awake senses read where it stood
and remember what followed, and worth is read off those counters. **Reach comes
from time rather than from sweeping a radius**: a sense sees the next move, and
many moves later it knows the graph. Downward keeps its radius, because a
fragment splits in two and the whole descent is a handful of nodes.

**Hot and cold.** Only `RZ_SENSE_HOT` senses read each step, and one sleeping
sense gets a turn in rotation — asleep must not mean buried, or the pool freezes
into whatever happened to be hot first.

**A glance does not dig.** The cost that was actually hiding: reading a neighbour
in a sleeping module woke it from disk, and with a handful resident they evicted
each other on every look — **245,159 module wakes for 34,940 readings**. A sense
now reads only what is already at hand. Eating with twelve senses went from
18.766 s to 0.495 s.

**Birth is instant.** A newborn has watched nothing and is worth nothing, so
gating it on worth would mean no sense could ever be born. Quality is the audit's
job, afterwards, and nothing is judged before it has watched enough to speak.

---

## P31 — The rule: the sense, and the way to use it

Taken from the reference organism, where a rule is a concept with `deep=1`:
`feat` says **when**, `dfrom→dto` says **what**, `dscore` says **how much**,
`level` says **how real** the experience that formed it was. Noticing that the
queen matters is an observation; the rule is that plus the way to act on it.

| Part | In Razar |
| --- | --- |
| when | `rz_situation`: the shape of where it stands, plus what the awake senses read there |
| what | from here, go to *that* fragment |
| how much | `score` |
| how real | `level` — 1 lived, 0 dreamt |

**It votes; it does not command.** The urge is priced in the only currency the
evidence already uses — traversals — so a rule worth two experiences cannot
overturn a path walked five hundred times, and needs no fudge factor to keep it
in its place. A rule born dreaming weighs half of one that was lived
(`0.5 + 0.5 × level`, from the reference). Meeting the same lesson again
reinforces it (`0.7·old + 0.3·new`) rather than duplicating it.

**Harvested only where thinking was expensive** — branching above 1.5 or a failed
prediction — because a library of the obvious saves nobody effort.

### The senses finally matter

They are what tells one moment from another. Without them the situation is the
shape alone: **27 distinct situations across 26,423 steps.** With 24 senses:
**247.** The senses stopped being observers with a score and became the key a
decision is filed under.

### And it memorises

| | without rules | with rules |
| --- | --- | --- |
| right on material it ate | 0.1160 | **0.3067** |
| right on material held back | 0.0509 | 0.0493 |

**15,919 lessons fighting over 256 slots.** It keeps what it saw last, which fits
the training material and does not transfer.

---

## P32 — The conversation it has with itself

### The loop that did not exist

It had senses that watched and rules that voted, and nothing closed the circle:
it never found out whether it had been right, so **nobody paid for being wrong**.
That is why the rules memorised.

Now every step is an exchange. **Before the next piece is looked at** it says what
it expects — in the dark, or it is not a claim. Reality answers. Whoever spoke is
credited or blamed, on the reference organism's own average. A rule that keeps
being wrong is never deleted; it fades until its vote cannot move anything, which
is what forgetting has always meant here.

It is born listening. Silence has to be asked for (`rz_inner_listen(m, 0)`).

### It can be heard

`razar think [path] [bite]` eats in mouthfuls and after each one says a little of
what it has been telling itself, flushing every line as it is written. Reading the
New Testament it stands on *"Hijo de"* and **sees "David" coming**; on the last
verse it stands on *"Señor"* and sees *"Jesús"*. The live panel streams this as it
happens.

### The loop closes, and it changes almost nothing

Over 202,720 exchanges on real source:

| | |
| --- | --- |
| right | 10.4% |
| rules charged for being wrong | 84,484 |
| rules credited for being right | 10,955 |
| rule standing afterwards | 0.0000 to 0.9923 |

The useless rules fell to nothing and the good ones survived. **The machinery
works exactly as intended.**

| | deaf | listening |
| --- | --- | --- |
| right on material never read | 0.0659 | **0.0664** |

**One percent.** Correcting the rules well barely moves prediction, because the
rules are wrong 88% of the time — correcting something that is mostly wrong
leaves something that is mostly silent.

### Lessons that came from getting it wrong

Two tests planted a rule on a fragment picked by hand, and measured nothing for
eighteen thousand exchanges: the organism had long since composed that piece into
something wider and never stood there again. **Rules have to be harvested where
the walk actually goes.**

---

## Open

- **D59** — rules are wrong 88% of the time. The problem is no longer how they are
  judged but how they are proposed. The reference organism harvests after a
  *deep search*; Razar harvests after a single uncertain step, which is a much
  weaker kind of thinking to keep the conclusion of.
- **D60** — a rule is judged only on the material it keeps meeting. That is still
  judging it on material from the same source that bore it; D51 asks for
  something neither the rule nor its harvest was optimising.
- **D61** — the inner conversation keeps 64 exchanges. Whether anything should
  read that ring besides an onlooker — the dream, for instance, which is supposed
  to start from what was uncomfortable — is unanswered.
