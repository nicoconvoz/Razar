#include "lego.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================== step 1: clean legos =============================== */

/* The split of per-atom values into two groups that best separates them,
   weighted by how often each atom stands there: the exact optimum, tried at
   every value. -HUGE_VAL when there is only one value. */
static double otsu(const double v[RZ_ATOMS], const uint64_t at[RZ_ATOMS])
{
    unsigned char ord[RZ_ATOMS];
    int k = 0;
    for (int b = 0; b < RZ_ATOMS; b++) {
        if (!at[b]) continue;
        int j = k++;
        while (j > 0 && v[ord[j - 1]] > v[b]) { ord[j] = ord[j - 1]; j--; }
        ord[j] = (unsigned char)b;
    }
    double W = 0, S = 0;
    for (int j = 0; j < k; j++) { W += (double)at[ord[j]]; S += (double)at[ord[j]] * v[ord[j]]; }
    double w0 = 0, s0 = 0, best = 0, mid = -HUGE_VAL;
    for (int j = 0; j + 1 < k; j++) {
        w0 += (double)at[ord[j]];
        s0 += (double)at[ord[j]] * v[ord[j]];
        if (v[ord[j + 1]] <= v[ord[j]]) continue;
        double d = s0 / w0 - (S - s0) / (W - w0);
        double between = w0 * (W - w0) * d * d;
        if (between > best) { best = between; mid = (v[ord[j]] + v[ord[j + 1]]) / 2.0; }
    }
    return mid;
}

/* the same over the mind's uncertainty about what follows each atom */
static double best_split(const rz_mind *m, const uint64_t at[RZ_ATOMS])
{
    double hv[RZ_ATOMS] = {0};
    for (int b = 0; b < RZ_ATOMS; b++) {
        if (!at[b]) continue;
        double v = rz_branching(m, (unsigned char)b);
        hv[b] = v > 0.0 ? log(v) : 0.0;
    }
    return otsu(hv, at);
}

size_t rz_lego_cut(const rz_mind *m, const unsigned char *t, size_t n,
                   unsigned char *ends)
{
    if (n == 0) return 0;
    memset(ends, 0, n);

    /* how uncertain the mind is about what follows each byte, in nats */
    double *h = malloc(n * sizeof *h);
    unsigned char *peak = calloc(n, 1);
    if (!h || !peak) { free(h); free(peak); ends[n - 1] = 1; return 1; }
    for (size_t i = 0; i < n; i++) {
        double b = rz_branching(m, t[i]);
        h[i] = b > 0.0 ? log(b) : 0.0;
    }

    /* a peak's height belongs to its byte, so the peaks are a histogram over
       at most RZ_ATOMS heights */
    uint64_t at[RZ_ATOMS] = {0};
    for (size_t i = 0; i < n; i++) {
        int up = i == 0 || h[i] > h[i - 1];
        int down = i + 1 == n || h[i] > h[i + 1];
        if (!(up && down)) continue;
        peak[i] = 1;
        at[t[i]]++;
    }

    /* The peaks come in two heights -- the small rises inside a word and the
       tall ones between words. Where one becomes the other is the split that
       best separates them (the exact optimum, tried at every height; a
       two-means started from the extremes got caught on a few rare bytes and
       halved its precision on long text). The text decides; no number is
       written here. */
    double mid = best_split(m, at);
    int two = mid > -HUGE_VAL;

    size_t legos = 0, len = 0;
    for (size_t i = 0; i < n; i++) {
        len++;
        int end = peak[i] && (!two || h[i] > mid);
        if (end || len == RZ_LEGO_SPAN || i + 1 == n) {
            ends[i] = 1;
            legos++;
            len = 0;
        }
    }
    free(h);
    free(peak);
    return legos;
}

size_t rz_word_cut(const rz_mind *m, const unsigned char *t, size_t n,
                   unsigned char *ends)
{
    size_t legos = rz_lego_cut(m, t, n, ends);
    if (legos == 0) return 0;

    /* the atoms legos end on, and the split of their heights once more */
    uint64_t at[RZ_ATOMS] = {0};
    for (size_t i = 0; i < n; i++) if (ends[i]) at[t[i]]++;
    double mid = best_split(m, at);
    if (mid == -HUGE_VAL) return legos;    /* one height: every lego is a word */

    size_t words = 0;
    for (size_t i = 0; i < n; i++) {
        if (!ends[i]) continue;
        double v = rz_branching(m, t[i]);
        double h = v > 0.0 ? log(v) : 0.0;
        if (h > mid || i + 1 == n) words++;
        else ends[i] = 0;
    }
    return words;
}

/* ===================== the lexicon ======================================= */

static uint32_t spell_hash(const unsigned char *s, size_t len)
{
    uint32_t v = 2166136261u;
    for (size_t i = 0; i < len; i++) { v ^= s[i]; v *= 16777619u; }
    return v;
}

static uint32_t pair_hash(uint32_t a, uint32_t b)
{
    uint64_t k = ((uint64_t)a << 32 | b) * 0x9E3779B97F4A7C15ull;
    return (uint32_t)(k >> 32);
}

static void drop_derived(rz_lexicon *x)
{
    free(x->follow);    x->follow = NULL;
    free(x->out_start); x->out_start = NULL;
    free(x->out_pair);  x->out_pair = NULL;
    free(x->top);       x->top = NULL;
    x->kinds = 0;
}

void rz_lexicon_init(rz_lexicon *x)
{
    memset(x, 0, sizeof *x);
}

void rz_lexicon_free(rz_lexicon *x)
{
    drop_derived(x);
    free(x->lego);
    free(x->slot);
    free(x->pair);
    free(x->pslot);
    free(x->seq);
    memset(x, 0, sizeof *x);
}

static int rehash_legos(rz_lexicon *x, uint32_t slots)
{
    int32_t *s = malloc((size_t)slots * sizeof *s);
    if (!s) return 0;
    for (uint32_t i = 0; i < slots; i++) s[i] = -1;
    for (uint32_t id = 0; id < x->n; id++) {
        uint32_t k = spell_hash(x->lego[id].s, x->lego[id].len) & (slots - 1);
        while (s[k] >= 0) k = (k + 1) & (slots - 1);
        s[k] = (int32_t)id;
    }
    free(x->slot);
    x->slot = s;
    x->slots = slots;
    return 1;
}

static int rehash_pairs(rz_lexicon *x, uint32_t slots)
{
    int32_t *s = malloc((size_t)slots * sizeof *s);
    if (!s) return 0;
    for (uint32_t i = 0; i < slots; i++) s[i] = -1;
    for (uint32_t p = 0; p < x->npair; p++) {
        uint32_t k = pair_hash(x->pair[p].a, x->pair[p].b) & (slots - 1);
        while (s[k] >= 0) k = (k + 1) & (slots - 1);
        s[k] = (int32_t)p;
    }
    free(x->pslot);
    x->pslot = s;
    x->pslots = slots;
    return 1;
}

int32_t rz_lexicon_find(const rz_lexicon *x, const unsigned char *s, size_t len)
{
    if (!x->slots || len == 0 || len > RZ_LEGO_SPAN) return -1;
    uint32_t k = spell_hash(s, len) & (x->slots - 1);
    while (x->slot[k] >= 0) {
        const rz_lego *g = &x->lego[x->slot[k]];
        if (g->len == len && memcmp(g->s, s, len) == 0) return x->slot[k];
        k = (k + 1) & (x->slots - 1);
    }
    return -1;
}

static int32_t intern(rz_lexicon *x, const unsigned char *s, size_t len)
{
    int32_t id = rz_lexicon_find(x, s, len);
    if (id >= 0) { x->lego[id].count++; return id; }

    if (x->n == x->cap) {
        uint32_t cap = x->cap ? x->cap * 2 : 1024;
        rz_lego *g = realloc(x->lego, (size_t)cap * sizeof *g);
        if (!g) return -1;
        x->lego = g;
        x->cap = cap;
    }
    rz_lego *g = &x->lego[x->n];
    memset(g, 0, sizeof *g);
    g->len = (uint8_t)len;
    memcpy(g->s, s, len);
    g->count = 1;
    g->kind = -1;
    x->n++;
    if ((uint64_t)x->n * 2 > x->slots) {
        if (!rehash_legos(x, x->slots ? x->slots * 2 : 2048)) { x->n--; return -1; }
    } else {
        uint32_t k = spell_hash(s, len) & (x->slots - 1);
        while (x->slot[k] >= 0) k = (k + 1) & (x->slots - 1);
        x->slot[k] = (int32_t)(x->n - 1);
    }
    return (int32_t)(x->n - 1);
}

static int32_t find_pair(const rz_lexicon *x, uint32_t a, uint32_t b)
{
    if (!x->pslots) return -1;
    uint32_t k = pair_hash(a, b) & (x->pslots - 1);
    while (x->pslot[k] >= 0) {
        const rz_lego_pair *p = &x->pair[x->pslot[k]];
        if (p->a == a && p->b == b) return x->pslot[k];
        k = (k + 1) & (x->pslots - 1);
    }
    return -1;
}

