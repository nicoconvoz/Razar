# The floor rises (P28)

Author, 2026-08-03.

> *"A medida que el mapa crece en concepto, un carácter se descompone en bits,
> pero una palabra se descompone en caracteres y una oración se descompone en
> palabras. Subimos la vara de la visión, más general a medida que crece la
> visión — así no tenemos un telescopio y un microscopio siempre al mismo tiempo,
> que cuesta carísimo."*

---

## 1. The principle

**D45 — A thing decomposes into the level immediately below it, never all the
way to the bottom.**

A character decomposes into bits. A word into characters. A sentence into words.
Nothing decomposes a sentence into bits, and nothing should have to.

The consequence is not aesthetic. It is that **the resolution the organism
carries at all times is the resolution of one level down**, and everything below
that is reachable rather than resident. Telescope and microscope, but never both
switched on.

## 2. What it costs today, measured

| | bytes | share of `rz_frag` |
| --- | --- | --- |
| `atoms[RZ_FRAG_SPAN]` — the flattened expansion all the way down | 32 | **62%** |
| `left` + `right` + `span` — the relation | 10 | 19% |
| `seen`, `attention` | 6 | 12% |

Every fragment in memory carries its **full expansion to bare bytes**, whatever
level it sits at. The microscope is welded on. It is 62% of the fragment table.

This was already visible from the other side — the standing note that *"when only
relations remain"* the flattened atoms are redundant, because the decomposition
already lives in `left`/`right`. The author's framing supplies the reason it is
not merely redundant but **wrong in principle**: a word has no business storing
its bits.

## 3. The invariant survives, and this is the part to be careful about

The founding rule is that **nothing is ever opaque** — everything decomposes to
atoms. Dropping the flattened expansion does not break it.

Decomposition remains *possible* by walking down `left`/`right`. What changes is
that resolving a word to its bytes stops being free and becomes something **paid
for when it is needed**. Which is exactly §7.2 already: attenuation, and revival
costs effort.

**D46 — Resolution is a cost paid on demand, not a cost paid always.** The
invariant is that the descent exists, not that it is precomputed.

## 4. Why this lands now: the price of a new unit

The unit-discovery probe stopped early, and the reason was exactly this defect in
a different place. A candidate unit was charged `length × 8 bits` — its spelling
**in raw bytes**. So remembering `jesús` cost 48 bits from scratch even though
the organism already held `jesú` and needed one more character.

Under D45 the price is the difference, not the total: a unit is spelled in the
units one level down. Long words then become **cheap precisely because they share
roots with what is already known**, which is the behaviour the concept map was
supposed to have from the first day.

The same principle fixes the storage and the price. That is the sign it is the
right principle and not a convenience.

## 5. Open

### D47 — answered (author, 2026-08-03)

> *"Cuando en ese nivel hay muchos núcleos estables de palabras, o de unidades
> más grandes en caso de ser un fragmento mayor."*

The implementable core is **stable**, not **many**. A count would be a declared
number, and declaring numbers is the one thing this project does not get to do.
What can be read without declaring anything is whether the level has **stopped
moving**: new material arrives and the units at that level are no longer re-cut,
no longer abandoned, no longer replaced.

So the floor rises on a derivative — the same shape as everything that worked
today. The heartbeat is a rise rather than a level. Harmony is a direction
rather than a state. And a level becomes the floor when its churn falls to
nothing, not when its population reaches a number.

A unit is a stable nucleus when it keeps paying for itself as unfamiliar
material arrives. Churn is units being adopted and then abandoned. When churn
stops across a stretch of new material, that level has settled and may be stood
on.

**The floor is never declared solid. It is observed to have stopped moving.**

### Still open
- **D48** — a raised floor is a claim that can be wrong. If the unit layer turns
  out to be badly cut, everything above it inherits the error and the descent is
  the only way back. What tells the organism to lower the floor again?
- **D49** — `rz_frag` is a fixed struct and `RZ_MOD_VER` derives from its size,
  so removing `atoms[]` invalidates every stored nest. Cheap now, expensive later.
