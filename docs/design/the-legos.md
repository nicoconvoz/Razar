# The legos (P33)

Built 2026-10-08. `src/lego.h`, `src/lego.c`, `razar legos [path] [kinds]`.

## Why this, and why now

On the fifth of the New Testament it never read, Razar names the next piece
correctly 2.76% of the time — 1.1× a blind guess among the ways out. And 59% of
the steps go to a piece it had **never seen follow the one before**. No amount of
memory reaches those: there is nothing stored to recall.

The author's diagnosis: the problem is not remembering, it is understanding. A
child who meets a word for the first time knows it is a noun from where it sits,
and knows a noun comes after "the" before knowing which one. The organism should
know **what legos it has**, recognise a new lego by its context, and then play
with them.

Three steps, each built on the one before.

| Step | What | Function |
| --- | --- | --- |
| 1 clean legos | cut where a piece really ends | `rz_lego_cut` |
| 2 kinds | same company, same kind; a new lego known by its neighbours | `rz_lexicon_sort`, `rz_lexicon_guess_kind` |
| 3 play | guess the kind that comes next, then the lego inside it | `rz_lexicon_next_kind`, `rz_lexicon_next` |

All of it is read-only on the mind: the lexicon is a separate thing built by
looking (tested: reading legos moves no traversal).

---

## Step 1 — clean legos

The kinds probe first ran on Razar's own fragments and came out mixed. The reason
was the fragments themselves: a fragment like `s, y ` swallows the end of one word
and the start of the next, so it has no single kind.

**Rule.** A lego ends at byte *i* when the mind's uncertainty about what follows
`t[i]` (the entropy of the atom-pair row, `rz_branching`) is a local peak, **and**
the peak is tall. The peaks fall into two heights — small rises inside a word and
tall ones between words — and the split between them is the one that best
separates the two groups (the exact optimum over every height, weighted by how
often each byte peaks). The text decides where; no threshold is written.

**The spaces are the exam, never the rule.** Two tests hold this:

- `a_lego_never_crosses_from_one_word_into_the_next` — every space ends a lego.
- `the_cut_never_looks_at_the_space` — the same world written with `_` between
  words is cut in exactly the same places.

**Measured** on the held-back fifth of the NT, against the spaces:

| Candidate | Precision | Recall |
| --- | --- | --- |
| every uncertainty peak | 0.492 | 1.000 |
| piece-level branching, span ≤ 2 | 0.459 | 0.879 |
| **tall peaks, optimal split** | **0.817** | **1.000** |

Recall 1.000 means no lego ever crosses a word boundary — the defect that broke
the kinds is gone. The remaining 18% are extra cuts *inside* a word, mostly
before a suffix: `Espíri | tu`, `impusi | mos`, `ocasi | ón`. Those are not
random; they are where the word's stem ends.

### Lesson that came from getting it wrong

The first version split the heights with two-means started at the lowest and
highest peak. On 60 KB it scored 0.809 — and on the whole text it fell to 0.50,
because a handful of rare bytes with extreme uncertainty captured one of the two
centres. **The 0.809 was the luck of the stretch measured.** The exact optimal
split gives 0.809 / 0.814 / 0.817 / 0.824 on 60 KB, 200 KB, the whole held fifth
and the four fifths: it no longer depends on the length of the text.

---

## Step 2 — kinds by company

Two legos are the same kind when the same legos tend to stand just before them
and just after them. Nothing says what a noun is.

- A lego's **company** is a sparse vector: its left and right neighbours among the
  256 commonest legos (`RZ_LEGO_COMPANY`), on log counts. Every counted pair
  touches exactly one cell of two vectors, so all of it is read straight off the
  pair table — nothing dense per lego is ever stored.
- **k-means** (k-means++ start, cosine, deterministic for a seed) over the legos
  seen at least twice; a single sighting is an anecdote. Every other lego with any
  company is placed against the kinds found.