static void remember(rz_lexicon *x, int32_t id)
{
    if (x->nseq == x->seqcap) {
        uint32_t cap = x->seqcap ? x->seqcap * 2 : 4096;
        int32_t *q = realloc(x->seq, (size_t)cap * sizeof *q);
        if (!q) return;
        x->seq = q;
        x->seqcap = cap;
    }
    x->seq[x->nseq++] = id;
}

static void count_pair(rz_lexicon *x, uint32_t a, uint32_t b)
{
    int32_t p = find_pair(x, a, b);
    if (p >= 0) { x->pair[p].count++; return; }

    if (x->npair == x->pcap) {
        uint32_t cap = x->pcap ? x->pcap * 2 : 4096;
        rz_lego_pair *q = realloc(x->pair, (size_t)cap * sizeof *q);
        if (!q) return;
        x->pair = q;
        x->pcap = cap;
    }
    x->pair[x->npair] = (rz_lego_pair){ a, b, 1 };
    x->npair++;
    if ((uint64_t)x->npair * 2 > x->pslots) {
        if (!rehash_pairs(x, x->pslots ? x->pslots * 2 : 8192)) x->npair--;
        return;
    }
    uint32_t k = pair_hash(a, b) & (x->pslots - 1);
    while (x->pslot[k] >= 0) k = (k + 1) & (x->pslots - 1);
    x->pslot[k] = (int32_t)(x->npair - 1);
}

static void read_cut(rz_lexicon *x, const unsigned char *t, size_t n,
                     unsigned char *ends);

void rz_lexicon_read(rz_lexicon *x, const rz_mind *m,
                     const unsigned char *t, size_t n)
{
    if (n == 0) return;
    unsigned char *ends = malloc(n);
    if (!ends) return;
    rz_lego_cut(m, t, n, ends);
    read_cut(x, t, n, ends);
}

/* the cut at the widest gap between sorted values: no weights, no parameter */
static double widest_gap(const double v[RZ_ATOMS], const uint64_t at[RZ_ATOMS])
{
    unsigned char ord[RZ_ATOMS];
    int k = 0;
    for (int b = 0; b < RZ_ATOMS; b++) {
        if (!at[b]) continue;
        int j = k++;
        while (j > 0 && v[ord[j - 1]] > v[b]) { ord[j] = ord[j - 1]; j--; }
        ord[j] = (unsigned char)b;
    }
    double best = 0, mid = -HUGE_VAL;
    for (int j = 0; j + 1 < k; j++)
        if (v[ord[j + 1]] - v[ord[j]] > best) {
            best = v[ord[j + 1]] - v[ord[j]];
            mid = (v[ord[j + 1]] + v[ord[j]]) / 2.0;
        }
    return mid;
}

/* The atoms that close a word, without naming one. The separators are the
 * atoms words end on. A mark must do two things:
 *
 *   close    be followed by a separator nearly always -- cut at the widest gap
 *            between atoms. (Cut by the weighted optimum, the letters -- most of
 *            the text -- dragged the cut down and "s", then every letter,
 *            joined: "que" came apart as "q" + "ue". Grown round by round, a
 *            small vocabulary let whole words join, letter after letter.)
 *   attach   come after many different atoms. The "g" of "dog" closes its word
 *            but only ever after an "o".
 *
 * On the New Testament read as continuous text this finds , . : ; ) and the
 * space. One round: with line breaks read as spaces there is no "
" standing
 * between a period and its separator. */
static void edge_atoms(const unsigned char *t, size_t n, const unsigned char *wend,
                       uint8_t sep[RZ_ATOMS], uint8_t edge[RZ_ATOMS])
{
    memset(sep, 0, RZ_ATOMS);
    /* the last byte is left out: the text simply stopped there */
    for (size_t i = 0; i + 1 < n; i++) if (wend[i]) sep[t[i]] = 1;
    memcpy(edge, sep, RZ_ATOMS);

    uint64_t at[RZ_ATOMS] = {0}, one[RZ_ATOMS] = {0}, closed[RZ_ATOMS] = {0};
    for (size_t i = 0; i < n; i++) at[t[i]]++;
    for (int b = 0; b < RZ_ATOMS; b++) one[b] = at[b] ? 1 : 0;
    for (size_t i = 0; i + 1 < n; i++) if (sep[t[i + 1]]) closed[t[i]]++;

    double share[RZ_ATOMS] = {0}, variety[RZ_ATOMS] = {0};
    for (int b = 0; b < RZ_ATOMS; b++)
        if (at[b]) share[b] = (double)closed[b] / (double)at[b];

    uint32_t *before = calloc((size_t)RZ_ATOMS * RZ_ATOMS, sizeof *before);
    if (!before) return;
    for (size_t i = 1; i < n; i++) before[(size_t)t[i] * RZ_ATOMS + t[i - 1]]++;
    for (int b = 0; b < RZ_ATOMS; b++) {
        if (!at[b]) continue;
        double tot = 0, h = 0;
        for (int a = 0; a < RZ_ATOMS; a++) tot += before[(size_t)b * RZ_ATOMS + a];
        for (int a = 0; a < RZ_ATOMS; a++) {
            double c = before[(size_t)b * RZ_ATOMS + a];
            if (c > 0) h -= c / tot * log(c / tot);
        }
        variety[b] = h;
    }
    free(before);

    double smid = widest_gap(share, at), vmid = otsu(variety, one);
    if (smid == -HUGE_VAL || vmid == -HUGE_VAL) return;
    for (int b = 0; b < RZ_ATOMS; b++)
        if (at[b] && !sep[b] && share[b] > smid && variety[b] > vmid) edge[b] = 1;
}

/* Every word becomes its core and, when it has one, its closing mark; the
   separators themselves are dropped -- the order already says where a word
   ends. Learning interns and counts; looking up only finds. */
static uint32_t walk_words(rz_lexicon *x, const rz_mind *m,
                           const unsigned char *t, size_t n, int learn,
                           int32_t *ids, uint32_t cap)
{
    if (n == 0) return 0;
    unsigned char *wend = malloc(n);
    if (!wend) return 0;
    rz_word_cut(m, t, n, wend);
    uint8_t sep[RZ_ATOMS], edge[RZ_ATOMS];
    edge_atoms(t, n, wend, sep, edge);

    if (learn) drop_derived(x);
    uint32_t k = 0;
    int32_t prev = -1;
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        if (!wend[i]) continue;
        size_t c = i + 1;
        while (c > start && edge[t[c - 1]]) c--;
        unsigned char mark[RZ_LEGO_SPAN];
        size_t ml = 0;
        for (size_t q = c; q <= i && ml < RZ_LEGO_SPAN; q++)
            if (!sep[t[q]]) mark[ml++] = t[q];
        if (c == start && ml == 0) { start = i + 1; continue; }

        for (int part = 0; part < 2; part++) {
            const unsigned char *s0 = part ? mark : t + start;
            size_t len = part ? ml : c - start;
            if (len == 0) continue;
            if (len > RZ_LEGO_SPAN) len = RZ_LEGO_SPAN;
            if (learn) {
                int32_t id = intern(x, s0, len);
                if (id >= 0 && prev >= 0) count_pair(x, (uint32_t)prev, (uint32_t)id);
                remember(x, id);
                prev = id;
            } else if (k < cap) {
                ids[k++] = rz_lexicon_find(x, s0, len);
            }
        }
        start = i + 1;
    }
    if (learn) remember(x, -1);
    free(wend);
    return k;
}

void rz_lexicon_read_words(rz_lexicon *x, const rz_mind *m,
                           const unsigned char *t, size_t n)
{
    walk_words(x, m, t, n, 1, NULL, 0);
}

uint32_t rz_lexicon_words_of(const rz_lexicon *x, const rz_mind *m,
                             const unsigned char *t, size_t n,
                             int32_t *ids, uint32_t cap)
{
    /* the walker only finds when not learning; nothing in x is touched */
    return walk_words((rz_lexicon *)x, m, t, n, 0, ids, cap);
}

/* takes ends and frees it */
static void read_cut(rz_lexicon *x, const unsigned char *t, size_t n,
                     unsigned char *ends)
{
    drop_derived(x);                  /* new counts: the kinds are stale */

    int32_t prev = -1;
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        if (!ends[i]) continue;
        int32_t id = intern(x, t + start, i + 1 - start);
        if (id >= 0 && prev >= 0) count_pair(x, (uint32_t)prev, (uint32_t)id);
        remember(x, id);
        prev = id;
        start = i + 1;
    }
    remember(x, -1);                  /* two reads never run into each other */
    free(ends);
}

/* ===================== step 2: kinds by company ========================== */

/* A lego's company is a sparse vector: who stood just before it and who just
   after, among the commonest legos, on log counts. Every pair touches exactly
   one cell of two vectors, so all of it is read off the pair table directly. */
typedef struct {
    int32_t *fidx;      /* lego -> feature column, or -1 */
    uint32_t F;
    double  *norm;      /* length of each lego's company vector */
} company;

