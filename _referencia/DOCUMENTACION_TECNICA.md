# Organismo ajedrecista — Documentación técnica (v2)

> Mapeo completo del sistema, parte por parte, **verificado contra el código real**
> `organismo.c` (1617 líneas, C) más el dashboard (`servidor.py` + `dashboard.html`).
>
> Esta versión reemplaza al doc anterior: el organismo evolucionó en varios frentes y el doc viejo
> quedó desactualizado. Los cambios más importantes desde entonces están marcados con **[CAMBIO]**.

---

## 0. Tesis del proyecto

El organismo aprende a jugar al ajedrez **sin que le metamos a mano principios de estrategia**: ni
tablas de aperturas, ni reglas posicionales, ni "ataca al rey", ni "controlá el centro". Le damos un
**cuerpo en un mundo**:

- **ojos** — percibe el tablero (48 rasgos crudos + sentidos que él mismo evoluciona),
- **manos** — solo movimientos legales (motor validado con *perft*),
- **hambre** (`energía`) — capturar suma, que lo capturen resta,
- **gloria** (`placer`) — ganar, dar mate, o **sacrificar y ganar igual** sube el placer.

**[CAMBIO IMPORTANTE — los valores de pieza ahora EXISTEN, pero emergen.]** El doc viejo decía "ni
valores de pieza". Eso ya no es exacto. El organismo aprende una **función de valor** `V(estado) =
P(ganan blancas)` —una logística sobre los 48 rasgos base, entrenada al **resultado real** de las
partidas por Montecarlo (§5.7)— y de los pesos de esa función **caen solos los valores de pieza**
(`read_piece_values`, §5.7). La tesis se mantiene en un sentido más honesto y más fuerte: **nadie le
dijo que la dama vale 9**; el valor de cada pieza está **anclado a ganar/perder**, no a ninguna
creencia material sembrada a dedo. Arrancan todos parejos (0.30) y la experiencia los diferencia.

El sistema tiene **dos canales de aprendizaje que no se tocan**:

1. **La mente (conceptos + valor)** — un mapa conceptual plástico archivado por **situación**
   (ataque/defensa × piezas en relación), más la función de valor `V`. Se llena con lo que el bicho
   *vive*: la **escuela** (juega contra rivales fijos que suben de nivel) y la **observación de
   maestros** (mira partidas `.pgn` y las absorbe). Aprende *qué siente* (`v_p`), *cómo le va*
   materialmente (`v_e`) y *cuánto vale cada posición* (`V`), y consolida en proporción a cuán **real**
   fue la experiencia (realidad graduada).
2. **Los instintos (genoma)** — una cadena ACGT que codifica su carácter (**15 parámetros**) y sus
   sentidos de percepción (calientes/fríos, con despertar por contexto), afinada por **evolución**
   coevolutiva contra su mejor versión.

> **[CAMBIO] El modo principal hoy es la ESCUELA + OBSERVACIÓN, no el "estudio-imitación" del doc viejo.**
> El motor que comparaba la jugada del bicho con la del maestro y la validaba (`best_own_move`,
> `pgn_agree`, `master_moves`, `sense_fuel`) **se retiró por completo**. Hoy el PGN se **observa**
> (percibe posiciones, refuerza conceptos, entrena `V`) y el crecimiento de fuerza viene de **jugar el
> currículum**. El autojuego sigue en el código (`opp_rung<0`) pero el lazo principal no lo usa.

---

## 1. Arquitectura de alto nivel

El proceso es **inmortal**: nace una vez, vive aprendiendo, y al reiniciarse despierta su ADN y su
memoria del disco. El lazo de vida (`main`), por tick, alterna **observar maestros** y **jugar la
escuela**:

```
al arrancar:  version_guard -> load_genome/stats/vhead/nucleo -> study_all_masters -> measure -> snapshot

tick t (c->age = t):
  poll_command(c)                       # ¿el usuario quiere jugar? (efímero, vía comando.txt)
  GTRACK = 1                            # rastrea uso de sentidos durante el tick
  if (dreams impar) o (t < SELFPLAY_FROM):
      CUR_REALITY = 0.95
      study_pgn_drip(c, GAMES_PER_DRIP) # OBSERVA 2 partidas de maestro: percibe + refuerza + entrena V
  else:
      CUR_REALITY = 0.30 + school_rung*0.10
      r = play_and_learn_ex(c, learn, school_rung, color, observe=0)   # JUEGA contra su nivel
      school_step(c, r)                 # el resultado sube/baja la vara del currículum
  GTRACK = 0; dreams++

  cada 50  ticks: adjust_gene_priorities   # el uso reparte la prioridad de los sentidos
  cada 100 ticks: measure                   # aptitud vs azar (vara baja, display)
  cada 400 ticks (t>0): measure_sparring    # FUERZA REAL (Elo) contra la escalera fija
  cada 80  ticks: evolve                    # evoluciona instintos y sentidos
  cada 50  ticks (t>0): compete_nucleo      # demota lo inferior/podrido + tope duro del núcleo
  cada 5   ticks: write_snapshot            # telemetría para el dashboard
  cada 200 ticks: save_all_memory + save_genome + log_cap
  cada 5000 ticks: save_presets