- **A lego never met** gets the kind that best explains its neighbours:
  `P(kind | kind before) · P(kind after | kind)`.

What it found on the NT (40 kinds, by commonest members), with no dictionary:

| Kind | Members |
| --- | --- |
| 3 | `es` `son` `he` `será` `sea` `soy` `hay` `puede` — forms of *to be / can* |
| 7 | `judíos` `hombres` `hermanos` `sumos` `días` — plural nouns |
| 8 | `hombre` `Señor` `día` `Espíri` `cami` — singular nouns (and noun stems) |
| 0 | `«¿` `«No` `«Señor,` `«El` `«Yo` `«Maestro,` — the opening of speech |
| 4, 5 | `eron.` `nada.` `Jesús.` `allí.` `no.` — the end of a sentence |

**Measured**, hiding a lego and reading its kind from its two neighbours only:

| Kinds | Recognised | Always the same kind | Lift |
| --- | --- | --- | --- |
| 16 | 40.2% | 25.8% | 1.56× |
| **40** | **40.9%** | **28.2%** | **1.45×** |
| 80 | 47.5% | 41.0% | 1.16× |

Recognising a lego by where it sits is **real**.

---

## Step 3 — play

Guess the kind that follows this lego's kind; inside that kind, play the lego that
followed this one most, else the commonest of the kind.

**Measured** on the held fifth (40 kinds), each baseline built from the four
fifths only:

| | Steps | Kind right | Always the same kind | Lego right | Memory alone |
| --- | --- | --- | --- | --- | --- |
| pair seen before | 25,408 | 38.8% | 30.3% | 16.5% | **24.4%** |
| pair **never** seen | 9,712 | 22.8% | 22.6% | 0.4% | 0.0% |

Two honest verdicts:

1. **The next kind is not predicted where it matters.** On the pairs it never
   saw — the 59% problem this piece was built for — guessing the kind is no
   better than always guessing the same kind (22.8% vs 22.6%). The same holds at
   8, 16 and 80 kinds. Knowing *what this lego is* works (step 2); knowing *what
   kind comes next* from a single kind before it does not.
2. **Playing kind-first loses to plain memory** on the pairs it has seen (16.5%
   vs 24.4%): every wrong kind forbids the right lego. Kind-first is a filter,
   and a filter with a 61% error rate throws away more than it saves.

---

## The sentence as a frame (author's idea, 2026-10-08)

> Every sentence starts at a capital and ends at a period, has a subject and a
> predicate most of the time, and that structure is a recognisable pattern across
> many variants. That can order it further.

Under the master rule the capital and the period cannot be declared; they enter
as the bar. So the question measured was: **does the frame find itself?**

**What already emerged.** Step 2 found the sentence's edges with no help: four
kinds are made of sentence endings (72–92% of their legos hold a period, against
3.9% overall), and one kind is the opening of speech (`«¿` `«No` `«Señor,`).
The pieces of the frame are there as kinds.

**What did not.** Two readings tried to find the edges *as edges*, one level up
from the word cut:

| Reading | Where it pointed | Share holding a period |
| --- | --- | --- |
| the past stops informing the future (MI of kind before / kind after, low) | nouns and determiners | 2.7% (base 3.9%) |
| the next lego is most uncertain (entropy after the kind, high) | determiners: `sus` `la` `este` | 1.7% at the top |

Neither finds the period. At the level of kinds, the most uncertain place is not
the end of a sentence but **after an article** — anything may follow "la". The
word cut worked because uncertainty peaks where a word ends; one level up, the
same reading picks the noun slot, not the frame.

**Slot in the frame**, counted from the closers the MI reading chose, adds
nothing to predicting the next kind (34.1% vs 34.3% from the kind before; always
the same kind 28.2%). Two kinds before, with no frame at all, gives **36.0%** —
small, and the only lift measured.

