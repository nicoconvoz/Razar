# Behaviours (P35) — the first three built

Recorded 2026-10-08, in the author's words and order:

> We need to include behaviours. First, exploration. Second, imitation. Then
> behaviours to get what it wants: it repeats what works and abandons what does
> not. Then it generalises: finds what is alike and acts understanding the
> **silhouette** of the concept, not stuck on the rigid datum. It adjusts
> itself. It gets things wrong, and learns from being wrong. It learns by
> breaking. It seeks what it likes or what catches its attention. It plays with
> experience and combines things.

## The missing piece is not any one behaviour

Razar already has organs for most of these. What it does not have is **acting**:
every organ today runs when a command calls it, on material someone chose. A
behaviour is the organism choosing its next act, from a repertoire, because of
what the last acts did to it. So the first thing P35 needs is the loop all nine
behaviours share:

```
  choose an act  ->  do it  ->  see what came of it  ->  credit the act  -> (again)
```

An **act** is anything it can do to its own world, which is text: read a stretch,
ask a question (P34), predict the next piece (P32), write (P17), dream (P14),
draw a sense (P30). **What came of it** has to be measured, never declared —
and the measure is the open decision below.

## The nine behaviours, mapped onto what exists

| # | Behaviour | Organ that exists | What is missing |
| --- | --- | --- | --- |
| 1 | **Exploration** | senses drawn from a template (P30); the pen's wandering (P17) | choosing *what to do or read next* toward what it has not tried, instead of what it is handed |
| 2 | **Imitation** | the question brings its own relation (P34) | keeping a form someone used ("¿Quién es X?") and reusing it on other X by itself |
| 3 | **Repeat what works, drop what does not** | rules: credited when right, fade when wrong, never deleted (P31, P32) | the same credit on *acts*, not only on predictions — needs "what it wants" |
| 4 | **Generalise by silhouette** | kinds by company; a never-seen lego known by where it sits, 1.45× (P33) | acting on the kind: what worked for one member is tried on the others |
| 5 | **Adjust itself** | the inner conversation re-weights every rule on every step (P32) | — exists; it moves held-out prediction only 1% (D59) |
| 6 | **Learn from error** | surprise read against chance (spike 13); the triage of dreams | an error that changes the *next act*, not only a weight |
| 7 | **Learn by breaking** | ablation is designed for senses (P30) but not built | breaking a piece on purpose and keeping what the break taught |
| 8 | **Seek what it likes / what catches its attention** | interest: opaque, frontier, mastered (P16); attention left by dreams | turning interest into a choice: going toward the frontier |
| 9 | **Play and combine** | the pen (P17); dreams recombine what it lived (P14) | the dream cannot propose anything new (known NO-GO); play needs acts whose result is kept when it pays |

## The decision that has to come first: what does it want?

Behaviours 3, 6, 8 and 9 all depend on "it worked". Three candidates already
exist in the project, each measured:

- **harmony** (D39) — describing the same material with less without discarding
  anything; closed D24 and already punishes the obsessed mind;
- **transfer** (spike 12) — what it learned helps on what it never read;
- **being right** (P32) — fewer surprises in its own conversation.

Being right alone selects obsession (that is why D23 became a prerequisite).
Harmony and transfer do not. This is the author's choice to make.

## What it wants: decided

**A mix of harmony and transfer** (author, 2026-10-08; being right alone stays
out, it selects obsession). Made measurable before building, so it could not be
bent afterwards — for a question "X r ?" answered Y:

- **harmony** — the shared mass of what follows X and what follows Y: how much
  of X's company Y's company already describes;
- **transfer** — in text it has not read yet, of the steps after X it had never
  seen, the share it had seen after Y;
- each against the same count with the 8 words nearest Y in frequency, picked
  blind; the worth is the sum of the two gains.

## Built: exploration, imitation, repeat or let go

`rz_player_act`, `razar play [acts] [relation]`.

- **Imitation** — a way of asking someone used (`es`) enters the repertoire
  first and is tried first.
- **Exploration** — never the same question twice; once every way of asking it
  has was tried, it finds a new one (the commonest word it never used as a
  relation), up to 32.
- **Repeat what pays, let go of what does not** — each way of asking keeps its
  pay; the next is the highest mean pay plus what it still does not know about
  it, scaled by how much pays spread (UCB, no number chosen).
- **The bar** — the same player choosing its way of asking at random.

### In the little world it behaves

"rex is ?" → dog, and that answer carries into the text it had not read: later,
rex barks (transfer 1.000; the wrong answer, cat, 0.000). With a budget of 200
acts it asks again the ways that pay (`my`, `the`, `one`: 15–23 tries) and lets
go of the ones that do not (`yet`, `then`: 3).

Two things it showed:

- **Harmony as defined finds syntactic twins.** "yet" and "is" are both followed
  by articles, so asking through an article returns a word that stands in the
  same slot. That is harmony honestly — merging them describes the same with
  less — but it is the silhouette, not the meaning, and in the mix it dominates.
