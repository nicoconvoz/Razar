# Weaving meaning while reading (P52)

Built 2026-10-27. `rz_loom_read` in `src/lego.c`; `razar loom [path]`.

> The association: discover the meaning of what is not structure by
> associations across the parts of the document — not one by one, but while
> processing it, so we save the heuristic of the search: as it finds the word
> it finds the context and the relations, and weaves its meaning. — the author

The riddle before it was *the album*: which sticker does the collector lack?
Recomposition (P51) searched afterwards, one by one, and a search needs an
order; the order chosen — strongest first — crowned glue. Woven while reading
there is no order to choose: every encounter adds to a meaning.

## What it does

One pass, left to right.

- **A silhouette is born** when it is put together with a second different
  pair (the anecdote rule).
- **A word that fills a hole of a born silhouette adds that place to its
  meaning**: beside that partner, in that structure.
- **Two words are associates** when they filled the same hole beside the same
  partner in two different silhouettes (a form counts once).
- **The live bar.** Each time the word just read stands in a place it never
  stood in: were its associates there already? Against a word picked blind
  from what was already read, so the baseline has the frequencies of the text.
  Nothing in the bar is learned from it.

## In the little world

The path world, and only at its end, for the first time, *el caballo es un
animal*. Reading *animal* there, it was expected: its associate *mamífero*
already stood beside the horse (*el caballo es un mamífero*).

70.1 % of new places foreseen; blind, 2.3 %.

## On the New Testament

Read once, the whole book: 193,111 words, 263,640 silhouettes born, 3,165,950
pairs of associates.

| | New places for the word just read | Foreseen |
| --- | --- | --- |
| woven meaning | 539,711 | **39.94 %** |
| blind word | 523,858 | **32.31 %** |

1.24× over blind. Some of what it foresaw, at the very end of the book:

- *"Señor Jesús permanezca con **todos**"* — *todos* expected, because
  *ustedes* stood there ("permanezca con ustedes").
- *"con todos . **Amén**"*.

Glue is part of it: the blind word is often glue too, and glue has
associates everywhere, which is why blind already reaches 32 %.

## The loom lives in the memory (2026-10-27)

> That is what the AMMC is for: build the conceptual design of meaning as it
> sees it, and the connections stay in the memory. We keep the melody — the
> silhouette — and the meaning. — the author. And: the associations are what
> we need.

`rz_loom_new`, `rz_loom_save`, `rz_loom_load`; `razar loom` loads and saves
`nido/loom.dat`. Every piece is known by the hash of its spelling, so what one
book wove is there for the next one: silhouettes (born and heard once), the
places each word filled, and the associations with their count of forms.
Everything is saved, so the memory comes back exactly (round trip tested);
forgetting what was heard once is a decision of its own.

In the little world: a book that never says anything of mammals or horses,
read alone, cannot foresee *animal* beside *caballo*; read after the path
world — in memory or loaded from disk — it does.

Quiroga, read from nothing and read with the New Testament in memory:

| Quiroga | New places | Foreseen | Blind |
| --- | --- | --- | --- |
| from nothing | 49,133 | 9,612 — **19.56 %** | 13.34 % (1.47×) |
| with the NT remembered | 66,101 | 20,600 — **31.16 %** | 26.71 % (1.17×) |

The memory foresees twice as many places, but the blind word gains as much:
the NT brings associations for every frequent word, so it lifts everything,
and the margin over chance shrinks. What carries between books is mostly
glue so far.

**The cost**: the NT alone makes a 197 MB `nido/loom.dat` (the nest is not in
the repository). Most of it is silhouettes heard once and pairs that are not
yet associates.

## Open

- **D118** — the structure here is the silhouette, not the sticker (P51): the
  stickers need a fixed point over the whole text, and reading live has only
  what was read so far. Whether stickers can be woven live too.
- **D120** — the loom's memory is 197 MB for one book; what to forget (heard
  once, never associated) without losing what a later book needs.
- **D119** — what was foreseen is not used yet: the meaning is woven and
  measured, but Razar does not yet bet with it while reading (PENZAR, P45).
