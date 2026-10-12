# Weaving (P38)

Built 2026-10-11. `rz_lexicon_weave` in `src/lego.c`, `razar weave [type] [path]`.

> Remember: Razar only sees music in the concepts. It is an abstract structure
> that forms and deforms until it finds a new one in another. Now mother,
> brother and sister — and let it be able to discover whatever it is, and weave
> new structures without our telling it. — the author

## The principle: the same music, deformed

"**su** madre" is "la madre **de** X" with the owner moved in front. Because the
owner is already there, the "de" vanishes. That is how a new binding shows
itself: it is a word that **takes the place of a part the structure already
has**. An article does not: "**el** hijo **de** David" keeps both.

Measured once, before building, on the NT (after a type, "de" follows 57% of the
time):

| Before the type | "de" after | |
| --- | --- | --- |
| su | 3% | takes the owner's place |
| mi | 5% | takes the owner's place |
| tu | 12% | takes the owner's place |
| la | 59% | replaces nothing |
| el | 67% | replaces nothing |

## The loop

Starting from one type the question brings ("hijo") and its binding (P37):

1. **types** — for every binding, the words on the tall side of the share of
   their life they spend bound that way, seen bound at least twice;
2. **candidates** — every word just before or just after a type, more than it
   stands there in general, seen twice;
3. **replacement** — how often the binding on the **other side** of the type
   survives the candidate, relative to the base rate, with one imagined case at
   the base rate (three cases cannot say 0%). Replacing means making it rarer:
   below 1 by definition; among those, the low side of the split;
4. **strength** — among the replacers, the tall side of lift × log(count);
5. new bindings are woven in; repeat until a round adds nothing, or 64.

No word is named; no threshold is chosen. Every cut is a split of the material's
own values.

## In the little world

Sons, kings and friends are "of" someone, and sometimes "his"; mothers, sisters
and wives are only ever "his" or "her". From "son": **his** and **her** are woven
in, "the" and "a" are not; mother, sister and wife become types, dog and rex do
not.

Without the bridge — if "his" never stood before a type it already knew — nothing
could show that "his mother" is the same music as "the son of". The first version
of the test world had no bridge and failed, rightly: the New Testament has it
("su hijo", "su padre").

## On the New Testament

From "hijo", 64 bindings in five rounds:

| Round | Woven in |
| --- | --- |
| 1 | su (plus Buena, cada) |
| 2 | tu, mi, Su, Mi, nuestro, nuestra, este, esa, aquella, propia, propio, «Tu, querido |
| 3 | ese, esta, otra, misma, del, «Mi |
| 4 | un, mismo, sola, tal, segunda |
| 5 | sus, mis, aquel, ningún, alguna |

Types: **hija, padre, madre, hermano, hermana, esposa, marido, amigo, rey** are
in; **perro, Jesús, dijo, y** are out. 955 types in all.

## The melody carried to another text (2026-10-11)

> Take another text and try it with the learned melody. — the author

The other Spanish text on board: the technical documentation of the chess
organism (`feed/DOCUMENTACION_TECNICA.md`, 6,820 words; another subject, another
register). The melody woven on the NT from "hijo" was carried by spelling —
**no seed, no weaving again** — and read by a mind that had read only the new
text. Probe: `melody.c` (scratchpad).

- 22 of the 64 bindings occur in the new text.
- Types it hears there: **rey, dama, piezas, valores, función, pesos, motor,
  evaluación, derechos** — the nouns that take an owner in a text about chess
  ("valores de pieza", "función de valor"), with nothing about chess taught.
- **The bar.** Types found in the first half; in the second half, not yet read,
  they take an owner 48% of the time against 6% for every other word: **8.25×**.
  Eight blind melodies — every binding swapped for an NT word of the nearest
  frequency, same side — reach **5.26×** on average.

The learned melody beats the blind ones by 1.6×, on only 25 occurrences in the
second half. It carries; the text is too short to say by how much. A blind
melody scores high too because any word predicts its own company from one half
to the other — the bar is not zero, and the margin over it is what counts.