- **It abandoned the way it was shown.** "is" was tried on the commonest words
  first ("the", "a"), paid nothing, and was let go after 3 tries — it never
  reached "rex". A child dropping the adult's question because it tried it on
  the wrong things.

### On the New Testament: not better than blind

Four fifths lived, the last fifth as what comes later, 300 acts, shown `es`:

| | Mean pay |
| --- | --- |
| choosing | +0.0021 |
| blind (the bar) | +0.0037 |

**Choosing does not beat chance.** The pays are noise-sized because it asks
about the commonest words first — `.`, `se`, `los` — whose answers mean nothing.
`es` ended as the best-paying way (+0.028 over 20 tries), but the margin is
inside the noise.

### Lessons that came from getting it wrong

- The first behaviour test let the player ask everything; every way ended with
  the same count and the test said nothing. Repetition is only visible with a
  budget.
- The little world first used "the" and "then" everywhere; after a space came a
  "t" so often that the word cut took the "o" for the edge. **The word cut
  assumes the separator is the most uncertain atom** — true on the NT, not on a
  poor vocabulary.

## P42 — Where a question applies (2026-10-11)

> We cannot guess at random, nor try forever. We have to find the structure
> and see when to apply it and when not — what separates "is" not going with
> "the" or "a" but with "rex". That is the part of understanding. Sometimes, to
> understand a structure, you have to study a little what you do not
> understand; it may stay there a while until it finds the melody, and then
> try the melody on something new. — the author, answering riddle 2 (D77)

`rz_form_subjects`, `rz_form_reaches`; the player's plan.

- **Study first.** The text already says to whom a question can be put: the
  words that stand in its subject's place far more than anywhere (D50). "is"
  stands after "rex" and "tom", never after "the" or "a".
- **Then something new.** Words that never stood before the question but keep
  the company those subjects keep — the silhouette's two cuts. "zed" never stood
  before "is" and is reached; "the", "dog" and "rex" (already studied) are not.
- **Not forever.** A way of asking has a plan — the studied, then the reached —
  and is spent when the plan is. No falling back on frequency.

In the little world the question it was shown is now put to rex first, pays,
and is kept; P35's player had let it go after "the" and "a".

### On the New Testament

300 acts, shown "es", against the same player choosing its way of asking at
random (the bar):

| | Before (subjects by frequency) | After (subjects by plan) |
| --- | --- | --- |
| choosing | +0.0021 | **+0.0156** |
| blind | +0.0037 | **+0.0248** |

**Studying whom to ask multiplied the pay by about seven**, for the choosing
player and the blind one alike — the questions now read: "cree en ?" → aquel,
"vivirá por ?" → fe, "dice la ?" → Escritura.

What did not change: **choosing a way of asking still does not beat choosing it
at random.** The plan fixed *whom* to ask, not *how*. And "es" itself pays
−0.028 on the NT: the words that stand before "es" in this text are "que",
"lo" — what it studies for "es" here is not names.

The old behaviour test ("a way that pays is asked more times") had to be
rewritten: with plans, every way of asking is spent when its plan is, so the
count of tries measures the plan, not a preference. What a preference shows is
the order: the way that pays gets through more of its plan first.

## Proposed order, from the most basic

1. **The act loop** with one act (ask) and the curiosity of P34 as the first
   user: it explores by asking about what it has not asked yet (1), reusing the
   question forms it was given (2).
2. **Credit on acts** once "what it wants" is chosen: questions that paid are
   asked again in new forms, the ones that never pay fade (3, 6).
3. **Acting on the kind** (4): an answer that paid for one lego is tried on its
   kind.
4. **Going toward the frontier** of interest (8), and **ablation** (7).
5. **Play** (9) last, because it needs every measure before it: a combination
   is kept only when it pays.

Every step carries its bar: the same act chosen at random, on the same
material.

## Open

- **D73** — *decided:* a mix of harmony and transfer. Still open: their units
  differ, and harmony as defined rewards syntactic twins over meaning.
- **D76** — what to ask about. By frequency it asks about function words and
  learns nothing; the frontier of interest (P16) or its own surprises (P32)
  would point it at what it does not understand.
- **D77** — *answered by P42:* a way of asking is put first to whom the text
  puts it. Still open, as **D95**: choosing *which* way to ask does not yet beat
  chance; and what is studied for "es" on the NT is not names.
- **D77 (as first written)** — a way of asking is judged on its first subjects. The shown form
  should keep a longer trust before being let go, or be judged per kind of
  subject (the silhouette, behaviour 4).
- **D74** — what counts as an act, and whether reading a stretch it chose is an
  act (it changes the mind; asking does not).
- **D75** — silhouette: a kind (P33) is one silhouette; whether the shape of a
  frame (D67) is another, at the level of sentences.
