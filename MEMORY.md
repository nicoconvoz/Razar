# Razar — Memory Index

Durable knowledge for this project: decisions made, constraints agreed, gotchas
found. One line per entry, newest last. Anything a future session would waste
time rediscovering belongs here.

This index is a pointer, not a container. Entries longer than one line live in
`docs/decisions/` and are linked from here.

## Format

```
- [slug](path-or-inline) — the fact, stated so it can be acted on without reading further
```

Mark entries that must not be re-litigated with ⭐. Mark known-broken things with ⚠️.

---

## Identity

- **razar-is-artificial-reasoning** — Razar = *Razonamiento Artificial* (Artificial Reasoning). Name confirmed by the user 2026-08-02; it states the domain, not the stack.
- **vision-digital-organism** — a digital organism that reasons and learns, built on a low-cost memory model and a novel structure, running on any computer. Stated 2026-08-02.
- ⭐ **runs-on-any-computer-is-the-driver** — of the whole vision, this is the real design constraint: measurable (RAM, CPU without GPU, disk footprint, latency) and it eliminates entire solution families on its own. Treat it as a first-class requirement, not a nice-to-have.
- ⭐ **memory-model-is-the-first-target** — "low-cost memory" is falsifiable today: bytes per stored item, retrieval latency, recall quality. Reasoning is not. Build and measure memory first.
- ⚠️ **learn-anything-is-not-testable** — "reasons and learns almost anything" cannot have a failing test written against it. It is the hoped-for emergent property, not a requirement. Do not accept it as a spec.
- ⚠️ **invent-from-scratch-is-the-main-risk** — inventing a structure without knowing the state of the art produces a worse reimplementation of something that already exists, not novelty. Survey before inventing.

## Constraints

- ⭐ **user-drives-the-design** — the user designs the path, basic to complex, at their pace. During the vision framing phase: no testable-spec requests, no stack proposals, no code, no closing every reply with a question. Listen and record.
- ⭐ **scope-stays-in-razar** — all reading, searching, and writing stays inside `E:/proyectos/Razar`. Do not pull conventions or examples from other projects on this machine. If a needed file is missing here, create it here.
- ⭐ **no-assumed-stack** — the repository is empty and the stack is undecided. Never infer a language or framework from the project name or domain. Scaffolding an unrequested stack is a defect.
- **strict-tdd-enabled** — Strict TDD is on globally and applies here. Test first, confirm it fails, then minimum code, then refactor.
- **artifacts-in-english** — code, comments, identifiers, docs, and commit messages default to English regardless of conversation language.
- **conventional-commits-no-ai-attribution** — conventional commits only; never add `Co-Authored-By` or AI attribution trailers.

## State

- **bootstrap-zero-commits** — as of 2026-08-02: branch `master`, zero commits, no source, no manifest, no tests, no build config. Only `.atl/` and `.claude/` exist.
- ⚠️ **no-test-runner-yet** — Strict TDD is enabled but nothing can run it. The first implementation task after the stack is chosen must establish the test runner and a green baseline before any production code.
- **sdd-initialized-engram** — SDD is initialized with the engram artifact store. Stack detection correctly returned "none". `sdd-new` is the meaningful entry point; `sdd-explore` is pointless on an empty repo.
- **gentle-ai-2.2.4-clean** — gentle-ai v2.2.4 installed; `review status` returns clean and authoritative. Receipt-driven development is on by default with no explicit override. The switch is user-owned.

## Gotchas

- ⚠️ **sdd-subagents-lack-engram** — SDD phase sub-agents do not have Engram `mem_*` tools in their executor tool set, despite the MCP instructions describing them. They fall back to scratchpad files; the orchestrator must run `mem_save` itself and must not claim persistence succeeded until it has.
- ⚠️ **mem-save-explicit-project-fails-when-empty** — calling `mem_save` with an explicit `project: "razar"` fails with `unknown_project` while the project has zero observations, even though it is listed in `available_projects`. Omit `project` and let git-root auto-detection resolve it.

## Design