static int company_build(const rz_lexicon *x, company *c)
{
    c->fidx = malloc((size_t)x->n * sizeof *c->fidx);
    c->norm = calloc(x->n, sizeof *c->norm);
    uint32_t *order = malloc((size_t)x->n * sizeof *order);
    if (!c->fidx || !c->norm || !order) { free(order); return 0; }

    for (uint32_t i = 0; i < x->n; i++) { order[i] = i; c->fidx[i] = -1; }
    c->F = x->n < RZ_LEGO_COMPANY ? x->n : RZ_LEGO_COMPANY;
    /* partial selection: the F commonest, ties to the earlier-born */
    for (uint32_t f = 0; f < c->F; f++) {
        uint32_t best = f;
        for (uint32_t j = f + 1; j < x->n; j++)
            if (x->lego[order[j]].count > x->lego[order[best]].count) best = j;
        uint32_t tmp = order[f]; order[f] = order[best]; order[best] = tmp;
        c->fidx[order[f]] = (int32_t)f;
    }
    free(order);

    for (uint32_t p = 0; p < x->npair; p++) {
        const rz_lego_pair *q = &x->pair[p];
        double v = log1p((double)q->count);
        if (c->fidx[q->a] >= 0) c->norm[q->b] += v * v;
        if (c->fidx[q->b] >= 0) c->norm[q->a] += v * v;
    }
    for (uint32_t i = 0; i < x->n; i++) c->norm[i] = sqrt(c->norm[i]);
    return 1;
}

static void company_free(company *c)
{
    free(c->fidx);
    free(c->norm);
}

/* cosine of every lego against every centroid (rows of 2F) */
static void company_dots(const rz_lexicon *x, const company *c,
                         const double *C, uint32_t K, double *score)
{
    memset(score, 0, (size_t)x->n * K * sizeof *score);
    uint32_t W = 2 * c->F;
    for (uint32_t p = 0; p < x->npair; p++) {
        const rz_lego_pair *q = &x->pair[p];
        double v = log1p((double)q->count);
        if (c->fidx[q->a] >= 0)
            for (uint32_t k = 0; k < K; k++)
                score[(size_t)q->b * K + k] += v * C[(size_t)k * W + (uint32_t)c->fidx[q->a]];
        if (c->fidx[q->b] >= 0)
            for (uint32_t k = 0; k < K; k++)
                score[(size_t)q->a * K + k] += v * C[(size_t)k * W + c->F + (uint32_t)c->fidx[q->b]];
    }
    for (uint32_t i = 0; i < x->n; i++)
        for (uint32_t k = 0; k < K; k++)
            score[(size_t)i * K + k] = c->norm[i] > 0 ? score[(size_t)i * K + k] / c->norm[i] : 0;
}

/* add lego i's normalised company into row `row` */
static void company_add(const rz_lexicon *x, const company *c, uint32_t i,
                        double *row)
{
    if (c->norm[i] <= 0) return;
    for (uint32_t p = 0; p < x->npair; p++) {
        const rz_lego_pair *q = &x->pair[p];
        double v = log1p((double)q->count) / c->norm[i];
        if (q->b == i && c->fidx[q->a] >= 0) row[c->fidx[q->a]] += v;
        if (q->a == i && c->fidx[q->b] >= 0) row[c->F + (uint32_t)c->fidx[q->b]] += v;
    }
}

static void unit(double *row, uint32_t W)
{
    double s = 0;
    for (uint32_t j = 0; j < W; j++) s += row[j] * row[j];
    s = sqrt(s);
    if (s > 0) for (uint32_t j = 0; j < W; j++) row[j] /= s;
}

static uint64_t next_rand(uint64_t *r)
{
    *r ^= *r << 13; *r ^= *r >> 7; *r ^= *r << 17;
    return *r;
}

static void build_index(rz_lexicon *x)
{
    free(x->out_start);
    free(x->out_pair);
    x->out_start = calloc((size_t)x->n + 1, sizeof *x->out_start);
    x->out_pair = malloc(((size_t)x->npair + 1) * sizeof *x->out_pair);
    if (!x->out_start || !x->out_pair) return;
    for (uint32_t p = 0; p < x->npair; p++) x->out_start[x->pair[p].a + 1]++;
    for (uint32_t i = 0; i < x->n; i++) x->out_start[i + 1] += x->out_start[i];
    uint32_t *fill = malloc(((size_t)x->n + 1) * sizeof *fill);
    if (!fill) return;
    memcpy(fill, x->out_start, ((size_t)x->n + 1) * sizeof *fill);
    for (uint32_t p = 0; p < x->npair; p++) x->out_pair[fill[x->pair[p].a]++] = p;
    free(fill);
}

static void build_kinds(rz_lexicon *x)
{
    uint32_t K = x->kinds;
    free(x->follow);
    free(x->top);
    x->follow = calloc((size_t)K * K, sizeof *x->follow);
    x->top = malloc((size_t)K * sizeof *x->top);
    if (!x->follow || !x->top) return;
    for (uint32_t k = 0; k < K; k++) x->top[k] = -1;
    for (uint32_t i = 0; i < x->n; i++) {
        int32_t k = x->lego[i].kind;
        if (k < 0) continue;
        if (x->top[k] < 0 || x->lego[i].count > x->lego[x->top[k]].count) x->top[k] = (int32_t)i;
    }
    for (uint32_t p = 0; p < x->npair; p++) {
        int32_t ka = x->lego[x->pair[p].a].kind, kb = x->lego[x->pair[p].b].kind;
        if (ka >= 0 && kb >= 0) x->follow[(size_t)ka * K + (uint32_t)kb] += x->pair[p].count;
    }
    build_index(x);
}

void rz_lexicon_sort(rz_lexicon *x, uint32_t kinds, uint64_t seed)
{
    drop_derived(x);
    for (uint32_t i = 0; i < x->n; i++) x->lego[i].kind = -1;
    if (x->n == 0 || kinds == 0) return;

    company c;
    if (!company_build(x, &c)) { company_free(&c); return; }

    /* Only a lego seen twice has a company worth judging; one sighting is an
       anecdote. The rest are placed afterwards, against the kinds found. */
    uint32_t *member = malloc((size_t)x->n * sizeof *member);
    uint32_t nm = 0;
    for (uint32_t i = 0; i < x->n; i++)
        if (x->lego[i].count >= 2 && c.norm[i] > 0) member[nm++] = i;
    uint32_t K = kinds < nm ? kinds : nm;
    if (K == 0) { free(member); company_free(&c); return; }

    uint32_t W = 2 * c.F;
    double *C = calloc((size_t)K * W, sizeof *C);
    double *score = malloc((size_t)x->n * K * sizeof *score);
    double *d = malloc((size_t)nm * sizeof *d);
    int32_t *label = malloc((size_t)x->n * sizeof *label);
    if (!C || !score || !d || !label) goto out;

    /* k-means++: each new centre drawn in proportion to how far it is from
       the centres already chosen */
    uint64_t r = seed * 2654435761u + 0x9E3779B97F4A7C15ull;
    if (!r) r = 1;
    company_add(x, &c, member[next_rand(&r) % nm], C);
    for (uint32_t k = 1; k < K; k++) {
        company_dots(x, &c, C, k, score);
        double tot = 0;
        for (uint32_t j = 0; j < nm; j++) {
            double best = -1;
            for (uint32_t q = 0; q < k; q++)
                if (score[(size_t)member[j] * k + q] > best) best = score[(size_t)member[j] * k + q];
            d[j] = 2.0 - 2.0 * best;
            if (d[j] < 0) d[j] = 0;
            tot += d[j];
        }
        double pick = tot > 0 ? (double)(next_rand(&r) % 1000000007u) / 1000000007.0 * tot : 0;
        uint32_t chosen = member[next_rand(&r) % nm];
        for (uint32_t j = 0; j < nm && tot > 0; j++) {
            if (pick < d[j]) { chosen = member[j]; break; }
            pick -= d[j];
        }
        company_add(x, &c, chosen, C + (size_t)k * W);
    }

    for (uint32_t i = 0; i < x->n; i++) label[i] = -1;
    for (int it = 0; it < 30; it++) {
        company_dots(x, &c, C, K, score);
        int moved = 0;
        for (uint32_t j = 0; j < nm; j++) {
            uint32_t i = member[j];
            int32_t best = 0;
            for (uint32_t k = 1; k < K; k++)
                if (score[(size_t)i * K + k] > score[(size_t)i * K + (uint32_t)best]) best = (int32_t)k;
            if (best != label[i]) { label[i] = best; moved = 1; }
        }
        if (!moved && it > 0) break;

        double *nc = calloc((size_t)K * W, sizeof *nc);
        if (!nc) break;
        for (uint32_t p = 0; p < x->npair; p++) {
            const rz_lego_pair *q = &x->pair[p];
            double v = log1p((double)q->count);
            if (label[q->b] >= 0 && c.fidx[q->a] >= 0)
                nc[(size_t)label[q->b] * W + (uint32_t)c.fidx[q->a]] += v / c.norm[q->b];
            if (label[q->a] >= 0 && c.fidx[q->b] >= 0)
                nc[(size_t)label[q->a] * W + c.F + (uint32_t)c.fidx[q->b]] += v / c.norm[q->a];
        }
        for (uint32_t k = 0; k < K; k++) {
            double s = 0;
            for (uint32_t j = 0; j < W; j++) s += nc[(size_t)k * W + j] * nc[(size_t)k * W + j];
            if (s > 0) memcpy(C + (size_t)k * W, nc + (size_t)k * W, W * sizeof *C);  /* an empty kind keeps its centre */
            unit(C + (size_t)k * W, W);
        }
        free(nc);
    }

    /* every lego with any company gets the nearest kind, members included */
    company_dots(x, &c, C, K, score);
    for (uint32_t i = 0; i < x->n; i++) {
        if (c.norm[i] <= 0) continue;
        int32_t best = 0;
        for (uint32_t k = 1; k < K; k++)
            if (score[(size_t)i * K + k] > score[(size_t)i * K + (uint32_t)best]) best = (int32_t)k;
        x->lego[i].kind = best;
    }
    x->kinds = K;
    build_kinds(x);

out:
    free(C); free(score); free(d); free(label); free(member);
    company_free(&c);
}