So the idea stands and the pieces are found, but the way to *read* the frame is
not yet found. Recorded as D67.

### The atoms that open and close it (author's correction)

> It must identify the atom that gives origin and end to a sentence — or the atoms.

Not the kind: the **atom**. Measured on bytes alone (probe `atomframe.py`), with
capitals and periods only as the bar:

- **Openers.** An atom that opens a frame always stands after the same few
  things. For every atom, the entropy of the atom two places before it; the exact
  optimal split of those entropies picks the concentrated ones. **19 of the 22
  common capitals** come out, plus the bytes of `«` `¿` and the line feed — the
  other ways a frame opens in this text.
- **Closers.** What the openers depend on: the atoms that stand two places before
  them, ranked by how much more often than usual. In order: **`.` 12.2×, line
  end 8.9×, `!` 5.7×, `?` 5.3×**, the bytes of `»`, then `;` 3.6×. The space,
  which is everywhere, scores 1.8×.
- **Bar.** Two atoms after a `.` there is an opener 96.0% of the time; after any
  atom, 7.9%.

So the atoms that begin and end a sentence **are** discoverable without being
named. One number in the probe is still declared: the distance *two* (closer,
separator, opener). It should be read off the material — the distance at which
the concentration is sharpest — before this goes into `src/`.

### Capital as a property, whatever the letter (author's next step)

> Now distinguish that one property is being a capital, no matter the letter.

`E` and `e` keep the same **right** (`El`/`el`, `En`/`en`) and differ in their
**left** (a capital stands after a closer). So each opener is paired with the
non-opener atom whose start-of-lego right — the two atoms after it, wherever a
lego begins — is most alike (probe `capital.c`; letter case is the bar):

- **17 of 20 openers pair with their own lowercase** (`A→a` … `Y→y`, `Q→q`
  at 0.97). A blind pick among the candidates would be about 1 in 40. The three
  misses: `G→t`, `U→e` — `U` could not find `u` because `u` itself was wrongly
  taken for an opener (it stands after `q`) — and `u→e`, that same false opener.
- **It is one property, not seventeen.** Take what separates each capital from
  its twin (the left of one minus the left of the other) and compare pair against
  pair: **0.45** alike, against **0.08** between two plain letters. The pairs
  differ *in the same way*.
- **And that way points at the frame.** The shared difference is, in order:
  line end, the first byte of `«`/`¿`, `.`, `?`. Being a capital means *standing
  where a frame opens* — the property is the position, carried by the letter.

So the organism can tell, without being told, that `E` is `e` plus something,
and that the something is the same for every letter. Not built into `src/` yet.

---

## Open

- **D62** — what comes next is not decided by the one kind before. The kind *of
  the whole stretch* (a sentence opening, a speech, a list) is the obvious
  candidate; one previous kind is to grammar what one previous byte was to
  words.
- **D63** — kind-first must stop being a filter. The kind should vote, the way a
  rule votes (P31), and memory should keep the floor where it has seen the pair.
- **D64** — the extra cuts inside a word fall where a stem ends (`Espíri | tu`).
  That is P28's root+suffix again, reached from the other side; whether the
  lego cut and the floor that rises should be one mechanism is unanswered.
- **D65** — the lexicon is built by the `legos` command from material, not while
  eating. The organism does not yet carry its legos with it as it lives; that
  needs it to build them online, the way senses learned to score online.
- **D66** — the number of kinds is still chosen (40, not tuned). Harmony (D39)
  should be the one to say how many kinds pay for themselves.
- **D67** — the sentence as a frame. The edge kinds emerge by company, but
  neither "the past stops informing" nor "the next lego is most uncertain" finds
  them as edges. Candidates not yet tried: the frame as what **repeats as a
  whole** (a sequence of kinds that recurs across many sentences, the way a
  fragment is a sequence of bytes that recurs), and subject–predicate as two
  halves that each recur on their own.