- ⭐ **ammc-is-the-starting-point** — Razar starts with memory, not reasoning. The **Concept Map Memory Architecture (AMMC)** is the author's long-held idea, stated 2026-08-02. Decomposed into 12 pieces and 9 open decisions in [docs/design/memory-concept-map-architecture.md](docs/design/memory-concept-map-architecture.md).
- ⭐ **atoms-are-types-concepts-are-projections** *(D1, D5 — resolved 2026-08-03)* — the atomic base is defined once, paid once, and bounded; it does not grow with experience. Concepts are neither stored nodes nor stored subgraphs: they are **coordinate projections computed on demand**, building only the part of the map the operation needs. The map is generated, not read. This dissolves the ×512 blowup — unmaterialized occurrences cost nothing.
- ⭐ **cost-moved-to-the-plastic-layer** — three layers, not two: bounded atomic base, **unbounded plastic state** (intensities, affect compositions, attenuation levels, traces of the pruned), ephemeral projection. Plasticity cannot live in a layer regenerated identically each time, so P5/P7/P8 all operate on the middle layer — the only one with no natural bound. "Runs on any computer" now applies *there*, not to the atoms.
- ⭐ **pruning-attenuates-and-revival-costs-effort** *(D2 — resolved 2026-08-03)* — pruning attenuates or compresses, never deletes. Revival is neither free nor binary: it costs **effort**, scaling with how attenuated the memory is. Effort doubles as a natural budget and give-up condition.
- ⭐ **color-is-the-contour-and-must-stay-invertible** *(D3 — resolved 2026-08-03)* — color is the contour of the data and a mode of expression, not a bolted-on attribute. Channel count is not the obstacle (3×8 bits = 16.7M variations). The real constraint: **mixing is lossy**, so storage holds the *composition* (roots + proportions) and color is what it renders to. Storing only the resulting color makes roots unrecoverable.
- ⭐ **every-layer-stays-decomposable** — the design invariant behind "understanding, not statistical response". A statistical model's representation is opaque and global; here a concept is a structure over atoms that can be walked, and the path taken is itself an object. The moment any layer stops being decomposable, that layer is statistical again.
- ⭐ **language-is-decided-last** — representation determines access patterns, access patterns determine what the runtime must be good at. Picking the language first is guessing and then being stuck with the guess.
- ⭐ **ingestion-is-traversal-not-import** *(P13, 2026-08-03)* — atoms are types so reading adds no atoms; concepts are ephemeral so reading stores no concepts. Ingestion therefore writes to the plastic layer and nowhere else. **The file is not stored — the trace of having read it is.** Reading the same file twice is not the same experience twice; ingestion is non-idempotent by design.
- ⭐ **wake-sleep-cycle-and-pruning-belongs-to-sleep** *(P13 + P14)* — wake ingests from [feed/](feed/) and strengthens what it traverses; sleep dreams, consolidates, and prunes what nothing touched. Pruning during intake would attenuate paths while fresh material is still arriving. Gives the system a natural clock: experience, then consolidate.
- ⚠️ **self-training-collapses-by-default** *(P14)* — strengthening what you traverse while traversing what is strong is positive feedback: the map narrows to a small attractor and the mind becomes obsessive. This is the expected outcome, not an edge case. Needs three counterforces: a novelty source (walking *weak* paths makes dreaming the counterweight to pruning rather than its accomplice), a lower learning rate than waking, and a criterion for which dreams count.
- ⭐ **dream-criterion-comes-from-existing-pieces** — two candidates, both already paid for and both decomposable: **affect coherence** (did the dream land in a clean composition of roots?) and **effort** (did it reach something cheaply, or thrash?).
- ⚠️ **illegible-dreams-fine-illegible-traces-not** — dream *output* may be arbitrary text no human understands. The dream *trace* may not: without seeing which paths were walked, what affect was reached, and what was strengthened or attenuated, there is no way to tell whether dreaming is consolidating the mind or destroying it. This makes P12 a **prerequisite** of P14.
- **instrumentation-has-no-prerequisites** — P12 is the only piece that can start immediately, and it is what turns "cheap without losing performance" into a measured number instead of an opinion. It is also now blocking P14.
- ⭐ **autopsy-is-a-diff-not-a-snapshot** *(P12 specified, 2026-08-03)* — see [docs/design/autopsy.md](docs/design/autopsy.md). "1.4M paths" means nothing; "1.4M paths, up 11%, while effective support fell 40%" means the organism is growing and narrowing at once. The system keeps its own history and the primary output is the delta. The first autopsy is the least useful one there will ever be.
- ⭐ **autopsy-must-not-perturb** — if reading state counts as traversal, measuring strengthens what it measures. The autopsy runs against a **frozen snapshot**, never the live organism, and its reads are non-plastic. Metaphor and engineering agree: it is performed on a copy and the organism survives.
- ⭐ **baseline-before-the-first-breath** — drift is only detectable against a starting point, so the first autopsy is taken on an organism that has ingested nothing.
- ⭐ **effective-support-is-the-collapse-alarm** — `exp(normalised traversal entropy)` = how many paths are *actually* in use. Effective support or reachable fraction falling while total path count rises is the collapse signature from §8.3.
- ⭐ **decomposability-is-pass-fail** — the affect panel's decomposability check must read 100%. It is an invariant, not a trend. Below 100% means a layer went statistical.

