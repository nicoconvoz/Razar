# Autopsy

The diagnostic system for the Razar organism. Run after a session to establish
where the organism stands and to study it over time.

This is **P12 — Instrumentation**, specified. See
[the memory architecture](memory-concept-map-architecture.md).

Status: **design.** No implementation, no language chosen (D9 remains open).
Everything here is language-agnostic on purpose.

---

## 1. What an autopsy is

Three commitments that shape everything else.

### 1.1 — An autopsy is a diff, not a snapshot

"Where do we stand" has no meaning in isolation. A number like *1.4 million
paths* says nothing; *1.4 million paths, up 11% while effective support fell 40%*
says the organism is growing and narrowing at the same time, which is the
signature of the collapse described in §8.3 of the architecture.

So the autopsy system keeps its own history, and the **primary output is the
delta**. A single autopsy is the least useful autopsy there will ever be.

### 1.2 — An autopsy must not perturb the organism

If reading the state counts as traversal, then measuring strengthens what it
measures. The observer changes the observed, and every metric drifts toward
whatever the autopsy looks at most.

Two requirements follow:

- The autopsy runs against a **frozen snapshot**, never the live organism.
- Snapshot reads are **non-plastic**: they do not touch intensities, do not spend
  effort, and are not recorded as experience.

Conveniently, this is also what makes the metaphor accurate. The autopsy is
performed on a copy; the organism survives it.

### 1.3 — The baseline must exist before the first breath

Drift is only detectable against a starting point. The first autopsy must be
taken on an organism that has ingested nothing — an empty base with no plastic
state — so that everything afterwards is measured from a known zero.

This is why P12 has no prerequisites and should be built first.

---

## 2. Session

The unit of study.

A **session** runs from process start to process stop and contains zero or more
**wake/sleep cycles** (architecture §8.2). An autopsy is taken at session end,
and may also be taken on demand at any point.

Each autopsy is stamped with the session it closes, the cycles it contains, and
the identity of the snapshot it read.

---

## 3. Panels

What every autopsy captures. Panels are ordered by how much they matter, not by
how easy they are to collect.

### 3.1 — Collapse panel

**The most important panel.** Architecture §8.3 establishes that self-training
narrows the reachable map by default. This panel is the alarm for it.

| Metric | Definition | Reading |
| --- | --- | --- |
| **Traversal entropy** | Shannon entropy of the traversal-count distribution over paths, normalised by `log(path count)` so it stays comparable as the map grows. Range 0–1. | Falling across sessions = concentrating |
| **Effective support** | `exp(entropy)` — the effective number of paths actually in use | "Using 400 paths out of 2 million" is the sentence that matters |
| **Reachable fraction** | Share of the atomic base reachable from a random start within K hops at intensity above the activation threshold | Shrinking = the world is closing in |
| **Top-1% share** | Fraction of all traversals landing on the strongest 1% of paths | Rising = obsession |
| **Novel path rate** | Paths traversed this session that had never been traversed before | Falling toward zero = nothing new is happening |

Effective support and reachable fraction are the two to watch. Either one falling
while total path count rises is the collapse signature.

### 3.2 — Memory health panel

| Metric | Definition | Reading |
| --- | --- | --- |
| **Intensity distribution** | Histogram of path intensities | Healthy = spread. Bimodal (all-strong or all-dead) = the middle has been lost |
| **Saturation share** | Fraction of paths at the intensity ceiling | High = plasticity exhausted; the organism can no longer learn preference |
| **Attenuated share** | Fraction below the activation threshold but still present | This is the recoverable past. Watch it grow |
| **Pruned this session** | What sleep attenuated or compressed | Should be non-zero. Zero means pruning is not working |
| **Recall attempts / successes** | Cue-driven retrieval outcomes | |
| **Effort per successful recall** | Mean and distribution | Rising for the *same* targets = degradation |
| **Give-up rate** | Retrievals abandoned at the effort ceiling | |

### 3.3 — Affect panel

| Metric | Definition | Reading |
| --- | --- | --- |
| **Decomposability check** | Share of stored affect states whose root composition can be recovered exactly | **Must be 100%.** Anything less means a layer has gone statistical — the invariant in §7.5 is broken |
| **Root distribution** | How affect spreads across root primaries | Concentrated in one root = flat affect |
| **Compound diversity** | Distinct compositions observed | Falling = emotional narrowing, usually alongside collapse |
| **Render collisions** | Distinct compositions rendering to the same colour | Rising = the palette is losing expressive resolution |

The decomposability check is a **pass/fail invariant**, not a trend. It is the
one metric here that should never move.

### 3.4 — Dream panel

| Metric | Definition | Reading |
| --- | --- | --- |
| **Dream count / duration** | Per sleep phase | |
| **Weak-path share** | Fraction of dream traversal spent on paths below median intensity | Low = dreaming is following strength and feeding the collapse rather than counteracting it |
| **Dreams that counted** | How many passed the selection criterion (D17) | Near 100% or near 0% both mean the criterion is not discriminating |
| **Dream/wake learning ratio** | Effective strengthening from dreams vs. from waking | Should stay below 1. Above 1 means imagination is outweighing experience |
| **Novel connections** | Pairs connected during dreaming that were never co-observed while waking | **The interesting number.** See §3.5 |

