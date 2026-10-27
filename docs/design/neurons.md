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

## Open

- **D121** — one step from one word is 1.5 million ideas: an associate said
  twice is too weak a branch, and glue makes it a flood. What makes a branch
  worth walking, found by the text and not set by eye.
- **D122** — the use and variant branches (where and with whom; *perro*/*perra*,
  P44) are not built.
- **D120** — the memory's weight, now 462 MB.