## Reference material

Extraction: [docs/design/reference-organismo-extraction.md](docs/design/reference-organismo-extraction.md). Source: `_referencia/organismo.c` (1617 lines) + its technical documentation. A working plastic concept-map mind, read in full 2026-08-03.

- ⭐ **graduated-reality-closes-d16** — every concept carries `level`: self-play 0.10 < school 0.30–0.90 < master 0.95, rising toward the reality of each reinforcement. Reality is a scalar attached to the *experience*, not a rate constant. A dream needs no special rule — it simply carries low reality and consolidates less. The counterforce to dream-collapse already existed.
- ⭐ **reality-confirmation-closes-d17** — `born_dream` quarantines dream-born concepts from the core; they graduate only when real experience touches them. Judges the dream *afterwards*, by whether the world agrees. No new metric, no threshold, cannot be gamed by a tidy-looking dream.
- ⭐ **context-wakeup-closes-d15** — a cold, low-priority sense wakes momentarily when its score for *this* situation beats the median of the active hot set, then sleeps again. The counterweight to usage-driven pruning is not randomness — it is **a second addressing route that ignores strength entirely**.
- ⭐ **the-folder-comes-from-what-is-happening** — the deepest idea in the reference. `module_key` is not a hash of the feature vector; it classifies the situation. *Being in the folder "defence of my queen" **is** perceiving the queen is in danger.* Classification is perception. This is how real-time projections stay tractable: never project the whole map, identify the situation and project inside its folder.
- ⭐ **merit-is-the-external-anchor** — two signals, not one: `strength` (recurrence) and `merit` (mean outcome quality in real games, slow EMA). Traversal intensity alone is a closed loop; merit is grounded outside it.
- **retention-separate-from-permanence** — permanence decided by merit, retention by use. Death requires three conditions at once.
- ⚠️ **lossy-affect-proves-d3** — `organismo.c:880-885` collapses five distinct affect sources into one float. Given `v_p=-0.4` nothing can say whether it feels bad from being eaten or from a wasted sacrifice. The cheapest thing to write, and it silently destroys what §7.5 requires.
- ⚠️ **hand-given-vocabulary-does-not-transfer** — the 48 features were supplied, and `situacion()` is hand-enumerable because chess has 73 situations. Razar starts from atoms with no given vocabulary. Harder, and the reference does not solve it.
- ⭐ **a-bar-that-rises-with-its-subject-measures-nothing** — coevolution against your own champion is a treadmill. Every panel in the autopsy design is *internal* and can look excellent while the organism is useless. A fixed external bar is missing (D23).
- **playing-it-is-the-lie-detector** — several regressions were caught by playing the organism, not by reading the dashboard. No instrumentation replaces contact with the thing.

## Spike 1 — measured, not argued

Built 2026-08-03. `make test` → 77 checks. Fed `_referencia/organismo.c`, the Spanish documentation, and 240 KB of one repeated phrase.

- ⭐ **type-not-token is real and visible** — `plastic_bytes` stayed pinned at 263168 across all five autopsies while the cost ratio fell monotonically 2.086 → 0.340 bytes of memory per byte eaten. The plastic layer did not grow by a single byte. The hand arithmetic in §3.1 is now an observed number.
- ⭐ **the second reading behaved exactly as predicted** — +0 distinct paths, traversals doubled, entropy identical. *"It discovered nothing new and lived it again."*
- **real text is sparse** — 126 KB of C source uses 120 distinct atoms of 256 and 3002 paths of 65536, with effective support 661. Of 3002 distinct paths, only ~661 are actually in use.
- ⭐ **the collapse alarm failed on its first real run** — the verdict required *paths up AND support down*. Obsessive material introduces no unseen pairs, so paths did not move and the alarm never fired while effective support fell 77%. Fixed: **a fall in effective support is the alarm on its own**, and it must outrank every gentler description, because obsessive input looks identical to consolidation from the outside. Threshold `RZ_COLLAPSE_DROP` = 5% relative, flagged as a first guess to calibrate.
- **running it found what reasoning could not** — the defect above was invisible on paper and obvious after one feeding. This is the method, and it is the same lesson as *"the lie detector is still playing it"*.