int32_t rz_lexicon_kind(const rz_lexicon *x, int32_t lego)
{
    if (lego < 0 || (uint32_t)lego >= x->n || !x->kinds) return -1;
    return x->lego[lego].kind;
}

static uint64_t row_total(const rz_lexicon *x, int32_t k, int incoming)
{
    uint64_t s = 0;
    for (uint32_t j = 0; j < x->kinds; j++)
        s += incoming ? x->follow[(size_t)j * x->kinds + (uint32_t)k]
                      : x->follow[(size_t)k * x->kinds + j];
    return s;
}

int32_t rz_lexicon_guess_kind(const rz_lexicon *x, int32_t left, int32_t right)
{
    int32_t kl = rz_lexicon_kind(x, left), kr = rz_lexicon_kind(x, right);
    if ((kl < 0 && kr < 0) || !x->follow) return -1;
    uint32_t K = x->kinds;

    /* P(kind | the one before) * P(the one after | kind) */
    int32_t best = -1;
    double best_s = 0;
    for (uint32_t k = 0; k < K; k++) {
        double s = 1.0;
        if (kl >= 0) {
            uint64_t t = row_total(x, kl, 0);
            s *= t ? (double)x->follow[(size_t)kl * K + k] / (double)t : 0;
        }
        if (kr >= 0) {
            uint64_t t = row_total(x, (int32_t)k, 0);
            s *= t ? (double)x->follow[(size_t)k * K + (uint32_t)kr] / (double)t : 0;
        }
        if (s > best_s) { best_s = s; best = (int32_t)k; }
    }
    return best;
}

/* ===================== step 3: play ====================================== */

int32_t rz_lexicon_next_kind(const rz_lexicon *x, int32_t kind)
{
    if (kind < 0 || (uint32_t)kind >= x->kinds || !x->follow) return -1;
    int32_t best = -1;
    uint64_t most = 0;
    for (uint32_t k = 0; k < x->kinds; k++) {
        uint64_t v = x->follow[(size_t)kind * x->kinds + k];
        if (v > most) { most = v; best = (int32_t)k; }
    }
    return best;
}

int32_t rz_lexicon_next(const rz_lexicon *x, int32_t lego)
{
    int32_t nk = rz_lexicon_next_kind(x, rz_lexicon_kind(x, lego));
    if (nk < 0 || !x->out_start) return -1;

    int32_t best = -1;
    uint32_t most = 0;
    for (uint32_t o = x->out_start[lego]; o < x->out_start[lego + 1]; o++) {
        const rz_lego_pair *q = &x->pair[x->out_pair[o]];
        if (x->lego[q->b].kind == nk && q->count > most) { most = q->count; best = (int32_t)q->b; }
    }
    return best >= 0 ? best : x->top[nk];
}

/* ===================== persistence ======================================= */

#define RZ_LEXICON_MAGIC 0x584C5A52u   /* "RZLX" */
#define RZ_LEXICON_VER   (2u + 1000u * (uint32_t)sizeof(rz_lego))

int rz_lexicon_save(const rz_lexicon *x, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return RZ_ERR_IO;
    uint32_t head[6] = { RZ_LEXICON_MAGIC, RZ_LEXICON_VER, x->n, x->npair, x->kinds, x->nseq };
    int ok = fwrite(head, sizeof head, 1, f) == 1
          && (x->n == 0 || fwrite(x->lego, sizeof *x->lego, x->n, f) == x->n)
          && (x->npair == 0 || fwrite(x->pair, sizeof *x->pair, x->npair, f) == x->npair)
          && (x->nseq == 0 || fwrite(x->seq, sizeof *x->seq, x->nseq, f) == x->nseq);
    if (ok && x->kinds)
        ok = fwrite(x->follow, sizeof *x->follow, (size_t)x->kinds * x->kinds, f)
             == (size_t)x->kinds * x->kinds;
    if (fclose(f) != 0) ok = 0;
    return ok ? RZ_OK : RZ_ERR_IO;
}

int rz_lexicon_load(rz_lexicon *x, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return RZ_ERR_MISSING;
    uint32_t head[6];
    if (fread(head, sizeof head, 1, f) != 1) { fclose(f); return RZ_ERR_IO; }
    if (head[0] != RZ_LEXICON_MAGIC || head[1] != RZ_LEXICON_VER) { fclose(f); return RZ_ERR_VERSION; }

    rz_lexicon y;
    rz_lexicon_init(&y);
    y.n = y.cap = head[2];
    y.npair = y.pcap = head[3];
    y.kinds = head[4];
    y.nseq = y.seqcap = head[5];
    y.lego = malloc(((size_t)y.n + 1) * sizeof *y.lego);
    y.pair = malloc(((size_t)y.npair + 1) * sizeof *y.pair);
    y.seq = malloc(((size_t)y.nseq + 1) * sizeof *y.seq);
    int ok = y.lego && y.pair && y.seq
          && fread(y.lego, sizeof *y.lego, y.n, f) == y.n
          && fread(y.pair, sizeof *y.pair, y.npair, f) == y.npair
          && fread(y.seq, sizeof *y.seq, y.nseq, f) == y.nseq;
    if (ok && y.kinds) {
        y.follow = malloc((size_t)y.kinds * y.kinds * sizeof *y.follow);
        ok = y.follow && fread(y.follow, sizeof *y.follow, (size_t)y.kinds * y.kinds, f)
                         == (size_t)y.kinds * y.kinds;
    }
    fclose(f);
    uint32_t ls = 2048, ps = 8192;
    while ((uint64_t)y.n * 2 > ls) ls *= 2;
    while ((uint64_t)y.npair * 2 > ps) ps *= 2;
    ok = ok && rehash_legos(&y, ls) && rehash_pairs(&y, ps);
    if (!ok) { rz_lexicon_free(&y); return RZ_ERR_IO; }

    if (y.kinds) {
        uint64_t *keep = y.follow;
        y.follow = NULL;
        build_kinds(&y);                 /* top and index; follow recounted */
        free(keep);
    }
    rz_lexicon_free(x);
    *x = y;
    return RZ_OK;
}


/* ===================== curiosity (P34) =================================== */

/* Where the answer can stand around the relation r: just after it, one
   further, or just before it (with the subject on the other side). */
enum { AT_NEXT, AT_FAR, AT_BEFORE, AT_N };

static int32_t at(const rz_lexicon *x, uint32_t i, int d)
{
    int64_t j = (int64_t)i + d;
    return (j >= 0 && (uint64_t)j < x->nseq) ? x->seq[j] : -1;
}