```

> **[CAMBIO] El Elo se mide desde el arranque (cada 400 ticks), NO "a partir del tick 1500".** El
> arranque tardío del doc viejo se revirtió: hoy `measure_sparring` corre en `t>0 && t%400==0`. La media
> móvil del Elo (`spar_elo*0.85 + nuevo*0.15`) se asienta sola con las primeras lecturas.

Las capas, de cuerpo a mente:

| Capa | Qué es | Funciones clave |
|------|--------|-----------------|
| **Cuerpo** | motor de ajedrez legal | `gen_moves`, `gen_caps_legal`, `make_move`, `in_check`, `is_attacked` |
| **Ojos** | percepción (48 rasgos + sentidos calientes/fríos) | `board_features`, `compute_preset` |
| **Mente** | conceptos por SITUACIÓN + memoria + **función de valor V** | `situacion`, `perceive`, `reinforce`, `accrue_merit`, `vpredict` |
| **Pensar** | intuición + material aprendido + cálculo + reflexión | `intuition`, `quiesce`, `search`, `deep_pass`, `choose` |
| **Núcleo** | permanencia por **MÉRITO** (top-K por situación) | `accrue_merit`, `compete_nucleo`, `nucleo_prio`, `demote_core` |
| **Aprender** | escuela (currículum) + observación de maestros | `play_and_learn_ex`, `study_pgn_drip`, `school_step` |
| **Genoma** | instintos heredables (15 params + sentidos) | `decode_pheno`, `apply_genes` |
| **Evolución** | mejora del genoma vs el campeón | `evolve`, `audit_sense`, `tune_instincts`, duelos |
| **Fuerza real** | vara fija externa (Elo) | `measure_sparring`, `spar_best`, `ladder_move` |

---

## 2. El cuerpo — motor de ajedrez

### 2.1 Representación

```c
typedef struct { int b[64]; int side; int castle; int ep; } Pos;
typedef struct { int from,to,promo,flag; } Move;
```

- `b[64]`: tablero lineal. Pieza **blanca positiva, negra negativa** (`enum P=1..K=6`), vacío `0`.
  Índice = `rank*8 + file`.
- `side`: `+1` blancas / `-1` negras al mover.
- `castle`: 4 bits de derechos de enroque. `ep`: casilla al paso o `-1`.
- `Move.flag`: `1`=avance doble (habilita ep), `2`=captura al paso, `3`=enroque corto, `4`=enroque largo.

### 2.2 Generación de movimientos

- `gen_pseudo` genera pseudo-legales pieza por pieza (la promoción se expande en Q/R/B/N vía `addp`).
- `gen_moves` filtra legalidad de la forma directa: aplica con `make_move` y descarta lo que deja al
  propio rey en jaque.
- **`gen_caps_legal`** genera **solo capturas legales** para la quiescencia: chequea `is_attacked`
  únicamente en las ~5 capturas en vez de en las ~35 jugadas. `is_attacked` era el 48% del costo; esto
  lo corta sin cambiar el resultado.
- El motor está **validado con perft** (estándar para garantizar generación correcta).
- `piece_count(p,side)` cuenta piezas de un bando (sin valores): sustrato del hambre/energía y de la
  detección de sacrificios.

> Nota: `piece_val_` (valores clásicos `{0,1,3,3,5,9,99}`) **sí** existe, pero **solo** lo usa el
> clasificador de situación (`situacion`/`attacker_type`, §4.2) para decidir *cuál amenaza es la
> dominante* — qué carpeta archivar. **No** evalúa jugadas. La evaluación de jugadas usa los valores
> **aprendidos** de `V` (§5.7), no estos.

---

## 3. Los ojos — percepción

### 3.1 Los 48 rasgos base (`board_features`)

Produce `float[72]`: **48 rasgos base** + **24 ranuras** para los sentidos despiertos (§3.2). Los 48:

| Índices | Rasgo |
|---------|-------|
| 0–5 / 6–11 | conteo de piezas blancas / negras por tipo (÷8) |
| 12 | balance material (blancas − negras, sin rey) ÷16 |
| 13 | lado a mover (1 = blancas) |
| 14–17 | ocupación de las 4 casillas centrales (+1 blanca, −1 negra) |
| 18–21 | control del centro (atacado por blancas +0.5 / negras −0.5) |
| 22 | movilidad (nº de pseudo-movimientos ÷40) |
| 23 | jaque al lado a mover |
| 24–25 | derechos de enroque del lado a mover |
| 26–33 / 34–41 | peones blancos / negros por columna (÷2) |
| 42 / 43 | escudo de peones del rey blanco / negro (÷8) |
| 44 | fase de la partida (piezas menores+mayores ÷16) |
| 45–47 | reservados (cero) |

Son **crudos**: cuentan y miden, no juzgan. **Estos 48 son además la entrada de la función de valor
`V`** (§5.7): los pesos `vw[0..11]` sobre los conteos por tipo **son** los valores de pieza emergentes.

### 3.2 Sentidos evolucionados (genes de percepción)

Sobre los 48, el genoma inventa sus propios sentidos. Un sentido es un gen:

```c
typedef struct { int on, type, side, ref, radius, trig, prio; } Gene;
```

Cuenta piezas de cierto `type` (0=cualquiera, 1–6) y `side` (mías/rival/cualquiera) **cerca de** una
referencia `ref` (mi rey / rey rival / centro / todo) dentro de un `radius` (1–4). Así construye solo
nociones como *"enemigos cerca de mi rey"* (peligro) o *"mis peones cerca del rey rival"* (ataque). El
conteo (cap 8, ÷8) va a una ranura `out[48..71]`.

> **[CAMBIO] Los 5 "sentidos de valoración" están clavados (`decode_pheno`).** Se garantiza **un sentido
> por cada tipo de pieza** (peón=1 … dama=5) en los genes 0–4: `type` fijo, siempre `on` y prioridad 100.
> El bicho **ve las 5 piezas en CADA jugada** (no solo cuando su trigger casualmente matchea), para
> valorar bien el material. El resto de sus campos (referencia/radio/cuándo) sí evoluciona; el
> reemplazo-si-mejor lo hace la evolución normal.

### 3.3 El selector dinámico por condición (cuándo despierta cada sentido)

Cada sentido lleva, además del "qué mira", un "**cuándo despierta**": el campo `trig`, máscara de **12
micro-condiciones**.

| Bit | Micro-condición | Peso (`CONDW`) |
|-----|------------------|------|
| 0 | mi rey en jaque | 12 |
| 1 | rey rival en jaque | 10 |
| 2 | dama rival viva | 10 |
| 3 | mi dama viva | 8 |
| 4 | es final (≤10 piezas) | 20 |
| 5 | es apertura (≥26 piezas) | 16 |
| 6 | ventaja material (≥+2) | 12 |
| 7 | desventaja material (≤−2) | 14 |
| 8 | juego con blancas | 8 |
| 9 | mi rey expuesto (escudo <2) | 16 |
| 10 | dama rival cerca de mi rey (peligro fino) | 24 |
| 11 | mi dama cerca del rey rival (ataque fino) | 24 |

En cada jugada se evalúa qué micro-condiciones están activas. Cada sentido suma los pesos de **sus**
condiciones que matchean; despiertan los **`KSLOT`=24** de mayor puntaje (más los 5 de valoración,
siempre); el resto duerme sin costar cómputo.

**Sentidos calientes/fríos.** El pool va hasta `GPOOL`=8192 genes; el bicho mantiene un **set caliente**
de `GMAX`=256 (los de mayor prioridad) y el resto **fríos, dormidos**. La prioridad **nace genética** (4
bases por gen → 256 niveles) y **el uso la reparte** hacia su cuota relativa (EMA, `adjust_gene_priorities`,
`PRIO_BLEND`=0.34). Un gen frío de baja prioridad **despierta momentáneamente** si, para *esta* jugada,
su puntaje supera la **mediana** de los calientes activos — juega esa jugada y se vuelve a dormir.

### 3.4 Caché de presets (memoización del selector)

El ranking depende solo de qué condiciones están activas — una **firma de 12 bits** (≤4096 firmas). Se
cachea por firma el ranking ya calculado (`compute_preset`), lookup O(1) en las repeticiones. Cuando el
genoma cambia, `GENOME_VER` sube e invalida los presets (recálculo perezoso). Se persisten en
`nido/presets/` validados por hash del genoma. Por esto el genoma puede tener cientos de sentidos sin
caerse la velocidad.

---

## 4. La mente — mapa conceptual

### 4.1 El concepto y sus valores

```c
typedef struct {
  long gid; int module_key; float feat[72];
  float v_e, v_p;                          // supervivencia / placer
  float strength, useful; long uses, last_age;
  int core, deg, alive; int born_dream; float level; long edge_to[24]; float edge_w[24];
  int deep; int dfrom, dto; float dscore;  // rama PROFUNDA (concepto que piensa)
  float merit; long merit_n, merit_pos;    // [NUEVO] MÉRITO: rendimiento en partidas reales
  long pin_until;                          // [NUEVO] PIN ESPECIAL provisional
} Concept;
```

Un concepto es un **prototipo** en el espacio de 72 rasgos (`feat` = centroide) con:

- **`v_e` (energía)** — cuán bien le fue *materialmente* en posiciones como esta.
- **`v_p` (placer)** — valor afectivo, con cuatro fuentes que el bicho descubre solo y que pueden ser
  **positivas o negativas (asco)**: gozo por **comer** (+), **gloria** por ganar/mate/sacrificio que
  pagó (+), **asco** por que le **coman** (−), **asco** por un **sacrificio en vano** (−). A esto se
  suma el **ansia de rey** (§5/§7) cuando ganó.
- **[NUEVO] `merit` / `merit_n` / `merit_pos`** — la **calidad media de los desenlaces de partidas
  REALES** en las que el concepto disparó. `merit_n` = muestras; `merit_pos` = positivas; la **eficacia**
  = `merit_pos/merit_n`. Es la moneda con la que se compite por el núcleo (§4.6).
- **`level` = realidad graduada** — cuán real fue la experiencia que lo formó: autojuego 0.10 < escuela
  0.30–0.90 (sube con el nivel) < maestro 0.95. Gobierna consolidación y desempata.
- **`deep` + `dfrom/dto/dscore`** — concepto profundo: rama reflexionada (jugada + consecuencia) (§5.5).
- **[NUEVO] `pin_until`** — núcleo **provisional** por pin especial (§4.6).

`module_key` = la **SITUACIÓN** (no un hash de features): la carpeta sale de *qué está pasando*.

### 4.2 Memoria particionada por SITUACIÓN (las "carpetas")

`situacion(p,side)` clasifica la posición en **modo** (ATAQUE / DEFENSA / neutral) × **mi pieza** ×
**pieza rival** en relación. La amenaza **dominante** se elige con `piece_val_` (la pieza más valiosa en
juego): si la dama está colgada, *eso* es lo que está pasando. La terna es la llave (`situ_key`, 0–72:
neutral 0, ataque 1–36, defensa 37–72). `nearest_concept` **solo busca dentro de la misma situación**,
así la memoria es un archivo ordenado por significado táctico. De acá emerge la percepción de amenazas:
estar en la carpeta "Defensa de mi dama" *es* percibir que la dama está en peligro.

### 4.3 Memoria rápida (RAM) + profunda (disco)

`mem[MAXCONCEPTS]` con **`MAXCONCEPTS`=256** conceptos en RAM. Los módulos completos viven en disco
(`nido/memoria/mod_NNN.dat`, dump binario de `Concept`, versionado por `MEM_VER`).

> **[CAMBIO] `MAXLOADED`=1 (un módulo en RAM a la vez).** El doc viejo experimentó con 6 y con 16/2048;
> todo eso se revirtió. Hoy se carga **un** módulo por vez (la situación actual). `ensure_loaded` flushea
> el módulo previo a disco y carga el pedido. Memoria chica, densa y rápida: las mismas posiciones caen
> en los mismos pocos conceptos, se refuerzan rápido y el grafo se arma enseguida.

`alloc_slot`, cuando la RAM se llena, descarta el concepto **más desechable** no-núcleo (menor
`strength`); como último recurso, cede el núcleo de **peor mérito** (nunca se traba). `flush_module`
**purifica** al escribir (`worth_keeping`): tira el ruido de un solo uso y guarda solo lo que merece
(núcleo, probado en real, recurrente, o ramas profundas que volvieron).

### 4.4 Percibir: crear o emparejar (`perceive`)

Dado `p` y su vector `x`: calcula la situación, asegura el módulo cargado, busca el concepto simple más
cercano dentro de esa situación. Si hay uno **dentro de `P_JOIN`**: lo *empareja* (mueve el centroide
hacia `x`, `strength+=0.25`, `uses++`, sube `level` hacia `CUR_REALITY`). Si no: **crea** uno nuevo
(`level = CUR_REALITY`, `born_dream` = soñando?). Los conceptos **profundos** se ignoran acá (§5.5).

### 4.5 El grafo de conceptos

Al percibir, conecta el concepto previo con el actual (`add_edge`, cap `MAXEDGES`=24 por nodo). El grado
`deg` es condición para ascender a núcleo (un concepto bien conectado es parte de un patrón).

### 4.6 [REDISEÑO] El núcleo por MÉRITO

> Este es el cambio conceptual más grande respecto del doc viejo. El núcleo dejó de ser un "salón de la
> fama" gobernado solo por las puertas real/gema. Ahora **cada concepto lleva un mérito** y el núcleo se
> mantiene como **el mejor-de vivo por situación**, con competencia, demociones y tope duro.

**Acumular mérito (`accrue_merit`).** Tras una partida **real** (`CUR_REALITY ≥ MERIT_REAL`=0.30), cada
concepto que disparó recibe el desenlace desde su óptica (`+1` ganó / `−1` perdió / `0` tablas),
ponderado por recencia, con EMA lenta (`MERIT_LR`=0.06): una sola partida no lo define. **El maestro NO
suma mérito** — el mérito es el rendimiento **propio** del bicho jugando, no la victoria del maestro.

**Tres puertas a núcleo (`reinforce`):**

```c
puerta_imit = !born_dream && level>=0.80 && strength>6.0 && deg>=3 && uses>=8;   // [NUEVO]
puerta_real = !born_dream && strength>3.5 && deg>=2 && useful>0.6 && merito_ok;  // merito_ok exige mérito
puerta_gema =  born_dream && strength>6.0 && deg>=4 && useful>2.5 && uses>400 && (age-last_age)<200;
```

- **Puerta imitación [NUEVA]** — un concepto formado por **ajedrez de maestro** (realidad alta,
  `level≥0.80`) que ya recurrió y está conectado entra **por imitación**, sin pedir mérito propio:
  aprende imitando primero. Selectivo por **recurrencia**, no por desenlace, así no satura.
- **Puerta real** — confirmada por lo real + fuerza/conexiones/utilidad **+ mérito** (`merit_n≥8` y
  `merit>0.05`): tiene que haber **rendido** en partidas reales.
- **Puerta gema** — vara altísima: un hallazgo del autojuego probado muchísimo con utilidad sostenida.

**Pin especial (`accrue_merit`).** Una solución **muy eficaz** (>75% de positivos en ≥3 apariciones,
`MERIT_SPECIAL`) para una situación **rara** —que nunca juntaría las muestras de la puerta normal— sube
**ya** al núcleo, **provisional** (`pin_until = age + SPECIAL_WINDOW`=2000 ticks). Si la situación
reaparece y sigue eficaz, se refresca; si se enfría, vence y cae.

**Competencia (`compete_nucleo`, cada 50 ticks).** Prioridad = `nucleo_prio = level + 0.5*merit` (el
maestro, con `level` alto, pesa más que el autojuego):

- **(a) Pin vencido** → demota.
- **(b) Podrido** → un núcleo con mérito netamente negativo (`<MERIT_ROT`=−0.20) y muy probado
  (`merit_n ≥ 2·MERIT_MIN_N`=24) sale.
- **(c) Top-K por situación** → si una situación tiene más de `NUC_PER_SITU`=3 núcleos, demota a los de
  peor prioridad — pero solo si tienen muestra (`merit_n≥12`) y la brecha es clara (`MERIT_GAP`=0.25):
  anti-ruido de media móvil. El maestro (sin mérito propio) es inmune a esta competencia.
- **(d) Tope duro** → el núcleo **jamás** pasa de `NUC_CAP = MAXCONCEPTS/2` = 128. Si excede, demota los
  de peor prioridad hasta volver al tope. Como el maestro tiene `level` alto, lo último que se va es la
  imitación: el núcleo queda dominado por ajedrez de maestro, con el autojuego ganándose el lugar si rinde.

Demotar **no borra**: vuelve a concepto normal y puede reconquistar el núcleo si mejora. El núcleo se
persiste aparte (`_nucleo`, residente desde el arranque).

### 4.7 Poda, retención y olvido (`prune_memory`)

Cada partida, los conceptos **no-núcleo** decaen `strength` por `P_DECAY`. Pero un concepto **bien
probado por el uso** (≥`RETAIN_USES`=20 usos, o `useful`>`RETAIN_USEFUL`=1.0) se protege del olvido.
Solo muere el **débil, poco usado y viejo** (`strength<0.12` y `age−last_age>40`). Separa **permanencia**
(núcleo, por mérito) de **retención** (qué no se olvida, por uso).

---

## 5. Pensar — intuición + material aprendido + cálculo

### 5.1 Intuición (`intuition`)

La evaluación de una posición combina **dos términos**:

```c
conv = (we*v_e + wp*v_p) * conf;             // valor del concepto más cercano (si d<=0.9)
mb   = Σ (0.1 + 0.9*pval[t]) * (#mias - #rivales)_t;   // MATERIAL con los valores APRENDIDOS de V
return conv + mb;
```

- **`conv`** — el concepto más cercano aporta su `v_e`/`v_p`, pesado por confianza (`conf=1/(1+3d)`).
- **[CAMBIO] `mb` — término de MATERIAL explícito**, con los valores de pieza **aprendidos** (`pval[t]`,
  derivados de `V`, §5.7), reusando los conteos que `board_features` ya calculó (sin re-barrer 64
  casillas). Piso chico al peón para que no quede en cero.

**[CAMBIO] El humor amplifica `wp` HACIA ARRIBA cuando va ganando** (corrige el "solo empata"): abajo o
parejo (`mood≤0.5`) `wp≈1.0` → aguanta sin regalar; arriba (`mood 0.5..1`) `wp` sube hasta ~1.6× → presiona
por el mate y **convierte** la ventaja. El lazo viejo bajaba `wp` al ir ganando y apagaba el ansia de
rey justo cuando había que rematar → barajaba a tablas. Ahora el `king_drive` por fin "muerde".

### 5.2 Búsqueda de quietud (`quiesce`)

Antes de juzgar, extiende la línea **solo por capturas** (y evasiones de jaque) hasta que se aquieta, y
recién ahí evalúa con la intuición. Mata el efecto horizonte. Profundidad `QD_REAL`=3 en lo que cuenta,
`QD_DREAM`=3 soñando.

### 5.3 Cálculo (`search`) — alfa-beta

> **[CAMBIO] Profundidad real = 3, no 1.** El doc viejo decía `THINK_MAXD=1` con "la hoja evaluada por
> conceptos". Eso se revirtió: las partidas que cuentan (sparring / vs vos) usan **`EVAL_DEPTH`=3**.

`search` es alfa-beta con `order_caps_first` (capturas primero, mejor poda sin valores de pieza) y hojas
evaluadas por `quiesce`. En `choose` la profundidad arranca en **1** en medio juego parejo y sube a
**2–3** (`P_DEPTH`) donde se **convierte**: con ventaja material (`mine−opp≥1`) o en final (`≤12` piezas,
rama barata). Ver una jugada más permite rematar/coronar en vez de quedarse en tablas.

### 5.4 Elegir jugada (`choose`)

Puntúa cada movimiento con `-search` y agrega:

- **hambre** (`P_HUNGER`): bonus por capturar,
- **reflexión profunda** (`deep_pass`, §5.5): sesgo por la consecuencia reflexionada / la rama reconocida,
- **curiosidad** (`wander`, solo soñando): bonus por posiciones nunca vistas, escalado por `P_CREAT`,
- **temperatura** (`P_TEMP` + extra): softmax estocástico. **En lo que se mide/cuenta** (sparring, tus
  partidas, partidas que califican el currículum) la temperatura es **0**: juega su **mejor** jugada
  (convierte ganadas en victorias, no en tablas por ruido). La exploración queda para el autojuego viejo.

Tras elegir, si la situación es caliente, **guarda el plan** (la línea) arrancando por la jugada elegida.

### 5.5 Conceptos profundos — el concepto que PIENSA

En una **situación caliente** (modo ≠ neutral), el bicho arma el tablero imaginario y reflexiona **mi
jugada → qué me hacen → cómo respondo**:

- **Búsqueda selectiva con poda** (`reflect_ng`, `plausible_move`): explora **solo lo que alguien
  jugaría** — capturas, jaques, promociones, salidas de pieza amenazada, avances hacia el rey rival. Lo
  manso se juzga estático. Un **sacrificio con jaque sobrevive** (no se poda por perder material).
- **Profundidad selectiva** (`reflect_move`): mira a 1; si la consecuencia es clara corta; si no
  profundiza, hasta `DREAM_MAXD`=3 en sueños, **3** en sparring, **7** jugándote a vos.
- **Cosecha** (`harvest_deep`): guarda la mejor rama y las casi-empatadas (`margin`=40) como conceptos
  profundos que cuelgan del tronco-situación.
- **Cache de pensamiento**: la próxima vez **reconoce** la rama cacheada (si `dist < P_JOIN*1.5`) y usa
  su puntaje directo (CAMINO 2: la rama se **siente como valor**, pesada por nivel). Lento al principio,
  rápido después. Telemetría: `reflected`/`recognized` (`DEEP_HARVEST`/`DEEP_HITS`).

> **[CAMBIO] La "base estéril" (`harvest_barren`/`barren`) se retiró.** Ya no deja marcas estériles.

### 5.6 Memoria de plan (`build_plan_from` / `try_plan`)

Cuando reflexiona una situación caliente, guarda **la línea entera** que calculó (la PV, vía
`reflect_pv`): mi jugada → respuesta esperada → mi jugada → … hasta `PLAN_MAX`=200.

> **[CAMBIO] `PLAN_MAX`=200 — planes de verdad, no de un paso.** El doc viejo tenía `PLAN_MAXD=1` (la PV
> era de una jugada, "sigue su plan = 0" por diseño). Ahora la línea es larga.

En cada movida, antes de gastar cómputo, `try_plan`: si la realidad coincide con lo imaginado, **ejecuta
la siguiente del plan sin repensar**; un vistazo barato (prof. 2) **abandona** el plan si surgió algo
claramente mejor (`+200`, oportunismo); si el rival se sale del guion **o** se acaba la línea, repiensa.
El plan es **efímero por partida** y **por lado**. Telemetría: `plan_follow` / `plan_break`.

### 5.7 [NUEVO] La función de valor `V` y los valores de pieza emergentes

El corazón de la versión nueva.

```c
V(estado) = sigmoide( vb + Σ vw[j]·x[j] )   // j=0..47, x = los 48 rasgos base
```

`V` estima **P(ganan blancas)**. Se entrena **online por Montecarlo**: al terminar cada partida (jugada
o de maestro), cada posición vivida se empuja hacia el desenlace real (1 ganaron blancas / 0 negras /
0.5 tablas) con regresión logística (`err = resultado − V`, paso + L2 chico para que no explote).

- En **partidas jugadas** (`play_and_learn_ex`): `lrv`=0.04.
- En **observación de maestros** (`learn_one_pgn_game`): `lrv`=0.015 (suave; los maestros suelen ser
  balanceados, señal de material débil — los valores los anclan las partidas decisivas de la escuela).

**Los valores de pieza caen de los pesos de `V`** (`read_piece_values`): `pval[t]` = cuánto sube `V` por
tener una pieza de tipo `t` (blancas) menos cuánto la baja tenerla el rival, normalizado y suavizado con
media móvil. Arrancan parejos (0.30) y la experiencia los diferencia. **Esto es lo que hace honesta la
tesis**: el bicho *descubre* que la dama vale más que el peón porque tenerla **correlaciona con ganar**,
no porque se lo dijimos. `V` se persiste en `nido/memoria/_vhead.txt`; `pval` viaja en `_stats.txt`.

---

## 6. Aprender — escuela (currículum) + observación

El motor del crecimiento. Cada tick alterna (vía la paridad de `dreams`):

**a) Juega contra su nivel (currículum).** `play_and_learn_ex(c, learn, school_rung, color, observe=0)`:
juega una partida contra un **rival fijo de la escalera** (`ladder_move`) y aprende de ella. Hay
`NRUNG`=7 niveles, del casi-azar al minimax profundo:

| Nivel | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
|-------|---|---|---|---|---|---|---|
| `RUNG_DEPTH` | 1 | 1 | 1 | 2 | 2 | 3 | 4 |
| `RUNG_BLUNDER` (% azar) | .90 | .70 | .45 | .22 | .08 | 0 | 0 |
| `RUNG_ELO` (aprox) | 400 | 600 | 800 | 1050 | 1250 | 1450 | 1650 |

`school_step`: con **>60%** sostenido (ventana >40 partidas) **sube** la vara; con **<15%**, baja un
escalón. `CUR_REALITY` de estas partidas sube con el nivel (0.30 + nivel·0.10).

**b) Observa maestros (`study_pgn_drip`, `GAMES_PER_DRIP`=2/tick).** No juega: percibe cada posición de
los `.pgn`, refuerza conceptos con el resultado, y **entrena `V`** (§5.7). `CUR_REALITY`=0.95 (lo más
real). El cursor es persistente y cicla por los archivos de `nido/maestros/`.

> **[CAMBIO] El "modo observador" máquina-vs-máquina (`observe=1`) sigue implementado** en
> `play_and_learn_ex` pero el lazo principal **no lo invoca**: hoy la observación es de **PGN**, no de
> partidas generadas en vivo. El autojuego (`opp_rung<0`) también está pero inactivo en el lazo.

**El ciclo de refuerzo (al terminar la partida jugada):** calcula economía material por bando,
brillantez (`ganó && mate && déficit≥2`), y para cada concepto visitado refuerza `v_e` (economía) y
`v_p` (gozo de comer + gloria + ansia de rey si ganó − asco por que lo coman − asco por sacrificio en
vano), pesado por recencia; **acumula mérito** (`accrue_merit`); **sacude los pulsos** energía/placer; y
**entrena `V`** con el resultado. El **placer** ahora tiene un objetivo (`p_target`) y pulsa como el
hambre: sube hacia ~25 ganando/atacando, baja hacia ~10 cuando lo muelen (ya no se clava en 9).

---

## 7. El genoma — instintos heredables

Cadena ACGT en un buffer `GENOME_CAP` (holgura para todo el pool). `decode_pheno` la traduce.

### 7.1 [CAMBIO] Los 15 parámetros de carácter (primeras 45 bases)

Cada parámetro usa 3 bases (0–63), mapeado a `[LO,HI]`. **`NPARAM`=15** (`GENE_OFF`=45).

| # | Parámetro | Rango | Qué controla |
|---|-----------|-------|--------------|
| 0 | `aprendizaje` (P_LEARN) | 0.05–0.45 | plasticidad de la memoria |
| 1 | `umbral` (P_JOIN) | 0.15–0.55 | fundir vs crear concepto |
| 2 | `retencion` (P_DECAY) | 0.985–0.9995 | decaimiento de la memoria |
| 3 | `profundidad` (P_DEPTH) | 1–3 | profundidad a la que sube `choose` al convertir |
| 4 | `hambre` (P_HUNGER) | 0–1.2 | tira a capturar material |
| 5 | `peso_energia` (P_WE) | 0–1.5 | cuánto pesa la supervivencia al evaluar |
| 6 | `peso_placer` (P_WP) | 0–2.0 | cuánto pesa la gloria (audacia) |
| 7 | `temperatura` (P_TEMP) | 0–0.6 | azar / pensamiento lateral |
| 8 | `mutabilidad` (P_MUT) | 0.05–0.5 | propensión a mutar |
| 9 | `creatividad` (P_CREAT) | 0–1.0 | novedad en los sueños (wander) |
| 10 | `gozo_comer` (P_EAT) | 0–0.9 | placer por comer piezas |
| 11 | `asco_comido` (P_BITTEN) | 0–0.9 | asco por que le coman |
| 12 | `asco_vano` (P_VAIN) | 0–1.2 | asco por sacrificio en vano |
| 13 | `metabolismo` (P_METAB) | 0.2–1.2 | cuánto el comer mueve la energía |
| 14 | **`apetito_rey` (P_KHUNT)** | **0.30–1.00** | **[NUEVO] ANSIA DE REY** |

> **[NUEVO] `P_KHUNT` — el ansia de rey.** Deseo de **cazar al rey**, alto de base (nace ~0.75 en
> `seed_genome`). Su clave de diseño es que **solo premia si GANÓ**: `king_joy = won_s ? P_KHUNT·(0.5 +
> enemy_stripped)·kpress : 0`. Atacar y morir **no** da placer — aprende a perseguir al rey para dar
> **mate**, no a tirarse en vano. En vivo (`king_drive`), además, **solo empuja si no va abajo en
> material** (`not_losing_mat`): antes empujaba en toda partida violenta, también desde posiciones
> perdidas, y lo mataban.

### 7.2 Los genes de percepción (de la base 45 en adelante)

Bloques de `GENE_LEN`=16 bases por sentido: b[0]=`on` (G/T expresado), b[1..2]=`type`, b[3]=`side`,
b[4]=`ref`, b[5]=`radius`, b[6..11]=`trig` (12 condiciones), **b[12..15]=`prio`** (256 niveles → spread
1–100). Pool sin techo (`GPOOL`=8192) con set caliente acotado (`GMAX`=256); el cómputo por jugada es
constante por más que el pool crezca. La prioridad viaja en las bases (sobrevive al reinicio sin archivo
aparte; FASTA en `nido/genoma.dna`).

---

## 8. Evolución — cómo mejora los instintos y los sentidos

Corre cada **80 ticks** (`evolve`). Toda mejora se valida por **duelos contra el campeón** (la mejor
versión de sí mismo, siempre vigente). Filosofía: **adopción instantánea con gate suave + auditoría
diferida.**

1. **Adopción instantánea** (~45% de las veces, si hay cupo de pool) — nace un sentido nuevo (copia
   divergente que elige su propia condición y contexto). **[NUEVO] `ENEMY_EYES_BIAS`=0.6**: con esa
   probabilidad el sentido nuevo nace **mirando al enemigo** (piezas del rival y/o referencia al rey
   rival), para sacar a la población del pozo de "mirarse a sí misma" y aprender a percibir **debilidades
   enemigas**. El gate descarta lo que no aporta, así que no fuerza nada: solo diversifica la mutación.
   **Gate suave al nacer** (4 duelos cortos): entra salvo que baje la vara claro (`BIRTH_GATE`=−0.8).
2. **Auditoría de fondo** (~50% del resto, `audit_sense`) — agarra un sentido viejo al azar: lo **poda**
   solo si molesta de verdad (`AUDIT_PRUNE`=−0.6) o lo **reemplaza** si un candidato es claramente mejor
   (`AUDIT_SWAP`=0.8). Un sentido neutro se queda → los sentidos se acumulan en vez de borrarse.
3. **Si no toca ninguna**, afina instintos (`tune_instincts`): muta una base de instinto, duela el
   mutante (4 partidas), adopta solo si gana claro.

`chal_margin` mide la *verdadera tarea*: `resultado + 0.30·(material/8) + resistencia` — un sentido que
no flipea el resultado pero alarga la partida o conserva material no se filtra de más. Contadores:
`coevo_chal` / `coevo_win` / `coevo_veto`.

---

## 9. Medición — de la vara baja a la fuerza real

Tres varas, de menor a mayor honestidad:

**a) Aptitud vs azar (`measure`, cada 100 ticks).** Juega contra un movedor aleatorio; puntaje continuo
(premia mate rápido y margen). Vara baja y ruidosa: dice que dejó de ser un tronco, no que sea bueno.

**b) Coevolución vs campeón (§8).** Tasa de cambios que superan a la mejor versión de sí mismo. **Cinta
de correr**: el campeón sube con el bicho; mide progreso, no fuerza absoluta.

**c) Fuerza real — sparring fijo + Elo (`measure_sparring`, cada 400 ticks).** La vara honesta: una
**escalera de 3 rivales fijos** (`SPAR_DEPTHS`={1,2,3}, minimax que **sí** respeta material, con
quiescencia) que **no sube con el bicho**, con anclas de Elo **fijas** `SPAR_ELO`={800,1100,1400}. Del
puntaje contra cada rampa se despeja un Elo con la logística (`SPAR_ELO[r] + 400·log10(sc/(1−sc))`),
combinando las rampas pesadas por confiabilidad (cerca del 50%), con media móvil
(`spar_elo·0.85 + nuevo·0.15`). Juega `SPAR_RUNG_GAMES`=2 por rampa alternando color. Guarda el récord
G/E/P (`spar_wins/draws/losses`), el puntaje por rampa (`spar_rung_sc`) y el **pico** (`spar_elo_peak`).

> El número absoluto depende de las anclas (aproximado); como son **fijas**, la **tendencia es real**.
> Y el detector de mentiras sigue siendo **jugarle**: varias regresiones se cazaron jugando, no en el
> dashboard.

---

## 10. Persistencia y formatos (el nido)

| Archivo | Contenido | Formato |
|---------|-----------|---------|
| `genoma.dna` | el ADN (ACGT) + prioridades en las bases | FASTA |
| `snapshot.json` | estado completo para el dashboard | JSON (cada 5 ticks, atómico) |
| `bitacora.log` | diario de vida | texto, tope 5 MB (`log_cap`) |
| `memoria/mod_NNN.dat` | módulos de memoria profunda (por situación) | binario (`Concept` versionado por `MEM_VER`) |
| `memoria/_stats.txt` | contadores + `pval1..5` (valores de pieza) | texto clave-valor |
| `memoria/_vhead.txt` | **[NUEVO]** pesos `vw[48]` + sesgo `vb` de la función de valor | texto |
| `memoria/_nucleo*` | **[NUEVO]** el núcleo, residente desde el arranque | binario |
| `memoria/_index.dat` | `next_gid` | binario |
| `presets/sNNNN.dat` | caché del selector por firma | binario (hash de genoma) |
| `maestros/*.pgn` | partidas de maestros (las ponés vos) | PGN |
| `comando.txt` | canal de comandos del usuario | texto efímero |

> **Compatibilidad:** `MEM_VER` depende de `sizeof(Concept)`. Como el struct cambió (se agregaron
> `merit`/`merit_n`/`merit_pos`/`pin_until`), **un nido viejo se rechaza limpio** (`version_guard` lo
> limpia antes de cargar nada). Hay que **arrancar de un nido nuevo** viniendo del doc anterior.

---

## 11. El dashboard (`servidor.py` + `dashboard.html`)

`servidor.py` es un HTTP server mínimo: sirve `dashboard.html`, pasa el `snapshot.json` crudo, sirve la
cola de `bitacora.log` y el `genoma.dna`, y escribe `comando.txt` cuando jugás. Toda la inteligencia
está en el C; el dashboard solo lee y pinta. `dashboard.html` hace polling del snapshot cada 1.2 s y
pinta **cinco pestañas**:

**Signos.** Dos trazas tipo electrocardiograma (`energy` / `placer`, escala /25). Dos tarjetas
protagonistas: **ESCUELA** (`school.rung`/`elo`/`score`/`games`, con barra de progreso hacia el 60% que
promueve) y **FUERZA REAL** (`sparring.elo`/`elo_peak`/`games`/G-E-P, con una barra por rampa desde
`rung_score[3]`/`rung_elo[3]` y un `▶` en la rampa actual `rung_cur`). Abajo, una grilla: **"Partidas
estudiadas · soñadas"** (`study.pgn_games` · `games`), brillanteces, conceptos, núcleo, récord
**Ganó/Tablas/Perdió** (de la **escuela**), coevolución (`wins`/`challenges`), generación (+`adoptions`)
y pensamiento lateral.

**Aprendizaje.** Panel **Memoria** (`concepts`/`core` ascendidos, **Salud del núcleo** desde el bloque
`nucleo` —`merit_avg` coloreado por signo, `rated`, `special`, `per_situ`—, `folders_atk`/`folders_def`,
`deep` con el ratio de ramas reconocidas, `plan_follow`/`plan_break`, `imagined`, `deep_modules`,
`wakes`/`sleeps`, `adoptions`, coevolución, aptitud suavizada, `sparring.elo` rotulado "Fuerza real",
`senses` hot/pool), panel **Estudio de maestros** (`pgn_games`/`pgn_positions`) y las **barras de
instintos** del genoma (las 20 entradas de `params`: los 15 parámetros de carácter **+ los 5 `valor_*`
aprendidos**, con sus rangos), que incluyen `apetito_rey` y los valores de pieza emergentes.

**Partida** (jugarle, tablero interactivo), **Genoma** (la cadena ACGT) y **Bitácora** (cola del log,
coloreada por tag).

### 11.1 Reconciliación con el C (hecha)

El `dashboard.html` se sincronizó con el código; ya **no** arrastra elementos del bicho viejo. Los
ajustes aplicados, como registro:

- **Se borraron las tres filas zombi del motor de imitación** (pestaña Aprendizaje): "Jugadas del
  maestro aprendidas", "Coincidió solo" y "Sentidos nutridos" leían `memory.master_moves`/`pgn_agree`/
  `sense_fuel` — campos que el C ya no escribe (§0/§14) y que mostraban `0` fijo.
- **Se agregó la fila "Salud del núcleo"** (panel Memoria): pinta el bloque `nucleo` (`merit_avg`
  coloreado por signo, `rated`, `special`, `per_situ`) que antes el C emitía y nadie leía (§4.6).
- **Cadencia del sparring corregida**: ahora dice "a partir del tick 400" y "6 partidas cada 400 ticks
  contra las 3 rampas fijas" (era "tick 50 / 1 partida cada 25 ticks").
- **Resaltado del genoma a 45 bases** (`codon = i<45`), antes 30 — los 15 parámetros × 3 (§7).
- **Récord G/T/P rotulado "(escuela)"** (antes "(autojuego)"): lo alimentan las partidas de currículum.
- **Se quitó el contador de "miradas"** muerto de la tarjeta Escuela (el modo observador
  máquina-vs-máquina no se invoca; daba `0` fijo). La observación real de PGN se ve en su propio panel.

El `snapshot.json` real (lo que el C escribe en `write_snapshot`) expone: `tick`, `generation`,
`energy`, `placer`, récord `games/wins/draws/losses`, `brilliancies`, `dreams`, `lateral`, `adoptions`,
**`coevolution`** (`challenges`/`wins`/`vetoed`/`last_margin`), **`sparring`**
(`games`/`wins`/`draws`/`losses`/`score`/`elo`/`elo_peak`/`depth`/`rung_score[3]`/`rung_elo[3]`/`rung_cur`),
**`school`** (`rung`/`rungs`/`elo`/`score`/`games`/`observed`/`obs_elo`), **`memory`**
(`concepts`/`core`/`imagined`/`deep_modules`/`wakes`/`sleeps`/`folders_atk`/`folders_def`/`deep`/`reflected`/`recognized`/`plan_follow`/`plan_break`),
**[NUEVO] `nucleo`** (`merit_avg`/`merit_min`/`rated`/`per_situ`/`special`), **`senses`**
(`genes`/`expressed`/`hot`/`pool`/`on_trial`/`trial_ends_in`), **`study`**
(`pgn_games`/`pgn_positions`), `genome`, `params` (los 15 + los 5 `valor_*`), y `game`.

---

## 12. Parámetros y umbrales afinables (referencia rápida)

| Constante | Valor | Efecto |
|-----------|-------|--------|
| `EVAL_DEPTH` | **3** | profundidad de las partidas que cuentan (sparring/vs vos) |
| `QD_REAL` / `QD_DREAM` | 3 / 3 | profundidad de quiescencia |
| `DREAM_MAXD` | 3 | profundidad de reflexión en sueños |
| `KSLOT` | 24 | sentidos despiertos por jugada (+ los 5 de valoración, siempre) |
| `GMAX` / `GPOOL` | 256 / 8192 | set caliente / pool frío de genes |
| `GENE_LEN` / `GENE_OFF` | 16 / 45 | bases por gen / offset tras los 15 instintos |
| `NPARAM` | **15** | parámetros de carácter (nuevo: `P_KHUNT`) |
| `MAXCONCEPTS` / `MAXLOADED` | **256 / 1** | conceptos en RAM / módulos cargados a la vez |
| `MAXEDGES` | 24 | aristas por concepto |
| `PLAN_MAX` | **200** | largo de un plan ejecutado sin repensar |
| `MERIT_LR` | 0.06 | velocidad del mérito (lento: una partida no lo define) |
| `MERIT_PROMO_N` / `MERIT_PROMO` | 8 / 0.05 | muestras y mérito mínimos para puerta real |
| `MERIT_MIN_N` / `MERIT_GAP` | 12 / 0.25 | demotar cuesta más muestras + brecha clara |
| `MERIT_ROT` | −0.20 | núcleo podrido (sale aunque nadie lo supere) |
| `NUC_PER_SITU` / `NUC_CAP` | 3 / 128 | tope de núcleos por situación / tope duro global |
| `MERIT_SPECIAL_N` / `MERIT_SPECIAL` / `SPECIAL_WINDOW` | 3 / 0.75 / 2000 | pin especial provisional |
| `MERIT_REAL` | 0.30 | realidad mínima para que una partida cuente al mérito |
| `puerta_imit` | level≥0.80, str>6, deg≥3, uses≥8 | imitación del maestro al núcleo |
| `RETAIN_USES` / `RETAIN_USEFUL` | 20 / 1.0 | retención por uso (no olvidar) |
| `NRUNG` | 7 | niveles de la escuela |
| escuela: promover / bajar | >60% / <15% (ventana >40) | umbrales de la vara |
| `ELO_RUNGS` / `SPAR_DEPTHS` / `SPAR_ELO` | 3 / {1,2,3} / {800,1100,1400} | escalera de sparring + anclas |
| `SPAR_RUNG_GAMES` | 2 | partidas por rampa (alternando color) |
| `GAMES_PER_DRIP` | 2 | partidas de maestro OBSERVADAS por tick |
| `SELFPLAY_FROM` | 0 | ticks de solo-maestros antes de jugar |
| `RESTUDY_EVERY` | 100000 | ticks entre re-estudios batch de maestros |
| `BIRTH_GATE` / `AUDIT_PRUNE` / `AUDIT_SWAP` | −0.8 / −0.6 / 0.8 | gates de evolución |
| `ENEMY_EYES_BIAS` | **0.6** | prob. de que un sentido nuevo nazca mirando al enemigo |
| `evolve` / `compete_nucleo` / `measure` / `measure_sparring` | 80 / 50 / 100 / 400 | cadencias (ticks) |
| `CUR_REALITY` | autojuego 0.10 · escuela 0.30–0.90 · maestro 0.95 | realidad graduada |
| `V` `lrv` | jugar 0.04 · maestro 0.015 | tasa de entrenamiento de la función de valor |

---

## 13. Cómo correrlo

```bash
gcc -O2 -o organismo organismo.c -lm           # compilar
./organismo --nido ./nido                       # vivir (inmortal; Ctrl+C y relanzar = despierta)
python3 servidor.py --nido ./nido --port 8000   # dashboard en http://localhost:8000
./organismo --nido ./nido --autopsia            # diseccionar: vuelca nido/autopsia.json
```

Para que estudie maestros, poné `.pgn` en `nido/maestros/`. `--autopsia` vuelca el genoma decodificado,
los **valores aprendidos** por pieza, cada sentido con su `despierta_cuando`, y cada concepto con
`v_e`/`v_p`/`core`/`imaginada`/`nivel`/fuerza/usos.

---

> **Método que vale guardar:** el detector de mentiras de las métricas sigue siendo **jugarle**. Después
> de cada cambio grande, el test humano periódico vale tanto como cualquier número del dashboard.