## Spike 2 — recall (P8)

Built 2026-08-03. `make test` → 107 checks. Recall follows the trace from the cue's last atom, spending **effort equal to each step's ambiguity** (`exp(outgoing entropy)` — the effective branching factor). Read-only: recalling never strengthens what it recalls.

- ⭐ **collapse is legible, not only measurable** — the mind fed 480 KB of one repeated phrase, asked to continue `"static "`, returns `rararararararara…`. It is inside its attractor and every path leads back in. The panel reported this as effective support 68.74; recall makes it readable. Both belong in the autopsy: **a metric persuades, a transcript convinces.**
- ⭐ **one atom of context is not enough** — on a clean mind, recall is right for one or two atoms and then dies: `q`→`ue`, `voi`→`nt` (it walked into *int*), `retur`→`e`, `in`→`t`. Every walk then falls into the space character and never leaves, at ~26 effort per atom. The cause is structural: **the walk carries one atom of state**, and whitespace is the strongest attractor in the graph. This is now a *measured* requirement for P2, not an architectural preference — *a memory addressed by single atoms cannot recall more than one atom's worth of anything.*
- **effort earned its keep** — 26 per atom is not noise, it is the instrument saying *this cue is hopeless* before a human reads the output.

## Spike 3 — fragments (P2)

Built 2026-08-03 after the author's correction: **the atom is the medium of decomposition, not the unit of meaning. The fragment — a conjunction of atoms — is what addresses the memory, and the path traffic is what makes it work.** `make test` → 155 checks.

Atoms are span-1 fragments. A pair that travels together 8 times **is born** as a fragment. Every fragment keeps `left`/`right` and its atom expansion, so **nothing is ever opaque**. Segmentation takes the widest known conjunction.

- ⭐ **the trigger is overlap, not suffix** — the first attempt looked for the fragment that was a *suffix* of the cue and failed: `"hello"` is not the end of anything, it is the **beginning** of `"hello world "`. The rule is: a fragment fires when some **suffix of the cue matches a prefix of the fragment**, longest overlap first. That unifies both readings — `"…ab"` against `"ab"` continues by paths, `"hello"` against `"hello world "` has `" world "` left to say. And a candidate with nothing to say and nowhere to go makes the cue **back off to a shorter overlap** — a hint that does not fire is tried smaller, not abandoned.
- ⭐ **orphaned fragments** — born early, then displaced by wider conjunctions; no traffic, nowhere to go. 229 of 2920. Dead weight, and **the first thing pruning (P7) should be pointed at.** Also: a fragment born at the threshold has no traffic yet — being known is not being usable.
- ⭐ **fragments recall what atoms could not** — `"for(int i=0"` fires the trigger **`"for(int i=0;i<"`**: it learned the whole idiom as one 14-atom unit by reading C. `"static "` recalls `const char* f`. Addressed by atoms, the same cues return nothing but whitespace.
- ⚠️ **greedy walking is periodic by construction** — all three cues end stuck in `" %ld\nspar_elo %ld\nspar_elo "`. It is not recalling wrongly; it recalls the most-travelled thing faithfully and then loops. Any deterministic strongest-path walk on a finite graph *must* cycle. The memory is fine — **the walk** is what needs something. D15 (novelty) is not a dream-only concern: it applies to every self-directed walk.
- ⚠️ **35187 paths evicted** by the 24-edge-per-fragment cap on 421 KB of material. The cap is tight and structure is being lost — counted, never silent.

## Spike 4 — memory modules

Built 2026-08-03 to relieve the squeeze. `make test` → 167 checks.

