# The reading adapts (P49)

Designed 2026-10-12. The word cut in `rz_word_cut`, `src/lego.c`.

> I don't want you to patch it, I want it to adapt. — the author
>
> Atomic associations and branched associations, from the simple to the
> complex, and it goes up and down between the branches dynamically. — the author

## What broke

The RAE dictionary parts its words with tabs (2.77 M) and with spaces (0.83 M).
Legos end on both. The word cut then split the atoms legos end on by the
height of their uncertainty, and that split always makes two groups: it kept
the space and left the tab inside words. "1. El" became one word, the
silhouette of "es" became *lugar, Número, río*, and the map knew nothing of
dogs.

The first fix written by hand (a different split) was refused: a criterion
chosen by us breaks again on the next text. What has to exist is the ability
to notice a wrong reading and change it.

## Up and down between branches

- **Atomic association**: what happens where an atom stands. A piece ends
  after it, or it does not.
- **Branched association**: a word, built of atoms; a statement, built of
  words.
- **Up**: from the atoms, a reading of words (the cut by heights, as before).
- **Down**: inside a word, an atom where legos end. It is a seam the reading
  left glued. Every such atom is a proposal.
- **Up again**: the text is read again with the seam as a part, and the new
  reading is kept only if it **describes the same text with less** in its own
  units (D39, D45): the spelling of every distinct word, every word said, and
  which separator stood at each edge, so nothing is dropped for free.
- It goes on until no proposal pays. A part may also be given back the same
  way: a separator whose removal makes the reading cheaper is dropped.

The order of the proposals is a bias (the atoms legos end on most first); the
acceptance is only the judge (D41: bias in the proposal, never in the
acceptance).

## Open

- **D111** — the same loop one branch higher: the dictionary explains its own
  abbreviations ("adj.: adjetivo"). A reading that associates the two should be
  proposed and kept if it pays.
- **D112** — the headword → genus relation ("perro, rra. m. y f. 1. Mamífero")
  is said by the layout, not by a word: the map should be able to propose the
  layout as a relation.