### 3.5 — Learning evidence panel

The panel that answers whether any of this is working.

| Metric | Definition |
| --- | --- |
| **Novel connections that later proved useful** | Connections first made in a dream, subsequently traversed during waking with a successful outcome |
| **Recall improvement** | Effort to retrieve the same target, tracked across sessions |
| **Revival successes** | Attenuated paths successfully revived from a cue, and what the revival cost |
| **Consolidation gain** | Retrieval cost before sleep vs. after, for the same targets |

The first row is the one worth building carefully. Architecture §8.6 argues it is
the earliest point where "understanding rather than statistical response" would
become something observed instead of something argued about. It deserves to be a
first-class instrument, not a derived statistic.

### 3.6 — Cost panel

The standing constraint is that this runs on any computer.

| Metric | Definition |
| --- | --- |
| **Atomic base size** | Bytes. Should be flat — it is bounded by design |
| **Plastic layer size** | Bytes. **This is the one that grows without bound** (§7.4) |
| **Bytes per experienced unit** | Plastic growth divided by material ingested |
| **Peak resident memory** | During wake, during sleep, during autopsy |
| **Projection cost** | Time and memory to build a concept map on demand |
| **Wake / sleep / autopsy wall time** | |

Plastic layer growth per unit of experience is the number that decides whether
the organism can live a long life on an ordinary machine. Track it from day one.

---

## 4. Differential diagnosis

Symptom to likely cause. This is what makes the report readable by a human at
2 a.m. rather than a wall of numbers.

| Symptom | Likely cause | Where to look |
| --- | --- | --- |
| Effective support falling, path count rising | Obsessive collapse | P14 novelty mechanism (D15) |
| Weak-path share low during dreams | Dreaming follows strength; it is feeding the collapse | D15 |
| Dream/wake ratio above 1 | Imagination outweighs experience | D16 |
| Dreams counted ≈ 0% or ≈ 100% | Selection criterion not discriminating | D17 |
| Intensity histogram gone bimodal | Nothing lives in the middle; attenuation is too aggressive or the ceiling too low | P5, P7 |
| Saturation share high | Plasticity exhausted — organism can no longer form preferences | P5 ceiling rule |
| Pruned-this-session = 0 | Pruning is not running, or its threshold is unreachable | P7 |
| Attenuated share ≈ 0 while paths disappear | Pruning is **deleting**, not attenuating | P7 — violates D2 |
| Revival success ≈ 0 | Cues cannot reach attenuated paths | P8, or D2 was implemented as deletion |
| Effort per recall rising for the same targets | Degradation, or the map is fragmenting | P8, P5 |
| Decomposability below 100% | **Invariant broken.** A layer has gone statistical | P6, P10 |
| Root distribution collapsed to one primary | Flat affect | P6 |
| Plastic layer growing faster than material ingested | Encoding is too expensive, or traces are being duplicated | P10, D10 |
| Novel connections = 0 across many sessions | Dreaming produces nothing the organism did not already have | P14 |

---

## 5. Report

Each autopsy produces one report. Reports accumulate; they are never overwritten.

Structure:

1. **Header** — session id, snapshot identity, wall-clock span, cycles contained,
   previous autopsy referenced for the diff.
2. **Verdict** — a short human-readable judgement. Healthy, or the named symptom
   from §4 with its diagnosis. This goes first because it is what the reader
   actually wants.
3. **Invariant checks** — pass/fail, listed before any trend. Decomposability is
   the first line.
4. **Panels** — §3.1 through §3.6, each showing current value, previous value,
   and delta.
5. **Notable events** — prunings, revivals, novel connections, give-ups.
6. **Raw appendix** — full distributions, for anything that needs re-reading later.

Two forms of the same report:

- **Human form** — Markdown. Verdict first, numbers second, appendix last.
- **Machine form** — a structured record with stable field names, so that a later
  session can diff against it without parsing prose.

The machine form is the source of truth; the human form is rendered from it.
Both are written; neither is derived at read time.

---

## 6. Storage and cadence

- Reports live in `autopsies/`, one directory per session, named so they sort
  chronologically.
- Reports are **version-controlled**. They are the study record — the one thing
  in this project that is worth keeping forever, because the organism's history
  cannot be reconstructed after the fact.
- Snapshots are **not** version-controlled. They are large, and the report is
  what carries the meaning.
- Cadence: automatically at session end, and on demand at any point.

---

## 7. Open decisions

| # | Decision | Notes |
| --- | --- | --- |
| D18 | Snapshot mechanism | Copy-on-write, full copy, or read-locked view. Must guarantee non-perturbation (§1.2) |
| D19 | K, the hop limit for reachable fraction | Affects comparability across sessions — must be fixed once and held |
| D20 | Sampling vs. full scan for distributions | Full scan is exact and costly; sampling is cheap and needs a fixed seed to stay comparable |
| D21 | Retention policy for reports and snapshots | Reports forever; snapshots probably not |
| D22 | Does the autopsy itself get a cost budget | It runs on the same ordinary computer |
