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
- **D87** — the cap of 64 is reached in round 5 and "hermanos" stays out: "sus"
  arrives in the last round. Whether the loop should stop by itself (nothing new
  pays) rather than at a cap.
- **D88** — this weaves one structure (type + owner). The same loop, started from
  any seed and any binding, is how any other structure would be woven; nothing in
  it is about kinship. Not yet run on other seeds.