### And to a novel

The author then put a longer text on board: Horacio Quiroga, *Historia de un amor
turbio* (`descargas/`, 23,658 words) — a Buenos Aires novel, nothing like the NT.
The same melody, carried the same way:

| | Cases (second half) | Types bound | Other words | Lift |
| --- | --- | --- | --- | --- |
| melody woven on the NT | 711 | 31% | 8% | **4.14×** |
| eight blind melodies | | | | 1.96× |

**2.1× the blind bar, on 711 cases.** 44 of the 64 bindings occur in the novel.
The types it hears there are the owned things of a love story: amor, boca, casa,
novia, cara, hermana, mirada, brazo, hija, padre, hijo, corazón, cuello, beso,
amigo — "su boca", "su hermana", "su padre". The structure learned on scripture
recognises the owned things of a novel it never read.

Noise that came along: `.` and `—dijo` (the dash of dialogue is not yet found as
a mark, D71), and "después", "dejar".

## P39 — The melody: it always weaves the new (2026-10-11)

> Let it weave — always let it weave the new. — the author

`rz_melody_carry`, `rz_melody_learn`, `rz_lexicon_weave_from`, `razar melody <path>`;
kept in `nido/melody.dat`.

The bindings are kept as **notes** — a side and a spelling — so they belong to no
text. Every text it reads, it carries the melody in, weaves on from it with no
seed, and keeps whatever new note that text gave. A note is never forgotten.

### Without a judge, weaving never stopped

Given room for 512 bindings instead of 64, the loop filled all 512 on the NT —
plurals after the type, verbs before it, anything. The cap had been hiding it
(D87). With the melody full there was no room left for a second text to add
anything, and on the chess documentation the woven melody predicted *worse* than
the one carried in (5.22× → 3.21×).

**The judge** is the behaviour P35 already had — repeat what works, let go of
what does not. It weaves on the first half of what it read and keeps a round only
if, on the second half, being a type says **more** about taking an owner than it
did before: the mutual information of the two. That rewards a melody that is
both right and wide; one that binds everything to everything says less.

### The voyage, measured

Each text: the melody woven on its first half, judged on its second half (not
read), against eight blind melodies of the same frequencies.

| Text | Notes that sound | Woven new | Melody | Blind | Cases |
| --- | --- | --- | --- | --- | --- |
| New Testament (from "hijo") | — | 46 | **12.12×** | 1.29× | 3,471 |
| Quiroga, *Historia de un amor turbio* | 30 of 46 | none passed the judge | 2.95× | 2.55× | 1,059 |
| chess documentation | 12 of 46 | en, rival | 15.05× (carried alone 18.45×) | 7.13× | 18 |

What it shows:

- **On the NT the judge turned noise into structure**: 12× against 1.3× blind,
  where the unjudged 512 gave 3.17×.
- **It wove a second music by itself.** Besides the possessives, the NT melody
  holds *ha, han, hemos, había, habían, haya, he, has* and *será, sea, ser*
  before the type — auxiliaries before a participle. The same test found a
  different structure, nobody pointed at it.
- **Quiroga added nothing.** No round passed the judge, and the margin over the
  blind melodies is small (2.95× vs 2.55×). The earlier, unjudged 64-binding
  melody carried better to this novel (4.14× vs 1.96×): what the judge cut on the
  NT was partly what travels.
- **The chess text is too short to read** (18 cases): weaving "en" and "rival"
  passed the judge inside the first half and then predicted a little worse on the
  second.

So it always weaves, and now it stops by itself; whether the judge should also
weigh what *travels* to other texts, and not only what holds within the text it
learned on, is open (D90).

## P40 — Why it worked here and not there (2026-10-11)

> Let it ask itself why it worked in one text and not in another — see the
> differences, draw conclusions. I expect the melody to write itself and rewrite
> itself. — the author

`rz_melody_judge`, `rz_melody_why`, `rz_melody_remember`, `rz_melody_trusted`;
wired into `razar melody`.