- ⭐ **the module was already in the design** — a fragment is born from `(left, right)` and therefore begins with whatever atom `left` begins with. Every fragment belongs to the **lineage of its first atom**, and that lineage *is* the module: 256 of them, related by descent rather than by a hash, **inherited at birth without anyone assigning it**. The `by_atom` index already was this. It is a partial answer to D25 for this domain — the folder comes from the composition structure, not from a hash.
- ⭐ **the squeeze is gone** — paths come out of a **pool shared across the module**, linked per fragment, so there is no per-fragment cap. Measured against spike 3 on the same 421 KB: paths refused **35187 → 0**, busiest fragment **24 (the cap) → 247**, fragments born **2664 → 3572**. Those ~900 extra fragments are structure that was previously discarded in silence.
- ⚠️ **the nest weighed 123 MB for 421 KB of material** — `mod_save` wrote the whole fixed 1 MB struct even for a lineage holding three fragments. Writing only the used portion took it to **949 KB, 130× smaller**. The lesson: *"low cost on any computer" has two axes and only RAM was being measured.* Disk counts too.
- **plastic bytes now reports the resident ceiling** (~8.2 MB), not growing memory. A fixed ceiling regardless of how much it eats is the entire point of paging.
- ⚠️ **greedy periodicity is untouched** — modules do nothing about it. Still D15.

## Spike 5 — lateral thinking, and two defects

Built 2026-08-03. `make test` → 184 checks. The author's framing: the nest will weigh a lot, but later, **when only relations remain**, it recalibrates with better paths and lateral thinking, and stops behaving like a **sponge** to behave like a **brain** that self-models and optimises paths.

- ⭐ **"when only relations remain" is implementable today** — `rz_frag` stores `left`, `right` **and** the flattened atom expansion (32 of its ~48 bytes). If the decomposition already lives in left/right, the flattened atoms are **redundant** — a matching cache, not the record. A mature organism drops them and keeps the relation, rebuilding atoms by walking down the tree. **Two thirds of the fragment table**, with the invariant untouched: the relation *is* the decomposition. Not implemented yet; this is the concrete road to the brain phase.
- **lateral thinking (D15)** — `rz_recall_lateral(..., temperature, seed, ...)` samples paths with `p ∝ w^(1/T)`. T=0 is faithful recall (identical to `rz_recall_frag`), T≈1 is proportional to how travelled each path is. **Deterministic given the seed.** As in the reference organism: what is measured stays at 0; wandering belongs to self-directed walks. Effort is paid the same — laterality is not a discount.
- ⭐ **composition swallows branch points** — fed `"hello world "` and `"hello there "` as separate buffers, each becomes one span-12 fragment with **degree 0**, and the choice inside it stops being a choice. Counterintuitive and real: **as the organism composes, decision points get absorbed into longer fragments and lateral thinking runs out of material to work on.** Pinned by `composition_can_swallow_a_branch_whole`.
- ⚠️ **eviction without a nest was silent data loss** — `mod_read` failed on re-waking and `mod_seed` reset the module empty; fragments and paths vanished without a word. A probe showed module `'g'` holding one fragment after 400 repetitions — `"go"` had been born and erased. Rule now: **a module that cannot be written down cannot be forgotten.** If it is dirty with nowhere to go, it is held open and the resident set exceeds its target. Counter: `held_open`.

## Spike 6 — other directions

Built 2026-08-03, from the author's observation that *a mind does not only look backwards and forwards*. `make test` → 195 checks.

- ⭐ **the memory holds four directions and recall was using one** — **forward** (what followed, the only one walked); **down** (its parts, recorded as `left`/`right` and never used); **backward** (what preceded, not recorded — needs a reverse index); **up** (what contains it, not recorded).
- ⭐ **going down costs nothing extra and fixes the dead end that killed the walk three times this session** — `rz_descend` returns a fragment's tail, or -1 for a bare atom. When branching is zero, recall drops into the tail and carries on from inside instead of giving up. A wide conjunction may lead nowhere while its tail has been everywhere: `"hello there "` has degree 0, but `"there "` has paths. Each descent is paid for in effort and counted in `rz_recall.descents`.
- **descent and laterality attack the same symptom from opposite sides** — laterality avoids the cycle when there *are* options; descent finds options when there are *none*. Complementary, not alternatives.
- **still missing**: backward (needs a reverse edge index) and up (needs a container pointer per fragment). Both cost storage, unlike down.

## Spike 13 — the night is triaged

Built 2026-08-03, from the author's rule: *stuck → option 1; something important
to affirm → option 2; overstimulated or stressed → option 3, so none is
discarded.* `make test` → 461 checks. CLI: `./razar sleep [seed]`.

