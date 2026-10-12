# Concept Map Memory Architecture

Original name (author's): **Arquitectura de Memoria de Mapa Conceptual (AMMC)**.
Author: Nico. First stated 2026-08-02.

Status: **the founding decomposition.** Written 2026-08-02 before any code, to
break the stated concept into separable pieces and name the decision each forces.
It did not choose. Most of those decisions have since been made and many of the
pieces built — §6 records which, and links the documents that came after.

---

## 1. The concept as stated

A memory built from concept maps living in a multidimensional space.

- It starts from the most fundamental, atomic units — a character, a bit.
- Those atoms interlink, forming structures and meanings that are fully mappable.
- It behaves **plastically**, like a real mind.
- Paths between nodes carry **intensity**, derived from how heavily travelled
  they are — heavily travelled versus rarely travelled.
- **Colors** mark what each thing makes the mind feel. Each color and its
  variations map onto a combination of feelings, yielding a compound feeling such
  as *"peaceful joy"*. A palette must be defined per feeling type, starting from
  each color's root feelings and passing through every intermediate.
- It must **prune** rarely travelled paths.
- It must **reactivate a lost path through cues**, modelled on how a mind recalls
  something it had forgotten.

Standing constraints: low resource cost, runs on any computer, cost reduced
without losing performance — a balance, not a sacrifice of one for the other.

---

## 2. The pieces

Numbered so they can be referenced directly in discussion.

### P1 — Atomic substrate

The floor of the system: what an atom *is*.

Stated: a character, a bit.

Decisions this forces:
- Is the atom fixed by decree (byte, codepoint, bit) or discovered from data?
- Is there one atom alphabet, or several by modality (text, audio, image)?
- **Type vs. token**: does an atom exist once as an identity, or once per
  occurrence? This single choice dominates the entire memory budget — see §3.1.

### P2 — Composition

How atoms interlink into structures that carry meaning.

Decisions this forces:
- Is a composite a *new node*, or a *named subgraph* over existing nodes?
- Is composition bounded in depth, or unbounded?
- What makes a composite worth existing — repetition, utility, both?
- Can the same atoms participate in many composites at once?

### P3 — Multidimensional space

Where nodes sit, and what their coordinates mean.

Decisions this forces:
- How many dimensions, and are they fixed or grown?
- Are coordinates **assigned** (structural, derived from position in the graph)
  or **learned** (adjusted by experience)?
- Is the space one global space, or several spaces per modality/abstraction level?
- Is proximity in the space *meaningful* (semantic) or *incidental*?
- What numeric type holds a coordinate — this is a direct cost lever (see §3.1).

### P4 — Topology

The edges themselves, independent of their weight.

Decisions this forces:
- Directed or undirected? If directed, is the reverse edge implicit or stored?
- Typed edges (different kinds of relation) or a single untyped edge?
- Is degree bounded per node? An unbounded hub is a performance cliff.
- How are edges stored — adjacency list, CSR, edge table? Directly drives cost.

### P5 — Intensity and traversal

Path strength as a function of how often the path is travelled.

Decisions this forces:
- What increments intensity: a traversal, a successful recall, an external signal?
- Increment rule: linear, logarithmic, saturating?
- Is there a ceiling? Without one, hot paths dominate forever and plasticity dies.
- Decay: does intensity fall with time, with disuse, or only relative to others?
- Numeric resolution of a weight — is one byte enough? This is a cost lever.

This piece is what makes the structure *plastic*. It is the mechanism behind
"behaves like a real mind"; everything else is scaffolding around it.

### P6 — Affect layer (color)

What a node or path makes the mind feel.

Stated: colors, their variations, root feelings per color, intermediates, and
compound feelings like *"peaceful joy"*.

Decisions this forces:
- **Is color the storage, or the view?** See §3.3 — this is the sharpest
  unresolved question in the whole concept.
- What are the root feelings, and how many?
- How do two feelings compose into a compound one — blending, addition,
  a lookup table of named combinations?
- Does affect attach to nodes, to paths, or to both?
- Does affect decay like intensity, or persist differently?
- Does affect *influence traversal*, or only describe it? If a strongly-felt path
  is preferentially travelled, affect becomes part of the retrieval engine rather
  than metadata — a much deeper role.

### P7 — Pruning

Removal of rarely travelled paths.

Decisions this forces:
- Threshold: absolute, relative to neighbours, or budget-driven?
- Trigger: continuous, periodic, or only under memory pressure?
- **What does "pruned" actually mean?** See §3.2 — this collides head-on with P8.
- Is pruning reversible?
- Are nodes ever pruned, or only edges?

### P8 — Cue-based recall

Reactivating a lost path from a hint, the way a mind recovers something forgotten.

Decisions this forces:
- What is a cue — a partial pattern, a neighbouring node, an affect state?
- How does activation spread: breadth-limited, energy-budgeted, decaying by hop?
- How is a *partial* match scored?
- What does "lost" mean, given P7's answer?
- Does a successful recall *strengthen* the recovered path? A mind that
  remembers something forgotten usually remembers it more easily next time.

### P9 — Persistence

How the structure survives a restart.

Decisions this forces:
- One file, many files, or an embedded store?
- Memory-mapped, or loaded and serialized?
- Is the on-disk form the same as the in-memory form? Identical forms cost
  nothing to load and everything in flexibility; different forms cost the reverse.
- Incremental writes or full snapshots?
- Does the whole graph need to fit in RAM, or is partial residency required?
  On the "any computer" constraint, partial residency is likely mandatory.

### P10 — Representation and encoding

How every piece above becomes bytes. This is where low cost is won or lost.

Decisions this forces:
- Node identity: pointer, dense integer index, hash?
- Coordinate encoding: float32, float16, int8-quantized, bit-packed?
- Weight encoding: float, fixed-point, single byte?
- Sparse or dense storage for the space?
- Is the graph structure compressed, and does compression survive random access?

### P11 — Language and runtime

The implementation vehicle.

**This is downstream of P10, not upstream of it.** See §3.4.

### P13 — Intake (the feed folder)

A folder. Anything dropped into it is devoured to give the mind context and to
make its behaviour observable.

Stated 2026-08-03. This is the first piece that produces something watchable, and
it is the boundary between the world and the system.

Decisions this forces:
- Which formats are accepted, and what happens to one that is not?
- Batch or continuous watching?
- Re-ingesting the same file: idempotent, or a second experience? See §8.1 — the
  architecture already answers this, and the answer is *not* idempotent.
- Does ingestion order matter? A mind that reads A then B is not the same mind
  that read B then A.
- Are source files ever modified or moved? They should not be.
- What is the observable result of an ingestion? Without this the folder cannot
  be used to test anything.

### P14 — Dreaming

Offline self-generated traversal. With no external input, the mind walks its own
map, feeds back on prior experience, and applies what it learned. The output need
not be legible to a human — it only has to serve the mind's own brain.

Stated 2026-08-03.

Decisions this forces:
- What starts a dream, and what ends it?
- How is the traversal path chosen? See §8.3 — the default choice collapses.
- Does a dream strengthen paths at the same rate as waking experience?
- **What makes a dream count?** Without a criterion, dreaming is noise injection.
  Candidates that already exist in this architecture are discussed in §8.4.
- Does dreaming create new structure, or only re-weight existing structure?
- Is the dream output retained, or only its effect on the plastic layer?

### P12 — Instrumentation

How any of the above is observed and measured.

Decisions this forces:
- What is counted — nodes, edges, bytes, traversals, recall hits and misses?
- Can the graph be visualized at a useful scale?
- Without this piece, "cheap without losing performance" is an opinion rather
  than a measurement.

---

## 3. Tensions

The pieces above are separable. These tensions are not — they cut across pieces
and must be resolved deliberately, because each one has a default that is wrong.

### 3.1 — Atoms versus cost

If every occurrence of an atom becomes a node carrying its own coordinate, the
memory budget collapses under the "any computer" constraint.

Order-of-magnitude illustration, 1 MB of ASCII text, one node per character
occurrence:

| Coordinate encoding | Bytes/node | Total for ~1.05M nodes | Blowup vs. source |
| --- | --- | --- | --- |
| 128-dim float32 | 512 | ~512 MiB | ~512× |
| 64-dim float32 | 256 | ~256 MiB | ~256× |
| 32-dim int8 | 32 | ~32 MiB | ~32× |
| 16-dim int8 | 16 | ~16 MiB | ~16× |

Edges add on top: at ~8 edges per node and ~8 bytes per edge, roughly another
64 MiB per million nodes.

The escape is the **type/token distinction** in P1. There are ~150k Unicode
codepoints in total; one coordinate per *distinct* character costs ~77 MiB at
128-dim float32 — for every character that will ever exist, not per megabyte of
input. Occurrences then become positions inside structures, not independently
embedded nodes.

Whether atoms are types or tokens is therefore not a modelling detail. It decides
whether the system can run on an ordinary machine at all.

### 3.2 — Pruning versus recovering the forgotten

P7 and P8 are in direct conflict as stated. If pruning deletes, then what was
pruned cannot be recalled by any cue — the second feature destroys the input of
the first.

A mind does not appear to delete. Paths weaken until they fall below the
activation threshold; a strong enough cue can still fire them, which is exactly
the *"I had forgotten that"* experience the concept is modelled on.

So pruning must be something other than deletion. The candidate meanings are
different architectures, not different tunings:
- **Attenuation** — the path stays, its weight goes to near-zero.
- **Compression** — detail is discarded, a low-cost trace remains.
- **Eviction with residue** — the path leaves working structure but leaves a
  reconstruction hint behind.
- **True deletion** — accepted as permanent loss, for content deemed unrecoverable.

The system likely needs more than one of these, chosen per case. **This is the
central design decision of the whole memory**, and it is what would make the
architecture distinctive rather than a conventional weighted graph.

### 3.3 — Color as storage versus color as view

Color is stated as the carrier of feeling. Two readings, with very different
consequences:

- **Color is the storage.** Affect *is* a point in a 3-channel space. Cheap
  (3 bytes), directly visualizable, and hard-capped at three degrees of freedom.
  Compound feelings must be expressible as blends of three axes.
- **Color is the view.** Affect lives in an N-dimensional space of root feelings;
  color is a projection of it for human consumption. More expressive, costlier,
  and it requires defining the projection.

*"Peaceful joy"* is the test case. If peace and joy are independent axes, the
question becomes how many such axes exist — and whether three channels can hold
them without collapsing distinct feelings onto the same color.

The palette work described in the concept — root feelings per color, through all
intermediates — is well defined under either reading, but it is a *different task*
in each. Under the first it defines the space; under the second it defines the
projection.

### 3.4 — Language is the last decision, not the first

The stated need is to pick a language and pick a representation. These are not
peers, and the order matters.

Representation (P10) determines the access patterns. Access patterns determine
what the runtime must be good at. Only then does the language follow:

- If the hot path is pointer-chasing over an irregular graph, cache behaviour and
  memory layout control dominate.
- If it is bulk arithmetic over coordinates, vectorization dominates.
- If the graph must live partly on disk, mmap and zero-copy access dominate.
- If working sets are small and iteration speed matters more than throughput,
  a higher-level language may be entirely adequate.

Choosing the language first means guessing which of these is true, then being
stuck with the guess. The honest sequence is: decide P1 and 3.1 → sketch P10 →
measure the access pattern on a representative shape → *then* choose P11.

---

## 4. Dependency order

What must be settled before what. Arrows mean "constrains".

```
P1 (atoms, type vs token)
  └─> P2 (composition)
        └─> P3 (space, dimensionality)
              └─> P10 (encoding)
                    └─> P11 (language)

P4 (topology) ──> P10
P5 (intensity) ──> P7 (pruning)
P7 (pruning) <──> P8 (recall)      # mutual: tension 3.2 must resolve both at once
P6 (affect) ──> P10                # cost depends on tension 3.3
P9 (persistence) <── P10
P12 (instrumentation) ── independent, can start early
```

Two observations:

- **P1 is the true root.** Everything downstream inherits its cost.
- **P12 is the only piece with no prerequisites.** It can be built first, and it
  is what turns "cheap without losing performance" from an aspiration into a
  number.

---

## 5. Prior art worth naming

Several pieces have established names. Knowing them is not a reason to adopt
them — it is what makes it possible to tell where this architecture actually
diverges, rather than discovering later that a piece was reinvented.

| Piece | Established name |
| --- | --- |
| P5 intensity from traversal | Hebbian / use-dependent strengthening |
| P8 cue-based recall | Spreading activation; content-addressable memory |
| P7 pruning by disuse | Synaptic pruning; graph sparsification |
| P3 multidimensional space | Vector space embeddings |
| P6 affect axes | Dimensional models of emotion |
| P2 composition | Hierarchical / compositional representation |

The combination — a single plastic structure where affect, traversal intensity,
pruning and cue recovery operate over the same atoms — is where novelty would
live, if it lives anywhere.

---

## 6. Open decisions

Updated 2026-10-08 against what is built. The original statuses are kept where
they still hold; a status changed because code now answers it says so.

| # | Decision | Status | Blocks |
| --- | --- | --- | --- |
| D1 | Atoms: types or tokens | **Resolved** — types, fixed base (§7.1) | Everything (3.1) |
| D2 | What "pruned" means | **Resolved** — attenuate / compress, revival costs effort (§7.2) | P7, P8 (3.2) |
| D3 | Color: storage or view | **Resolved in direction** — roots stored, color is expressive contour (§7.3). Not built. | P6, P10 (3.3) |
| D4 | Dimensionality and coordinate encoding | **Open, and possibly moot** — nothing is placed in a coordinate space; the graph is the space. The only coordinate-like thing is the 64-class shape. | P3, P10 |
| D5 | Composites: nodes or subgraphs | **Resolved** — neither; real-time coordinate projections (§7.1) | P2 |
| D6 | Does affect steer traversal | **Open** — the affect layer (P6) was never built | P6, P8 |
| D7 | Must the whole graph fit in RAM | **Answered, built** — no; modules page in and out, `RZ_RESIDENT` held at once | P9 |
| D8 | Representation | **Decided in code** — fragments with left/right in per-module tables, edges in a per-module pool, integer weights | P10 |
| D9 | Language and runtime | **Resolved** — C11, gcc, no dependencies beyond libm (2026-08-03) | P11 |
| D10 | What persists in the plastic layer | **Built** — fragments, edges, weights; reopened by D45/D46, since the flattened atoms are 62% of a fragment and redundant | Cost, P5, P7, P8 |
| D11 | What "effort" is, and its budget | **Resolved** — effective branching, `exp(H)` of the outgoing paths | P8 |
| D12 | Number of root primaries | **Open** — affect not built | P6, P10 |
| D13 | Is affect composition invertible | **Open in practice** — required (§7.3), not built | P6, P10 |
| D14 | Repeated ingestion | **Resolved** — a second experience, not idempotent (§8.1); interest relies on it | P13 |
| D15 | Novelty mechanism during dreaming | **Answered** — context wake-up ([extraction](reference-organismo-extraction.md) §1.3) | P14 |
| D16 | Dream learning rate relative to waking | **Answered** — graduated reality (extraction §1.1) | P14, P5 |
| D17 | Criterion for which dreams count | **Answered** — reality-confirmation after the fact (extraction §1.2) | P14 |
| D18–D22 | Autopsy decisions | See [autopsy.md](autopsy.md); the autopsy is built | P12 |
| D23 | Razar's fixed external bar | **Built** — `rz_measure` / `razar predict`: guesses before reading, learns nothing | P12 |
| D24 | What carries "outcome quality" with no game to win | **Closed by D39** — harmony, describing the same material with less while still accounting for all of it ([harmony.md](harmony.md)) | P5, P7 |
| D25 | What defines "what is happening" for a general organism | **Partly answered** — `rz_situation` (P31): the shape where it stands plus what the awake senses read there | P2, P3, P13 |

### Later decisions, by document

| Range | Where |
| --- | --- |
| D26–D30 | [skills.md](skills.md) |
| D31–D38 | [dreaming-as-analogy.md](dreaming-as-analogy.md) — design and its NO-GO verdict |
| D39–D44 | [harmony.md](harmony.md) |
| D45–D49 | [the-floor-rises.md](the-floor-rises.md) |
| D50–D53 | [what-the-context-did-not-give.md](what-the-context-did-not-give.md) |
| D54–D58 | [senses.md](senses.md) |
| D59–D61 | [rules-and-the-inner-voice.md](rules-and-the-inner-voice.md) |
| D62–D68 | [the-legos.md](the-legos.md) |
| D69–D72 | [curiosity.md](curiosity.md) |
| D73–D77 | [behaviours.md](behaviours.md) |
| D78–D82 | [concepts.md](concepts.md) |
| D83–D85 | [types-and-identities.md](types-and-identities.md) |
| D86–D88 | [weaving.md](weaving.md) |

### Pieces built after this document

| Piece | Where |
| --- | --- |
| P16 interest, P17 the pen, P18 vertical composition | [interest-pen-composition.md](interest-pen-composition.md) |
| P19–P24 analogy dreaming — designed, not built | [dreaming-as-analogy.md](dreaming-as-analogy.md) |
| P25–P27 harmony, symmetry, the unit | [harmony.md](harmony.md) |
| P28 the floor rises | [the-floor-rises.md](the-floor-rises.md) |
| P29 what the context did not give | [what-the-context-did-not-give.md](what-the-context-did-not-give.md) |
| P30 senses | [senses.md](senses.md) |
| P31 rules, P32 the inner conversation | [rules-and-the-inner-voice.md](rules-and-the-inner-voice.md) |
| P33 the legos: clean cuts, kinds, play | [the-legos.md](the-legos.md) |
| P34 curiosity: words, marks, asking, why | [curiosity.md](curiosity.md) |
| P35 behaviours — exploration, imitation, repeat-or-let-go built | [behaviours.md](behaviours.md) |
| P36 concepts | [concepts.md](concepts.md) |
| P37 types and identities | [types-and-identities.md](types-and-identities.md) |
| P38 weaving | [weaving.md](weaving.md) |

---

## 7. Author's answers — round 1

Answers given 2026-08-03. These resolve four decisions and open four more.

### 7.1 — Atoms are a fixed base; concepts are projections

> *"El átomo es lo que más cuesta en peso total, pero cuando están definidas las
> bases atómicas el resto de los conceptos es solo una proyección de coordenadas
> que construyen en tiempo real esa parte esencial del mapa conceptual requerido."*

Resolves **D1** and **D5** together:

- Atoms are **types**, not tokens. The atomic base is defined once and paid for
  once. It is bounded and does not grow with experience.
- Composites are **neither stored nodes nor stored subgraphs**. A concept is a
  **coordinate projection computed on demand**, and only the portion of the map
  the current operation requires is built.

This is a significant architectural commitment. The concept map is not a stored
artifact that is read — it is **generated**. Storage holds what is needed to
reproduce a projection, not the projections themselves.

It also dissolves tension 3.1 as originally posed. The ×512 blowup came from
materializing every occurrence. If occurrences are never materialized, that cost
never appears.

**But the cost does not vanish — it moves.** See §7.4.

### 7.2 — Pruning is attenuation; revival costs effort

> *"Me encantó lo de atenuar o comprimir, y con algo de esfuerfo revivir el
> recuerdo."*

Resolves **D2**: pruning attenuates or compresses; it does not delete. A
sufficiently strong cue can revive an attenuated path.

The new element is **effort**. Revival is not free and not binary — it has a
price, and the price scales with how attenuated the memory is. This matches the
phenomenon being modelled: recovering something nearly forgotten takes work, and
you can feel it taking work.

Effort is a useful mechanic beyond realism: it is a natural budget. A system that
must spend to remember has a built-in reason to stop searching, which is exactly
what a mind does when it gives up on a name it cannot retrieve.

Opens **D11** — what effort actually is, and how it is bounded:

- Is effort measured in hops, in nodes activated, in compute, in wall time?
- Does successful revival restore the path fully, partially, or to a level
  proportional to the effort spent?
- What is the give-up condition?
- Is a *failed* retrieval attempt itself recorded? A mind remembers having tried.

### 7.3 — Color is the contour of the data, and a mode of expression

> *"El color es el contorno del dato del mapa conceptual, puede variar y nos da
> una certeza de qué está sintiendo. Como hay muchos colores, quise copiar que
> haya variaciones pseudo-infinitas de sentimientos que se puedan entender
> teniendo en cuenta las variaciones y los sentimientos de las raíces que
> conforman los colores principales de la composición. Es un modo de expresión,
> el color en este caso."*

Resolves **D3** in direction. Color is not a separate attribute bolted onto a
node, and it is not merely a debugging view. It is the **contour** of the data —
intrinsic to it — and it functions as **expression**: the form in which the
affective state becomes readable.

The stated requirement is **pseudo-infinite variation**. Three 8-bit channels
yield 16.7 million distinct colors, so channel count alone is not the obstacle.

The real constraint is different, and it is sharp:

> **Color mixing is lossy. Composition must remain invertible.**

The concept requires understanding a compound feeling *by way of* the root
feelings that compose it. But if only the resulting color is stored, the
decomposition is unrecoverable — many different root combinations render to the
same color. Orange gives no way to know whether it was red-plus-yellow, or a root
orange, or a dozen other mixtures.

Losing the decomposition means losing exactly the thing §7.5 asks for. A feeling
you cannot decompose into its roots is a statistical blur, not something
understood.

Therefore the storage is the **composition** — roots and their proportions — and
the color is what that composition **renders to** for expression. Color is the
output form, not the record.

Opens **D12** and **D13**:

- How many root primaries are there, and what feeling does each anchor? Three
  gives the fewest axes and the cheapest storage; more primaries give richer
  decomposition at higher cost.
- Must composition be *fully* invertible, or is it enough that dominant roots are
  recoverable? Full invertibility is strictly more expensive.
- Do the intermediates between roots need naming, or do they emerge from the
  proportions?

### 7.4 — The cost moved to the plastic layer

This is a consequence of §7.1 that was not stated, and it needs facing.

If concepts are projections built in real time, then three layers exist, not two:

| Layer | Contents | Lifetime | Growth |
| --- | --- | --- | --- |
| **Atomic base** | The alphabet of atoms and their coordinates | Permanent | **Bounded** — does not grow with experience |
| **Plastic state** | What experience changed: path intensities, affect compositions, attenuation levels, traces of the pruned | Persistent | **Unbounded** — grows with everything ever experienced |
| **Projection** | The concept map built for the current operation | Ephemeral | Per-operation |

The original cost worry was the atomic base. §7.1 bounds it. But plasticity
cannot live in a layer that is recomputed from scratch — you cannot attenuate
something you regenerate identically each time, and you cannot revive a path that
was never recorded as having weakened.

So P5 (intensity), P7 (pruning) and P8 (recall) all operate on the **middle
layer**, and that layer is the one with no natural bound. It grows with
experience, which for an organism intended to learn continuously means it grows
forever.

**The "runs on any computer" constraint now applies to the plastic layer**, not
to the atoms. That is where the cost question actually lives.

This is progress, not a setback: a bounded base plus a growing learned state is a
far better position than an unbounded base. But it makes the growth policy of the
middle layer a first-class design problem rather than an afterthought — and it is
precisely what attenuation, compression and pruning exist to control.

Opens **D10** — what the plastic layer stores, exactly, and how its growth is
bounded over a long life.

### 7.5 — Understanding, not statistical response

> *"Lo que quiero no es solo una respuesta estadística, sino buscar que la propia
> máquina entienda los propios conceptos como lo hace un humano."*

Recorded as the guiding intent of the architecture.

One property of the design is worth naming precisely, because it is concrete
rather than philosophical, and it is the point where this diverges from a purely
statistical system:

In a statistical model the representation is **opaque and global** — meaning is
distributed across weights with no addressable structure. In this architecture a
concept is a **structure over atoms that can be walked**. The path by which a
concept was reached is itself an object in the system.

That property — call it **traceability of meaning** — does not settle any
philosophical question about understanding. But it is real, it is inspectable,
and it is not something a statistical model provides natively. It is also what
§7.3's invertibility requirement protects: the moment any layer stops being
decomposable, that layer has become statistical again.

Worth holding as a design invariant: **every layer stays decomposable.**

---

## 8b. What running P8 revealed (2026-08-03)

Cue-based recall was built and tested (107 checks). Two observations came out of
feeding it real material, neither of which was visible on paper.

### 8b.1 — Collapse is legible as behaviour, not only as a number

A mind fed 480 KB of one repeated phrase, asked to continue the cue `"static "`,
returned:

```
rararararararararararararararararararararararararararararara
```

It is inside its attractor and every path leads back in. The §3.1 collapse panel
reports this as a number; recall makes it something you can read. Both belong in
the autopsy — a metric persuades, a transcript convinces.

### 8b.2 — One atom of context is not enough, and whitespace proves it

On a clean mind fed only the reference organism and its documentation, recall is
**correct for one or two atoms and then dies**:

| Cue | Recalled | Reading |
| --- | --- | --- |
| `q` | `ue` + spaces | q→u→e is genuinely remembered |
| `voi` | `nt` + spaces | i→n→t — it walked into *int* |
| `retur` | `e` + spaces | r→e correct, then gone |
| `in` | `t` + spaces | n→t correct, then gone |

Every walk then falls into the space character and never leaves, at ~26 effort
per atom — the instrument correctly reporting that it is working hard and
arriving nowhere.

The cause is structural, not a defect in P8. **The walk carries one atom of
state.** Whitespace is the most frequent and most connected atom in source text,
so it is the strongest attractor in the graph; any greedy walk reaches it and
stays. After the cue's last atom is consumed, there is no context left to carry.

This is precisely the gap **P2 (composition)** exists to fill, and it is now a
measured requirement rather than an architectural preference:

> A memory addressed by single atoms cannot recall more than one atom's worth of
> anything.

It also gives the effort mechanic its first real validation. 26 effort per atom
is not noise — it is the number saying *this cue is hopeless*, and it says so
before a human reads the output.

---

## 8. Intake and dreaming

Added 2026-08-03 (P13, P14). These two pieces together give the architecture its
first observable behaviour and its first autonomous one.

### 8.1 — Ingestion is traversal, not import

The architecture already determines what ingestion does, and the answer follows
from §7.1 rather than being a new choice.

- Atoms are **types**, so reading a file adds no atoms — every atom it contains
  already exists in the base.
- Concepts are **ephemeral projections**, so reading a file stores no concepts.

Therefore **ingestion writes to the plastic layer and nowhere else.** Reading is
walking paths through the atomic base, and the walking is what strengthens them.

The file is not stored. **The trace of having read it is.**

The feed folder is consequently not a database import. It is an **experience
feed**, and it has a property worth stating explicitly:

> Reading the same file twice is not the same experience twice.

The second reading traverses paths that are already strong. It costs less, it
strengthens differently, and it may consolidate rather than discover. Ingestion
is therefore **not idempotent, by design** — and that is correct behaviour, not a
bug to be engineered away.

Opens **D14** — whether repeated ingestion is allowed, throttled, or detected and
reported.

### 8.2 — Wake and sleep as the system cycle

Intake and dreaming are naturally two phases of one cycle, and pruning belongs
with dreaming rather than with intake.

| Phase | Input | What happens to the plastic layer |
| --- | --- | --- |
| **Wake** | The feed folder | Traversed paths strengthen |
| **Sleep** | None — self-generated | Dreaming consolidates; pruning attenuates what nothing has touched |

Pruning during intake would attenuate paths while fresh material is still
arriving, competing with the very traversals that should be strengthening them.
Separating the phases avoids that, and it matches the biological metaphor the
architecture is already reaching for.

This also gives the system a natural clock: **experience, then consolidate.**

### 8.3 — Self-training collapses by default

This is the one genuine hazard in the concept, and it must be designed against
rather than discovered later.

A system that strengthens whatever it traverses, and traverses by preferring
strong paths, is a positive feedback loop. Strong paths grow stronger, traversal
concentrates further, and the reachable map narrows until the system loops on a
small attractor. It becomes obsessive. Everything it dreams is a variation of the
same thing.

This is the *default* outcome of dreaming, not an edge case. Three counterforces
are needed:

1. **A source of novelty.** Traversal during dreaming must not simply follow the
   strongest paths. Options include stochastic selection, deliberate preference
   for weak or attenuated paths, and affect-driven wandering. Deliberately
   walking *weak* paths is attractive here — it makes dreaming the counterweight
   to pruning rather than its accomplice.
2. **A lower learning rate than waking.** A dream must strengthen less than a
   real experience. Otherwise imagination overwrites what actually happened, and
   the mind loses the ability to distinguish the two.
3. **A criterion for which dreams count.** Without selection, every dream
   reinforces equally and dreaming is pure noise injection. See §8.4.

Note that counterforce 1 gives dreaming a clear role that nothing else in the
architecture provides: **rehearsing paths that are useful but rarely travelled.**
Pruning is usage-driven, so without dreaming the system inevitably forgets things
that are rare but important. Dreaming is what keeps them alive.

Opens **D15** (novelty mechanism) and **D16** (dream learning rate relative to
waking).

### 8.4 — What makes a dream count

The selection criterion should come from pieces the architecture already has,
rather than from a new subsystem. Two candidates, both of which respect the
decomposability invariant:

- **Affect coherence.** A dream that lands in a clean affective composition —
  roots combining into a coherent compound — succeeded. One that produces
  incoherent mush did not. This uses P6 directly and costs nothing extra.
- **Effort.** A dream that reached something valuable cheaply found a shortcut
  worth keeping. One that thrashed without arriving found nothing. This uses the
  effort mechanic introduced in §7.2.

Both are measurable, both are decomposable, and both are already paid for.

Opens **D17** — which criterion, or what combination.

### 8.5 — Illegible dreams are fine; illegible traces are not

The stated position is that a dream may be text no human understands, so long as
it serves the mind's own brain. That is acceptable as *output*.

It is not acceptable for the *trace*. If neither the dream nor its effects can be
observed, there is no way to tell whether dreaming is consolidating the mind or
destroying it — and per §8.3, destroying it is the default trajectory.

So the requirement splits:

- **Dream output** — may be arbitrary, illegible, non-linguistic.
- **Dream trace** — must be legible: which paths were walked, what affect was
  reached, what was strengthened, what was attenuated, how much effort was spent.

This makes P12 (instrumentation) a prerequisite of P14 rather than a nicety.
Dreaming without instrumentation is unobservable self-modification.

### 8.7 — What a dream leaves behind (author, 2026-08-03)

The phenomenology, in the author's words: a human dream often puts us in
**uncomfortable** situations; some are a fragment or a mixture, others so
coherent they are hard to tell from reality; **dreams generally lie about the
situation lived**, but they always leave a lesson or a synthesis — something to
think about — and *that* is what remains. Often it is forgotten; sometimes it
stays forever. Some dreams are remembered clearly, others serve only as training
and are gone before waking.

Four things fall out of this, and they change the design.

#### 8.7.1 — The content is false. Only the residue persists.

This is the important one, and it dissolves the §8.3 hazard at its source.

Razar currently makes no distinction between **what a walk went through** and
**what the walk left behind**. A walk *is* the thing: traverse a path and the
path strengthens. Under that rule, dreaming necessarily amplifies whatever it
happened to wander into, and the feedback loop is unavoidable.

The author's observation breaks it: the dream's episode never happened and
**should be discarded**. What persists is the synthesis. So a dream must not
write its traversals into the plastic layer at all — it writes only its residue.

That is a stronger and cleaner answer than §8.5's "illegible output is fine as
long as the trace is legible". The output is not merely illegible: it is
**false, and thrown away**.

#### 8.7.2 — The residue is attention, not truth

"A synthesis of thought, *what to think about*" is not a conclusion. It is a
direction of attention.

The distinction matters because it decides what a dream is allowed to write:

- **Strengthening** says *this is how it is.* A dream has no business saying that.
- **Marking for attention** says *look here again.* A dream is entitled to that.

So a dream's residue should raise **priority**, not intensity. Nothing fictional
enters the record of what actually happened; what changes is where the organism
looks next when it is awake. Reality still does all the strengthening.

#### 8.7.3 — Discomfort is the seed

Dreams confront us with what is uncomfortable — they rehearse the hard thing, not
the easy one. A dream that walked the strongest paths would be rehearsing what
the organism already does effortlessly, which is worth nothing.

Razar already knows exactly where it struggled, and has since spike 2:

| Signal | Where the discomfort is |
| --- | --- |
| `gave_up` | it could not get there at all |
| `effort` | it got there expensively |
| `descents` | it hit a wall and had to go around |

**Dreams should be seeded from failures, not from strengths.** That answers the
open question of what a dream walks, it is the exact opposite of the greedy
default, and it needs no new instrumentation.

#### 8.7.4 — Coherence is not reality

Some dreams are "so coherent they are hard to distinguish from reality". That is
a warning, not a feature.

Coherence and reality are **independent axes**, and the natural implementation
mistake is to treat them as one — to let a smooth, high-confidence, low-branching
walk count as more real than a fragmentary one. It is not. A seamless dream is
still false.

So: **reality must never be inferred from coherence.** `CUR_REALITY` is a
property of *where the experience came from*, never of how good it looked. A
perfectly coherent dream still carries dream-level reality.

#### 8.7.5 — The three tiers already have a mechanism

Forgotten before waking / remembered a while and then lost / kept forever maps
directly onto machinery the author already built in `organismo.c`:

| Tier | Mechanism |
| --- | --- |
| gone before waking | never reinforced; decays below threshold and dies |
| remembered, then lost | `born_dream`, held in quarantine, never confirmed by reality |
| kept forever | graduated by reality, then promoted to the core |

Nothing new is needed for the tiering. It needs porting, not inventing.

#### 8.7.6 — Which dream, decided by the state it woke up in (author, 2026-08-03)

The three jobs a dream does are not alternatives to choose between once. They
are answers to three different conditions, and the organism knows which one it
is in. In the author's words: *if it has got stuck, option 1; if something
important arrived that should be affirmed, put more force on option 2; if it has
been overstimulated or is stressed by what it received, option 3 — that way none
of them is discarded.*

So the night is triaged. Three counters are kept while awake, over the same
traffic that ingestion already produces — nothing is measured on purpose:

| Reading | What it is | Dream |
| --- | --- | --- |
| strain per step > `RZ_OVERWHELMED` | too much of what it met was a wall | **CALM** |
| births per step > `RZ_FERTILE` | it built a lot it has not exercised yet | **SETTLE** |
| neither | nothing new, nothing hurting: it is circling | **LOOSEN** |

Order matters, and it is not arbitrary: **drowning outranks everything.** You
cannot consolidate or explore while most of what arrives cannot be placed. The
same principle already appeared in the autopsy — support falling on its own is
the alarm and must outrank gentler readings (§8b.1).

Each kind seeds the walk from a different fragment and walks it at a different
temperature. Nothing else about the mechanism changes:

| Dream | Seeded from | Temperature | Because |
| --- | --- | --- | --- |
| LOOSEN | the most travelled fragment | 1.6 | the deepest rut is where a rut is escaped from |
| SETTLE | the newest fragment | 0.2 | the new is what has not been exercised |
| CALM | where merit is lowest against how often it was met | 0.8 | go back over the wall it kept hitting |

**Measuring strain against chance, not in absolute.** The first attempt read
stress as "how often the abstract layer had no prediction", and it was backwards:
random bytes are *easy* to call in the abstract, because every piece looks like
every other piece. It reported noise as restful. The second attempt used raw
surprise (`1 - p`) and saturated — from any fragment there are hundreds of
continuations, so even the right one carries little probability and everything
scored 0.97.

What works is surprise read **against a blind guess from that same place**:
`1 - p·degree`, floored at zero. Measured over real material the readings
separate cleanly and the bar can be placed on evidence rather than taste:

| Material | Strain per step |
| --- | --- |
| C source, first meal | 0.4151 |
| the same source, second meal | 0.2918 |
| the same source, third meal | 0.1989 |
| Spanish prose | 0.4443 |
| a compiled binary | 0.4475 |
| random bytes | 0.65 – 0.72 |

`RZ_OVERWHELMED = 0.55` sits above every real reading and below every
structureless one. **Meeting new structure is supposed to cost.** A bar under
0.45 would call ordinary learning an emergency, which is the failure mode this
whole section exists to avoid. Pinned by `learning_is_strain_but_it_is_not_stress`.

Fed real material over successive nights the organism settles while it is still
building, then — once the same material stops yielding anything new — starts
circling and asks to be loosened, without being told to.

---

### 8.6 — Where understanding would first become visible

If the system, while dreaming, connects two things it never encountered together,
and that connection later proves useful during waking, that is not a statistical
correlation being replayed. It is a connection the system produced on its own.

This is the earliest point in the architecture where the goal stated in §7.5
could show up as something observed rather than argued about. Worth watching for,
and worth instrumenting specifically.
