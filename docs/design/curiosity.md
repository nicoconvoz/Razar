# Curiosity: asking, and asking why (P34)

Built 2026-10-08. `src/lego.c`, `razar ask <word> [relation] [path]`.

> Now comes the most important thing about children: curiosity. The questions,
> the "why?" — and that is when Razar understands.

## What was built

| Piece | What it does | Function |
| --- | --- | --- |
| words, found | a second split of the lego ends finds the separator | `rz_word_cut` |
| marks, found | the atoms that close a word, split off it | `rz_lexicon_read_words` |
| asking | read the other side of the relation the question brings | `rz_lexicon_ask` |
| why | ask again about the answer, until a circle or a gap | `rz_lexicon_wonder` |

Read as continuous writing: line breaks are one space (author's decision, see
below).

### Words are a second level of the same cut

The lego cut leaves stems apart (`Cri | sto`), so "Cristo" was never a lego and
could not be asked about. Splitting the atoms that end legos into two heights
again, the tall side holds **exactly the space and the line feed** — precision
1.000, recall 1.000 against the spaces. Nothing named the space: it is the
uncertainty peak of the uncertainty peaks.

### Marks are split off, without being named

"Cristo", "Cristo." and "Cristo," were three different words, and an answer said
once with a period never added up with one said with a comma. A mark is an atom
that does two things, each on the tall side of its own split:

- **closes** — followed by a separator nearly always, cut at the widest gap;
- **attaches** — preceded by many different atoms. The "g" of "dog" closes its
  word too, but only ever after an "o".

On the NT read continuously this finds `, . : ; )` and nothing else. A word
becomes its core plus its mark, each a word in the sequence.

**Lessons that came from getting it wrong**, three in a row, each found by
printing the raw words:

1. Multiplying "closes" by "attaches" let one make up for the other: "e" attaches
   to anything and closes half the time, so "que" came apart as `q` + `ue`.
2. The weighted optimal split is dominated by letters, which are most of the
   text; the cut fell between letters and `s`, then every letter, joined.
3. Growing the set round by round, a small vocabulary let whole words join
   letter by letter. The rounds were only needed because `\r` stood between a
   period and its separator; with line breaks read as spaces, one round does.

### Asking

The question brings its relation (`es`) and its subject. An answer is what the
subject **added** beyond what the relation gives by itself (D50): for each place
around the relation — just after, one further, just before with the subject on
the other side — how much more a word stands there when this subject is asked
about. No article is named or skipped; "el" follows "es" whatever is asked.

Ranked by that lift, not by volume: weighted by volume, `el` (10 times after
"Jesús es") outranked `Mesías` (3). Said once is an anecdote and ranks behind
anything said twice — the same rule the kinds use.

### Why

The child's "why?" is asking again about the answer. It stops when every answer
leads back to something already asked (a circle closed) or when nothing answers
(a gap: the question stays open).

## What it says, on the New Testament

| Asked | It answers | Asked why |
| --- | --- | --- |
| ¿Quién es **Dios**? | amor, testigo, veraz; and "Cristo es de Dios" | amor → paciente, plenitud |
| ¿Quién es **Jesús**? | **Mesías** (3), **Hijo** (3), Cristo | Mesías → hijo (of David) → hombre → … |
| ¿Quién es **Señor**? | para, porque, Espíritu, tierra | drifts: ojo, cuerpo |
| ¿Quién es **Cristo**? | que, la, anunciado, paz | drifts: que, qué, lo, también |

Where the text says it plainly and more than once, it finds it: *Dios es amor*,
*el amor es paciente*, *Jesús es el Mesías*, *el Hijo*. Where the text says it
once, or through a function word, the chain drifts.

## Does it understand? Measured: not yet

Understanding was defined before building, so it could not be argued afterwards:
**an answer is understood when it lets it predict what it never saw.** If "rex is
a dog" and "the dog ran" was seen, "rex ran" should no longer surprise it.

Measured with every frequent word allowed to be the relation (words at spaces,
four fifths learned, the fifth it never read; 11,115 steps memory cannot reach):

| Relation | Right where memory never saw | Always the commonest |
| --- | --- | --- |
| de (best) | 1.72% | 3.38% |
| y | 1.07% | 3.38% |
| **es** (rank 23 of 49) | 0.26% | 3.38% |

**No relation transfers.** Borrowing what follows the answer predicts worse than
naming the commonest word. And the similarity reading of a relation failed too:
the words that join the most alike things are `sino` and `pero` (contrast), with
`es` at rank 26 of 66.

So it can ask and it can answer; the answers do not yet change what it expects.

## The author's decision recorded here

**Line breaks are read as continuous writing** (2026-10-08): a run of line
breaks is one space. It is applied at the intake of `legos` and `ask`; `feed`
still eats raw bytes. This is a decision about the material, not something the
organism discovered — recorded as such.

## Open

- **D69** — the chain drifts through function words ("que", "lo", "para") when an
  answer comes from behind ("___ es Cristo"). Backward answers need the subject
  of the frame (D67/D68), not the word before the relation.
- **D70** — understanding does not transfer as next-word prediction. Either the
  measure is wrong (an answer should change what *kind* it expects, or which
  questions it can answer, not the next word) or the answers are too few: *Dios
  es amor* is said twice in the whole NT.
- **D71** — `?` and `!` are not found as marks: in this text they are followed by
  a closing quote more often than by a space. A mark that closes into another
  mark needs the round-by-round growth, without its cascade.
- **D72** — it asks only when asked. Curiosity on its own would start the chain
  from what surprised it — the inner conversation (P32) already knows where it
  was wrong — and keep the gaps as open questions to read for.