uint32_t rz_lexicon_ask(rz_lexicon *x, int32_t subject, int32_t relation,
                        rz_answer *out, uint32_t cap)
{
    if (subject < 0 || relation < 0 || cap == 0 || !x->nseq || !x->n) return 0;

    /* What the relation gives by itself, at each place -- and what it gives
       when the subject is on the other side. An answer is only what the
       subject added (D50): "la" stands after "es" whatever is asked, so it
       answers nothing; "cabeza" stands after "Cristo es" far more than after
       "es" alone. No article is named or skipped. */
    uint32_t *base = calloc((size_t)x->n * AT_N, sizeof *base);
    uint32_t *with = calloc((size_t)x->n * AT_N, sizeof *with);
    uint32_t nbase = 0, nwith[AT_N] = {0};
    if (!base || !with) { free(base); free(with); return 0; }

    for (uint32_t i = 0; i < x->nseq; i++) {
        if (x->seq[i] != relation) continue;
        nbase++;
        int32_t next = at(x, i, 1), far = at(x, i, 2), before = at(x, i, -1);
        if (next >= 0)   base[(size_t)next * AT_N + AT_NEXT]++;
        if (far >= 0)    base[(size_t)far * AT_N + AT_FAR]++;
        if (before >= 0) base[(size_t)before * AT_N + AT_BEFORE]++;

        if (before == subject) {
            nwith[AT_NEXT]++; nwith[AT_FAR]++;
            if (next >= 0) with[(size_t)next * AT_N + AT_NEXT]++;
            if (far >= 0)  with[(size_t)far * AT_N + AT_FAR]++;
        }
        if ((next == subject || far == subject) && before >= 0) {
            nwith[AT_BEFORE]++;
            with[(size_t)before * AT_N + AT_BEFORE]++;
        }
    }

    uint32_t n = 0;
    double *score = malloc((size_t)x->n * sizeof *score);
    rz_answer *all = malloc((size_t)x->n * sizeof *all);
    if (!score || !all) { free(base); free(with); free(score); free(all); return 0; }
    for (uint32_t y = 0; y < x->n; y++) {
        if ((int32_t)y == subject || (int32_t)y == relation) continue;
        double best = 0;
        uint32_t sup = 0;
        int8_t dir = 1;
        for (int p = 0; p < AT_N; p++) {
            uint32_t w = with[(size_t)y * AT_N + p];
            if (!w) continue;
            double lift = ((double)w / nwith[p]) / ((double)base[(size_t)y * AT_N + p] / nbase);
            if (lift <= 1.0) continue;
            /* How much more it stands here when this subject is asked about --
               not how often. Weighted by volume, "el" (10 times after "Jesús
               es") outranked "Mesías" (3); it is said after "es" whatever is
               asked, Mesías is not. Said once is an anecdote, the same rule the
               kinds use: it ranks behind anything said twice. */
            double s = log(lift) + (w >= 2 ? 1e6 : 0.0);
            if (s > best) { best = s; sup = w; dir = p == AT_BEFORE ? -1 : 1; }
        }
        if (best <= 0) continue;
        score[n] = best;
        all[n++] = (rz_answer){ (int32_t)y, sup, dir };
    }

    /* the most telling first */
    for (uint32_t i = 1; i < n; i++) {
        rz_answer v = all[i];
        double sv = score[i];
        uint32_t j = i;
        while (j > 0 && score[j - 1] < sv) { all[j] = all[j - 1]; score[j] = score[j - 1]; j--; }
        all[j] = v;
        score[j] = sv;
    }
    uint32_t k = n < cap ? n : cap;
    memcpy(out, all, k * sizeof *out);
    free(base); free(with); free(score); free(all);
    return k;
}

uint32_t rz_lexicon_wonder(rz_lexicon *x, int32_t subject, int32_t relation,
                           int32_t *chain, uint32_t cap, rz_wonder_end *end)
{
    *end = RZ_WONDER_GAP;
    if (cap == 0 || subject < 0) return 0;
    chain[0] = subject;
    uint32_t k = 1;
    rz_answer a[64];
    for (;;) {
        uint32_t got = rz_lexicon_ask(x, chain[k - 1], relation, a, 64);
        if (got == 0) { *end = RZ_WONDER_GAP; return k; }

        int32_t next = -1;
        for (uint32_t i = 0; i < got && next < 0; i++) {
            int asked = 0;
            for (uint32_t c = 0; c < k; c++) if (chain[c] == a[i].lego) asked = 1;
            if (!asked) next = a[i].lego;
        }
        if (next < 0) { *end = RZ_WONDER_CIRCLE; return k; }
        if (k == cap) { *end = RZ_WONDER_DEEP; return k; }
        chain[k++] = next;
    }
}



/* ===================== behaviours (P35) ================================== */

static void ensure_index(rz_lexicon *x)
{
    if (!x->out_start) build_index(x);
}

/* how much of what follows a is also what follows b: the shared mass of the
   two follower distributions */
static double shared_company(const rz_lexicon *x, double *scratch, int32_t a, int32_t b)
{
    double ta = 0, tb = 0, s = 0;
    for (uint32_t o = x->out_start[a]; o < x->out_start[a + 1]; o++) ta += x->pair[x->out_pair[o]].count;
    for (uint32_t o = x->out_start[b]; o < x->out_start[b + 1]; o++) tb += x->pair[x->out_pair[o]].count;
    if (ta <= 0 || tb <= 0) return 0.0;
    for (uint32_t o = x->out_start[a]; o < x->out_start[a + 1]; o++) {
        const rz_lego_pair *q = &x->pair[x->out_pair[o]];
        scratch[q->b] = q->count / ta;
    }
    for (uint32_t o = x->out_start[b]; o < x->out_start[b + 1]; o++) {
        const rz_lego_pair *q = &x->pair[x->out_pair[o]];
        double pb = q->count / tb;
        s += scratch[q->b] < pb ? scratch[q->b] : pb;
    }
    for (uint32_t o = x->out_start[a]; o < x->out_start[a + 1]; o++) scratch[x->pair[x->out_pair[o]].b] = 0;
    return s;
}

/* in the text still to come: of the steps after `a` it never saw, how many it
   had seen after `b` */
static double carried(const rz_lexicon *x, const int32_t *later, uint32_t nlater,
                      int32_t a, int32_t b)
{
    uint32_t steps = 0, hits = 0;
    for (uint32_t i = 0; i + 1 < nlater; i++) {
        if (later[i] != a || later[i + 1] < 0) continue;
        if (find_pair(x, (uint32_t)a, (uint32_t)later[i + 1]) >= 0) continue;
        steps++;
        if (find_pair(x, (uint32_t)b, (uint32_t)later[i + 1]) >= 0) hits++;
    }
    return steps ? (double)hits / steps : 0.0;
}

#define RZ_BLIND 8   /* words of similar frequency the answer is compared against */

void rz_answer_worth(rz_lexicon *x, const int32_t *later, uint32_t nlater,
                     int32_t subject, int32_t answer,
                     double *harmony, double *transfer)
{
    *harmony = *transfer = 0.0;
    if (subject < 0 || answer < 0 || (uint32_t)subject >= x->n || (uint32_t)answer >= x->n) return;
    ensure_index(x);
    double *scratch = calloc(x->n, sizeof *scratch);
    if (!x->out_start || !scratch) { free(scratch); return; }

    double h = shared_company(x, scratch, subject, answer);
    double tr = carried(x, later, nlater, subject, answer);

    /* the blind comparison: the words whose count is nearest the answer's */
    int32_t near[RZ_BLIND];
    uint32_t gap[RZ_BLIND], k = 0;
    uint32_t want = x->lego[answer].count;
    for (uint32_t i = 0; i < x->n; i++) {
        if ((int32_t)i == subject || (int32_t)i == answer) continue;
        uint32_t c = x->lego[i].count;
        uint32_t g = c > want ? c - want : want - c;
        if (k < RZ_BLIND) { near[k] = (int32_t)i; gap[k++] = g; continue; }
        uint32_t worst = 0;
        for (uint32_t j = 1; j < k; j++) if (gap[j] > gap[worst]) worst = j;
        if (g < gap[worst]) { near[worst] = (int32_t)i; gap[worst] = g; }
    }
    double hb = 0, tb = 0;
    for (uint32_t j = 0; j < k; j++) {
        hb += shared_company(x, scratch, subject, near[j]);
        tb += carried(x, later, nlater, subject, near[j]);
    }
    if (k) { hb /= k; tb /= k; }
    *harmony = h - hb;
    *transfer = tr - tb;
    free(scratch);
}

void rz_player_init(rz_player *p)
{
    memset(p, 0, sizeof *p);
}

void rz_player_free(rz_player *p)
{
    free(p->asked);
    memset(p, 0, sizeof *p);
}

static int has_habit(const rz_player *p, int32_t relation)
{
    for (uint32_t i = 0; i < p->nhabit; i++) if (p->habit[i].relation == relation) return 1;
    return 0;
}

void rz_player_imitate(rz_player *p, int32_t relation)
{
    if (relation < 0 || has_habit(p, relation) || p->nhabit == RZ_HABITS) return;
    p->habit[p->nhabit++] = (rz_habit){ relation, 0, 0.0, 1 };
}

/* the commonest word, seen at least twice, not yet asked this way */
static int32_t next_subject(const rz_player *p, const rz_lexicon *x, uint32_t h)
{
    int32_t best = -1;
    for (uint32_t i = 0; i < x->n; i++) {
        if (p->asked[(size_t)h * x->n + i] || x->lego[i].count < 2) continue;
        if ((int32_t)i == p->habit[h].relation) continue;
        if (best < 0 || x->lego[i].count > x->lego[best].count) best = (int32_t)i;
    }
    return best;
}

