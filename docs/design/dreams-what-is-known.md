# What is actually known about dreaming

Researched 2026-08-03 at the author's request, to get past one person's
perception. Sources at the bottom.

Read this as **input to the design, not authority**. One of the most useful
frameworks here is explicitly a theoretical proposal with no empirical support
yet, and it is flagged where that matters.

---

## 1. Dreaming is not one job. It is at least three.

The literature does not converge on a single function, and the three candidates
are not mutually exclusive. They imply **different mechanisms**, so the design
has to pick deliberately rather than assume "dreaming" means one thing.

| Job | Claim | What it needs the dream to do |
| --- | --- | --- |
| **Regularise** | Dreams prevent overfitting to waking experience | Produce **corrupted** samples that pull representations away from the training data |
| **Consolidate** | Sleep replay strengthens and reorganises what was learned | **Reactivate** real traces, prioritised by value |
| **Attenuate** | Dreams strip emotional charge from memories | Keep the content, **lower the affect** |

The reference organism [`celula.c`](../../_referencia/biocell/celula.c)
implements exactly **one** of these — consolidation — and does it deliberately:
`jt = P_JOIN * (dreaming ? 3.0 : 1.0)` widens the merge threshold threefold while
dreaming, *"more consolidating, less inventing"*. Measured over 4000 ticks: 162
dreams, 101 consolidations, 20 new ideas, **0 ever confirmed by real data**.

Worth stating plainly, because it cuts against the first job:

> **Widening the merge threshold makes dreams more faithful. Faithful replay is
> exactly what does not help generalisation.**

Consolidation and regularisation are both real jobs, but they pull in opposite
directions, and the organism currently only does the one that does not
generalise.

---

## 2. Regularisation — the overfitted brain hypothesis

Hoel's proposal: the brain overfits to waking experience, learning what works for
the situations it met and failing to generalise. Dreams are **augmented samples
that guide representations away from overfitting** — the biological equivalent of
noise injection, data augmentation and regularisation in machine learning. Dream
loss produces a brain that can still memorise but generalises badly.

**Caveat, and it matters:** the review of this hypothesis states there is **no
empirical evidence** for it. It is a normative theory with testable predictions,
not an established finding. Treat it as a well-argued design idea.

The part most useful here is the answer to *why a dream is strange rather than
random*:

> Dreams are "less detailed and more fantastic than waking experience but retain
> the sequential ordering that we are familiar with."

That is not noise and it is not replay. It is **structured corruption** —
sequence preserved, detail degraded. In Razar's terms that is a **middle
temperature**: not 0 (faithful, teaches nothing new) and not high (incoherent,
teaches nothing at all).

---

## 3. Consolidation — what replay actually does

Two findings that contradict the naive reading of "replay = rehearsal":

- **Replay is not faithful.** "Replay does not necessarily reflect previous
  behaviour and can construct never-experienced sequences."
- **Replay is prioritised, not uniform.** Not all memories are equally worth
  replaying; value drives what gets reactivated.

And from dream-content research, the same point from the other side: dream
incorporation of waking events is "rarely the faithful reproduction of an
episodic memory in its entireness."

So even the consolidation story is not "repeat what happened".

---

## 4. What gets dreamed about — and it is not what is strongest

This directly contradicts how the reference organism seeds a dream.

- **Emotional and personally significant experiences are preferentially
  incorporated** over mundane daily activities.
- Incorporation peaks **2 nights after** an event, and then again **5–7 nights
  after** — the day-residue and dream-lag effects. Two waves, not a decay curve.

`celula.c` seeds from the **strongest concept of a random sample of eight** — it
rehearses what it already knows well. The research says the seed is what
**mattered**, not what is strong.

That also sharpens a proposal made earlier in this project. Seeding from failures
(`gave_up`, `effort`, `descents`) is closer to the evidence than seeding from
strength, but it is still not the same thing: **significance is not difficulty
either.** What Razar lacks is any notion of *mattering* — which is what the affect
layer (P6) was supposed to provide and does not yet exist.

---

## 5. Attenuation — dreams turn the volume down

A specific, measurable effect, and none of it is in Razar:

- For both positive and negative waking events, **the dreamt version is rated as
  emotionally less intense than the original**; dreams shift emotional tone
  toward neutral.
- REM depotentiates amygdala reactivity, in a state where noradrenergic activity
  is suppressed. Sleeping between viewings of emotional images reduces both rated
  intensity and amygdala response; staying awake **increases** both.
- The reduction happens in **dream recallers and not in non-recallers**, which is
  why this is attributed to dreaming rather than to sleep alone.
- Dream emotions skew more negative than pre-sleep emotions, yet **morning
  emotions are more positive**.

So the dream keeps the content and drops the charge. For Razar this is a concrete
operation on the affect layer that is the **opposite of strengthening**, and it
would need P6 to exist before it can be built.

---

## 6. Forgetting a dream is machinery, not weakness

This one changes how confident the design can be.

- Noradrenaline falls to near zero during REM. It is required for encoding.
- The hippocampus is "effectively offline".
- MCH neurons in the hypothalamus are active during REM and **actively suppress
  memory formation**.

The brain is having rich experiences while **deliberately preventing itself from
storing them**.

This project had already converged on "the dream's content is false and should be
discarded". The evidence says something stronger: **discarding is a dedicated
mechanism, not an accident of weak encoding.** You do not hope dream content
fades — you make it unstorable by construction.

That is a much firmer footing for the rule that a dream writes no traversals.

---

## 7. The range is real, and the pleasant ones have a function

The author's correction — that not all dreams are uncomfortable, "there are all
colours" — is supported, and the pleasant end is not decoration:

- Dreamers evaluate their dream emotions **more positively than an independent
  judge does** (a positivity bias).
- That positivity bias was the **second strongest predictor of positive morning
  mood**.

So a design that only rehearsed threat would be modelling half the phenomenon and
losing a measured effect. Threat-rehearsal is one reading of dreaming; it is not
the whole of it.

---

## 8. What this changes for Razar

| Decision | Before | After reading |
| --- | --- | --- |
| What a dream writes | content discarded, residue kept | **unchanged, and now on firmer ground** — the discarding is machinery |
| Dream fidelity | undecided | **structured corruption**: sequence kept, detail degraded — a middle temperature, not 0 and not high |
| What seeds a dream | strongest (celula.c) / failures (proposed here) | **what mattered** — and Razar has no notion of mattering yet. Blocked on P6 |
| Effect on affect | not considered | **attenuation** — keep content, lower charge. New, and needs P6 |
| Which job | assumed "consolidation" | **three jobs; pick deliberately.** The reference does the one that does not generalise |

The honest summary: the strongest single finding for this project is that
**consolidation and regularisation pull in opposite directions**, and the working
reference implements only consolidation. If Razar copies it, Razar gets a better
memory of what it read and no better ability to handle what it has not.

---

## Sources

- [The overfitted brain hypothesis (review) — PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC8134936/)
- [Hoel, The Overfitted Brain: Dreams evolved to assist generalization — arXiv](https://arxiv.org/abs/2007.09560)
- [The role of experience in prioritizing hippocampal replay — PMC](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC10710481/)
- [Replay of large-scale spatio-temporal patterns from waking during subsequent NREM sleep in human cortex — PMC](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC5727134/)
- [The nature of delayed dream incorporation (dream-lag effect) — PMC](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC6849581/)
- [Incorporation of recent waking-life experiences in dreams correlates with frontal theta activity in REM sleep — PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC6022568/)
- [Evidence of an active role of dreaming in emotional memory processing — Scientific Reports](https://www.nature.com/articles/s41598-024-58170-z)
- [Dreamers' evaluation of the emotional valence of their dreams indicates a mood regulation function — PMC](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC9523572/)
- [Overnight therapy? The role of sleep in emotional brain processing — PubMed](https://pubmed.ncbi.nlm.nih.gov/19702380/)
- [Why we forget our dreams: acetylcholine and norepinephrine in wakefulness and REM sleep — Cambridge Core](https://www.cambridge.org/core/journals/behavioral-and-brain-sciences/article/abs/why-we-forget-our-dreams-acetylcholine-and-norepinephrine-in-wakefulness-and-rem-sleep/9C71B973B2BE9F117C17042BC0B43E7E)
