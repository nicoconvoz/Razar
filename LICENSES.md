# Licenses

This repository carries **two licenses**, one for the code and one for the
content. They are different things and they get used differently.

| What | License | File | In plain words |
|---|---|---|---|
| **The code** — `src/`, `tests/`, `tools/`, and any other source | **AGPL-3.0** | [LICENSE](LICENSE) | free to use, study and modify — but if you put it in your product, or serve it over a network, you open your source too |
| **The copyright notice and who owns what** | — | [NOTICE](NOTICE) | all of this summarised on one page |
| **The code, for closed use** | **Commercial** | [COMMERCIAL-LICENSE.md](COMMERCIAL-LICENSE.md) | exempts you from the AGPL by agreement |
| **The content** — `docs/` (design notes, autopsies, figures) | **CC BY 4.0** | [LICENSE-CONTENT.txt](LICENSE-CONTENT.txt) | use it, copy it, change it, even commercially — **but say where it came from** |

---

## How to cite

For a design note, a figure or a finding, this is enough:

```
Jesús Nicolás Astorga and RESOURCES OPEN DOORS S.A.S,
"Razar — Razonamiento Artificial", 2026.
https://github.com/nicoconvoz/Razar
CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
```

For the code you have to meet the AGPL: keep the copyright notice and the
license, and **publish the source of whatever you build on it** — including
when you serve it over a network (article 13). If that does not work for you,
there is a [commercial license](COMMERCIAL-LICENSE.md).

---

## Why two and not one

- **AGPL on the code**, so the work can circulate without anyone closing it:
  whoever uses it and improves it, gives back. Article 13 covers the modern
  case — not distributing the binary is not enough; if you serve it over a
  network, you open as well. And that matters here more than usual: **an
  organism is the kind of thing somebody serves rather than ships.** On that
  base rests the **commercial license**: whoever will not open their code,
  pays. It is the model MySQL, Qt and Ghostscript live on.
- **CC BY 4.0 on the content**, because the design notes and the autopsies are
  not software — they are a work. The MIT license is not written for that. And
  CC BY asks for the only thing that matters here: **that they say where it
  came from.**

---

## What these licenses do NOT say

- **They say nothing about `feed/`.** That folder is intake — material dropped
  in for the organism to read. It is gitignored, it is not part of this
  repository, and some of it carries third-party copyright. No right over it is
  granted here, by anyone, to anyone.
- **They give no warranty.** Both the AGPL and CC BY are explicit: the work is
  provided as is. Razar is a laboratory, not a product.
- **They do not settle what the organism produces.** What comes out of a run is
  derived from what was fed in. If you fed it something you do not own, the
  output does not become yours because a program touched it.