- ⭐ **the organism picks its own dream from the state it woke up in** — three counters kept over the traffic ingestion already produces: strain per step, births per step, steps. Strain over `RZ_OVERWHELMED` → **CALM**; else fertile over `RZ_FERTILE` → **SETTLE**; else **LOOSEN**. Each seeds from a different fragment (deepest rut / newest / lowest merit) and walks at a different temperature (1.6 / 0.2 / 0.8).
- ⭐ **drowning outranks everything** — you cannot consolidate or explore while most of what arrives cannot be placed. Same shape as the autopsy rule that support falling on its own is the alarm.
- ⚠️ **the first stress signal was backwards** — "how often the abstract layer had no prediction" made **random bytes look restful** (90.6% called correctly vs 54% on familiar code), because noise is trivial to call in the abstract: every piece looks like every other piece. A signal that ranks noise as easier than structure is not a stress signal.
- ⚠️ **raw surprise saturates and decides nothing** — `1 - p` sat at 0.96–0.99 for everything. From any fragment there are hundreds of continuations, so even the *right* one carries little probability. It ordered the material correctly but with no usable gap.
- ⭐ **surprise has to be read against chance from that same place** — `1 - p·degree` floored at zero: "how much worse than a blind guess was it here". Now C source reads 0.42 → 0.29 → 0.20 across three meals, Spanish prose 0.44, a compiled binary 0.45, random bytes 0.65–0.72. **The bar is placed on that evidence: `RZ_OVERWHELMED = 0.55`,** above every real reading, below every structureless one. Under 0.45 it would call ordinary learning an emergency.
- ⭐ **a dream still writes nothing but attention** — the episode is discarded; only `attention` is marked, which spares from decay and itself fades (`RZ_ATTENTION_KEEP`). Pinned by `a_dream_writes_nothing_but_attention`.
- **it behaves like something alive without being told to** — fed the same material night after night it keeps asking to SETTLE while it is still building, and on the sixth night, with nothing new left to build, starts asking to LOOSEN.
- ✅ **no false zeros this time** — every new field (`awake_strain`, `awake_steps`, `awake_births`, `awake_anticipated`, `sleeps_had`, `dreams[]`) was added to `rz_save`/`rz_load` in the same movement. The rule from the three previous persistence bugs held.

## Next — P15, skills the organism finds for itself

Stated 2026-08-03, explicitly as the next step and explicitly **not to be built yet**. Design in [docs/design/skills.md](docs/design/skills.md).

- ⭐ **one skill is already hardcoded, and it is the only one it has** — a skill is a *way of cutting or addressing the material*, and both examples given (beginning/middle/end, syllables) are exactly that. Razar already has one, installed by us: **longest-match segmentation, left to right**. Every fragment that ever existed came out of it. So this is not a feature on the side — it is **making plural the one operation everything else was built on**.
- ⭐ **effort closes D24** — the reference organism judged a sense by duelling; Razar has no game. But it now has `effort`, `descents` and `gave_up`, already instrumented. A skill that makes a retrieval arrive where it used to give up has demonstrably helped. The effort mechanic, built for fidelity, turns out to be the fitness function.
- ⚠️ **minimising effort alone selects for obsession** — a skill that collapsed everything onto one path would score perfectly: free retrieval, one thought. Fitness must be effort **against a demand that does not move**, so **D23 (a fixed external bar) is now a prerequisite and must be answered before P15 is built, not after.**
- ⭐ **skills must stay decomposable** — no black boxes, or the layer goes statistical. They must be composed from a **small vocabulary of named primitives** so the autopsy can print what one does. The real design question is what that minimal vocabulary is: too small and nothing interesting is reachable, too large and the search is dead.
- **"the same system" is right and free** — modules already give wake/sleep, LRU, paging, and earning a place. Frequent/infrequent maps onto the hot/cold pool with context wake-up.
- ⚠️ **D30 decides whether this is cheap or ruinous** — a new segmentation does not re-cut what was already eaten. Re-reading everything for every new skill does not survive the standing constraint.

## Open

- **stack-undecided** — the language, runtime, and architecture for Razar have not been chosen. Everything downstream is blocked on this. Tracked as D9; resolve last.
- **d1-d9-open** — all nine AMMC design decisions are open. See the register at the end of the design document.