Every note now keeps a history: in how many texts it sounded, in how many it
**held**, and — where it held — what work it did: the share of its uses that
stand next to a type. In every text it reads, every note that sounds is judged
on its own: does it tell, in the half not read, which words take an owner,
better than a blind word of its frequency on its side? A note that held
elsewhere and fails here is explained by the difference:

| Conclusion | Measured as |
| --- | --- |
| silent | it does not sound in this text |
| other work | it sounds, and stands next to a type less often than where it held |
| tells nothing | same work as before, and still no information here |

The melody rewrites itself from this: it weaves only with the notes it trusts —
held at least as often as they failed — and a note is never deleted, so one that
was let go comes back when a later text finds it holding.

**First lesson from getting it wrong:** a note was first judged against the types
the whole melody marks — including the ones it marks itself. It always agreed
with itself. Each note is judged against the types **the rest** of the melody
marks, as a concept's member is judged against the others (P36).

### What it concludes

On Quiroga, after the New Testament, among others:

> **ha ___** held before, fails here: other work — next to a type 10% here, 47%
> where it held.
> **Su ___** held before, fails here: other work — 17% here, 67% where it held.
> **mi ___** failed before, holds here.

In the NT "ha" stands before participles the melody treated as types ("ha
sido", "ha resucitado"); in a novel of Buenos Aires it does not. In the chess
documentation "su" and "mi" are next to a type 0% of the time: possessives are
rare in technical prose and do other work there. These are the differences, and
they are read off the texts, not written in.

### Does the rewritten melody carry better? Not yet

The honest test is a text read for the first time, after lessons from other
texts. Quiroga read first after the NT alone: 3.14× against 2.86× blind; after
the NT and the chess documentation: **3.14× against 2.86× — the same**. The
chess text (18 cases) is too short to teach anything Quiroga uses: none of the 22
trusted notes changed.

Reading Quiroga a second time gave 10.78× against 3.20× — and **that number does
not count**: the lessons of the first reading came from the same held half it was
then measured on. It is recorded here only so that it is never quoted.

## Lessons that came from getting it wrong

1. **A word after the type always "replaces" "de"** — it is standing in de's own
   place. The first loop let the comma and the period in as bindings and the
   types grew to 2,257. Replacement is only measured against the binding on the
   *other* side.
2. **A ratio above 1 is not replacement.** With an open split the cut let "la"
   (1.13) and "el" (1.19) in: with them, "de" is *more* likely, not less.
3. **Three cases are not evidence.** Rare adjectives seen twice ("cilicio") read
   0% and walked in; one imagined case at the base rate fixed most of it.

## Open

- **D86** — noise still woven in: rare adjectives after the type ("sacerdotal",
  "adoptivos"), verbs before it ("quedó", "tener"), and "Buena" (of "Buena
  Noticia"). The candidate's own kind — does it itself behave like a binding
  elsewhere — is the next filter.
- **D87** — *answered by P39:* the loop now stops by itself — a round is kept
  only when it pays on the half it did not weave on.
- **D89** — *measured:* the melody carries — 1.6× the blind bar on the chess
  documentation (25 cases), **2.1× on Quiroga's novel (711 cases)**. Still open:
  whether weaving again on the new text, starting from the carried melody, adds
  the bindings that text has and the NT does not.
- **D90** — the judge looks only inside the text being read. What it cut on the
  NT was partly what carried to Quiroga. A melody that has crossed several texts
  could judge a note by whether it holds in all of them.
- **D91** — the rewriting has not yet improved a first reading: three texts, one
  of them too short. A fourth text, and a fifth, read each for the first time,
  are what can show whether lessons carry.
- **D92** — "other work" is read as one number (share next to a type). What the
  other work *is* — "ha" before a participle in one text, before a noun in
  another — is the next question to ask, and the answer would be a new melody.
- **D88** — this weaves one structure (type + owner). The same loop, started from
  any seed and any binding, is how any other structure would be woven; nothing in
  it is about kinship. Not yet run on other seeds.
