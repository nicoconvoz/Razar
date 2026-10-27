# The neuron that thinks one step at a time (P53)

Built 2026-10-27. `rz_loom_think_from`, `rz_loom_step` in `src/lego.c`;
`razar neuron [word]`.

> I want multidimensional branches making paths in many directions, like
> neurons, with the woven structures and the meanings associated with every
> word, its use and its variants, learning and generating on its own. — the
> author. And: one depth at a time, as if it were thinking; the depth carries
> a cache of the path walked — I don't want what it builds to be lost. And:
> the cache is tied to each path, which can vary and find other ways to
> circulate.

## What it does

- **Every word is a neuron.** Its branches today: the places it stands in (a
  structure of the loom, beside a partner) and its associates (two silhouettes
  shared, P52). Use and variants are not branches yet.
- **One step of thought**, from every path the last step reached: the
  associates of its word stand in places the word never stood in, and it could
  stand there too — an idea. The associates become the next step's paths.
  Nothing is searched in depth; each step only looks beside it.
- **The cache belongs to each path.** A word reached by two paths is two
  entries, each with its own trail (*caballo > gato > perro*,
  *caballo > ratón > perro*), and thinks from both. A path never walks back
  onto itself.
- **Nothing is lost.** The cache and every idea are kept in the loom's memory,
  apart from what was read, and saved with it: loaded tomorrow, the next step
  is the one after.
- **The bar of thinking.** Every idea has a blind twin — a word met
  somewhere, in the same place. A book read later that says an idea confirms
  it; the same for the blind twins.

## In the little world

From *caballo*, step 1: *caballo es un animal* — where *gato* stood. Step 2
goes on from *gato*, *perro*, *sol*…; by step 3 one word is reached by two
paths and thinks from both. A second book that says, for the first time, that
the horse is an animal confirms the idea.

## On the New Testament, then Quiroga

One step from *pan*, with the whole NT woven:

- **1,540,052 ideas new to the memory** in one step. *pan* has associates by
  the thousand, glue among them (*no*, *que*), and every associate brings
  every place it stands in. Some are good — *"salvar a su pan"* where
  *familia* stood, *"a su pan ,"* where *riqueza* stood — most are not
  (*"que pan perdonarán"*, where *no* stood).
- **Quiroga, read afterwards, says 26 of them; of the blind twins, 0.** Real,
  and tiny: 26 in 1.5 million.
- **The memory on disk**: 197 MB for the NT woven, 313 MB with the
  associations' new fields, 462 MB after one step of thought and Quiroga.

## Travelled paths (2026-10-27)

> No: let the branches form their own way — the idea is to give it no rules
> and explain nothing; it must discover the relations between structure and
> words, and when we put it to the test with real data, incorporate as more
> travelled paths, of better reading and memory, what turned out favourable to
> learning and understanding according to the associations. — the author,
> answering D121

No branch is pruned or filtered. When a book says an idea, the branch that
thought it — the word and the associate it came by — is travelled one step
more (`rz_loom_traveled`). Every idea remembers how travelled its branch was
when it was thought, and the report splits what a book says by that.

In the little world a book that says *caballo es un animal* travels the
branches that thought it and leaves the others untouched; thinking again, the
ideas carry the travel of their branch.

On real data — NT woven; one step from *pan* and *agua*; Quiroga's first half;
the same step again; Quiroga's second half:

| | Open ideas | Said by the book |
| --- | --- | --- |
| first half — new branches | 3,177,230 | 28 |
| first half — blind | 3,177,140 | 1 |
| second half — travelled branches | 2,921 | 0 |
| second half — new branches | 3,391,745 | 2 |
| second half — blind | 3,394,598 | 2 |

**It cannot tell yet.** Quiroga says almost nothing the NT taught about bread
and water: 28 confirmations in three million ideas, and in the second half 2
against 2. The mechanism is there; the test needs books that share what they
talk about, or many more of them, before travel can show whether it pays.

## What a word understood (2026-10-27)

> You are not seeing the fifth dimension: that tells us nothing. What will
> tell us the truth is how it understands the meaning, the use of a word —
> which paths it has, what it understood. — the author

`rz_loom_meaning`; `razar meaning <word>`. A word opened: every place it
stands in and every associate, nothing filtered, ordered only by how much each
was shared — and a second order, the associates *most its own*: forms shared
against how many associates that one has (a word joined to everything says
little of this one).

Regenerated from zero (NT, then Quiroga), *agua* stands in 1,262 places; the
most shared are glue (*"agua , y"*, *"de agua ."*). Its associates most its
own: *candelero*, *césped*, *maná*, *alma*, *cielo*, *altar*, *mar*, *patio*,
*Jordán*. *amor*: *Reino*, *Espíritu*, *reflejo*, *conocimiento*, *don*,
*Hijo*, *corazón*, *alma*, *misterio*.

## Open

- **D121** — answered by the author: no rules; travelled paths (above). Open
  within it: a test with books that share their subject, enough to measure.
- **D122** — the use and variant branches (where and with whom; *perro*/*perra*,
  P44) are not built.
- **D120** — the memory's weight, now 462 MB.
