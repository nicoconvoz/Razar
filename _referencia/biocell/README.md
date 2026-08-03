# Célula digital — inmortal, con memoria jerárquica e imaginación

Organismo digital en C que vive en **una sola carpeta** (su *nido*), come datos, aprende en
un **mapa conceptual**, predice, **imagina**, recuerda entre reinicios y **evoluciona en todos
los aspectos** — sin morir nunca. Todo observable desde un dashboard.

## Percepción fina (48 rasgos)
El extractor de rasgos ahora mira 48 dimensiones: histograma de 16 buckets, **n-gramas**
(patrones de pares), **autocorrelación** a varias escalas (periodicidad/repetición), y
**estadística por ventanas** (cómo cambia la textura). Distingue muchas más texturas, así
que forma más conceptos y reparte la memoria profunda en más módulos (hasta 256 sectores).

## Memoria jerárquica (rápida + profunda)
- **Rápida (RAM):** memoria de trabajo, conceptos activos.
- **Profunda (disco):** todo el conocimiento, en **módulos independientes** (`nido/memoria/mod_NNN.dat`).
- Un estímulo **despierta** su módulo (lo sube a RAM); el menos usado se **duerme** (se persiste
  y libera RAM). Por eso la mente **sobrevive a los reinicios**. Cada concepto tiene un id global
  estable: el grafo se reconstruye aunque los módulos entren y salgan de RAM.

## Imaginación y creatividad (lazo generativo)
- **Imagina:** recombina dos conceptos conocidos (+ algo de ruido) para crear ideas nuevas.
- **Consolida (rehearsal):** soñar refuerza y protege la memoria real de la poda — como dormir
  fija recuerdos. Este es el feedback de refinamiento: el organismo retiene mejor lo que sabe.
- **Compone:** camina su grafo de conceptos (su "gramática" aprendida) y genera **artefactos**
  en `nido/creaciones/` — bytes sintetizados con la firma estadística de lo que aprendió.
- **Autorregulación:** la creatividad **útil** se premia por fitness y la inútil se poda. Una
  idea imaginada que después los datos reales confirman pasa a conocimiento (evento raro).
  Aprender de lo propio puede degenerar (colapso del modelo); por eso el fitness solo cuenta
  conocimiento real validado, no las ideas crudas.

## Inmortal y evolutivo
- **Nunca muere.** Uno solo, continuo, energía con piso. Por eso la evolución toca *todo* sin
  riesgo. **(μ+λ)-ES** muta genes, estructura del ADN y los **parámetros** (aprendizaje,
  predicción, creatividad, sueño…), todos codificados en el ADN. Adopta solo mejoras.
- **Lazo cognitivo:** predice el próximo patrón; acertar lo hace más apto (efecto Baldwin).

## Compilar y correr
    gcc -O2 -o celula celula.c -lm        # o: make
    ./celula --nido ./nido --ticks 0      # vive para siempre (Ctrl-C para pausar)

Genera/retoma `nido/celula.dna`, escribe `snapshot.json`, anota en `bitacora.log`, guarda su
memoria en `nido/memoria/` y sus creaciones en `nido/creaciones/`.

> IMPORTANTE (actualización de sentidos): los rasgos pasaron de 16 a 48 dimensiones, así que la
> memoria profunda vieja es **incompatible** y se ignora de forma segura (el organismo arranca con
> una mente nueva y sentidos más finos; el genoma se reseed). Si querés conservar el organismo
> anterior, **respaldá la carpeta `nido/` entera antes** de correr esta versión.

## Darle de comer
Archivos en `nido/comedero/` (texto, código, csv, números, binarios). Para PDFs, pasalos a texto
(`pdftotext`). Más variedad => más conceptos, más módulos, más material para imaginar.

## Dashboard
    cp dashboard.html nido/ && cd nido && python3 -m http.server 8080
Tabs: Resumen, Evolución (con métricas de creatividad), Mapa conceptual (memoria RAM/profunda;
las ideas imaginadas se marcan con anillo punteado violeta), Microscopio, Genoma, Bitácora.

## Qué falta
- **Fork Darwin/Lamarck:** el toggle lamarckiano (que lo aprendido sesgue la mutación).
- **Fase 4:** varias células inmortales en un mundo, con señalización.
