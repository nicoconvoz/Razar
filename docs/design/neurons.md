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

### Which book brought what (2026-10-27)

> Make it feed on both books and tell you every path and association of that
> word. — the author. And: let it adjust itself, nothing rigid; anything fixed
> is by eye.

Every place a word stands in and every pair of associates remembers the books
that made it (`rz_loom_book`, a bit per book; memory v5). `razar meaning
<word> all` writes everything to `nido/meaning-<word>.txt` — in the nest, not
the repository, because it quotes the books. The answer is sized to the word
(counted, then allocated: no cap); on screen each list is cut where its own
values part in two (the same exact split the word cut uses), not at a count.

*agua*, NT then Quiroga: 650 places (as they read) and 4,263 associates. The
NT brought 634 places and 3,856 associates, Quiroga 28 and 609, 12 and 202
came from both. What Quiroga added: *trago de agua*, *agua fresca*, *no bebas
agua sola*, *temblando del agua*, *agua irisada*; places both books share:
*agua cambiada en vino*, *un río de agua*, *movía el agua*. Its associates
from Quiroga are the novel's world — *piano*, *portón*, *césped*, *jardín*,
*tren*, *andén*, *salón* — and some landed on what the NT had taught
(*brazo*, *cuello*, *asiento*).

The Quiroga text cut leaves junk words with line breaks and a broken byte
(*"noche. XXII Cuand"*, *"Mercedes�"*): its line breaks and encoding are not
read as the NT's are (D123).

## Sentences out of relations (2026-10-27)

> Make sentences out of relations, even of different things; I want to see
> what comes out. — the author

`rz_loom_compose`; `razar speak <word> [all]`. Every place the word stands in,
with one of its associates most its own in its hole (the side where their
values part) — a path no book said. Each says which book the place came from
and which made the associate: when they differ, it joined different things.

*agua*: 157,005 sentences no book said, 50,684 joining the two books. Some
read: *"El Paráclito que yo le"*, *"El aliento que yo le"*, *"Jesús salió del
cielo"*, *"al soplo con sus propias"*, *"al maná con sus propias"*. Most do
not: *"Jesús salió del yogurt"*, *"El camarero que yo le"*. It has no way to
tell one from the other: nothing in it is true or false yet (the author's
next step, the switch of yes and no).

## Real or fantasy, a grade (2026-10-27)

> We need it to notice the switch of yes and no, to know whether something
> is false or true… I don't want creativity lost, only labelled real or
> fantasy — and not a rigid label: it can change with experience and
> knowledge. Make a grade from 0 to 1, through infinite middle points, to
> value reality or fantasy. — the author

Nothing it makes is weakened or thrown away. Every sentence `razar speak`
makes is kept as an idea; every book read afterwards adds to its story — the
books that said it, and the books that could have (they knew both its words).
The label is never written: it is read from the story when asked,

  **reality = books that said it / books that could have said it**,

0 when every book that could stayed silent (or none could yet), 1 when every
book that could said it, any point between with more books. No number is set.

NT woven alone, *agua* made 106,395 sentences. Then Quiroga: it knew the words
of 10,502 of them; it said 14 (reality 1.00) — *"en el alma"*, *"con el
cielo"*, *"el infierno de"*, *"al patio ."*, *"sin querer ,"* — and was
silent on 10,488 (0.00). 146,517 remain untested. With one book the grade can
only be 0 or 1; the middle needs more books.

The switch of yes and no is not found yet: *no* comes out second among the
pieces that slip into pairs said without them (327 pairs), and what follows
it changes (18 % as before, against 37–53 % for *también*, *todos*) — but
*que* and *y* do the same. It does not yet tell turning from joining (D124).

## The switch of yes and no (P54, 2026-10-27)

`rz_lexicon_switches`. The author's properties of NO, each measured against
the book itself, nothing named:

