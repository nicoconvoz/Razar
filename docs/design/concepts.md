# Concepts (P36)

Built 2026-10-11. `rz_concepts_find` in `src/lego.c`, `razar concepts [relation] [path]`.

> We have to feed it concepts, and find the mechanism for it to discover
> concepts. — the author

## What a concept is

Rex, Fido and the neighbour's dog are not alike, yet all of them bark and all of
them "are dogs". **A concept is what repeats across many different cases**: a
group of subjects that give the same answer to the same question ("X es ?"),
together with that answer. Not a word — a silhouette of answers shared by many.

## The mechanism is the one Razar already has, one floor up

| Floor | Unit | Adopted when |
| --- | --- | --- |
| bytes | lego | it pays for itself |
| legos | kind | it pays for itself |
| **facts** | **concept** | **it pays for itself** |

And "pays" is what the author chose Razar to want (D73):

- **harmony** — what follows each member is already described by what follows
  the rest of the concept (the member left out: it may not describe itself);
- **transfer** — in text it has not read, a member does something it was never
  seen doing, but the rest of the concept was.

Both against eight blind groups: every word of the concept swapped for a word of
about the same frequency, judged against the same real members. A concept is
adopted when the two gains add up above zero. **A group of one is an anecdote and
never forms.**

Nothing is declared: the question brings its relation, the answers come from
what the subject adds beyond the relation (D50, as in P34), and a concept is just
the subjects that share an answer.

## In the little world

Rex and Fido are dogs, Tom and Kit are cats, Lou alone is a cow; each animal has
its sound. Later, in text it has not read, Rex and Fido bark and Tom and Kit
meow — things none of them was ever seen doing by name.

| Concept | Members | Harmony | Transfer | |
| --- | --- | --- | --- | --- |
| dog | fido, rex | +0.104 | **+1.000** | adopted |
| cat | kit, tom | +0.167 | **+1.000** | adopted |

Lou forms nothing. This is the child's riddle 4 — knowing Rex is a dog and
expecting him to bark — answered in the small: the concept carried the bark.

## On the New Testament

Four fifths lived, the last fifth as what comes later; "X es ?": 27 concepts of
two or more, 16 adopted.

What reads as real:

| Concept | Members | Harmony | Transfer |
| --- | --- | --- | --- |
| Señor | «Jesús, «Jesucristo | +0.350 | 0.000 |
| Hijo | Jesucristo, él, revelado | +0.112 | +0.020 |
| ti | mano, pie, ojo, derecho, derecha | +0.156 | +0.109 |
| hombre | pedregoso, espinas, fértil | +0.285 | 0.000 |
| Dios | enseñanza, crecer | +0.236 | +0.312 |

*ti* gathers the parts of the body ("if your hand causes you to sin… for you");
*hombre* gathers the soils of the sower's parable ("the good ground is the man").

What does not: heads such as `que`, `la`, `el`, `,`, `:` — a subject whose
answer is the article of a varying noun gathers under the article.

**Mean over the blind groups: harmony +0.071, transfer −0.009.** Harmony is
there; transfer is zero. The concepts are tiny (two to six members) and judged on
a handful of steps each.

## Why: the New Testament is thin food for concepts

"Dios es amor" is said twice in the whole book. A concept needs many cases that
answer alike, and a narrative states few facts in the form "X es Y". The author's
point stands: **it has to be fed concepts** — material dense in facts, the kind
of text that defines and explains ("un perro es un animal que ladra").

The line with the master rule: such a text is **eaten**, like any material, and
the concepts are still found by Razar. A table of concepts loaded ready-made would
be declaring the answer, and enters only as the bar that grades.

## Open

- **D78** — food: a public-domain text dense in definitions (a children's
  dictionary or encyclopedia, in Spanish) to eat and measure on. Author's choice
  of material; nothing in `feed/` today is of that kind.
- **D79** — heads that are articles (`la`, `el`): the answer should be the word
  the article introduces. The subject of the frame (D69) and the kind of the
  answer (P33) are the two candidates.
- **D80** — concepts of concepts: dog and cat into "animal", the floor rising
  again. Not built.
- **D81** — curiosity toward the concept's frontier (riddle 1): a member missing
  a fact its siblings have is what to ask about next. The mechanism now exists
  to point at it; not wired to the player yet.
