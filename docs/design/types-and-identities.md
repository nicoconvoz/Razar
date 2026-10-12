# Types and identities (P37)

Built 2026-10-11. `rz_lexicon_roles` in `src/lego.c`, `razar roles [type] [path]`.

> It has to discover the silhouette that son, father, brother, sister, friend,
> uncle, aunt, mother… go attached to a single name — that "hijo" is the type
> and the identity is the name. It has to discover it. — the author

## The difference is an asymmetry

- A **type** asks for an owner: "hijo **de** ___", "rey **de** ___". Its slot is
  filled by many different names.
- An **identity** fills slots: David is "hijo de David", "casa de David", "rey
  David". It stands in the slot of many different types.

Nothing has to name either side:

| Step | How it is found |
| --- | --- |
| the binding | what follows the seed type far more than it follows anything, said twice or more — "hijo" → **de** (60% of its occurrences) |
| types | words followed by the binding on the tall side of the optimal split, at least twice |
| identities | fill the slot of 2+ different types, **and** spend the tall side of their life in those slots |

The second condition on identities is D50 again. "la", "su", "los" fill the slot
("hijo de **la** mujer") more than any name does — and they live everywhere. An
identity is what the slot gives that the rest of the text does not.

## Measured on the New Testament

Capital letters are the bar only: of all words seen twice or more, **11%** start
with a capital.

| | Count | With a capital |
| --- | --- | --- |
| identities | 48 | **50%** (4.5× the bar) |
| types | 563 | 7% |

Identities, without being told what a name is: Asia, Leví, Judá, Zebedeo, Tiro,
Melquisedec, Chipre, Jesé, Galacia, Creta, Fares, Aminadab, Naasón, Booz,
Salatiel, Zorobabel, Aarón, Manasés, Zabulón — the genealogies and the places of
the letters.

Types include hijo, padre, hijos, rey, reyes, dueño, familia, Reino, Iglesia —
and also *después, delante, acerca, alrededor, cerca*: everything that asks for
"de" behind it. That is correct about the language: those words take an object
too. **Kinship is a part of the types, not the whole of them.**

### Before it was built, the probe that failed

Asked first through the silhouette of "hijo" (P36's company similarity), the
family did not come out: its nearest company is padre (0.81), hermano (0.78) —
and then servidor, espíritu, rey, dueño. Company does not separate "a relative"
from "a thing that belongs to someone"; and *madre* went with *familia, cabeza,
casa*, because the feminine ("la madre") changes its company. The asymmetry is
what separates type from identity; company does not.

## What it is not yet

- **madre** (23% "de") and **hermano** (9%) fall under the cut: "su madre", "sus
  hermanos" take their owner *before* them, through a possessive. A type has more
  than one way to be bound.
- Identities still carry noise: infinitives ("haber", "engañar": "antes de
  haber…"), and words with a mark still attached ("Dios!", "mí!»") because `!`
  and `»` are not yet found as marks (D71).
- It knows *that* David is an identity and *that* hijo is a type; it does not yet
  hold "José — hijo — Jacob" as a fact about José. That is the frame of a person.

## Open

- **D83** — the other bindings: a possessive before the type ("su madre", "mi
  hermano"). The binding was found from one seed; the silhouette of the binding
  ("de" → del, su, sus, mi?) may give the rest.
- **D84** — kinship inside the types: relatives are types whose owners are
  themselves identities *and* whose fillers are bound in both directions
  (José hijo de Jacob, Jacob padre de José). Not built.
- **D85** — the frame of a person: an identity with its typed facts (David: rey,
  hijo de Jesé, padre de Salomón), to be asked about and judged by D82 — a
  sibling answering the same questions.