1. **Taken out, what is left is said too** ("el perro no ladra" / "el perro
   ladra"); "y" cannot be taken out ("pan y vino" / "pan vino").
2. **When it appears, what follows changes** ("no ladra, sino que corre");
   "también" adds the same again and what follows stays — the author: what
   matters is what the word does when it appears, not when it is absent.
3. **The widest door**: the relation is of three, subject, no, verb; many who
   stand before it and many acts behind it.
4. **Mostly the door**: a connector ("y") is a door now and then and joins
   everything else; of what the switch does, most is the door.
5. **Not inside another door**: "no la envió", "no lo dijo", "no me dijo" —
   *la*, *lo*, *me* stand inside the no's door; *ya* and *y* stand outside it
   ("ya no", "y no").

On the way, four measures failed and were dropped: the door at the level of
neighbouring words; the door of three in the map, mixed and separated (the
book negates doors that exist: *Jesús no respondió*, *Jesús respondió*);
aggregation by the gap to what was last said.

**On the New Testament, read by Razar with its own pieces: one switch, *no*.**
Asked first which words are things (P50), it fails (*la*): in the map *no*
stands as something spoken of, so that filter goes against it. On Quiroga
(24,475 words) it finds none: one novel is not enough to tell the door.

### Closed doors, and why this time (2026-10-27)

> Each time it finds something denied, let it ask why in that case it was
> different — unless it is a general rule, and then it should only ask when it
> finds the contrary, the affirmation of what in most cases is denied. — the
> author. And the scope: by situation, then by story, then by book, then as a
> general rule if there are more books.

`rz_loom_switches`, `rz_loom_questions`; `razar why [all]`. `razar loom`
finds the switches of each book (P54) and adds them to the memory, so a switch
found in one book serves the next. Every place said with a switch in front is
kept as a closed door — the same place, said denied — with one hole on each
side of the switch (the relation is of three). A denial of what is mostly said
asks *why this time no?*; an affirmation of what is mostly denied asks *why
this time yes?*. The rule is the majority of the two counts; nothing else.
Book and rule are built; situation and story are not yet (D125).

On the NT: its switch, *no*, found alone; 38,442 closed doors; Quiroga, with no
switch of its own, read with the NT's: 3,936 closed doors. Among the
questions: *"Jesús le respondió"* — said 26 times, denied once: why this time
no? (the silence before his judges); *"El que cree"* — said 5, denied once;
*"es digno"* — denied 3, said once: why this time yes? Most of the strongest
are glue (*", para que"*): runs that cross from one clause to the next.

### Looking for the why near it (2026-10-27)

> Let it look for the answer in the associations and the structures near
> that sentence. — the author

Every question keeps what stood near it: its sentence and the one on each
side, cut where the book's own marks break them (P33 — the lexicon now keeps
which legos it read as marks). "One sentence each side" is a criterion, not a
measurement: the author asked for the sentences near. The answer looked for is
what stood there that is **not the word's usual company** — and usual company
is each word's own cut, the high side of its associates' forms: a word joined
to the whole book, as *Jesús* is, keeps only what it shares most.

On the NT:

- *"El que cree"* — said 5 times, denied once. Near it, not its usual
  company: … **mentiroso** … — *"el que no cree a Dios lo hace pasar por
  mentiroso"*.
- *"Jesús le respondió"* — said 26 times, denied once. Near it: *Le hizo
  muchas preguntas, pero … nada* — Herod questioning him.

The answers still come mixed with glue (*pero*, *en*, *de*).

## Open

- **D121** — answered by the author: no rules; travelled paths (above). Open
  within it: a test with books that share their subject, enough to measure.
- **D122** — the use and variant branches (where and with whom; *perro*/*perra*,
  P44) are not built.
- **D120** — the memory's weight: 416 MB regenerated from zero, without ideas.
- **D123** — words glued across Quiroga's line breaks, and a broken byte.
- **D125** — the scope below the book: situation and story, found from the
  text; and closed doors read across clause breaks (glue questions).
- **D124** — the switch is found on the NT (*no*, alone) but not on Quiroga;
  the P50 thing filter works against it; a verb that carries the no inside
  (*negó*) is not looked for yet; questions (¿ ?) are not a mode yet; the
  switch is not yet wired to the reality grade (said with the no = denied).
- Fixed numbers still in the loom, by eye until measured: the mates' table
  size, the 200,000 paths `razar paths` builds, 64 books, the ring of 64
  foreseen, runs of 3 to 5 pieces.