int rz_player_act(rz_player *p, rz_lexicon *x, const int32_t *later,
                  uint32_t nlater, rz_act *out)
{
    memset(out, 0, sizeof *out);
    if (x->n == 0) return 0;
    if (!p->asked || p->asked_n != x->n) {
        free(p->asked);
        p->asked = calloc((size_t)RZ_HABITS * x->n, 1);
        p->asked_n = x->n;
        if (!p->asked) return 0;
    }

    /* Exploration: once every way of asking it has has been tried, it finds a
       new one -- the commonest word it never used as a relation. */
    int untried = 0;
    for (uint32_t i = 0; i < p->nhabit; i++) if (!p->habit[i].tries) untried = 1;
    if (!untried && p->nhabit < RZ_HABITS) {
        int32_t form = -1;
        for (uint32_t i = 0; i < x->n; i++) {
            if (x->lego[i].count < 2 || has_habit(p, (int32_t)i)) continue;
            if (form < 0 || x->lego[i].count > x->lego[form].count) form = (int32_t)i;
        }
        if (form >= 0) p->habit[p->nhabit++] = (rz_habit){ form, 0, 0.0, 0 };
    }

    /* Repeat what pays, let go of what does not: the mean pay plus how little
       it knows yet, scaled by how much pays spread -- no number chosen. A way
       never tried comes first. */
    double spread = 0.0;
    if (p->acts > 1) {
        double mean = p->sum / p->acts;
        double var = p->sumsq / p->acts - mean * mean;
        spread = var > 0 ? sqrt(var) : 0.0;
    }
    int32_t pick = -1, subject = -1;
    double best = -HUGE_VAL;
    uint32_t open_ways = 0;
    for (uint32_t i = 0; i < p->nhabit; i++) {
        int32_t s = next_subject(p, x, i);
        if (s < 0) continue;
        const rz_habit *h = &p->habit[i];
        double v = h->tries == 0 ? HUGE_VAL
                 : h->paid / h->tries
                   + spread * sqrt(2.0 * log((double)p->acts + 1.0) / h->tries);
        if (p->blind) {
            /* the bar: every open way equally likely (reservoir of one) */
            open_ways++;
            p->blind ^= p->blind << 13; p->blind ^= p->blind >> 7; p->blind ^= p->blind << 17;
            v = (p->blind % open_ways) == 0 ? 1.0 : -1.0;
            if (v > 0) { pick = (int32_t)i; subject = s; }
            continue;
        }
        if (v > best) { best = v; pick = (int32_t)i; subject = s; }
    }
    if (pick < 0) return 0;

    rz_habit *h = &p->habit[pick];
    p->asked[(size_t)pick * x->n + (uint32_t)subject] = 1;
    out->relation = h->relation;
    out->subject = subject;
    out->answer = -1;
    out->new_form = h->tries == 0;

    rz_answer a;
    if (rz_lexicon_ask(x, subject, h->relation, &a, 1) == 1) {
        out->answer = a.lego;
        rz_answer_worth(x, later, nlater, subject, a.lego, &out->harmony, &out->transfer);
        out->pay = out->harmony + out->transfer;
    }
    h->tries++;
    h->paid += out->pay;
    p->acts++;
    p->sum += out->pay;
    p->sumsq += out->pay * out->pay;
    return 1;
}


/* ===================== concepts (P36) ==================================== */

/* the answer each subject gives across the relation: what stands just after
   it or one further, ranked by how much more it stands there for this subject
   than for any (D50), said twice before said once -- the same reading as
   rz_lexicon_ask, done for every subject in one pass */
static void answers_of_all(const rz_lexicon *x, const uint8_t *isrel, int32_t *answer)
{
    for (uint32_t i = 0; i < x->n; i++) answer[i] = -1;
    uint32_t nocc = 0;
    uint32_t *base = calloc((size_t)x->n * 2, sizeof *base);
    uint32_t *nsub = calloc(x->n, sizeof *nsub);
    if (!base || !nsub) { free(base); free(nsub); return; }
    for (uint32_t i = 1; i + 1 < x->nseq; i++) {
        if (x->seq[i] < 0 || !isrel[x->seq[i]]) continue;
        nocc++;
        for (int d = 1; d <= 2; d++) {
            int32_t y = at(x, i, d);
            if (y >= 0) base[(size_t)y * 2 + (uint32_t)(d - 1)]++;
        }
        if (x->seq[i - 1] >= 0) nsub[x->seq[i - 1]]++;
    }

    /* the frames, grouped by subject */
    uint32_t *start = calloc((size_t)x->n + 1, sizeof *start);
    uint32_t *occ = malloc(((size_t)nocc + 1) * sizeof *occ);
    if (!start || !occ) { free(base); free(nsub); free(start); free(occ); return; }
    for (uint32_t s = 0; s < x->n; s++) start[s + 1] = start[s] + nsub[s];
    uint32_t *fill = calloc(x->n, sizeof *fill);
    if (!fill) { free(base); free(nsub); free(start); free(occ); return; }
    for (uint32_t i = 1; i + 1 < x->nseq; i++)
        if (x->seq[i] >= 0 && isrel[x->seq[i]] && x->seq[i - 1] >= 0) {
            uint32_t s = (uint32_t)x->seq[i - 1];
            occ[start[s] + fill[s]++] = i;
        }

    for (uint32_t s = 0; s < x->n; s++) {
        if (nsub[s] == 0 || isrel[s]) continue;
        double best = 0;
        for (uint32_t a = start[s]; a < start[s + 1]; a++)
            for (int d = 1; d <= 2; d++) {
                int32_t y = at(x, occ[a], d);
                if (y < 0 || y == (int32_t)s || isrel[y]) continue;
                uint32_t w = 0;
                for (uint32_t b = start[s]; b < start[s + 1]; b++) w += at(x, occ[b], d) == y;
                double lift = ((double)w / nsub[s]) / ((double)base[(size_t)y * 2 + (uint32_t)(d - 1)] / nocc);
                if (lift <= 1.0) continue;
                double v = log(lift) + (w >= 2 ? 1e6 : 0.0);
                if (v > best) { best = v; answer[s] = y; }
            }
    }
    free(base); free(nsub); free(start); free(occ); free(fill);
}

/* add what follows w, as a distribution, into pool */
static void pool_add(const rz_lexicon *x, double *pool, int32_t w, double sign)
{
    double tot = 0;
    for (uint32_t o = x->out_start[w]; o < x->out_start[w + 1]; o++) tot += x->pair[x->out_pair[o]].count;
    if (tot <= 0) return;
    for (uint32_t o = x->out_start[w]; o < x->out_start[w + 1]; o++) {
        const rz_lego_pair *q = &x->pair[x->out_pair[o]];
        pool[q->b] += sign * q->count / tot;
    }
}

/* Judge a group against a member: the member left out of the pool (it may not
   describe itself). Harmony is the shared mass of what follows the member and
   what follows the rest; transfer counts, in the text still to come, the steps
   after the member it never saw that the rest had been seen taking. */
static void judge_group(const rz_lexicon *x, double *pool, const int32_t *group,
                        uint32_t ng, const int32_t *members, uint32_t nm,
                        const int32_t *later, uint32_t nlater,
                        double *harmony, uint32_t *steps, uint32_t *hits)
{
    *harmony = 0;
    *steps = *hits = 0;
    for (uint32_t k = 0; k < nm; k++) {
        int32_t m = members[k];
        uint32_t others = 0;
        for (uint32_t g = 0; g < ng; g++) if (group[g] != m) { pool_add(x, pool, group[g], 1.0); others++; }
        if (others) {
            double tm = 0, s = 0;
            for (uint32_t o = x->out_start[m]; o < x->out_start[m + 1]; o++) tm += x->pair[x->out_pair[o]].count;
            for (uint32_t o = x->out_start[m]; o < x->out_start[m + 1] && tm > 0; o++) {
                const rz_lego_pair *q = &x->pair[x->out_pair[o]];
                double pm = q->count / tm, pp = pool[q->b] / others;
                s += pm < pp ? pm : pp;
            }
            *harmony += s;
            for (uint32_t i = 0; i + 1 < nlater; i++) {
                if (later[i] != m || later[i + 1] < 0) continue;
                if (find_pair(x, (uint32_t)m, (uint32_t)later[i + 1]) >= 0) continue;
                (*steps)++;
                if (pool[later[i + 1]] > 0) (*hits)++;
            }
        }
        for (uint32_t g = 0; g < ng; g++) if (group[g] != m) pool_add(x, pool, group[g], -1.0);
        for (uint32_t g = 0; g < ng; g++) if (group[g] != m)          /* clear rounding */
            for (uint32_t o = x->out_start[group[g]]; o < x->out_start[group[g] + 1]; o++)
                pool[x->pair[x->out_pair[o]].b] = 0;
    }
    if (nm) *harmony /= nm;
}

static int by_count_desc(const void *a, const void *b, const rz_lexicon *x)
{
    uint32_t ca = x->lego[*(const int32_t *)a].count, cb = x->lego[*(const int32_t *)b].count;
    return ca < cb ? 1 : ca > cb ? -1 : 0;
}

uint32_t rz_concepts_find(rz_lexicon *x, int32_t relation,
                          const int32_t *later, uint32_t nlater,
                          rz_concept *out, uint32_t cap)
{
    return rz_concepts_find_any(x, &relation, 1, later, nlater, out, cap);
}

