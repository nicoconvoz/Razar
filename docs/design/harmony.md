# Harmony (P25) — the search that never finishes

Author's framing, 2026-08-03.

> *"Tiene que ver con el sentido del universo, que tiende del caos a buscar un
> orden, una organización, una armonía que interprete todo, en todas las cosas.
> Las cosas procesadas realmente han necesitado pasar del caos a la armonía.
> Puede que el proceso de armonizar nunca termine, y ahí está la magia del
> pensamiento y la imaginación infinita: una mente que siempre encuentre mejores
> maneras de hacer la cosa. Perfectible y nunca perfecta."*

---

## 1. What is implementable in this

Three claims, each of which changes the code.

### 1.1 — Harmony is a direction, not a state

Never *"is this harmonious?"* — always *"did this become more harmonious?"* A
gradient has no target value to reach and no threshold to calibrate, which is why
it can be built without declaring anything.

### 1.2 — The process never finishes

So no organization is ever final. Every structure the organism has is **the best
order it has found so far**, and it stays open to a better one. Perfectible,
never perfect, stated as a property of the data model rather than as an
aspiration: **a shape is revisable.**

### 1.3 — It is what the organism is already doing

Harmony is not a new mechanism bolted on. It is the name of the thing every
existing piece already does, and naming it turns a scattering of parts into one
faculty with one measure:

| Existing piece | The harmonising it performs |
| --- | --- |
| fragment birth | two things that keep occurring together become one thing |
| effort = `exp(H)` | how much disorder is left at this point |
| forgetting | what carries no order stops being paid for |
| interest | surprise falling **is** the material becoming ordered to it |
| the shape | the role something plays, once the noise of identity is dropped |

---

## 2. The measure, and why it cannot be cheated

**D39 — Harmony is describing the same material with less, while still
accounting for all of it.**

Both halves are load-bearing, and the second one is what makes the first safe.

This closes **D24**, open since P15 was designed, and it closes it exactly where
it was stuck. The recorded hazard was:

> *minimising effort alone selects for obsession — a skill that collapsed
> everything onto one path would score perfectly: free retrieval, one thought.*

That hazard dies here. A mind that collapses onto one path is not describing its
material with less; it is **discarding the material**. Under D39 the account must
still be complete, so shrinking the description by throwing away what it was
supposed to explain scores zero, not perfectly.

Cheaper description, same material, nothing dropped. Compression with fidelity —
where fidelity is not an extra rule but the definition of what is being
compressed.

Concretely, and using only what already exists: a candidate organisation is
better when the organism can walk the same held-out material at lower total
effort — `Σ log(effort)` over the walk — **without** its opaque share rising. The
first term is the order found; the second is the guard that nothing was quietly
abandoned.

---

## 3. What this does to the shape

Today `rz_shape_of` computes branch × span × concentration from current
statistics. That is a **formula we wrote**, not an order the organism found. It
cannot improve, and when the material moved underneath it, it collapsed to four
classes that differed only in length — silently, because nothing was watching a
formula that could not be wrong.

Under §1.2 the shape becomes something else: **the best organisation of its
material that the organism has found so far**, carrying its own score under D39,
and replaceable the moment a better one scores higher on held-out material.

That is the difference between a measurement and an organ. A formula cannot get
better. A search can.

---

## 4. The faculty (P26): symmetry, and instruments made from it

The author's instruction, same day:

> *"Agregale al sistema una búsqueda permanente de sentido en la simetría:
> identificar qué se le parece, qué se repite, cuándo se lo ve y cuándo no, qué
> es compatible y qué es incompatible... y que lo que reconozca lo adopte como un
> instrumento. Tiene que instrumentar, como en el organismo ajedrecista que crea
> una regla donde dice que hay que proteger a la reina y el modo en que hay que
> hacerlo. Tienen que emerger los sentidos."*

Four questions, asked permanently, over whatever it has met:

| Question | What it looks for |
| --- | --- |
| what resembles what | two things standing in the same relations |
| what repeats | a regularity that survives new material |
| when is it seen, and when not | the condition under which it holds |
| what is compatible, what is not | which regularities can hold together |

### 4.1 — A sense is not a finding. It is a finding plus a procedure.

The chess organism did not merely notice the queen mattered. It produced a rule —
*protect the queen* — **and the way to do it**. Half of that is an observation;
both halves together are an instrument.

**D40 — An instrument is a detected regularity together with a procedure that
uses it, and it is kept only while it earns its keep under D39.**

An organism that notices without instrumenting has opinions. One that
instruments has senses.

### 4.2 — The word boundary is the first exam, and it must not be handed over

Measured on 155,597 words: read as a RISE in uncertainty rather than as a level,
the end of a word is found at precision 0.8396 against 0.6345 chance — with the
bar at 1.00 in every window, meaning *"did it go up?"*. No calibrated constant
anywhere.

That regularity is **real and present in the material**. It would have been
trivial to write it in by hand.

Writing it in by hand is forbidden, and not on principle — on evidence. It is a
known-answer test for the faculty itself: if the permanent search for symmetry
cannot find, unaided, something we have already proven is sitting there, then the
faculty does not work and no amount of hand-written rules on top will fix it.

**The organism must find the heartbeat. We already know the answer, so we can
grade it.**

---

## 5. The unit is not given either (P27)

> *"En el punto 2 hay que detectar qué es la palabra en un texto, qué es una
> palabra en un código, qué es una palabra en una imagen — quizás sea un píxel.
> La máquina sola debe descubrirlo en cada caso, incluso en la música o en lo que
> sea."*

Not one rule for what a unit is. A **faculty that finds the unit in whatever it
is fed**, because the answer differs per material and we do not know it in
advance for most of them.

Which is the same requirement as §4 with a different target: the unit is whatever
cut of the stream yields the most harmony under D39. Text and code are the two
where we can check the answer; music and images are where it has to work without
us.

---

## 6. Open

- **D41** — how is a candidate organisation proposed at all? Enumeration does not
  scale. The search needs a generator, and where that generator comes from is not
  answered.
- **D42** — held-out material is required to score anything under D39. Where does
  it come from during ordinary life, without us feeding it a test set?
- **D43** — two instruments that each raise harmony alone may conflict. The
  author's own question — *what is compatible and what is not* — applies to the
  instruments themselves, not only to what they observe.
- **D44** — when a shape is replaced by a better one, every merit and transfer
  number recorded under the old one becomes incomparable. The organism needs to
  know that its own history changed units.