uint32_t rz_concepts_find_any(rz_lexicon *x, const int32_t *relations, uint32_t nrel,
                              const int32_t *later, uint32_t nlater,
                              rz_concept *out, uint32_t cap)
{
    if (nrel == 0 || relations[0] < 0 || !x->n || !x->nseq || cap == 0) return 0;
    int32_t relation = relations[0];
    uint8_t *isrel = calloc(x->n, 1);
    if (!isrel) return 0;
    for (uint32_t r = 0; r < nrel; r++)
        if (relations[r] >= 0 && (uint32_t)relations[r] < x->n) isrel[relations[r]] = 1;
    ensure_index(x);
    int32_t *answer = malloc((size_t)x->n * sizeof *answer);
    double *pool = calloc(x->n, sizeof *pool);
    int32_t *order = malloc((size_t)x->n * sizeof *order);
    uint32_t *rank = malloc((size_t)x->n * sizeof *rank);
    if (!x->out_start || !answer || !pool || !order || !rank) {
        free(answer); free(pool); free(order); free(rank); free(isrel);
        return 0;
    }
    answers_of_all(x, isrel, answer);

    /* words by frequency, for the blind groups: a word's blind twin j is the
       j-th neighbour in that order */
    for (uint32_t i = 0; i < x->n; i++) order[i] = (int32_t)i;
    for (uint32_t gap = x->n / 2; gap > 0; gap /= 2)
        for (uint32_t i = gap; i < x->n; i++) {
            int32_t v = order[i];
            uint32_t j = i;
            while (j >= gap && by_count_desc(&order[j - gap], &v, x) > 0) { order[j] = order[j - gap]; j -= gap; }
            order[j] = v;
        }
    for (uint32_t i = 0; i < x->n; i++) rank[order[i]] = i;

    uint32_t n = 0;
    for (uint32_t h = 0; h < x->n && n < cap; h++) {
        rz_concept c;
        memset(&c, 0, sizeof c);
        c.head = (int32_t)h;
        c.relation = relation;
        for (uint32_t s = 0; s < x->n && c.nmember < RZ_CONCEPT_MEMBERS; s++)
            if (answer[s] == (int32_t)h) c.member[c.nmember++] = (int32_t)s;
        if (c.nmember < 2) continue;            /* one case is an anecdote */

        int32_t group[RZ_CONCEPT_MEMBERS + 1];
        uint32_t ng = 0;
        group[ng++] = c.head;
        for (uint32_t k = 0; k < c.nmember; k++) group[ng++] = c.member[k];
        double h0;
        uint32_t st0, hit0;
        judge_group(x, pool, group, ng, c.member, c.nmember, later, nlater, &h0, &st0, &hit0);

        /* the blind groups: every word of the group swapped for a word of about
           the same frequency, judged against the same real members */
        double hb = 0, tb = 0;
        int blinds = 0;
        for (int j = 1; j <= 8; j++) {
            int32_t blind[RZ_CONCEPT_MEMBERS + 1];
            for (uint32_t g = 0; g < ng; g++) {
                int64_t at_rank = (int64_t)rank[group[g]] + ((j & 1) ? (j + 1) / 2 : -(j / 2));
                if (at_rank < 0) at_rank += j;
                if (at_rank >= (int64_t)x->n) at_rank = (int64_t)x->n - 1 - j;
                if (at_rank < 0) at_rank = 0;
                blind[g] = order[at_rank];
            }
            double hj;
            uint32_t stj, hitj;
            /* the blind group holds no real member, so nothing is left out of
               it: its pool has one word more than the real one, which makes
               the bar harder to clear, never easier */
            judge_group(x, pool, blind, ng, c.member, c.nmember, later, nlater, &hj, &stj, &hitj);
            hb += hj;
            tb += stj ? (double)hitj / stj : 0.0;
            blinds++;
        }
        hb /= blinds;
        tb /= blinds;
        c.harmony = h0 - hb;
        c.transfer = (st0 ? (double)hit0 / st0 : 0.0) - tb;
        c.adopted = c.harmony + c.transfer > 0;
        out[n++] = c;
    }

    /* most members first */
    for (uint32_t i = 1; i < n; i++) {
        rz_concept v = out[i];
        uint32_t j = i;
        while (j > 0 && out[j - 1].nmember < v.nmember) { out[j] = out[j - 1]; j--; }
        out[j] = v;
    }
    free(answer); free(pool); free(order); free(rank); free(isrel);
    return n;
}


/* the split of a list of values into two that best separates them, one vote
   per value; -HUGE_VAL when they are all equal */
static double otsu_list(const double *v0, uint32_t n)
{
    if (n < 2) return -HUGE_VAL;
    double *v = malloc(n * sizeof *v);
    if (!v) return -HUGE_VAL;
    memcpy(v, v0, n * sizeof *v);
    for (uint32_t gap = n / 2; gap > 0; gap /= 2)
        for (uint32_t i = gap; i < n; i++) {
            double t = v[i];
            uint32_t j = i;
            while (j >= gap && v[j - gap] > t) { v[j] = v[j - gap]; j -= gap; }
            v[j] = t;
        }
    double S = 0, s0 = 0, best = 0, mid = -HUGE_VAL;
    for (uint32_t i = 0; i < n; i++) S += v[i];
    for (uint32_t i = 0; i + 1 < n; i++) {
        s0 += v[i];
        if (v[i + 1] <= v[i]) continue;
        double w0 = i + 1, w1 = n - w0;
        double d = s0 / w0 - (S - s0) / w1;
        double between = w0 * w1 * d * d;
        if (between > best) { best = between; mid = (v[i] + v[i + 1]) / 2.0; }
    }
    free(v);
    return mid;
}

uint32_t rz_lexicon_silhouette(rz_lexicon *x, int32_t word, int32_t *out, uint32_t cap)
{
    if (word < 0 || (uint32_t)word >= x->n || cap == 0) return 0;
    company c;
    memset(&c, 0, sizeof c);
    if (!company_build(x, &c)) { company_free(&c); return 0; }
    uint32_t W = 2 * c.F;
    double *row = calloc(W, sizeof *row);
    double *score = malloc((size_t)x->n * sizeof *score);
    double *vals = malloc((size_t)x->n * sizeof *vals);
    int32_t *who = malloc((size_t)x->n * sizeof *who);
    uint32_t k = 0, nv = 0;
    if (!row || !score || !vals || !who) goto out;

    company_add(x, &c, (uint32_t)word, row);
    unit(row, W);
    company_dots(x, &c, row, 1, score);

    for (uint32_t i = 0; i < x->n; i++)
        if ((int32_t)i != word && x->lego[i].count >= 2 && score[i] > 0) {
            vals[nv] = score[i];
            who[nv++] = (int32_t)i;
        }
    /* first split: the words that keep anything like its company */
    double first = otsu_list(vals, nv);
    if (first == -HUGE_VAL) goto out;
    uint32_t nt = 0;
    for (uint32_t i = 0; i < nv; i++)
        if (vals[i] > first) { vals[nt] = vals[i]; who[nt++] = who[i]; }
    /* most alike first */
    for (uint32_t i = 1; i < nt; i++) {
        double v = vals[i];
        int32_t w = who[i];
        uint32_t j = i;
        while (j > 0 && vals[j - 1] < v) { vals[j] = vals[j - 1]; who[j] = who[j - 1]; j--; }
        vals[j] = v;
        who[j] = w;
    }
    /* second: the widest gap inside the tall side is where its own kind ends */
    uint32_t keep = nt;
    double widest = 0;
    for (uint32_t i = 0; i + 1 < nt; i++)
        if (vals[i] - vals[i + 1] > widest) { widest = vals[i] - vals[i + 1]; keep = i + 1; }
    for (uint32_t i = 0; i < keep && k < cap; i++) out[k++] = who[i];
out:
    free(row); free(score); free(vals); free(who);
    company_free(&c);
    return k;
}


/* ===================== types and identities (P37) ======================== */

typedef struct { int32_t filler, type; } slot_fill;

static int by_fill(const void *a, const void *b)
{
    const slot_fill *p = a, *q = b;
    if (p->filler != q->filler) return p->filler < q->filler ? -1 : 1;
    return p->type < q->type ? -1 : p->type > q->type;
}

uint32_t rz_lexicon_roles(rz_lexicon *x, int32_t seed, int32_t *binding,
                          uint8_t *is_type, uint8_t *is_identity)
{
    *binding = -1;
    if (seed < 0 || (uint32_t)seed >= x->n || !x->nseq) return 0;
    memset(is_type, 0, x->n);
    memset(is_identity, 0, x->n);

    uint32_t *tot = calloc(x->n, sizeof *tot), *after = calloc(x->n, sizeof *after);
    uint32_t *bound = calloc(x->n, sizeof *bound), *inslot = calloc(x->n, sizeof *inslot);
    double *vals = malloc((size_t)x->n * sizeof *vals);
    int32_t *who = malloc((size_t)x->n * sizeof *who);
    slot_fill *fill = NULL;
    uint32_t ids = 0;
    if (!tot || !after || !bound || !inslot || !vals || !who) goto out;

    /* the binding: what follows the seed far more than it follows anything,
       said at least twice */
    double nseed = 0, nall = 0;
    for (uint32_t i = 0; i + 1 < x->nseq; i++) {
        if (x->seq[i] < 0 || x->seq[i + 1] < 0) continue;
        tot[x->seq[i]]++;
        nall++;
        if (x->seq[i] == seed) { after[x->seq[i + 1]]++; nseed++; }
    }
    double best = 0;
    for (uint32_t w = 0; w < x->n; w++) {
        if (after[w] < 2) continue;
        double lift = ((double)after[w] / nseed) / ((double)tot[w] / nall);
        double v = log(lift) * log((double)after[w]);
        if (lift > 1.0 && v > best) { best = v; *binding = (int32_t)w; }
    }
    if (*binding < 0) goto out;
    int32_t bind = *binding;

    /* types: followed by the binding on the tall side of the split */
    for (uint32_t i = 0; i + 1 < x->nseq; i++)
        if (x->seq[i] >= 0 && x->seq[i + 1] == bind) bound[x->seq[i]]++;
    uint32_t nv = 0;
    for (uint32_t w = 0; w < x->n; w++)
        if (tot[w] >= 2 && (int32_t)w != bind) { vals[nv] = (double)bound[w] / tot[w]; who[nv++] = (int32_t)w; }
    double cut = otsu_list(vals, nv);
    if (cut == -HUGE_VAL) goto out;
    for (uint32_t i = 0; i < nv; i++)
        if (vals[i] > cut && bound[who[i]] >= 2) is_type[who[i]] = 1;

    /* who fills "type binding ___", and for how many different types */
    uint32_t nf = 0;
    for (uint32_t i = 0; i + 2 < x->nseq; i++)
        if (x->seq[i] >= 0 && is_type[x->seq[i]] && x->seq[i + 1] == bind && x->seq[i + 2] >= 0) nf++;
    fill = malloc(((size_t)nf + 1) * sizeof *fill);
    if (!fill) goto out;
    nf = 0;
    for (uint32_t i = 0; i + 2 < x->nseq; i++)
        if (x->seq[i] >= 0 && is_type[x->seq[i]] && x->seq[i + 1] == bind && x->seq[i + 2] >= 0) {
            fill[nf++] = (slot_fill){ x->seq[i + 2], x->seq[i] };
            inslot[x->seq[i + 2]]++;
        }
    qsort(fill, nf, sizeof *fill, by_fill);

    /* identities: two or more different types, and the tall side of how much
       of their life they spend in those slots -- "la" fills them and lives
       everywhere */
    nv = 0;
    for (uint32_t i = 0; i < nf; ) {
        uint32_t j = i, distinct = 0;
        int32_t prev = -1;
        while (j < nf && fill[j].filler == fill[i].filler) {
            if (fill[j].type != prev) { distinct++; prev = fill[j].type; }
            j++;
        }
        int32_t f = fill[i].filler;
        if (distinct >= 2 && !is_type[f] && tot[f] > 0) {
            vals[nv] = (double)inslot[f] / tot[f];
            who[nv++] = f;
        }
        i = j;
    }
    cut = otsu_list(vals, nv);
    for (uint32_t i = 0; i < nv; i++)
        if (cut == -HUGE_VAL || vals[i] > cut) { is_identity[who[i]] = 1; ids++; }
out:
    free(tot); free(after); free(bound); free(inslot); free(vals); free(who); free(fill);
    return ids;
}


/* ===================== weaving (P38) ===================================== */

static int is_bound(const rz_lexicon *x, uint32_t i, const rz_binding *b, uint32_t nb, int side)
{
    for (uint32_t k = 0; k < nb; k++)
        if (b[k].side == side && at(x, i, b[k].side) == b[k].word) return 1;
    return 0;
}

uint32_t rz_lexicon_weave(rz_lexicon *x, int32_t seed, rz_binding *out, uint32_t cap,
                          uint8_t *is_type)
{
    if (cap == 0) return 0;
    memset(is_type, 0, x->n);
    uint8_t *ident = calloc(x->n, 1);
    int32_t first = -1;
    if (!ident) return 0;
    rz_lexicon_roles(x, seed, &first, is_type, ident);
    free(ident);
    if (first < 0) return 0;

    uint32_t nb = 0;
    out[nb++] = (rz_binding){ 1, first, 0.0, 0 };

    uint32_t *tot = calloc(x->n, sizeof *tot), *c = calloc(x->n, sizeof *c);
    double *vals = malloc((size_t)x->n * 2 * sizeof *vals);
    double *str = malloc((size_t)x->n * 2 * sizeof *str);
    int32_t *who = malloc((size_t)x->n * 2 * sizeof *who);
    int8_t *sd = malloc((size_t)x->n * 2);
    double *with = calloc((size_t)x->n * 2, sizeof *with), *all = calloc((size_t)x->n * 2, sizeof *all);
    double *kept = calloc((size_t)x->n * 2, sizeof *kept);
    if (!tot || !c || !vals || !str || !who || !sd || !with || !all || !kept) goto out;
    for (uint32_t i = 0; i < x->nseq; i++) if (x->seq[i] >= 0) tot[x->seq[i]]++;

    for (uint8_t round = 1; round < 255; round++) {
        /* types: for every binding, the tall side of the share of a word's
           life it spends bound that way, seen bound twice at least */
        memset(is_type, 0, x->n);
        for (uint32_t b = 0; b < nb; b++) {
            memset(c, 0, (size_t)x->n * sizeof *c);
            for (uint32_t i = 0; i < x->nseq; i++)
                if (x->seq[i] >= 0 && at(x, i, out[b].side) == out[b].word) c[x->seq[i]]++;
            uint32_t nv = 0;
            for (uint32_t w = 0; w < x->n; w++)
                if (tot[w] >= 2 && (int32_t)w != out[b].word) { vals[nv] = (double)c[w] / tot[w]; who[nv++] = (int32_t)w; }
            double cut = otsu_list(vals, nv);
            if (cut == -HUGE_VAL) continue;
            for (uint32_t i = 0; i < nv; i++)
                if (vals[i] > cut && c[who[i]] >= 2) is_type[who[i]] = 1;
        }
        if (nb == cap) break;

        /* candidates on each side of a type, and how often the binding on the
           OTHER side survives them: a word after the type always pushes "de"
           out of its own place, which proves nothing */
        memset(with, 0, (size_t)x->n * 2 * sizeof *with);
        memset(all, 0, (size_t)x->n * 2 * sizeof *all);
        memset(kept, 0, (size_t)x->n * 2 * sizeof *kept);
        double nw = 0, na = 0, side_n[2] = {0, 0}, side_b[2] = {0, 0};
        int has[2] = {0, 0};
        for (uint32_t k = 0; k < nb; k++) has[out[k].side > 0] = 1;
        for (uint32_t i = 0; i < x->nseq; i++) {
            int32_t a = x->seq[i];
            if (a < 0) continue;
            na++;
            for (int s2 = 0; s2 < 2; s2++) {
                int32_t p = at(x, i, s2 ? 1 : -1);
                if (p >= 0) all[(size_t)p * 2 + (uint32_t)s2]++;
            }
            if (!is_type[a]) continue;
            nw++;
            int bnd[2] = { is_bound(x, i, out, nb, -1), is_bound(x, i, out, nb, 1) };
            for (int s2 = 0; s2 < 2; s2++) if (has[s2]) { side_n[s2]++; side_b[s2] += bnd[s2]; }
            for (int s2 = 0; s2 < 2; s2++) {
                int32_t p = at(x, i, s2 ? 1 : -1);
                if (p < 0 || !has[!s2]) continue;
                with[(size_t)p * 2 + (uint32_t)s2]++;
                kept[(size_t)p * 2 + (uint32_t)s2] += bnd[!s2];
            }
        }

        uint32_t nc = 0;
        for (uint32_t w = 0; w < x->n; w++)
            for (int s2 = 0; s2 < 2; s2++) {
                double cnt = with[(size_t)w * 2 + (uint32_t)s2];
                if (cnt < 2 || !side_n[!s2] || side_b[!s2] <= 0) continue;
                double lift = (cnt / nw) / (all[(size_t)w * 2 + (uint32_t)s2] / na);
                if (lift <= 1.0) continue;
                int known = 0;
                for (uint32_t k = 0; k < nb; k++) known |= out[k].word == (int32_t)w && out[k].side == (s2 ? 1 : -1);
                if (known) continue;
                /* one imagined case at the base rate: three cases cannot say 0% */
                double base = side_b[!s2] / side_n[!s2];
                double r = (kept[(size_t)w * 2 + (uint32_t)s2] + base) / (cnt + 1.0) / base;
                if (r >= 1.0) continue;          /* replacing means making it rarer */
                vals[nc] = r;
                str[nc] = log(lift) * log(cnt);
                who[nc] = (int32_t)w;
                sd[nc++] = (int8_t)(s2 ? 1 : -1);
            }
        /* the low side of replacement, and among those the tall side of strength */
        double rcut = otsu_list(vals, nc);
        if (rcut == -HUGE_VAL) rcut = 1.0;
        uint32_t ns = 0;
        double *sv = malloc((nc + 1) * sizeof *sv);
        if (!sv) break;
        for (uint32_t i = 0; i < nc; i++) if (vals[i] < rcut) sv[ns++] = str[i];
        double scut = otsu_list(sv, ns);
        free(sv);
        uint32_t added = 0;
        for (uint32_t i = 0; i < nc && nb < cap; i++)
            if (vals[i] < rcut && (scut == -HUGE_VAL || str[i] > scut)) {
                out[nb++] = (rz_binding){ sd[i], who[i], vals[i], round };
                added++;
            }
        if (!added) break;
    }
out:
    free(tot); free(c); free(vals); free(str); free(who); free(sd);
    free(with); free(all); free(kept);
    return nb;
}
