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

/* What a reading of words costs, in nats, in its own units (D39, D45): the
   spelling of every distinct word, every word said, and which part stood at
   each edge -- so a reading cannot get cheaper by dropping what told two
   edges apart. A word is told by a 64-bit hash of its bytes; a collision would
   only make a reading look a little cheaper. */
static double reading_cost(const unsigned char *t, size_t n,
                           const unsigned char *lego, const uint8_t part[RZ_ATOMS])
{
    size_t cap = 1024;
    while (cap < n / 2) cap <<= 1;
    uint64_t *key = calloc(cap, sizeof *key);
    uint32_t *cnt = calloc(cap, sizeof *cnt), *len = calloc(cap, sizeof *len);
    if (!key || !cnt || !len) { free(key); free(cnt); free(len); return HUGE_VAL; }

    uint64_t seen[RZ_ATOMS] = {0}, edge[RZ_ATOMS] = {0};
    for (size_t i = 0; i < n; i++) seen[t[i]]++;
    double alphabet = 0;
    for (int b = 0; b < RZ_ATOMS; b++) alphabet += seen[b] ? 1 : 0;

    uint64_t said = 0, edges = 0;
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        int end = (lego[i] && part[t[i]]) || i + 1 == n;
        if (!end) continue;
        size_t stop = (lego[i] && part[t[i]]) ? i : i + 1;
        if (lego[i] && part[t[i]]) { edge[t[i]]++; edges++; }
        if (stop > start) {
            uint64_t h = 1469598103934665603ull;
            for (size_t q = start; q < stop; q++) { h ^= t[q]; h *= 1099511628211ull; }
            if (!h) h = 1;
            size_t s = (size_t)(h ^ (h >> 29)) & (cap - 1);
            while (key[s] && key[s] != h) s = (s + 1) & (cap - 1);
            if (!key[s]) { key[s] = h; len[s] = (uint32_t)(stop - start); }
            cnt[s]++;
            said++;
        }
        start = i + 1;
    }

    double cost = 0;
    for (size_t s = 0; s < cap; s++) {
        if (!key[s]) continue;
        cost += (len[s] + 1) * log(alphabet + 1);              /* its spelling */
        cost += cnt[s] * log((double)said / cnt[s]);           /* every time it is said */
    }
    for (int b = 0; b < RZ_ATOMS; b++)
        if (edge[b]) cost += edge[b] * log((double)edges / edge[b]);   /* which part */
    free(key); free(cnt); free(len);
    return cost;
}

size_t rz_word_cut(const rz_mind *m, const unsigned char *t, size_t n,
                   unsigned char *ends)
{
    size_t legos = rz_lego_cut(m, t, n, ends);
    if (legos == 0) return 0;

    /* Up: the atoms legos end on, and the split of their heights once more */
    uint64_t at[RZ_ATOMS] = {0};
    for (size_t i = 0; i < n; i++) if (ends[i]) at[t[i]]++;
    double mid = best_split(m, at);
    if (mid == -HUGE_VAL) return legos;    /* one height: every lego is a word */

    uint8_t part[RZ_ATOMS] = {0};
    for (int b = 0; b < RZ_ATOMS; b++) {
        if (!at[b]) continue;
        double v = rz_branching(m, (unsigned char)b);
        part[b] = (v > 0.0 ? log(v) : 0.0) > mid;
    }

    /* Down and up again (P49). The split by heights always makes two groups,
       and a dictionary that parts its words with tabs and with spaces lost
       one of them: "1. El" was a word. An atom legos end on that the reading
       left inside its words is a seam; it is proposed as a part, the text is
       read again, and the reading is kept only if it describes the same text
       with less. A part may be given back the same way. The order of the
       proposals -- the atoms legos end on most, first -- is only a bias; the
       judge alone accepts (D41). Until nothing pays. */
    double base = reading_cost(t, n, ends, part);
    for (int changed = 1; changed && base < HUGE_VAL; ) {
        changed = 0;
        uint8_t tried[RZ_ATOMS] = {0};
        for (;;) {
            int b = -1;
            for (int c = 0; c < RZ_ATOMS; c++)
                if (at[c] >= 2 && !tried[c] && (b < 0 || at[c] > at[b])) b = c;
            if (b < 0) break;
            tried[b] = 1;
            part[b] ^= 1;
            double c = reading_cost(t, n, ends, part);
            if (c < base) { base = c; changed = 1; break; }
            part[b] ^= 1;
        }
    }

    size_t words = 0;
    for (size_t i = 0; i < n; i++) {
        if (!ends[i]) continue;
        if (part[t[i]] || i + 1 == n) words++;
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

static uint32_t ask_core(rz_lexicon *x, int32_t subject, int32_t relation,
                         rz_answer *out, uint32_t cap, double *scores);

uint32_t rz_lexicon_ask(rz_lexicon *x, int32_t subject, int32_t relation,
                        rz_answer *out, uint32_t cap)
{
    return ask_core(x, subject, relation, out, cap, NULL);
}

/* the same, also handing back each answer's score: log(lift), plus 1e6 when
   it was said at least twice */
static uint32_t ask_core(rz_lexicon *x, int32_t subject, int32_t relation,
                         rz_answer *out, uint32_t cap, double *scores)
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
    if (scores) memcpy(scores, score, k * sizeof *scores);
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
    for (uint32_t i = 0; i < RZ_HABITS; i++) free(p->plan[i]);
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

/* Whom to ask next this way: the plan, studied first. By frequency it put
   "is" to "the" and "a", got nothing, and let the question go before it ever
   reached "rex" (P35). The plan is the question's own structure: the subjects
   the text puts it to, then the words that keep their company. When the plan
   is spent, this way of asking is spent -- it does not try for ever. */
static int32_t next_subject(rz_player *p, rz_lexicon *x, uint32_t h)
{
    if (!p->plan[h]) {
        int32_t *plan = malloc(2 * 256 * sizeof *plan);
        if (!plan) return -1;
        uint32_t ns = rz_form_subjects(x, p->habit[h].relation, plan, 256);
        uint32_t nr = rz_form_reaches(x, plan, ns, plan + ns, 256);
        p->plan[h] = plan;
        p->nplan[h] = ns + nr;
    }
    for (uint32_t k = 0; k < p->nplan[h]; k++) {
        int32_t s = p->plan[h][k];
        if (s >= 0 && !p->asked[(size_t)h * x->n + (uint32_t)s] && s != p->habit[h].relation) return s;
    }
    return -1;
}

int rz_player_act(rz_player *p, rz_lexicon *x, const int32_t *later,
                  uint32_t nlater, rz_act *out)
{
    memset(out, 0, sizeof *out);
    if (x->n == 0) return 0;
    if (!p->asked || p->asked_n != x->n) {
        free(p->asked);
        for (uint32_t i = 0; i < RZ_HABITS; i++) { free(p->plan[i]); p->plan[i] = NULL; p->nplan[i] = 0; }
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

/* types under the bindings, read over the first `upto` words: for every
   binding, the tall side of the share of a word's life it spends bound that
   way, seen bound twice at least */
static void weave_types(const rz_lexicon *x, const rz_binding *b, uint32_t nb, uint32_t upto,
                        uint32_t *tot, uint32_t *c, double *vals, int32_t *who, uint8_t *is_type)
{
    memset(is_type, 0, x->n);
    memset(tot, 0, (size_t)x->n * sizeof *tot);
    for (uint32_t i = 0; i < upto; i++) if (x->seq[i] >= 0) tot[x->seq[i]]++;
    for (uint32_t k = 0; k < nb; k++) {
        memset(c, 0, (size_t)x->n * sizeof *c);
        for (uint32_t i = 0; i < upto; i++)
            if (x->seq[i] >= 0 && at(x, i, b[k].side) == b[k].word) c[x->seq[i]]++;
        uint32_t nv = 0;
        for (uint32_t w = 0; w < x->n; w++)
            if (tot[w] >= 2 && (int32_t)w != b[k].word) { vals[nv] = (double)c[w] / tot[w]; who[nv++] = (int32_t)w; }
        double cut = otsu_list(vals, nv);
        if (cut == -HUGE_VAL) continue;
        for (uint32_t i = 0; i < nv; i++)
            if (vals[i] > cut && c[who[i]] >= 2) is_type[who[i]] = 1;
    }
}

/* how much, on the words from `from` on, being a type tells about taking an
   owner: the mutual information of the two, in nats. It rewards a melody that
   is both right and wide; one that binds everything to everything tells less */
static double held_information(const rz_lexicon *x, const rz_binding *b, uint32_t nb,
                               const uint8_t *is_type, uint32_t from)
{
    double n[2][2] = { { 0, 0 }, { 0, 0 } };
    for (uint32_t i = from; i < x->nseq; i++) {
        int32_t a = x->seq[i];
        if (a < 0) continue;
        int bound = is_bound(x, i, b, nb, -1) || is_bound(x, i, b, nb, 1);
        n[is_type[a] ? 1 : 0][bound]++;
    }
    double tot = n[0][0] + n[0][1] + n[1][0] + n[1][1], mi = 0;
    if (tot <= 0) return 0;
    for (int t = 0; t < 2; t++)
        for (int k = 0; k < 2; k++) {
            if (n[t][k] <= 0) continue;
            double pt = (n[t][0] + n[t][1]) / tot, pk = (n[0][k] + n[1][k]) / tot;
            mi += n[t][k] / tot * log(n[t][k] / tot / (pt * pk));
        }
    return mi;
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
    rz_binding known = { 1, first, 0.0, 0 };
    return rz_lexicon_weave_from(x, &known, 1, out, cap, is_type);
}

uint32_t rz_lexicon_weave_from(rz_lexicon *x, const rz_binding *known, uint32_t nknown,
                               rz_binding *out, uint32_t cap, uint8_t *is_type)
{
    if (cap == 0) return 0;
    memset(is_type, 0, x->n);
    uint32_t nb = 0;
    for (uint32_t k = 0; k < nknown && nb < cap; k++)
        if (known[k].word >= 0 && (uint32_t)known[k].word < x->n) {
            out[nb] = known[k];
            out[nb++].round = 0;
        }
    if (nb == 0) return 0;

    uint32_t *tot = calloc(x->n, sizeof *tot), *c = calloc(x->n, sizeof *c);
    double *vals = malloc((size_t)x->n * 2 * sizeof *vals);
    double *str = malloc((size_t)x->n * 2 * sizeof *str);
    int32_t *who = malloc((size_t)x->n * 2 * sizeof *who);
    int8_t *sd = malloc((size_t)x->n * 2);
    double *with = calloc((size_t)x->n * 2, sizeof *with), *all = calloc((size_t)x->n * 2, sizeof *all);
    double *kept = calloc((size_t)x->n * 2, sizeof *kept);
    if (!tot || !c || !vals || !str || !who || !sd || !with || !all || !kept) goto out;

    /* It weaves on the first half of what it read and judges each round on
       the second. Without a judge the loop never stopped by itself: given room
       for 512 bindings it filled them with plurals and verbs, and on a second
       text the woven melody predicted worse than the one carried in. */
    uint32_t learn = x->nseq / 2;

    for (uint8_t round = 1; round < 255; round++) {
        weave_types(x, out, nb, learn, tot, c, vals, who, is_type);
        double now = held_information(x, out, nb, is_type, learn);
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
        for (uint32_t i = 0; i < learn; i++) {
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
        uint32_t before = nb;
        for (uint32_t i = 0; i < nc && nb < cap; i++)
            if (vals[i] < rcut && (scut == -HUGE_VAL || str[i] > scut))
                out[nb++] = (rz_binding){ sd[i], who[i], vals[i], round };
        if (nb == before) break;

        /* the judge: keep the round only if, on the half it did not weave on,
           being a type now says more about taking an owner than it did */
        weave_types(x, out, nb, learn, tot, c, vals, who, is_type);
        if (held_information(x, out, nb, is_type, learn) <= now) { nb = before; break; }
    }
    /* the types of the melody it kept, read over everything it read */
    weave_types(x, out, nb, x->nseq, tot, c, vals, who, is_type);
out:
    free(tot); free(c); free(vals); free(str); free(who); free(sd);
    free(with); free(all); free(kept);
    return nb;
}


/* ===================== the melody (P39) ================================== */

uint32_t rz_melody_carry(const rz_lexicon *x, const rz_note *notes, uint32_t n,
                         rz_binding *out, uint32_t cap, uint32_t *note_of)
{
    uint32_t k = 0;
    for (uint32_t i = 0; i < n && k < cap; i++) {
        int32_t w = rz_lexicon_find(x, notes[i].s, notes[i].len);
        if (w < 0) continue;
        if (note_of) note_of[k] = i;
        out[k++] = (rz_binding){ notes[i].side, w, 0.0, 0 };
    }
    return k;
}

uint32_t rz_melody_learn(const rz_lexicon *x, const rz_binding *b, uint32_t nb,
                         rz_note *notes, uint32_t n, uint32_t cap, const char *where)
{
    for (uint32_t i = 0; i < nb && n < cap; i++) {
        if (b[i].word < 0 || (uint32_t)b[i].word >= x->n) continue;
        const rz_lego *g = &x->lego[b[i].word];
        int known = 0;
        for (uint32_t j = 0; j < n && !known; j++)
            known = notes[j].side == b[i].side && notes[j].len == g->len
                 && memcmp(notes[j].s, g->s, g->len) == 0;
        if (known) continue;
        rz_note note;
        memset(&note, 0, sizeof note);
        note.side = b[i].side;
        note.len = g->len;
        memcpy(note.s, g->s, g->len);
        if (where) {
            strncpy(note.where, where, sizeof note.where - 1);
            note.where[sizeof note.where - 1] = 0;
        }
        notes[n++] = note;
    }
    return n;
}

#define RZ_MELODY_MAGIC 0x4C454D52u   /* "RMEL" */
#define RZ_MELODY_VER   (1u + 1000u * (uint32_t)sizeof(rz_note))

int rz_melody_save(const rz_note *notes, uint32_t n, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return RZ_ERR_IO;
    uint32_t head[3] = { RZ_MELODY_MAGIC, RZ_MELODY_VER, n };
    int ok = fwrite(head, sizeof head, 1, f) == 1
          && (n == 0 || fwrite(notes, sizeof *notes, n, f) == n);
    if (fclose(f) != 0) ok = 0;
    return ok ? RZ_OK : RZ_ERR_IO;
}

int rz_melody_load(rz_note *notes, uint32_t cap, uint32_t *n, const char *path)
{
    *n = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return RZ_ERR_MISSING;
    uint32_t head[3];
    int rc = RZ_OK;
    if (fread(head, sizeof head, 1, f) != 1) rc = RZ_ERR_IO;
    else if (head[0] != RZ_MELODY_MAGIC || head[1] != RZ_MELODY_VER || head[2] > cap) rc = RZ_ERR_VERSION;
    else if (head[2] && fread(notes, sizeof *notes, head[2], f) != head[2]) rc = RZ_ERR_IO;
    else *n = head[2];
    fclose(f);
    return rc;
}


/* ===================== why it worked here and not there (P40) ============ */

/* what binding word w on `side` alone tells, in `later`, about which words are
   types: the mutual information of the two */
static double note_information(const int32_t *later, uint32_t nlater, const uint8_t *is_type,
                               int32_t w, int side)
{
    double n[2][2] = { { 0, 0 }, { 0, 0 } };
    for (uint32_t i = 0; i < nlater; i++) {
        int32_t a = later[i];
        if (a < 0) continue;
        int64_t j = (int64_t)i + side;
        int bound = j >= 0 && (uint64_t)j < nlater && later[j] == w;
        n[is_type[a] ? 1 : 0][bound]++;
    }
    double tot = n[0][0] + n[0][1] + n[1][0] + n[1][1], mi = 0;
    if (tot <= 0) return 0;
    for (int t = 0; t < 2; t++)
        for (int k = 0; k < 2; k++) {
            if (n[t][k] <= 0) continue;
            double pt = (n[t][0] + n[t][1]) / tot, pk = (n[0][k] + n[1][k]) / tot;
            mi += n[t][k] / tot * log(n[t][k] / tot / (pt * pk));
        }
    return mi;
}

void rz_melody_judge(rz_lexicon *x, const rz_binding *b, uint32_t nb,
                     const int32_t *later, uint32_t nlater, rz_note_verdict *v)
{
    memset(v, 0, nb * sizeof *v);
    if (!x->n || nb == 0) return;
    uint8_t *ty = calloc(x->n, 1);
    uint32_t *tot = calloc(x->n, sizeof *tot), *c = calloc(x->n, sizeof *c);
    double *vals = malloc((size_t)x->n * sizeof *vals);
    int32_t *who = malloc((size_t)x->n * sizeof *who);
    uint32_t *ord = malloc((size_t)x->n * sizeof *ord), *rank = malloc((size_t)x->n * sizeof *rank);
    if (!ty || !tot || !c || !vals || !who || !ord || !rank) goto out;

    /* the types each note marks on its own; a note is judged against the types
       the REST of the melody marks -- judged against its own, a note always
       agrees with itself (the first version did exactly that) */
    uint8_t *own = calloc((size_t)nb * x->n, 1);
    if (!own) goto out;
    for (uint32_t k = 0; k < nb; k++) {
        if (b[k].word < 0 || (uint32_t)b[k].word >= x->n) continue;
        weave_types(x, &b[k], 1, x->nseq, tot, c, vals, who, own + (size_t)k * x->n);
    }

    /* words by frequency, for the blind twins */
    for (uint32_t i = 0; i < x->n; i++) ord[i] = i;
    for (uint32_t gap = x->n / 2; gap > 0; gap /= 2)
        for (uint32_t i = gap; i < x->n; i++) {
            uint32_t w = ord[i], j = i;
            while (j >= gap && x->lego[ord[j - gap]].count < x->lego[w].count) { ord[j] = ord[j - gap]; j -= gap; }
            ord[j] = w;
        }
    for (uint32_t i = 0; i < x->n; i++) rank[ord[i]] = i;

    for (uint32_t k = 0; k < nb; k++) {
        int32_t w = b[k].word;
        if (w < 0 || (uint32_t)w >= x->n) continue;
        v[k].sounds = x->lego[w].count >= 2;
        if (!v[k].sounds) continue;
        memset(ty, 0, x->n);
        for (uint32_t o = 0; o < nb; o++)
            if (o != k)
                for (uint32_t i = 0; i < x->n; i++) ty[i] |= own[(size_t)o * x->n + i];

        /* the work it does here: its uses that stand next to a type */
        uint32_t uses = 0, next_to = 0;
        for (uint32_t i = 0; i < x->nseq; i++) {
            if (x->seq[i] != w) continue;
            uses++;
            int32_t t = at(x, i, -b[k].side);
            if (t >= 0 && ty[t]) next_to++;
        }
        v[k].role = uses ? (double)next_to / uses : 0.0;

        v[k].info = note_information(later, nlater, ty, w, b[k].side);
        double bl = 0;
        int runs = 0;
        for (int j = 1; j <= 8; j++) {
            int64_t r = (int64_t)rank[w] + ((j & 1) ? (j + 1) / 2 : -(j / 2));
            if (r < 0 || r >= (int64_t)x->n) continue;
            bl += note_information(later, nlater, ty, (int32_t)ord[r], b[k].side);
            runs++;
        }
        v[k].blind = runs ? bl / runs : 0.0;
        v[k].holds = v[k].info > v[k].blind;
    }
    free(own);
out:
    free(ty); free(tot); free(c); free(vals); free(who); free(ord); free(rank);
}

rz_why rz_melody_why(const rz_note *note, const rz_note_verdict *v)
{
    if (!v->sounds) return RZ_WHY_SILENT;
    if (v->holds) return RZ_WHY_HOLDS;
    /* where it held it stood next to types this often; here less: it is doing
       something else in this text */
    if (note->held > 0 && v->role < note->role) return RZ_WHY_OTHER_WORK;
    return RZ_WHY_SAYS_NOTHING;
}

void rz_melody_remember(rz_note *note, const rz_note_verdict *v)
{
    if (!v->sounds) return;
    if (note->heard < UINT16_MAX) note->heard++;
    if (v->holds && note->held < UINT16_MAX) {
        note->role = (float)((note->role * note->held + v->role) / (note->held + 1));
        note->held++;
    }
}

int rz_melody_trusted(const rz_note *note)
{
    return note->heard == 0 || 2u * note->held >= note->heard;
}


/* ===================== the specialised key (P41) ========================= */

uint32_t rz_melody_compare(const rz_binding *fresh, uint32_t nf,
                           const rz_binding *carried, uint32_t nc,
                           rz_binding *diff, uint32_t cap, uint32_t *agree)
{
    uint32_t nd = 0;
    *agree = 0;
    for (uint32_t i = 0; i < nf; i++) {
        int known = 0;
        for (uint32_t k = 0; k < nc && !known; k++)
            known = carried[k].word == fresh[i].word && carried[k].side == fresh[i].side;
        if (known) (*agree)++;
        else if (nd < cap) diff[nd++] = fresh[i];
    }
    return nd;
}


/* ===================== where a question applies (P42) ==================== */

uint32_t rz_form_subjects(rz_lexicon *x, int32_t relation, int32_t *out, uint32_t cap)
{
    if (relation < 0 || (uint32_t)relation >= x->n || !x->nseq) return 0;
    uint32_t *before = calloc(x->n, sizeof *before), *tot = calloc(x->n, sizeof *tot);
    double *score = malloc((size_t)x->n * sizeof *score);
    uint32_t k = 0;
    if (!before || !tot || !score) goto out;
    double nrel = 0, all = 0;
    for (uint32_t i = 0; i < x->nseq; i++) {
        if (x->seq[i] < 0) continue;
        tot[x->seq[i]]++;
        all++;
        if (x->seq[i] == relation && i > 0 && x->seq[i - 1] >= 0) { before[x->seq[i - 1]]++; nrel++; }
    }
    /* who stands in the subject's place far more than they stand anywhere,
       said twice at least */
    for (uint32_t w = 0; w < x->n; w++) {
        score[w] = -1;
        if (before[w] < 2 || (int32_t)w == relation) continue;
        double lift = ((double)before[w] / nrel) / ((double)tot[w] / all);
        if (lift <= 1.0) continue;
        score[w] = log(lift) * log((double)before[w]);
    }
    while (k < cap) {
        int32_t best = -1;
        for (uint32_t w = 0; w < x->n; w++)
            if (score[w] > 0 && (best < 0 || score[w] > score[best])) best = (int32_t)w;
        if (best < 0) break;
        out[k++] = best;
        score[best] = -1;
    }
out:
    free(before); free(tot); free(score);
    return k;
}

uint32_t rz_form_reaches(rz_lexicon *x, const int32_t *studied, uint32_t ns,
                         int32_t *out, uint32_t cap)
{
    if (ns == 0 || cap == 0) return 0;
    company c;
    memset(&c, 0, sizeof c);
    if (!company_build(x, &c)) { company_free(&c); return 0; }
    uint32_t W = 2 * c.F, k = 0, nv = 0;
    double *row = calloc(W, sizeof *row);
    double *score = malloc((size_t)x->n * sizeof *score);
    double *vals = malloc((size_t)x->n * sizeof *vals);
    int32_t *who = malloc((size_t)x->n * sizeof *who);
    uint8_t *is_studied = calloc(x->n, 1);
    if (!row || !score || !vals || !who || !is_studied) goto out;

    /* the company the studied subjects keep, together */
    for (uint32_t i = 0; i < ns; i++) {
        if (studied[i] < 0 || (uint32_t)studied[i] >= x->n) continue;
        is_studied[studied[i]] = 1;
        company_add(x, &c, (uint32_t)studied[i], row);
    }
    unit(row, W);
    company_dots(x, &c, row, 1, score);
    for (uint32_t i = 0; i < x->n; i++)
        if (!is_studied[i] && x->lego[i].count >= 2 && score[i] > 0) { vals[nv] = score[i]; who[nv++] = (int32_t)i; }

    /* the same two cuts the silhouette uses: the split of all of them, then
       the widest gap inside the tall side */
    double first = otsu_list(vals, nv);
    if (first == -HUGE_VAL) goto out;
    uint32_t nt = 0;
    for (uint32_t i = 0; i < nv; i++)
        if (vals[i] > first) { vals[nt] = vals[i]; who[nt++] = who[i]; }
    for (uint32_t i = 1; i < nt; i++) {
        double v = vals[i];
        int32_t w = who[i];
        uint32_t j = i;
        while (j > 0 && vals[j - 1] < v) { vals[j] = vals[j - 1]; who[j] = who[j - 1]; j--; }
        vals[j] = v;
        who[j] = w;
    }
    uint32_t keep = nt;
    double widest = 0;
    for (uint32_t i = 0; i + 1 < nt; i++)
        if (vals[i] - vals[i + 1] > widest) { widest = vals[i] - vals[i + 1]; keep = i + 1; }
    for (uint32_t i = 0; i < keep && k < cap; i++) out[k++] = who[i];
out:
    free(row); free(score); free(vals); free(who); free(is_studied);
    company_free(&c);
    return k;
}


/* ===================== the instruments of a word (P43) =================== */

typedef struct {
    const rz_lexicon *x;
    const uint32_t   *pos;           /* where the word sounds */
    uint32_t          nocc;
    const float      *timbre;        /* nocc x dim: the company before and after */
    uint32_t          dim;
    uint32_t          half;          /* seq positions below it are the half it splits on */
    uint8_t          *label;         /* nocc: the instrument, while splitting */
    uint32_t          nlabel;
    uint64_t          rng;
    const rz_keys    *keys;          /* the same note in another key is not a new kind */
} band;

static double dotf(const float *a, const double *b, uint32_t n)
{
    double s = 0;
    for (uint32_t i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

static uint64_t band_rand(band *bd)
{
    bd->rng ^= bd->rng << 13; bd->rng ^= bd->rng >> 7; bd->rng ^= bd->rng << 17;
    return bd->rng;
}

/* split `set` in two where the timbres part, and again inside each half,
   only while the held half says the split tells more than chance */
/* the commonest neighbour at offset d among the occurrences of one side of a
   split, over the half it splits on */
static int32_t commonest_beside(band *bd, const uint32_t *set, uint32_t n,
                                const uint8_t *side, uint8_t which, int d, uint32_t *count)
{
    int32_t best = -1;
    uint32_t bestc = 0;
    for (uint32_t k = 0; k < n; k++) {
        if (side[k] != which || bd->pos[set[k]] >= bd->half) continue;
        int32_t nb = at(bd->x, bd->pos[set[k]], d);
        if (nb < 0) continue;
        uint32_t c = 0;
        for (uint32_t j = 0; j < n; j++)
            if (side[j] == which && bd->pos[set[j]] < bd->half && at(bd->x, bd->pos[set[j]], d) == nb) c++;
        if (c > bestc) { bestc = c; best = nb; }
    }
    *count = bestc;
    return best;
}

static int alike(band *bd, int32_t a, int32_t b)
{
    if (a < 0 || b < 0) return a == b;
    if (a == b) return 1;
    if (bd->keys && rz_keys_same(bd->keys, a, b)) return 1;   /* "es el" and "es la" (P44) */
    int32_t sil[64];
    uint32_t k = rz_lexicon_silhouette((rz_lexicon *)bd->x, a, sil, 64);
    for (uint32_t i = 0; i < k; i++) if (sil[i] == b) return 1;
    k = rz_lexicon_silhouette((rz_lexicon *)bd->x, b, sil, 64);
    for (uint32_t i = 0; i < k; i++) if (sil[i] == a) return 1;
    return 0;
}

static int same_instrument(band *bd, const uint32_t *set, uint32_t n, const uint8_t *side)
{
    for (int d = -1; d <= 1; d += 2) {
        uint32_t ca, cb;
        int32_t a = commonest_beside(bd, set, n, side, 0, d, &ca);
        int32_t b = commonest_beside(bd, set, n, side, 1, d, &cb);
        if (!alike(bd, a, b)) return 0;
    }
    return 1;
}

static void split_band(band *bd, uint32_t *set, uint32_t n, uint32_t cap)
{
    uint32_t nl = 0, nh = 0;
    for (uint32_t k = 0; k < n; k++) {
        if (bd->pos[set[k]] < bd->half) nl++;
        else nh++;
    }
    uint8_t mine = bd->label[set[0]];
    if (nl < 4 || nh < 4 || bd->nlabel >= cap) return;

    uint32_t D = bd->dim;
    double *mean = calloc(D, sizeof *mean), *ca = calloc(D, sizeof *ca), *cb = calloc(D, sizeof *cb);
    uint8_t *side = malloc(n), *shuffled = malloc(n);
    uint32_t *held = malloc(n * sizeof *held);
    uint8_t *hside = malloc(n);
    if (!mean || !ca || !cb || !side || !shuffled || !held || !hside) goto done;

    /* two-means on the half it splits on: start at the occurrence farthest
       from the mean, and the one farthest from that */
    for (uint32_t k = 0; k < n; k++) if (bd->pos[set[k]] < bd->half)
        for (uint32_t i = 0; i < D; i++) mean[i] += bd->timbre[(size_t)set[k] * D + i];
    uint32_t fa = set[0], fb = set[0];
    double lo = HUGE_VAL;
    for (uint32_t k = 0; k < n; k++) if (bd->pos[set[k]] < bd->half) {
        double v = dotf(bd->timbre + (size_t)set[k] * D, mean, D);
        if (v < lo) { lo = v; fa = set[k]; }
    }
    for (uint32_t i = 0; i < D; i++) ca[i] = bd->timbre[(size_t)fa * D + i];
    lo = HUGE_VAL;
    for (uint32_t k = 0; k < n; k++) if (bd->pos[set[k]] < bd->half) {
        double v = dotf(bd->timbre + (size_t)set[k] * D, ca, D);
        if (v < lo) { lo = v; fb = set[k]; }
    }
    for (uint32_t i = 0; i < D; i++) cb[i] = bd->timbre[(size_t)fb * D + i];
    for (int it = 0; it < 30; it++) {
        int moved = 0;
        for (uint32_t k = 0; k < n; k++) {
            const float *v = bd->timbre + (size_t)set[k] * D;
            uint8_t sd = dotf(v, cb, D) > dotf(v, ca, D);
            if (it == 0 || sd != side[k]) moved = 1;
            side[k] = sd;
        }
        if (!moved) break;
        memset(ca, 0, D * sizeof *ca);
        memset(cb, 0, D * sizeof *cb);
        for (uint32_t k = 0; k < n; k++) if (bd->pos[set[k]] < bd->half) {
            double *cc = side[k] ? cb : ca;
            for (uint32_t i = 0; i < D; i++) cc[i] += bd->timbre[(size_t)set[k] * D + i];
        }
        /* unit centres: a raw sum is longer for the larger group, and every
           occurrence then fell to it -- the first run split nothing at all */
        unit(ca, D);
        unit(cb, D);
    }

    /* The judge, on the half it did not split on. An instrument is a timbre,
       not a neighbour: the first judge asked whether the split told which
       exact word stood alongside, and went on splitting "rex is" from "tom
       is" -- two players, one instrument. Now: do the two centres explain the
       held timbres better than the single centre, by more than eight splits of
       the same half drawn at random and judged the same way? Rex and Tom keep
       almost the same company, so splitting them explains no more than chance. */
    uint32_t ones = 0;
    nh = 0;
    for (uint32_t k = 0; k < n; k++)
        if (bd->pos[set[k]] >= bd->half) { held[nh] = set[k]; hside[nh] = side[k]; ones += side[k]; nh++; }
    if (ones < 2 || nh - ones < 2) goto done;

    /* Another instrument, or the same one with another player? Explained
       timbre could not tell: every "rex" keeps exactly the same company, so a
       group of rexes is perfectly tight and always "explains" something chance
       does not. The silhouette can (P36): on each side of the word, the
       commonest neighbour of one group either is, or is in the silhouette of,
       the commonest neighbour of the other -- rex and tom, "a" and "the" --
       or it is not ("what" beside "rex"). Only a change of kind on some side
       is a new instrument. */
    if (same_instrument(bd, set, n, side)) goto done;
    {
        double *parent = calloc(D, sizeof *parent), *ra = calloc(D, sizeof *ra), *rb = calloc(D, sizeof *rb);
        uint32_t *learn = malloc(n * sizeof *learn);
        uint8_t *rside = malloc(n);
        int beats = parent && ra && rb && learn && rside;
        uint32_t nl2 = 0;
        if (beats) {
            for (uint32_t k = 0; k < n; k++) if (bd->pos[set[k]] < bd->half) {
                learn[nl2++] = set[k];
                for (uint32_t i = 0; i < D; i++) parent[i] += bd->timbre[(size_t)set[k] * D + i];
            }
            unit(parent, D);
        }
        double real = 0;
        for (uint32_t k = 0; beats && k < nh; k++) {
            const float *v = bd->timbre + (size_t)held[k] * D;
            double best = dotf(v, ca, D), other = dotf(v, cb, D);
            if (other > best) best = other;
            real += best - dotf(v, parent, D);
        }
        for (int r = 0; r < 8 && beats; r++) {
            uint32_t on = 0;
            for (uint32_t k = 0; k < nl2; k++) { rside[k] = (uint8_t)(band_rand(bd) & 1u); on += rside[k]; }
            if (on == 0 || on == nl2) { r--; continue; }
            memset(ra, 0, D * sizeof *ra);
            memset(rb, 0, D * sizeof *rb);
            for (uint32_t k = 0; k < nl2; k++) {
                double *cc = rside[k] ? rb : ra;
                for (uint32_t i = 0; i < D; i++) cc[i] += bd->timbre[(size_t)learn[k] * D + i];
            }
            unit(ra, D);
            unit(rb, D);
            double rnd = 0;
            for (uint32_t k = 0; k < nh; k++) {
                const float *v = bd->timbre + (size_t)held[k] * D;
                double best = dotf(v, ra, D), other = dotf(v, rb, D);
                if (other > best) best = other;
                rnd += best - dotf(v, parent, D);
            }
            if (rnd >= real) beats = 0;
        }
        free(parent); free(ra); free(rb); free(learn); free(rside);
        if (!beats) goto done;
    }

    /* it pays: the second side becomes an instrument of its own, and each side
       is listened to again */
    uint8_t other = (uint8_t)bd->nlabel++;
    uint32_t na = 0, nb2 = 0;
    for (uint32_t k = 0; k < n; k++) {
        if (side[k]) bd->label[set[k]] = other;
        else bd->label[set[k]] = mine;
    }
    for (uint32_t k = 0; k < n; k++) if (!side[k]) set[na++] = set[k];
    uint32_t *rest = held;                     /* reuse: the other side's members */
    for (uint32_t k = 0; k < bd->nocc && nb2 < n; k++) if (bd->label[k] == other) rest[nb2++] = k;
    free(mean); free(ca); free(cb); free(side); free(shuffled); free(hside);
    split_band(bd, set, na, cap);
    split_band(bd, rest, nb2, cap);
    free(rest);
    return;
done:
    free(mean); free(ca); free(cb); free(side); free(shuffled); free(held); free(hside);
}

uint32_t rz_word_instruments(rz_lexicon *x, int32_t word, rz_instrument *out, uint32_t cap,
                             uint8_t *which, uint32_t which_cap)
{
    if (word < 0 || (uint32_t)word >= x->n || cap == 0 || !x->nseq) return 0;
    uint32_t nocc = 0, k = 0;
    for (uint32_t i = 0; i < x->nseq; i++) nocc += x->seq[i] == word;
    if (nocc == 0) return 0;

    company c;
    memset(&c, 0, sizeof c);
    if (!company_build(x, &c)) { company_free(&c); return 0; }
    uint32_t W = 2 * c.F, D = 2 * W;
    uint32_t *pos = malloc(nocc * sizeof *pos), *set = malloc(nocc * sizeof *set);
    float *timbre = calloc((size_t)nocc * D, sizeof *timbre);
    int32_t *slot = malloc((size_t)x->n * sizeof *slot);
    double *row = malloc(W * sizeof *row);
    double **cache = calloc(x->n, sizeof *cache);
    uint8_t *label = calloc(nocc, 1);
    uint32_t *cnt0 = calloc(x->n, sizeof *cnt0), *cnt1 = calloc(x->n, sizeof *cnt1), *cnt2 = calloc(x->n, sizeof *cnt2);
    if (!pos || !set || !timbre || !slot || !row || !cache || !label || !cnt0 || !cnt1 || !cnt2) goto out;

    nocc = 0;
    for (uint32_t i = 0; i < x->nseq; i++) if (x->seq[i] == word) pos[nocc++] = i;

    /* the timbre of every occurrence: the company of the word before it and
       of the word after it, each as a unit vector */
    for (uint32_t o = 0; o < nocc; o++) {
        for (int d = 0; d < 2; d++) {
            int32_t nb = at(x, pos[o], d ? 1 : -1);
            if (nb < 0) continue;
            if (!cache[nb]) {
                cache[nb] = calloc(W, sizeof *cache[nb]);
                if (!cache[nb]) continue;
                company_add(x, &c, (uint32_t)nb, cache[nb]);
                unit(cache[nb], W);
            }
            for (uint32_t i = 0; i < W; i++) timbre[(size_t)o * D + (uint32_t)d * W + i] = (float)cache[nb][i];
        }
        set[o] = o;
    }

    rz_keys keys;
    int have_keys = rz_lexicon_keys(x, &keys) == RZ_OK;
    band bd = { x, pos, nocc, timbre, D, x->nseq / 2, label, 1, 0x9E3779B97F4A7C15ull,
                have_keys ? &keys : NULL };
    split_band(&bd, set, nocc, cap < 256 ? cap : 256);

    /* study each track apart: what sounds before and after it, more than
       around the word as a whole */
    for (uint32_t g = 0; g < bd.nlabel && k < cap; g++) {
        rz_instrument *in = &out[k++];
        memset(in, 0, sizeof *in);
        for (int b = 0; b < 6; b++) in->before[b] = in->after[b] = -1;
        uint32_t first = UINT32_MAX;
        for (uint32_t o = 0; o < nocc; o++) if (label[o] == g) { in->uses++; if (first == UINT32_MAX) first = o; }
        in->example = first == UINT32_MAX ? 0 : pos[first];
        for (int d = 0; d < 2; d++) {
            int dir = d ? 1 : -1;
            for (uint32_t o = 0; o < nocc; o++) {
                int32_t nb = at(x, pos[o], dir);
                if (nb < 0) continue;
                cnt2[nb]++;
                if (label[o] == g) cnt0[nb]++;
            }
            int32_t *list = d ? in->after : in->before;
            for (int r = 0; r < 6; r++) {
                int32_t best = -1;
                double bv = 0;
                for (uint32_t o = 0; o < nocc; o++) {
                    int32_t nb = at(x, pos[o], dir);
                    if (nb < 0 || cnt0[nb] < 2) continue;
                    double lift = ((double)cnt0[nb] / in->uses) / ((double)cnt2[nb] / nocc);
                    double v = log(lift + 1.0) * log((double)cnt0[nb]);
                    if (v > bv) { bv = v; best = nb; }
                }
                if (best < 0) break;
                list[r] = best;
                cnt0[best] = 0;
            }
            for (uint32_t o = 0; o < nocc; o++) {
                int32_t nb = at(x, pos[o], dir);
                if (nb >= 0) cnt0[nb] = cnt2[nb] = 0;
            }
        }
    }
    if (which)
        for (uint32_t o = 0; o < nocc && o < which_cap; o++) which[o] = label[o];
    if (have_keys) rz_keys_free(&keys);
out:
    if (cache) for (uint32_t i = 0; i < x->n; i++) free(cache[i]);
    free(cache); free(pos); free(set); free(timbre); free(slot); free(row); free(label);
    free(cnt0); free(cnt1); free(cnt2);
    company_free(&c);
    return k;
}


/* ===================== the same song in another key (P44) ================ */

typedef struct { uint32_t dim; float v; } cell;
typedef struct { uint32_t *start; cell *cells; double *norm; } sparse_company;

static int by_dim(const void *a, const void *b)
{
    const cell *p = a, *q = b;
    return p->dim < q->dim ? -1 : p->dim > q->dim;
}

/* every word's company as a sorted sparse vector, read off the pair table */
static int sparse_build(const rz_lexicon *x, const company *c, sparse_company *sc)
{
    sc->start = calloc((size_t)x->n + 1, sizeof *sc->start);
    sc->norm = c->norm;
    if (!sc->start) return 0;
    for (uint32_t p = 0; p < x->npair; p++) {
        if (c->fidx[x->pair[p].a] >= 0) sc->start[x->pair[p].b + 1]++;
        if (c->fidx[x->pair[p].b] >= 0) sc->start[x->pair[p].a + 1]++;
    }
    for (uint32_t i = 0; i < x->n; i++) sc->start[i + 1] += sc->start[i];
    sc->cells = malloc(((size_t)sc->start[x->n] + 1) * sizeof *sc->cells);
    uint32_t *fill = malloc(((size_t)x->n + 1) * sizeof *fill);
    if (!sc->cells || !fill) { free(fill); return 0; }
    memcpy(fill, sc->start, ((size_t)x->n + 1) * sizeof *fill);
    for (uint32_t p = 0; p < x->npair; p++) {
        const rz_lego_pair *q = &x->pair[p];
        float v = (float)log1p((double)q->count);
        if (c->fidx[q->a] >= 0) sc->cells[fill[q->b]++] = (cell){ (uint32_t)c->fidx[q->a], v };
        if (c->fidx[q->b] >= 0) sc->cells[fill[q->a]++] = (cell){ c->F + (uint32_t)c->fidx[q->b], v };
    }
    free(fill);
    for (uint32_t i = 0; i < x->n; i++)
        qsort(sc->cells + sc->start[i], sc->start[i + 1] - sc->start[i], sizeof *sc->cells, by_dim);
    return 1;
}

static double sparse_cos(const sparse_company *sc, uint32_t a, uint32_t b)
{
    if (sc->norm[a] <= 0 || sc->norm[b] <= 0) return 0;
    uint32_t i = sc->start[a], j = sc->start[b];
    double d = 0;
    while (i < sc->start[a + 1] && j < sc->start[b + 1]) {
        if (sc->cells[i].dim < sc->cells[j].dim) i++;
        else if (sc->cells[i].dim > sc->cells[j].dim) j++;
        else { d += (double)sc->cells[i].v * sc->cells[j].v; i++; j++; }
    }
    return d / (sc->norm[a] * sc->norm[b]);
}

typedef struct { uint32_t w; uint8_t pos; } masked;
static const rz_lexicon *masked_x;

/* order by length, then by the spelling with one letter hidden */
static int by_masked(const void *pa, const void *pb)
{
    const masked *p = pa, *q = pb;
    const rz_lego *a = &masked_x->lego[p->w], *b = &masked_x->lego[q->w];
    if (a->len != b->len) return a->len < b->len ? -1 : 1;
    if (p->pos != q->pos) return p->pos < q->pos ? -1 : 1;
    for (uint8_t i = 0; i < a->len; i++) {
        if (i == p->pos) continue;
        if (a->s[i] != b->s[i]) return a->s[i] < b->s[i] ? -1 : 1;
    }
    return 0;
}

typedef struct { uint32_t a, b; int8_t from_end; uint8_t la, lb; double sim; } twin_pair;

void rz_keys_free(rz_keys *k)
{
    free(k->side); free(k->twin); free(k->marks); free(k->key);
    memset(k, 0, sizeof *k);
}

int rz_keys_same(const rz_keys *k, int32_t a, int32_t b)
{
    if (a == b) return 1;
    if (a < 0 || b < 0 || !k->twin) return 0;
    return k->twin[a] == b || k->key[a] == b;
}

int rz_lexicon_keys(rz_lexicon *x, rz_keys *k)
{
    memset(k, 0, sizeof *k);
    if (!x->n || !x->nseq) return RZ_ERR_IO;
    k->side = malloc(x->n);
    k->twin = malloc((size_t)x->n * sizeof *k->twin);
    k->marks = malloc(x->n);
    k->key = malloc((size_t)x->n * sizeof *k->key);
    if (!k->side || !k->twin || !k->marks || !k->key) { rz_keys_free(k); return RZ_ERR_IO; }
    for (uint32_t i = 0; i < x->n; i++) { k->side[i] = -1; k->twin[i] = -1; k->marks[i] = -1; k->key[i] = -1; }

    company c;
    memset(&c, 0, sizeof c);
    sparse_company sc;
    memset(&sc, 0, sizeof sc);
    masked *mk = NULL;
    twin_pair *tp = NULL;
    int rc = RZ_ERR_IO;
    if (!company_build(x, &c) || !sparse_build(x, &c, &sc)) goto out;

    /* 1. twins: same length, one letter different at one place, seen twice */
    uint32_t nm = 0;
    for (uint32_t i = 0; i < x->n; i++) if (x->lego[i].count >= 2 && x->lego[i].len >= 3) nm += x->lego[i].len;
    mk = malloc(((size_t)nm + 1) * sizeof *mk);
    if (!mk) goto out;
    nm = 0;
    for (uint32_t i = 0; i < x->n; i++)
        if (x->lego[i].count >= 2 && x->lego[i].len >= 3)
            for (uint8_t p = 0; p < x->lego[i].len; p++) mk[nm++] = (masked){ i, p };
    masked_x = x;
    qsort(mk, nm, sizeof *mk, by_masked);
    uint32_t ntp = 0, tcap = 1024;
    tp = malloc(tcap * sizeof *tp);
    if (!tp) goto out;
    for (uint32_t i = 0; i < nm; ) {
        uint32_t j = i + 1;
        while (j < nm && by_masked(&mk[i], &mk[j]) == 0) j++;
        for (uint32_t a = i; a < j; a++)
            for (uint32_t b = a + 1; b < j; b++) {
                if (ntp == tcap) {
                    twin_pair *g = realloc(tp, (tcap *= 2) * sizeof *tp);
                    if (!g) goto out;
                    tp = g;
                }
                const rz_lego *A = &x->lego[mk[a].w], *B = &x->lego[mk[b].w];
                uint8_t la = A->s[mk[a].pos], lb = B->s[mk[a].pos];
                uint32_t wa = mk[a].w, wb = mk[b].w;
                if (la > lb) { uint8_t tl = la; la = lb; lb = tl; uint32_t tw = wa; wa = wb; wb = tw; }
                tp[ntp++] = (twin_pair){ wa, wb, (int8_t)(A->len - 1 - mk[a].pos), la, lb,
                                         sparse_cos(&sc, wa, wb) };
            }
        i = j;
    }

    /* 2. No filter on company here. The first version kept only twins of
       similar company -- and dropped gato/gata while keeping toma/tomo: the key
       is exactly what changes a twin's company ("el gato", "la gata"), so
       judging twins by their whole company punishes them for being twins. The
       structure is read in step 4 instead: what changes with the key marks it,
       what stands before both sides is the rule for both. */
    double cut = -HUGE_VAL;

    /* 3. the property: the difference that repeats across the most of them */
    uint32_t best = 0, bestn = 0;
    for (uint32_t i = 0; i < ntp; i++) {
        if (tp[i].sim <= cut) continue;
        uint32_t n2 = 0;
        for (uint32_t j = 0; j < ntp; j++)
            n2 += tp[j].sim > cut && tp[j].from_end == tp[i].from_end && tp[j].la == tp[i].la && tp[j].lb == tp[i].lb;
        if (n2 > bestn) { bestn = n2; best = i; }
    }
    if (bestn == 0) goto out;
    k->property = (rz_property){ tp[best].from_end, tp[best].la, tp[best].lb, bestn };
    for (uint32_t i = 0; i < ntp; i++) {
        const twin_pair *t = &tp[i];
        if (t->sim <= cut || t->from_end != k->property.from_end || t->la != k->property.a || t->lb != k->property.b) continue;
        if (k->twin[t->a] >= 0 || k->twin[t->b] >= 0) continue;
        k->side[t->a] = 0; k->side[t->b] = 1;
        k->twin[t->a] = (int32_t)t->b; k->twin[t->b] = (int32_t)t->a;
    }

    /* 4. the markers: what stands before each side. Three groups by the exact
       optimum: before side 0 only, before both, before side 1 only. Twice:
       some twins are markers themselves (una/uno, esta/esto), and what stands
       before a marker is not a marker -- "es una" made "es" one, and the map
       of P47 lost its relation. The second time, only twins that are not
       markers count. */
    for (int pass = 0; pass < 2; pass++) {
        int8_t *was = NULL;
        if (pass) {
            was = malloc(x->n);
            if (!was) goto out;
            memcpy(was, k->marks, x->n);
            for (uint32_t i = 0; i < x->n; i++) k->marks[i] = -1;
        }
        uint32_t *c0 = calloc(x->n, sizeof *c0), *c1 = calloc(x->n, sizeof *c1);
        double *f = malloc((size_t)x->n * sizeof *f);
        uint32_t *who = malloc((size_t)x->n * sizeof *who);
        if (!c0 || !c1 || !f || !who) { free(c0); free(c1); free(f); free(who); free(was); goto out; }
        for (uint32_t i = 0; i + 1 < x->nseq; i++) {
            int32_t m = x->seq[i], nx = x->seq[i + 1];
            if (m < 0 || nx < 0 || k->side[nx] < 0) continue;
            if (was && was[nx] >= 0) continue;
            if (k->side[nx]) c1[m]++; else c0[m]++;
        }
        /* A marker stands before twins more often than twins stand anywhere
           (D50). Without this, every word that ever stood before a twin got a
           side -- on the NT "Dios", "es", "porque", even "amor" -- and the map
           of P47 found nothing to say, every word being glue. (Cut instead at
           the tall side of the share, "el" and "de" fell out: few nouns have a
           twin, so even "el" stands before one only now and then -- far more
           than other words do, which is what counts.) */
        uint32_t ntwin = 0, ntok = 0;
        for (uint32_t i = 0; i < x->nseq; i++) {
            if (x->seq[i] < 0) continue;
            ntok++;
            if (k->side[x->seq[i]] >= 0 && !(was && was[x->seq[i]] >= 0)) ntwin++;
        }
        double base = ntok ? (double)ntwin / ntok : 0;
        uint32_t nf = 0;
        for (uint32_t m = 0; m < x->n; m++)
            if (c0[m] + c1[m] >= 2 && (double)(c0[m] + c1[m]) / x->lego[m].count > base) who[nf++] = m;
        for (uint32_t i = 0; i < nf; i++) f[i] = (double)c1[who[i]] / (c0[who[i]] + c1[who[i]]);
        /* sort, then the two cuts that leave the least spread inside the groups */
        for (uint32_t i = 1; i < nf; i++) {
            double v = f[i];
            uint32_t w = who[i], j = i;
            while (j > 0 && f[j - 1] > v) { f[j] = f[j - 1]; who[j] = who[j - 1]; j--; }
            f[j] = v; who[j] = w;
        }
        double *ps = malloc(((size_t)nf + 1) * sizeof *ps), *pq = malloc(((size_t)nf + 1) * sizeof *pq);
        if (ps && pq && nf >= 3) {
            ps[0] = pq[0] = 0;
            for (uint32_t i = 0; i < nf; i++) { ps[i + 1] = ps[i] + f[i]; pq[i + 1] = pq[i] + f[i] * f[i]; }
            #define SSE(lo, hi) ((pq[hi] - pq[lo]) - (ps[hi] - ps[lo]) * (ps[hi] - ps[lo]) / (double)((hi) - (lo)))
            double bv = HUGE_VAL;
            uint32_t bi = 1, bj = 2;
            for (uint32_t i = 1; i + 1 < nf; i++)
                for (uint32_t j = i + 1; j < nf; j++) {
                    double v = SSE(0, i) + SSE(i, j) + SSE(j, nf);
                    if (v < bv) { bv = v; bi = i; bj = j; }
                }
            #undef SSE
            for (uint32_t i = 0; i < nf; i++) k->marks[who[i]] = i < bi ? 0 : i < bj ? 2 : 1;
        } else {
            for (uint32_t i = 0; i < nf; i++) k->marks[who[i]] = f[i] > 0.5 ? 1 : f[i] < 0.5 ? 0 : 2;
        }
        /* "For both" is only what was seen before both sides, twice each: the
           three-way split makes three groups even where there is no "both",
           and with few markers "la" landed in the middle. */
        for (uint32_t i = 0; i < nf; i++) {
            uint32_t m = who[i];
            if (k->marks[m] == 2 && (c0[m] < 2 || c1[m] < 2)) k->marks[m] = c1[m] > c0[m] ? 1 : 0;
        }
        free(ps); free(pq);
        free(c0); free(c1); free(f); free(who); free(was);
    }

    /* 5. one note in two keys: a marker of one side and a marker of the other
       that stand after the same words -- the key changes what follows a
       marker, not what comes before it ("de el…", "de la…"). Each must be the
       other's best. (Matched first by what follows, once twins were read as
       their partner: "el" and "la" never met, since most nouns have no twin --
       "el trono", "la Ciudad".) */
    {
        uint32_t nmk = 0;
        uint32_t *mks = malloc((size_t)x->n * sizeof *mks);
        int32_t *best_of = malloc((size_t)x->n * sizeof *best_of);
        if (!mks || !best_of) { free(mks); free(best_of); goto out; }
        for (uint32_t m = 0; m < x->n; m++) if (k->marks[m] == 0 || k->marks[m] == 1) mks[nmk++] = m;
        for (uint32_t i = 0; i < nmk; i++) {
            best_of[i] = -1;
            double bv = 0;
            for (uint32_t j = 0; j < nmk; j++) {
                if (k->marks[mks[j]] == k->marks[mks[i]]) continue;
                /* cosine of what stands before each: the first F dimensions */
                uint32_t a2 = mks[i], b2 = mks[j];
                uint32_t p = sc.start[a2], q = sc.start[b2];
                double d = 0, na = 0, nb = 0;
                for (uint32_t t2 = sc.start[a2]; t2 < sc.start[a2 + 1] && sc.cells[t2].dim < c.F; t2++)
                    na += (double)sc.cells[t2].v * sc.cells[t2].v;
                for (uint32_t t2 = sc.start[b2]; t2 < sc.start[b2 + 1] && sc.cells[t2].dim < c.F; t2++)
                    nb += (double)sc.cells[t2].v * sc.cells[t2].v;
                while (p < sc.start[a2 + 1] && q < sc.start[b2 + 1]
                       && sc.cells[p].dim < c.F && sc.cells[q].dim < c.F) {
                    if (sc.cells[p].dim < sc.cells[q].dim) p++;
                    else if (sc.cells[p].dim > sc.cells[q].dim) q++;
                    else { d += (double)sc.cells[p].v * sc.cells[q].v; p++; q++; }
                }
                double cs = (na > 0 && nb > 0) ? d / sqrt(na * nb) : 0;
                if (cs > bv) { bv = cs; best_of[i] = (int32_t)j; }
            }
        }
        for (uint32_t i = 0; i < nmk; i++) {
            int32_t j = best_of[i];
            if (j >= 0 && best_of[j] == (int32_t)i) k->key[mks[i]] = (int32_t)mks[j];
        }
        free(mks); free(best_of);
    }
    rc = RZ_OK;
out:
    free(mk); free(tp); free(sc.start); free(sc.cells);
    company_free(&c);
    if (rc != RZ_OK) rz_keys_free(k);
    return rc;
}


/* ===================== PENZAR: thinking in real time (P45) =============== */

void rz_penzar_init(rz_penzar *p)
{
    memset(p, 0, sizeof *p);
    p->mind = malloc(sizeof *p->mind);
    if (p->mind) rz_init(p->mind);
    rz_lexicon_init(&p->lex);
}

void rz_penzar_free(rz_penzar *p)
{
    if (p->have_keys) rz_keys_free(&p->keys);
    rz_lexicon_free(&p->lex);
    if (p->mind) { rz_free(p->mind); free(p->mind); }
    memset(p, 0, sizeof *p);
}

int rz_hunch_holds(const rz_hunch *h)
{
    /* won once is an anecdote, the same rule everywhere: "1 of 1" was being
       confirmed and lent sideways */
    if (h->tried < 2 || h->base_tried == 0) return 0;
    return (double)h->right / h->tried > (double)h->base_right / h->base_tried;
}

static int32_t hunch_of(const rz_penzar *p, int32_t word)
{
    for (uint32_t i = 0; i < p->nhunch; i++) if (p->hunch[i].word == word) return (int32_t)i;
    return -1;
}

void rz_penzar_bite(rz_penzar *p, const unsigned char *t, size_t n, rz_thought *out)
{
    memset(out, 0, sizeof *out);
    if (!p->mind || n == 0) return;

    /* 1. bet, before reading: words looked up in what it already knows */
    if (p->have_keys && p->nhunch && p->lex.n) {
        int32_t *ids = malloc((n + 1) * sizeof *ids);
        int32_t *hof = malloc((size_t)p->lex.n * sizeof *hof);
        if (ids && hof) {
            uint32_t nid = rz_lexicon_words_of(&p->lex, p->mind, t, n, ids, (uint32_t)n);
            for (uint32_t w = 0; w < p->lex.n; w++) hof[w] = -1;
            for (uint32_t i = 0; i < p->nhunch; i++) hof[p->hunch[i].word] = (int32_t)i;
            /* the base rate of each side in this very bite */
            uint32_t twins = 0, on[2] = { 0, 0 };
            for (uint32_t i = 0; i + 1 < nid; i++) {
                int32_t nx = ids[i + 1];
                if (nx < 0 || (uint32_t)nx >= p->lex.n || p->keys.side[nx] < 0) continue;
                twins++;
                on[p->keys.side[nx]]++;
            }
            double expect = 0;
            for (uint32_t i = 0; i + 1 < nid; i++) {
                int32_t m = ids[i], nx = ids[i + 1];
                if (m < 0 || nx < 0 || (uint32_t)nx >= p->lex.n || p->keys.side[nx] < 0) continue;
                int32_t hi = hof[m];
                if (hi < 0) continue;
                rz_hunch *hu = &p->hunch[hi];
                hu->tried++;
                int won = p->keys.side[nx] == hu->side;
                hu->right += (uint32_t)won;
                out->bets++;
                out->won += (uint32_t)won;
                expect += twins ? (double)on[hu->side] / twins : 0;
            }
            for (uint32_t i = 0; i < p->nhunch; i++) {
                p->hunch[i].base_tried += twins;
                p->hunch[i].base_right += on[p->hunch[i].side];
            }
            out->base_won = (uint32_t)(expect + 0.5);
        }
        free(ids);
        free(hof);
    }

    /* 2. read */
    rz_ingest(p->mind, t, n);
    rz_lexicon_read_words(&p->lex, p->mind, t, n);
    p->bites++;

    /* 3. think. The key it now hears is read again; then it thinks sideways
       before it looks at the markers -- a word the silhouette of a standing
       hunch points at is thought of before enough of its own uses have been
       seen for it to count as a marker */
    if (p->have_keys) rz_keys_free(&p->keys);
    p->have_keys = rz_lexicon_keys(&p->lex, &p->keys) == RZ_OK;
    if (!p->have_keys) return;
    /* lateral: a hunch that holds lends itself to its silhouette */
    uint32_t standing = p->nhunch;
    for (uint32_t i = 0; i < standing && p->nhunch < RZ_HUNCHES; i++) {
        rz_hunch *hu = &p->hunch[i];
        if (hu->lent || !rz_hunch_holds(hu)) continue;
        hu->lent = 1;
        int32_t sil[16];
        uint32_t k = rz_lexicon_silhouette(&p->lex, hu->word, sil, 16);
        for (uint32_t j = 0; j < k && p->nhunch < RZ_HUNCHES; j++) {
            if (hunch_of(p, sil[j]) >= 0) continue;
            p->hunch[p->nhunch++] = (rz_hunch){ sil[j], hu->side, (int32_t)i, p->bites, 0, 0, 0, 0, 0 };
            out->born++;
            out->lateral++;
        }
    }
    /* then the markers of the key become hunches */
    for (uint32_t m = 0; m < p->lex.n && p->nhunch < RZ_HUNCHES; m++) {
        if (p->keys.marks[m] != 0 && p->keys.marks[m] != 1) continue;
        if (hunch_of(p, (int32_t)m) >= 0) continue;
        p->hunch[p->nhunch++] = (rz_hunch){ (int32_t)m, p->keys.marks[m], -1, p->bites, 0, 0, 0, 0, 0 };
        out->born++;
    }
}


/* ===================== the family of two in the same chair (P46) ========= */

/* what a word is (forward answers said twice) and who is said to be it
   (backward answers said twice: names) */
static void what_it_is(rz_lexicon *x, int32_t w, const int32_t *rel, uint32_t nrel,
                       int32_t *is, uint32_t *nis, int32_t *names, uint32_t *nnames)
{
    rz_answer a[64];
    double sc[64];
    *nis = *nnames = 0;
    for (uint32_t r = 0; r < nrel; r++) {
        uint32_t k = ask_core(x, w, rel[r], a, 64, sc);
        for (uint32_t i = 0; i < k; i++) {
            if (sc[i] < 1e6) continue;                   /* said once: an anecdote */
            if (a[i].dir < 0) { if (*nnames < 64) names[(*nnames)++] = a[i].lego; }
            else if (*nis < 64) is[(*nis)++] = a[i].lego;
        }
    }
}

static int has(const int32_t *l, uint32_t n, int32_t w)
{
    for (uint32_t i = 0; i < n; i++) if (l[i] == w) return 1;
    return 0;
}

void rz_lexicon_family(rz_lexicon *x, int32_t a, int32_t b,
                       const int32_t *relations, uint32_t nrel, rz_family *out)
{
    memset(out, 0, sizeof *out);
    if (a < 0 || b < 0) return;
    int32_t ia[64], ib[64], na[64], nb[64];
    uint32_t nia, nib, nna, nnb;
    what_it_is(x, a, relations, nrel, ia, &nia, na, &nna);
    what_it_is(x, b, relations, nrel, ib, &nib, nb, &nnb);
    /* the others in the same chair: the silhouettes of both. What most of them
       are told as well is the chair ("here": the dog, the cat and the table);
       a first cut took the tall side of one word's own answers, and "animal",
       shared by two, fell under it */
    int32_t mates[32], sil[16];
    uint32_t nm = 0;
    for (int side2 = 0; side2 < 2; side2++) {
        uint32_t k = rz_lexicon_silhouette(x, side2 ? b : a, sil, 16);
        for (uint32_t i = 0; i < k; i++)
            if (sil[i] != a && sil[i] != b && !has(mates, nm, sil[i]) && nm < 32) mates[nm++] = sil[i];
    }
    uint32_t *told = calloc(x->n, sizeof *told);
    if (told) {
        for (uint32_t m = 0; m < nm; m++) {
            int32_t mi[64], mn[64];
            uint32_t nmi, nmn;
            what_it_is(x, mates[m], relations, nrel, mi, &nmi, mn, &nmn);
            for (uint32_t i = 0; i < nmi; i++) told[mi[i]]++;
        }
    }
    for (uint32_t i = 0; i < nia; i++) {
        int chair = told && nm && 2 * told[ia[i]] > nm;
        if (has(ib, nib, ia[i])) {
            if (!chair && out->nfamily < RZ_FAMILY && !has(out->family, out->nfamily, ia[i]))
                out->family[out->nfamily++] = ia[i];
        } else if (out->nonly_a < RZ_FAMILY && !has(out->only_a, out->nonly_a, ia[i]))
            out->only_a[out->nonly_a++] = ia[i];
    }
    free(told);
    for (uint32_t i = 0; i < nib; i++)
        if (!has(ia, nia, ib[i]) && out->nonly_b < RZ_FAMILY && !has(out->only_b, out->nonly_b, ib[i]))
            out->only_b[out->nonly_b++] = ib[i];
    for (uint32_t i = 0; i < nna; i++)
        if (has(nb, nnb, na[i]) && out->nnames < RZ_FAMILY && !has(out->names, out->nnames, na[i]))
            out->names[out->nnames++] = na[i];

    rz_keys k;
    int same_key = 0;
    if (rz_lexicon_keys(x, &k) == RZ_OK) { same_key = rz_keys_same(&k, a, b); rz_keys_free(&k); }
    out->kin = same_key ? RZ_KIN_KEY : out->nfamily ? RZ_KIN_SIBLINGS : RZ_KIN_CHAIR;
}


/* ===================== the map of associations (P47) ===================== */

static int is_marker(const rz_keys *k, int32_t w)
{
    return w >= 0 && k->marks && k->marks[w] >= 0;
}

static int by_link(const void *pa, const void *pb)
{
    const rz_link *a = pa, *b = pb;
    if (a->a != b->a) return a->a < b->a ? -1 : 1;
    if (a->r != b->r) return a->r < b->r ? -1 : 1;
    return a->b < b->b ? -1 : a->b > b->b;
}

static void add_link(rz_assoc *m, int32_t a, int32_t r, int32_t b)
{
    if (m->n == m->cap) {
        uint32_t cap = m->cap ? m->cap * 2 : 4096;
        rz_link *l = realloc(m->link, cap * sizeof *l);
        if (!l) return;
        m->link = l;
        m->cap = cap;
    }
    m->link[m->n++] = (rz_link){ a, r, b, 1, 0 };
}

uint32_t rz_assoc_build(rz_lexicon *x, const rz_keys *k, rz_assoc *m)
{
    memset(m, 0, sizeof *m);
    if (!k->marks || !x->nseq) return 0;

    /* A thing is a word the text puts after a marker most of the times it
       says it ("de Dios", "el perro"). The relation of a statement is never a
       thing: reading greedily from "porque", "porque Dios es amor" became
       porque --Dios--> es and ate the real statement. (Counted as a thing
       after a marker just twice, "es" was one -- it follows "que" and "rex",
       markers for both sides -- and no relation survived.) */
    uint8_t *thing = calloc(x->n, 1);
    uint32_t *after = calloc(x->n, sizeof *after);
    if (!thing || !after) { free(thing); free(after); return 0; }
    for (uint32_t i = 1; i < x->nseq; i++)
        if (x->seq[i] >= 0 && is_marker(k, x->seq[i - 1])) after[x->seq[i]]++;
    for (uint32_t w = 0; w < x->n; w++)
        thing[w] = after[w] >= 2 && 2 * after[w] > x->lego[w].count && !is_marker(k, (int32_t)w);
    free(after);

    /* An article: a marker of one side whose next word is mostly a thing
       ("una perra", "el gato"). It is neither what a statement is about nor its
       relation. Told first by having a partner in the other key, "una" -- left
       without one -- related: "then una perra" became then --una--> perra. A
       marker whose next word is mostly not a thing still relates: "es un",
       "es bueno". */
    uint8_t *article = calloc(x->n, 1);
    uint32_t *to_thing = calloc(x->n, sizeof *to_thing);
    if (!article || !to_thing) { free(thing); free(article); free(to_thing); return 0; }
    for (uint32_t i = 0; i + 1 < x->nseq; i++)
        if (x->seq[i] >= 0 && x->seq[i + 1] >= 0 && thing[x->seq[i + 1]]) to_thing[x->seq[i]]++;
    for (uint32_t w = 0; w < x->n; w++)
        article[w] = (k->marks[w] == 0 || k->marks[w] == 1) && 2 * to_thing[w] > x->lego[w].count;
    free(to_thing);

    /* Read left to right, as a person reads: a statement uses its words, and
       the next one starts after it. Read every position instead, "es un
       animal so el gato" also gave animal --so--> gato. */
    for (uint32_t i = 0; i + 1 < x->nseq; i++) {
        int32_t a = x->seq[i], r = x->seq[i + 1];
        if (a < 0 || r < 0) continue;
        /* an article is neither what a statement is about nor its relation:
           read so, "el perro ladra" became el --perro--> ladra */
        if (article[a] || article[r]) continue;
        if (thing[r]) continue;
        /* a statement: the thing, the word between, one or two markers, what
           it says it is -- "perro es un animal", "mesa es de madera". The
           thing may itself be a marker ("rex", when rex stands before both
           sides of a key) */
        int32_t m1 = at(x, i, 2), b = -1;
        uint32_t used = i + 1;
        if (is_marker(k, m1)) {
            int32_t c = at(x, i, 3);
            if (c >= 0 && !is_marker(k, c)) { b = c; used = i + 3; }
            else if (is_marker(k, c)) {
                int32_t d = at(x, i, 4);
                if (d >= 0 && !is_marker(k, d)) { b = d; used = i + 4; }
            }
        }
        /* or said bare, with no marker: "la madera es dura", "Dios es amor".
           What follows the relation is what it is, unless that word opens
           something new -- a marker stands after it ("vive so el perro": so
           begins the next). (Names go without an article in the NT: asking
           for a marker before the thing lost "Dios es amor".) */
        if (b < 0 && m1 >= 0 && !is_marker(k, m1) && m1 != a && !is_marker(k, at(x, i, 3))) {
            b = m1;
            used = i + 2;
        }
        if (b >= 0 && b != a) { add_link(m, a, r, b); i = used; continue; }
        /* an act: a thing -- not a marker, a marker before it -- and what it
           does, with nothing it is said to be: "el animal vive" */
        if (!is_marker(k, a) && i > 0 && is_marker(k, x->seq[i - 1]) && !is_marker(k, m1)) {
            add_link(m, a, r, -1);
            i += 1;
        }
    }
    free(thing);
    free(article);
    /* the same thing said again is one link said more times */
    qsort(m->link, m->n, sizeof *m->link, by_link);
    uint32_t w = 0;
    for (uint32_t i = 0; i < m->n; i++) {
        if (w && by_link(&m->link[w - 1], &m->link[i]) == 0) m->link[w - 1].said++;
        else m->link[w++] = m->link[i];
    }
    m->n = w;
    return m->n;
}

void rz_assoc_free(rz_assoc *m)
{
    free(m->link);
    memset(m, 0, sizeof *m);
}

/* the links about a: they are sorted by a */
static uint32_t links_of(const rz_assoc *m, int32_t a, uint32_t *end)
{
    uint32_t lo = 0, hi = m->n;
    while (lo < hi) { uint32_t mid = (lo + hi) / 2; if (m->link[mid].a < a) lo = mid + 1; else hi = mid; }
    uint32_t e = lo;
    while (e < m->n && m->link[e].a == a) e++;
    *end = e;
    return lo;
}

static int one_of(const int32_t *rel, uint32_t nrel, int32_t r)
{
    for (uint32_t i = 0; i < nrel; i++) if (rel[i] == r) return 1;
    return 0;
}

uint32_t rz_assoc_reach(const rz_assoc *m, int32_t start, const int32_t *rel, uint32_t nrel,
                        int32_t *out, uint32_t cap)
{
    /* walk the links of the question, said twice at least, as far as they go;
       what has been reached is not walked again */
    uint32_t n = 0, at2 = 0;
    int32_t queue[1024];
    uint32_t nq = 0;
    if (start < 0) return 0;
    queue[nq++] = start;
    while (at2 < nq) {
        int32_t node = queue[at2++];
        uint32_t end, i = links_of(m, node, &end);
        for (; i < end; i++) {
            const rz_link *l = &m->link[i];
            if (l->b < 0 || (l->said < 2 && !l->round) || !one_of(rel, nrel, l->r) || l->b == start) continue;
            int seen = 0;
            for (uint32_t q = 0; q < nq && !seen; q++) seen = queue[q] == l->b;
            if (seen || nq == 1024) continue;
            queue[nq++] = l->b;
            if (n < cap) out[n++] = l->b;
        }
    }
    return n;
}

uint32_t rz_assoc_traits(const rz_assoc *m, int32_t start, const int32_t *rel, uint32_t nrel,
                         rz_trait *out, uint32_t cap)
{
    int32_t up[256];
    uint32_t nu = rz_assoc_reach(m, start, rel, nrel, up, 256), n = 0;
    for (uint32_t s2 = 0; s2 <= nu; s2++) {
        int32_t node = s2 == 0 ? start : up[s2 - 1];
        uint32_t end, i = links_of(m, node, &end);
        for (; i < end && n < cap; i++) {
            const rz_link *l = &m->link[i];
            if (l->said < 2 && !l->round) continue;       /* said once: an anecdote */
            int dup = 0;
            for (uint32_t q = 0; q < n && !dup; q++) dup = out[q].r == l->r && out[q].b == l->b;
            if (!dup) out[n++] = (rz_trait){ l->r, l->b, node };
        }
    }
    return n;
}


/* ===================== the melody of the path (P48) ====================== */

/* a link that may be walked: said twice, or fallen in a domino round */
static int walkable(const rz_link *l)
{
    return l->b >= 0 && (l->said >= 2 || l->round > 0);
}

typedef struct { uint64_t key; uint32_t count; } tune_cell;
typedef struct { tune_cell *cell; uint32_t cap, used; } tune_table;

static uint64_t tune_key(int32_t r1, int32_t r2, int32_t r3)
{
    return ((uint64_t)(uint32_t)r1 << 42) | ((uint64_t)(uint32_t)r2 << 21) | (uint64_t)(uint32_t)(r3 & 0x1FFFFF);
}

static uint32_t *tune_slot(tune_table *t, uint64_t key)
{
    if ((t->used + 1) * 2 > t->cap) {
        uint32_t cap = t->cap ? t->cap * 2 : 4096;
        tune_cell *c = calloc(cap, sizeof *c);
        if (!c) return NULL;
        for (uint32_t i = 0; i < t->cap; i++) {
            if (!t->cell[i].count) continue;
            uint32_t h = (uint32_t)((t->cell[i].key * 0x9E3779B97F4A7C15ull) >> 40) & (cap - 1);
            while (c[h].count) h = (h + 1) & (cap - 1);
            c[h] = t->cell[i];
        }
        free(t->cell);
        t->cell = c;
        t->cap = cap;
    }
    uint32_t h = (uint32_t)((key * 0x9E3779B97F4A7C15ull) >> 40) & (t->cap - 1);
    while (t->cell[h].count && t->cell[h].key != key) h = (h + 1) & (t->cap - 1);
    if (!t->cell[h].count) { t->cell[h].key = key; t->used++; }
    return &t->cell[h].count;
}

static uint32_t tune_get(const tune_table *t, uint64_t key)
{
    if (!t->cap) return 0;
    uint32_t h = (uint32_t)((key * 0x9E3779B97F4A7C15ull) >> 40) & (t->cap - 1);
    while (t->cell[h].count) {
        if (t->cell[h].key == key) return t->cell[h].count;
        h = (h + 1) & (t->cap - 1);
    }
    return 0;
}

#define RZ_PATH_BUDGET 20000000u   /* walks counted, at most: a hub of glue can square the work */

uint32_t rz_assoc_tunes(const rz_assoc *m, rz_tune *out, uint32_t cap)
{
    tune_table paths = { 0 }, heard = { 0 };
    uint64_t walked = 0;
    for (uint32_t i = 0; i < m->n && walked < RZ_PATH_BUDGET; i++) {
        const rz_link *l1 = &m->link[i];
        if (!walkable(l1)) continue;
        uint32_t e2, j = links_of(m, l1->b, &e2);
        uint32_t ea, ia = links_of(m, l1->a, &ea);
        for (; j < e2 && walked < RZ_PATH_BUDGET; j++) {
            const rz_link *l2 = &m->link[j];
            if (!walkable(l2) || l2->b == l1->a) continue;
            walked++;
            uint32_t *p = tune_slot(&paths, tune_key(l1->r, l2->r, 0x1FFFFF));
            if (p) (*p)++;
            /* the shortcut said straight out, twice: a --r3--> c */
            for (uint32_t q = ia; q < ea; q++) {
                const rz_link *l3 = &m->link[q];
                if (l3->b != l2->b || l3->said < 2 || l3->round) continue;
                uint32_t *h = tune_slot(&heard, tune_key(l1->r, l2->r, l3->r));
                if (h) (*h)++;
            }
        }
    }
    /* heard twice: learned */
    uint32_t n = 0;
    for (uint32_t i = 0; i < heard.cap && n < cap; i++) {
        if (heard.cell[i].count < 2) continue;
        uint64_t k = heard.cell[i].key;
        int32_t r1 = (int32_t)(k >> 42), r2 = (int32_t)((k >> 21) & 0x1FFFFF), r3 = (int32_t)(k & 0x1FFFFF);
        out[n++] = (rz_tune){ r1, r2, r3, tune_get(&paths, tune_key(r1, r2, 0x1FFFFF)), heard.cell[i].count };
    }
    for (uint32_t i = 1; i < n; i++) {
        rz_tune v = out[i];
        uint32_t j = i;
        while (j > 0 && out[j - 1].heard < v.heard) { out[j] = out[j - 1]; j--; }
        out[j] = v;
    }
    free(paths.cell);
    free(heard.cell);
    return n;
}

static int has_link(const rz_assoc *m, int32_t a, int32_t r, int32_t b)
{
    uint32_t e, i = links_of(m, a, &e);
    for (; i < e; i++) if (m->link[i].r == r && m->link[i].b == b) return 1;
    return 0;
}

uint32_t rz_assoc_dominoes(rz_assoc *m, const rz_tune *t, uint32_t nt, uint32_t max_new,
                           uint32_t *rounds)
{
    uint32_t fell = 0;
    *rounds = 0;
    for (uint16_t round = 1; fell < max_new; round++) {
        /* everything this round lets fall, from the links standing now */
        rz_link *fall = NULL;
        uint32_t nf = 0, cf = 0;
        for (uint32_t ti = 0; ti < nt; ti++) {
            for (uint32_t i = 0; i < m->n && fell + nf < max_new; i++) {
                const rz_link *l1 = &m->link[i];
                if (!walkable(l1) || l1->r != t[ti].r1) continue;
                uint32_t e2, j = links_of(m, l1->b, &e2);
                for (; j < e2 && fell + nf < max_new; j++) {
                    const rz_link *l2 = &m->link[j];
                    if (!walkable(l2) || l2->r != t[ti].r2 || l2->b == l1->a) continue;
                    if (has_link(m, l1->a, t[ti].r3, l2->b)) continue;
                    int dup = 0;
                    for (uint32_t q = 0; q < nf && !dup; q++)
                        dup = fall[q].a == l1->a && fall[q].r == t[ti].r3 && fall[q].b == l2->b;
                    if (dup) continue;
                    if (nf == cf) {
                        cf = cf ? cf * 2 : 256;
                        rz_link *g = realloc(fall, cf * sizeof *g);
                        if (!g) { free(fall); return fell; }
                        fall = g;
                    }
                    fall[nf++] = (rz_link){ l1->a, t[ti].r3, l2->b, 0, round };
                }
            }
        }
        if (nf == 0) { free(fall); break; }
        for (uint32_t q = 0; q < nf; q++) {
            if (m->n == m->cap) {
                uint32_t cap = m->cap ? m->cap * 2 : 4096;
                rz_link *l = realloc(m->link, cap * sizeof *l);
                if (!l) break;
                m->link = l;
                m->cap = cap;
            }
            m->link[m->n++] = fall[q];
        }
        free(fall);
        qsort(m->link, m->n, sizeof *m->link, by_link);
        fell += nf;
        *rounds = round;
    }
    return fell;
}

/* ===================== asking every word (P50) ============================ */

enum { ASK_OF = 1, ASK_ABOUT = 2, ASK_RELATES = 4, ASK_BEFORE = 8, ASK_AFTER = 16 };
#define ASK_MAP (ASK_OF | ASK_ABOUT | ASK_RELATES)
#define ASK_SHAPES 32

static int asked(const rz_lexicon *x, int32_t w)
{
    return w >= 0 && (uint32_t)w < x->n && x->lego[w].count >= 2;
}

uint32_t rz_ask_every_word(const rz_lexicon *x, const rz_assoc *m, rz_answers *out)
{
    memset(out, 0, x->n * sizeof *out);
    /* what the map says, each link said twice: an answer said once is an
       anecdote; fallen links (P48) are thoughts, not answers */
    for (uint32_t i = 0; i < m->n; i++) {
        const rz_link *l = &m->link[i];
        if (l->said < 2 || l->round) continue;
        if (asked(x, l->a)) out[l->a].of++;
        if (asked(x, l->b)) out[l->b].about++;
        if (asked(x, l->r)) out[l->r].relates++;
    }
    /* the company it keeps, each neighbour met twice */
    for (uint32_t i = 0; i < x->npair; i++) {
        const rz_lego_pair *p = &x->pair[i];
        if (p->count < 2) continue;
        if (asked(x, (int32_t)p->a)) out[p->a].before++;
        if (asked(x, (int32_t)p->b)) out[p->b].after++;
    }
    uint32_t words = 0;
    for (uint32_t w = 0; w < x->n; w++) {
        if (!asked(x, (int32_t)w)) continue;
        rz_answers *a = &out[w];
        a->shape = (uint8_t)((a->of ? ASK_OF : 0) | (a->about ? ASK_ABOUT : 0) |
                             (a->relates ? ASK_RELATES : 0) |
                             (a->before ? ASK_BEFORE : 0) | (a->after ? ASK_AFTER : 0));
        words++;
    }
    return words;
}

uint32_t rz_used_together(const rz_lexicon *x, const rz_answers *ans, rz_together *out, uint32_t cap)
{
    /* every pair of shapes side by side; heard by one pair of words it is
       that pair, heard by several it is the shapes */
    static rz_together all[ASK_SHAPES * ASK_SHAPES];
    memset(all, 0, sizeof all);
    for (uint32_t i = 0; i < x->npair; i++) {
        const rz_lego_pair *p = &x->pair[i];
        if (p->count < 2 || !asked(x, (int32_t)p->a) || !asked(x, (int32_t)p->b)) continue;
        uint8_t s1 = ans[p->a].shape, s2 = ans[p->b].shape;
        rz_together *t = &all[s1 * ASK_SHAPES + s2];
        t->s1 = s1; t->s2 = s2;
        t->heard += p->count;
        t->pairs++;
    }
    uint32_t k = 0;
    for (uint32_t i = 0; i < ASK_SHAPES * ASK_SHAPES; i++) {
        if (all[i].pairs < 2) continue;
        uint32_t j = k < cap ? k++ : cap;
        if (j == cap) {
            if (cap == 0 || all[i].heard <= out[cap - 1].heard) continue;
            j = cap - 1;
        }
        while (j > 0 && out[j - 1].heard < all[i].heard) { out[j] = out[j - 1]; j--; }
        out[j] = all[i];
    }
    return k;
}

int rz_one_piece(const rz_answers *a, const rz_answers *b)
{
    /* one adds nothing to the map the other lacks, and the other adds
       something: the pair says what the fuller one says. Two that say the
       same are twins, not one piece. */
    uint8_t ma = a->shape & ASK_MAP, mb = b->shape & ASK_MAP;
    if (ma == mb) return 0;
    return (ma & mb) == ma || (ma & mb) == mb;
}

uint32_t rz_lexicon_pair_count(const rz_lexicon *x, int32_t a, int32_t b)
{
    if (a < 0 || b < 0) return 0;
    int32_t p = find_pair(x, (uint32_t)a, (uint32_t)b);
    return p < 0 ? 0 : x->pair[p].count;
}

/* ===================== the silhouette of a path (P51) ===================== */

#define RUN_MIN 3
#define RUN_MAX 5
/* Pairs of mates are counted in a table of fixed size; past half of it, new
   pairs are no longer counted (a budget, like the paths of P48). */
#define MATE_SLOTS (1u << 22)

typedef struct { uint64_t key; int32_t f1, f2; uint32_t at; uint8_t len, h1, h2; } path_rec;

static int rec_order(const void *pa, const void *pb)
{
    const path_rec *a = pa, *b = pb;
    if (a->key != b->key) return a->key < b->key ? -1 : 1;
    if (a->f1 != b->f1) return a->f1 < b->f1 ? -1 : 1;
    if (a->f2 != b->f2) return a->f2 < b->f2 ? -1 : 1;
    return 0;
}

static uint64_t path_key(const int32_t *run, int len, int h1, int h2)
{
    uint64_t v = 1469598103934665603ull ^ (uint64_t)(len * 64 + h1 * 8 + h2);
    for (int q = 0; q < len; q++) {
        uint64_t w = (q == h1 || q == h2) ? 0xFFFFFFFFull : (uint32_t)run[q];
        v ^= w; v *= 1099511628211ull; v ^= v >> 31;
    }
    return v ? v : 1;
}

typedef struct { int32_t a, b; uint32_t frames, stamp; } mate_slot;

static void mate_add(mate_slot *t, uint32_t *used, int32_t a, int32_t b, uint32_t stamp)
{
    if (a == b) return;
    if (a > b) { int32_t c = a; a = b; b = c; }
    uint32_t s = pair_hash((uint32_t)a, (uint32_t)b) & (MATE_SLOTS - 1);
    while (t[s].frames && !(t[s].a == a && t[s].b == b)) s = (s + 1) & (MATE_SLOTS - 1);
    if (!t[s].frames) {
        if (*used >= MATE_SLOTS / 2) return;
        (*used)++;
        t[s].a = a; t[s].b = b;
    } else if (t[s].stamp == stamp) {
        return;                       /* once per silhouette */
    }
    t[s].frames++;
    t[s].stamp = stamp;
}

static int by_f1(const void *pa, const void *pb)
{
    const rz_fill *a = pa, *b = pb;
    if (a->f1 != b->f1) return a->f1 < b->f1 ? -1 : 1;
    return (a->f2 > b->f2) - (a->f2 < b->f2);
}

static int by_f2(const void *pa, const void *pb)
{
    const rz_fill *a = pa, *b = pb;
    if (a->f2 != b->f2) return a->f2 < b->f2 ? -1 : 1;
    return (a->f1 > b->f1) - (a->f1 < b->f1);
}

/* the mates in one silhouette: fills that share the piece in the other hole.
   side 0: the partner is the first hole, the mates fill the second */
static void mates_in(const rz_fill *f, uint32_t n, int side, mate_slot *t, uint32_t *used,
                     const uint32_t *stamp, rz_fill *tmp)
{
    memcpy(tmp, f, n * sizeof *f);
    qsort(tmp, n, sizeof *tmp, side ? by_f2 : by_f1);
    for (uint32_t i = 0; i < n; ) {
        int32_t partner = side ? tmp[i].f2 : tmp[i].f1;
        uint32_t e = i;
        while (e < n && (side ? tmp[e].f2 : tmp[e].f1) == partner) e++;
        for (uint32_t a = i; a < e; a++)
            for (uint32_t b = a + 1; b < e; b++)
                mate_add(t, used, side ? tmp[a].f1 : tmp[a].f2, side ? tmp[b].f1 : tmp[b].f2, *stamp);
        i = e;
    }
}


/* --- stickers: silhouettes joined by their variants ------------------------
 *
 * The author: a collector keeps one of each sticker -- a repeated one is no
 * use -- and goes for the new ones; inside each sticker are its variants,
 * modulations of the same song, not another song. Counted as silhouettes,
 * "_ , que", "_ , por", "_ , no"... were 3,519 forms for the mates "y" and ",":
 * one sticker sung in 3,519 keys.
 *
 * Nothing says what a variant is. Two silhouettes of the same shape that
 * differ in one fixed piece are variants when those two pieces are mates --
 * and mates are counted by stickers. So it turns: mates join silhouettes into
 * stickers, stickers count the mates again, until a round joins nothing. Joins
 * are never undone, so it ends. The only number is the anecdote rule (mates
 * met in two stickers), the same as everywhere. */

static uint32_t uf_find(uint32_t *par, uint32_t a)
{
    while (par[a] != a) { par[a] = par[par[a]]; a = par[a]; }
    return a;
}

static const rz_silhouette *sticker_of;

static int by_sticker(const void *pa, const void *pb)
{
    uint32_t a = *(const uint32_t *)pa, b = *(const uint32_t *)pb;
    if (sticker_of[a].sticker != sticker_of[b].sticker) return sticker_of[a].sticker < sticker_of[b].sticker ? -1 : 1;
    return (a > b) - (a < b);
}

/* the mates, counted once per sticker: silhouettes visited sticker by sticker */
static void count_mates(rz_paths *p, const uint32_t *order, mate_slot *t, rz_fill *tmp)
{
    memset(t, 0, MATE_SLOTS * sizeof *t);
    free(p->m);
    p->m = NULL;
    p->nm = 0;
    uint32_t used = 0, stamp = 0, last = UINT32_MAX;
    for (uint32_t o = 0; o < p->ns; o++) {
        uint32_t i = order[o];
        if (p->s[i].sticker != last) { stamp++; last = p->s[i].sticker; }
        const rz_fill *f = p->fill + p->s[i].first;
        mates_in(f, p->s[i].count, 0, t, &used, &stamp, tmp);
        mates_in(f, p->s[i].count, 1, t, &used, &stamp, tmp);
    }
    uint32_t k = 0;
    for (uint32_t q = 0; q < MATE_SLOTS; q++) if (t[q].frames >= 2) k++;
    p->m = malloc((k + 1) * sizeof *p->m);
    if (!p->m) return;
    for (uint32_t q = 0; q < MATE_SLOTS; q++)
        if (t[q].frames >= 2) {
            p->m[p->nm].a = t[q].a; p->m[p->nm].b = t[q].b; p->m[p->nm].frames = t[q].frames;
            p->nm++;
        }
}

typedef struct { uint64_t key; int32_t piece; uint32_t sil; } variant_at;

static int variant_order(const void *pa, const void *pb)
{
    const variant_at *a = pa, *b = pb;
    if (a->key != b->key) return a->key < b->key ? -1 : 1;
    if (a->piece != b->piece) return a->piece < b->piece ? -1 : 1;
    return (a->sil > b->sil) - (a->sil < b->sil);
}

/* silhouettes of the same shape, alike but for one fixed piece, where the two
   pieces are mates: one sticker. Returns the joins made. */
static uint32_t join_variants(rz_paths *p, uint32_t *par, int32_t *keep)
{
    uint32_t *start = calloc((size_t)p->nwords + 1, sizeof *start);
    uint32_t *pos = malloc(((size_t)p->nwords + 1) * sizeof *pos);
    int32_t *mate = malloc(((size_t)p->nm * 2 + 1) * sizeof *mate);
    size_t ne = 0;
    for (uint32_t i = 0; i < p->ns; i++)
        for (int q = 0; q < p->s[i].len; q++) ne += p->s[i].word[q] >= 0;
    variant_at *v = malloc((ne + 1) * sizeof *v);
    if (!start || !pos || !mate || !v) { free(start); free(pos); free(mate); free(v); return 0; }
    for (uint32_t i = 0; i < p->nm; i++) { start[p->m[i].a + 1]++; start[p->m[i].b + 1]++; }
    for (uint32_t w = 0; w < p->nwords; w++) start[w + 1] += start[w];
    memcpy(pos, start, ((size_t)p->nwords + 1) * sizeof *pos);
    for (uint32_t i = 0; i < p->nm; i++) { mate[pos[p->m[i].a]++] = p->m[i].b; mate[pos[p->m[i].b]++] = p->m[i].a; }
    free(pos);

    /* every silhouette, once per fixed piece, keyed by everything but that piece */
    size_t k = 0;
    for (uint32_t i = 0; i < p->ns; i++) {
        const rz_silhouette *sl = &p->s[i];
        for (int q = 0; q < sl->len; q++) {
            if (sl->word[q] < 0) continue;
            uint64_t h = 1469598103934665603ull ^ (uint64_t)(sl->len * 8 + q);
            for (int z = 0; z < sl->len; z++) {
                uint64_t w = z == q ? 0xFFFFFFFEull : sl->word[z] < 0 ? 0xFFFFFFFFull : (uint32_t)sl->word[z];
                h ^= w; h *= 1099511628211ull; h ^= h >> 31;
            }
            v[k].key = h; v[k].piece = sl->word[q]; v[k].sil = i;
            k++;
        }
    }
    qsort(v, k, sizeof *v, variant_order);

    uint32_t joined = 0;
    for (size_t g = 0; g < k; ) {
        size_t e = g;
        while (e < k && v[e].key == v[g].key) e++;
        for (size_t a = g; a < e; a++) {
            int32_t w = v[a].piece;
            for (uint32_t q = start[w]; q < start[w + 1]; q++) {
                /* is the mate standing in this place, in a silhouette alike? */
                size_t lo = g, hi = e;
                while (lo < hi) { size_t mid = (lo + hi) / 2; if (v[mid].piece < mate[q]) lo = mid + 1; else hi = mid; }
                if (lo < e && v[lo].piece == mate[q]) {
                    uint32_t ra = uf_find(par, v[a].sil), rb = uf_find(par, v[lo].sil);
                    if (ra == rb) continue;
                    /* the structure is what every variant keeps (the author:
                       first the meaning of the structure, then what it
                       carries). Two stickers join only if they still keep a
                       piece in the same place; joined one swap at a time, a
                       chain drifted from "_ , que _" to "_ de aceite _" and
                       one sticker swallowed 10,004 silhouettes */
                    int32_t both[5];
                    int any = 0;
                    for (int z = 0; z < 5; z++) {
                        both[z] = keep[(size_t)ra * 5 + z] == keep[(size_t)rb * 5 + z] ? keep[(size_t)ra * 5 + z] : -1;
                        any |= both[z] >= 0;
                    }
                    if (!any) continue;
                    par[ra] = rb;
                    memcpy(keep + (size_t)rb * 5, both, sizeof both);
                    joined++;
                }
            }
        }
        g = e;
    }
    free(start); free(mate); free(v);
    return joined;
}

uint32_t rz_paths_build(const rz_lexicon *x, rz_paths *p)
{
    memset(p, 0, sizeof *p);
    p->nwords = x->n;
    p->fixed = calloc(x->n ? x->n : 1, sizeof *p->fixed);
    p->filled = calloc(x->n ? x->n : 1, sizeof *p->filled);
    if (!p->fixed || !p->filled) return 0;

    /* every run of 3 to 5 pieces, every way of leaving two holes in it */
    size_t per = 0;
    for (int len = RUN_MIN; len <= RUN_MAX; len++) per += (size_t)len * (len - 1) / 2;
    size_t cap = (size_t)x->nseq * per + 1, nr = 0;
    path_rec *r = malloc(cap * sizeof *r);
    if (!r) return 0;
    for (uint32_t i = 0; i < x->nseq; i++) {
        for (int len = RUN_MIN; len <= RUN_MAX && i + (uint32_t)len <= x->nseq; len++) {
            const int32_t *run = x->seq + i;
            int broken = 0;
            for (int q = 0; q < len; q++) if (run[q] < 0) broken = 1;
            if (broken) break;
            for (int h1 = 0; h1 < len; h1++)
                for (int h2 = h1 + 1; h2 < len; h2++) {
                    path_rec *e = &r[nr++];
                    e->key = path_key(run, len, h1, h2);
                    e->f1 = run[h1]; e->f2 = run[h2];
                    e->at = i; e->len = (uint8_t)len; e->h1 = (uint8_t)h1; e->h2 = (uint8_t)h2;
                }
        }
    }
    qsort(r, nr, sizeof *r, rec_order);

    /* the silhouettes heard with two different pairs in their holes */
    uint32_t scap = 1024, fcap = 4096;
    p->s = malloc(scap * sizeof *p->s);
    p->fill = malloc(fcap * sizeof *p->fill);
    uint32_t *seen = calloc(x->n ? x->n : 1, sizeof *seen), *held = calloc(x->n ? x->n : 1, sizeof *held);
    if (!p->s || !p->fill || !seen || !held) { free(r); free(seen); free(held); return 0; }
    for (size_t i = 0; i < nr; ) {
        size_t e = i;
        uint32_t distinct = 0;
        while (e < nr && r[e].key == r[i].key) {
            if (e == i || r[e].f1 != r[e - 1].f1 || r[e].f2 != r[e - 1].f2) distinct++;
            e++;
        }
        if (distinct >= 2) {
            if (p->ns == scap) {
                scap *= 2;
                rz_silhouette *ns = realloc(p->s, scap * sizeof *p->s);
                if (!ns) break;
                p->s = ns;
            }
            while (p->nfill + distinct > fcap) {
                fcap *= 2;
                rz_fill *nf = realloc(p->fill, fcap * sizeof *p->fill);
                if (!nf) { free(r); free(seen); free(held); return p->ns; }
                p->fill = nf;
            }
            rz_silhouette *s = &p->s[p->ns];
            uint32_t mark = p->ns + 1;
            s->len = r[i].len;
            for (int q = 0; q < 5; q++) s->word[q] = -1;
            for (int q = 0; q < s->len; q++)
                if (q != r[i].h1 && q != r[i].h2) s->word[q] = x->seq[r[i].at + (uint32_t)q];
            s->fills = distinct;
            s->said = (uint32_t)(e - i);
            s->first = p->nfill;
            s->count = distinct;
            for (size_t k = i; k < e; k++) {
                if (k == i || r[k].f1 != r[k - 1].f1 || r[k].f2 != r[k - 1].f2) {
                    rz_fill *f = &p->fill[p->nfill++];
                    f->f1 = r[k].f1; f->f2 = r[k].f2; f->said = 0;
                }
                p->fill[p->nfill - 1].said++;
                if (seen[r[k].f1] != mark) { seen[r[k].f1] = mark; p->filled[r[k].f1]++; }
                if (seen[r[k].f2] != mark) { seen[r[k].f2] = mark; p->filled[r[k].f2]++; }
            }
            for (int q = 0; q < s->len; q++)
                if (s->word[q] >= 0 && held[s->word[q]] != mark) {
                    held[s->word[q]] = mark;
                    p->fixed[s->word[q]]++;
                }
            p->ns++;
        }
        i = e;
    }
    free(r);
    free(seen);
    free(held);

    /* mates: pieces that filled the same hole beside the same piece, counted
       once per sticker; at first every silhouette is its own sticker */
    mate_slot *t = calloc(MATE_SLOTS, sizeof *t);
    rz_fill *tmp = malloc((p->nfill + 1) * sizeof *tmp);
    uint32_t *par = malloc(((size_t)p->ns + 1) * sizeof *par);
    uint32_t *order = malloc(((size_t)p->ns + 1) * sizeof *order);
    uint32_t *id = malloc(((size_t)p->ns + 1) * sizeof *id);
    int32_t *keep = malloc(((size_t)p->ns + 1) * 5 * sizeof *keep);
    if (t && tmp && par && order && id && keep) {
        for (uint32_t i = 0; i < p->ns; i++) {
            par[i] = i; order[i] = i; p->s[i].sticker = i;
            memcpy(keep + (size_t)i * 5, p->s[i].word, 5 * sizeof *keep);
        }
        p->nsticker = p->ns;
        count_mates(p, order, t, tmp);
        /* until a round joins nothing */
        while (join_variants(p, par, keep)) {
            for (uint32_t i = 0; i < p->ns; i++) id[i] = UINT32_MAX;
            p->nsticker = 0;
            for (uint32_t i = 0; i < p->ns; i++) {
                uint32_t r = uf_find(par, i);
                if (id[r] == UINT32_MAX) id[r] = p->nsticker++;
                p->s[i].sticker = id[r];
            }
            for (uint32_t i = 0; i < p->ns; i++) order[i] = i;
            sticker_of = p->s;
            qsort(order, p->ns, sizeof *order, by_sticker);
            count_mates(p, order, t, tmp);
        }
    }
    free(t); free(tmp); free(par); free(order); free(id); free(keep);
    return p->ns;
}

void rz_paths_free(rz_paths *p)
{
    free(p->s); free(p->fill); free(p->m); free(p->fixed); free(p->filled);
    memset(p, 0, sizeof *p);
}

static int fill_said(const rz_fill *f, uint32_t n, int32_t f1, int32_t f2)
{
    uint32_t lo = 0, hi = n;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (f[mid].f1 < f1 || (f[mid].f1 == f1 && f[mid].f2 < f2)) lo = mid + 1;
        else hi = mid;
    }
    return lo < n && f[lo].f1 == f1 && f[lo].f2 == f2;
}

static int stronger(const void *pa, const void *pb)
{
    const rz_mates *a = pa, *b = pb;
    if (a->frames != b->frames) return a->frames > b->frames ? -1 : 1;
    if (a->a != b->a) return a->a < b->a ? -1 : 1;
    return (a->b > b->b) - (a->b < b->b);
}

typedef struct { uint32_t sil, fill; uint8_t side; } fill_at;

uint32_t rz_paths_recompose(const rz_paths *p, rz_recomposed *out, uint32_t cap)
{
    /* The association that collected the most different forms puts paths
       together first (the author); weighing by the silhouette crowned glue,
       "_ , _" put together with thousands of pairs. Every place a piece
       filled, gathered by piece */
    uint32_t *start = calloc((size_t)p->nwords + 1, sizeof *start);
    fill_at *at = malloc(((size_t)p->nfill * 2 + 1) * sizeof *at);
    uint32_t *pos = malloc(((size_t)p->nwords + 1) * sizeof *pos);
    rz_mates *m = malloc(((size_t)p->nm + 1) * sizeof *m);
    size_t hcap = 1;
    while (hcap < (size_t)cap * 2 + 2) hcap <<= 1;
    uint64_t *made = calloc(hcap, sizeof *made);
    if (!start || !at || !pos || !m || !made) { free(start); free(at); free(pos); free(m); free(made); return 0; }
    for (uint32_t s = 0; s < p->ns; s++)
        for (uint32_t i = 0; i < p->s[s].count; i++) {
            const rz_fill *f = &p->fill[p->s[s].first + i];
            start[f->f1 + 1]++;
            start[f->f2 + 1]++;
        }
    for (uint32_t w = 0; w < p->nwords; w++) start[w + 1] += start[w];
    memcpy(pos, start, ((size_t)p->nwords + 1) * sizeof *pos);
    for (uint32_t s = 0; s < p->ns; s++)
        for (uint32_t i = 0; i < p->s[s].count; i++) {
            const rz_fill *f = &p->fill[p->s[s].first + i];
            at[pos[f->f1]++] = (fill_at){ s, i, 0 };
            at[pos[f->f2]++] = (fill_at){ s, i, 1 };
        }
    free(pos);
    memcpy(m, p->m, (size_t)p->nm * sizeof *m);
    qsort(m, p->nm, sizeof *m, stronger);

    uint32_t k = 0;
    for (uint32_t q = 0; q < p->nm && k < cap; q++)
        for (int dir = 0; dir < 2 && k < cap; dir++) {
            int32_t was = dir ? m[q].b : m[q].a, now = dir ? m[q].a : m[q].b;
            for (uint32_t o = start[was]; o < start[was + 1] && k < cap; o++) {
                const rz_silhouette *sl = &p->s[at[o].sil];
                const rz_fill *f = p->fill + sl->first;
                const rz_fill *here = &f[at[o].fill];
                int32_t f1 = at[o].side ? here->f1 : now, f2 = at[o].side ? now : here->f2;
                if (fill_said(f, sl->count, f1, f2)) continue;
                uint64_t key = ((uint64_t)at[o].sil * 0x9E3779B97F4A7C15ull) ^
                               ((uint64_t)(uint32_t)f1 << 32 | (uint32_t)f2);
                if (!key) key = 1;
                size_t h = (size_t)(key ^ (key >> 29)) & (hcap - 1);
                while (made[h] && made[h] != key) h = (h + 1) & (hcap - 1);
                if (made[h]) continue;
                made[h] = key;
                out[k].sil = at[o].sil; out[k].f1 = f1; out[k].f2 = f2;
                out[k].from = was; out[k].forms = m[q].frames; out[k].hole = at[o].side;
                k++;
            }
        }
    free(start); free(at); free(m); free(made);
    return k;
}

/* ===================== weaving meaning while reading (P52) ================= */

/* Every piece is known by the hash of its spelling, so the loom is the same
   memory whatever book is read: an id belongs to one reading, a spelling to
   all of them. */
typedef struct { uint64_t key, f1, f2; uint8_t born; } loom_sil;
typedef struct { uint64_t key, sil, partner; int32_t head; uint32_t size; uint8_t side; } loom_group;
typedef struct { uint64_t word; int32_t next; } loom_member;
typedef struct { uint64_t key, last, a, b; uint32_t count, traveled; uint8_t landed; } loom_pair;
typedef struct { uint64_t key; uint8_t len; unsigned char s[RZ_LEGO_SPAN]; } loom_word;
/* the shape of a born silhouette, to say its paths: the fixed pieces, holes 0 */
typedef struct { uint64_t key; uint64_t path[5]; uint8_t len, h1, h2; } loom_shape;
/* an idea (P53): a word that could stand in a group; blind twins apart */
typedef struct { uint64_t key, word, group, via; uint32_t traveled; uint8_t level, blind, confirmed; } loom_idea;
/* the cache of each path walked (P53): a word, and who reached it */
typedef struct { uint64_t word; int32_t parent; uint8_t level; } loom_step;
/* an associate of a word, gathered to walk or to compare */
typedef struct { uint64_t word; uint64_t other; } loom_edge;
/* what a read needs to judge enrichment: the meanings held before the book */
typedef struct {
    loom_edge        *held;  size_t nheld;
    const uint64_t   *h;
    const rz_lexicon *x;
    uint32_t          at;
    uint64_t          rng;
    rz_loom_report   *out;
} loom_reading;

struct rz_loom {
    loom_sil    *sil;   size_t sil_cap, sil_n;
    loom_group  *grp;   size_t grp_cap, grp_n;
    loom_member *mem;   size_t mem_n, mem_cap;
    loom_pair   *pair;  size_t pair_cap, pair_n;
    loom_word   *word;  size_t word_cap, word_n;
    loom_shape  *shape; size_t shape_cap, shape_n;
    loom_idea   *idea;  size_t idea_cap, idea_n;
    loom_step   *trail; size_t trail_n, trail_cap, front;   /* front: where the last depth begins */
    uint64_t     rng;
    loom_reading *reading;               /* while a book is read */
};

static void loom_enrich(rz_loom *l, uint64_t a, uint64_t b, loom_pair *p);
static int edge_order(const void *pa, const void *pb);

static uint64_t mix64(uint64_t v)
{
    v ^= v >> 33; v *= 0xff51afd7ed558ccdull; v ^= v >> 33; v *= 0xc4ceb9fe1a85ec53ull; v ^= v >> 33;
    return v ? v : 1;
}

static uint64_t spelling_key(const unsigned char *s, size_t len)
{
    uint64_t v = 1469598103934665603ull;
    for (size_t i = 0; i < len; i++) { v ^= s[i]; v *= 1099511628211ull; }
    return mix64(v);
}

/* open tables of 64-bit keys that double when half full; the key is the
   first field of every entry */
static void *loom_grow(void *tab, size_t *cap, size_t size)
{
    size_t ncap = *cap ? *cap * 2 : 1024;
    unsigned char *n = calloc(ncap, size), *o = tab;
    if (!n) return NULL;
    for (size_t i = 0; i < *cap; i++) {
        uint64_t k;
        memcpy(&k, o + i * size, sizeof k);
        if (!k) continue;
        size_t s = (size_t)k & (ncap - 1);
        for (;;) {
            uint64_t q;
            memcpy(&q, n + s * size, sizeof q);
            if (!q) break;
            s = (s + 1) & (ncap - 1);
        }
        memcpy(n + s * size, o + i * size, size);
    }
    free(tab);
    *cap = ncap;
    return n;
}

#define LOOM_SLOT(tab, cap, kv, s) \
    do { (s) = (size_t)(kv) & ((cap) - 1); \
         while ((tab)[s].key && (tab)[s].key != (kv)) (s) = ((s) + 1) & ((cap) - 1); } while (0)

/* the entry for a key in a growing table, made if absent; NULL if out of memory */
#define LOOM_AT(l, tab, cap, n, kv, out, fresh) \
    do { (out) = NULL; (fresh) = 0; \
         if (((n) + 1) * 2 > (cap)) { void *t_ = loom_grow((tab), &(cap), sizeof *(tab)); if (!t_) break; (tab) = t_; } \
         size_t s_; LOOM_SLOT((tab), (cap), (kv), s_); \
         if (!(tab)[s_].key) { (tab)[s_].key = (kv); (n)++; (fresh) = 1; } \
         (out) = &(tab)[s_]; } while (0)

static loom_sil *loom_sil_at(rz_loom *l, uint64_t key)
{
    loom_sil *e; int fresh;
    LOOM_AT(l, l->sil, l->sil_cap, l->sil_n, key, e, fresh);
    (void)fresh;
    return e;
}

static loom_group *loom_group_at(rz_loom *l, uint64_t key, uint64_t sil, int side, uint64_t partner)
{
    loom_group *g; int fresh;
    LOOM_AT(l, l->grp, l->grp_cap, l->grp_n, key, g, fresh);
    if (g && fresh) { g->head = -1; g->size = 0; g->sil = sil; g->side = (uint8_t)side; g->partner = partner; }
    return g;
}

static const loom_group *loom_group_find(const rz_loom *l, uint64_t key)
{
    if (!l->grp_cap) return NULL;
    size_t s;
    LOOM_SLOT(l->grp, l->grp_cap, key, s);
    return l->grp[s].key ? &l->grp[s] : NULL;
}

static void loom_spell(rz_loom *l, uint64_t key, const unsigned char *s0, uint8_t len)
{
    loom_word *w; int fresh;
    LOOM_AT(l, l->word, l->word_cap, l->word_n, key, w, fresh);
    if (!w || !fresh) return;
    w->len = len;
    memcpy(w->s, s0, len);
}

static const loom_word *loom_spelled(const rz_loom *l, uint64_t key)
{
    if (!l->word_cap) return NULL;
    size_t s;
    LOOM_SLOT(l->word, l->word_cap, key, s);
    return l->word[s].key ? &l->word[s] : NULL;
}

static void loom_shape_of(rz_loom *l, uint64_t sil, const uint64_t *run, int len, int h1, int h2)
{
    loom_shape *e; int fresh;
    LOOM_AT(l, l->shape, l->shape_cap, l->shape_n, sil, e, fresh);
    if (!e || !fresh) return;
    e->len = (uint8_t)len; e->h1 = (uint8_t)h1; e->h2 = (uint8_t)h2;
    for (int q = 0; q < 5; q++) e->path[q] = (q < len && q != h1 && q != h2) ? run[q] : 0;
}

static uint64_t loom_pair_key(uint64_t a, uint64_t b)
{
    if (a > b) { uint64_t c = a; a = b; b = c; }
    return mix64(a * 0x9E3779B97F4A7C15ull ^ mix64(b));
}

static loom_pair *loom_pair_find(const rz_loom *l, uint64_t a, uint64_t b)
{
    if (!l->pair_cap) return NULL;
    uint64_t key = loom_pair_key(a, b);
    size_t s;
    LOOM_SLOT(l->pair, l->pair_cap, key, s);
    return l->pair[s].key ? &l->pair[s] : NULL;
}

static uint32_t loom_shared(const rz_loom *l, uint64_t a, uint64_t b)
{
    if (!l->pair_cap) return 0;
    uint64_t key = loom_pair_key(a, b);
    size_t s;
    LOOM_SLOT(l->pair, l->pair_cap, key, s);
    return l->pair[s].key ? l->pair[s].count : 0;
}

/* a and b filled the same place beside the same partner: once per silhouette,
   however many partners or times (a form counts once) */
static void loom_associate(rz_loom *l, uint64_t a, uint64_t b, uint64_t sil)
{
    if (a == b) return;
    loom_pair *p; int fresh;
    LOOM_AT(l, l->pair, l->pair_cap, l->pair_n, loom_pair_key(a, b), p, fresh);
    if (!p) return;
    if (fresh) { p->a = a < b ? a : b; p->b = a < b ? b : a; }
    if (p->last == sil) return;
    p->last = sil;
    p->count++;
    if (p->count == 2 && l->reading) loom_enrich(l, a, b, p);
}

static int loom_has(const rz_loom *l, const loom_group *g, uint64_t w)
{
    for (int32_t m = g->head; m >= 0; m = l->mem[m].next) if (l->mem[m].word == w) return 1;
    return 0;
}

/* an associate of w already standing in this place: two silhouettes shared */
static uint64_t loom_foresees(const rz_loom *l, const loom_group *g, uint64_t w)
{
    for (int32_t m = g->head; m >= 0; m = l->mem[m].next)
        if (l->mem[m].word != w && loom_shared(l, w, l->mem[m].word) >= 2) return l->mem[m].word;
    return 0;
}

static void loom_add_member(rz_loom *l, loom_group *g, uint64_t w)
{
    if (l->mem_n == l->mem_cap) {
        size_t cap = l->mem_cap ? l->mem_cap * 2 : 1024;
        loom_member *m = realloc(l->mem, cap * sizeof *m);
        if (!m) return;
        l->mem = m;
        l->mem_cap = cap;
    }
    l->mem[l->mem_n].word = w;
    l->mem[l->mem_n].next = g->head;
    g->head = (int32_t)l->mem_n++;
    g->size++;
}

static uint64_t loom_idea_key(uint64_t group, uint64_t word, int blind)
{
    return mix64(group * 0x9E3779B97F4A7C15ull ^ mix64(word) ^ (uint64_t)blind);
}

/* w takes its place in the group, associated with everyone already there. A
   place someone thought of for it (P53) is confirmed by the reading. */
static void loom_join(rz_loom *l, loom_group *g, uint64_t w, uint64_t sil, rz_loom_report *out)
{
    if (loom_has(l, g, w)) return;
    for (int32_t m = g->head; m >= 0; m = l->mem[m].next) loom_associate(l, w, l->mem[m].word, sil);
    loom_add_member(l, g, w);
    if (!l->idea_cap || !out) return;
    for (int blind = 0; blind < 2; blind++) {
        size_t s;
        uint64_t k = loom_idea_key(g->key, w, blind);
        LOOM_SLOT(l->idea, l->idea_cap, k, s);
        if (!l->idea[s].key || l->idea[s].confirmed) continue;
        l->idea[s].confirmed = 1;
        if (blind) { out->blind_confirmed++; continue; }
        out->thoughts_confirmed++;
        if (l->idea[s].traveled) out->said_traveled++;
        else out->said_untraveled++;
        /* the branch that thought it is travelled one step more (the author:
           what reality confirms becomes a more travelled path) */
        loom_pair *p = loom_pair_find(l, l->idea[s].word, l->idea[s].via);
        if (p) p->traveled++;
    }
}

static uint64_t loom_group_key(uint64_t sil, int side, uint64_t partner)
{
    return mix64(sil ^ mix64(partner * 2 + (uint64_t)side));
}

static uint64_t loom_path_key(const uint64_t *run, int len, int h1, int h2)
{
    uint64_t v = 1469598103934665603ull ^ (uint64_t)(len * 64 + h1 * 8 + h2);
    for (int q = 0; q < len; q++) {
        uint64_t w = (q == h1 || q == h2) ? 0x5555555555555555ull : run[q];
        v ^= w; v *= 1099511628211ull; v ^= v >> 31;
    }
    return mix64(v);
}

rz_loom *rz_loom_new(void)
{
    rz_loom *l = calloc(1, sizeof(rz_loom));
    if (l) l->rng = 0x9E3779B97F4A7C15ull;
    return l;
}

void rz_loom_free(rz_loom *l)
{
    if (!l) return;
    free(l->sil); free(l->grp); free(l->mem); free(l->pair); free(l->word);
    free(l->shape); free(l->idea); free(l->trail);
    free(l);
}

static loom_group *loom_place(rz_loom *l, uint64_t sil, int side, uint64_t partner)
{
    return loom_group_at(l, loom_group_key(sil, side, partner), sil, side, partner);
}

int rz_loom_read(rz_loom *l, const rz_lexicon *x, rz_loom_report *out)
{
    memset(out, 0, sizeof *out);
    uint64_t *h = malloc(((size_t)x->n + 1) * sizeof *h);
    if (!h) return RZ_ERR_IO;           /* out of memory: nothing was read */
    for (uint32_t w = 0; w < x->n; w++) h[w] = spelling_key(x->lego[w].s, x->lego[w].len);

    /* the ideas open when the book begins: the denominators of the bar */
    for (size_t s = 0; s < l->idea_cap; s++) {
        const loom_idea *d = &l->idea[s];
        if (!d->key || d->confirmed) continue;
        if (d->blind) out->open_blind++;
        else if (d->traveled) out->open_traveled++;
        else out->open_untraveled++;
    }

    /* the meanings held before this book: every associate of every word */
    loom_reading rd;
    memset(&rd, 0, sizeof rd);
    rd.h = h; rd.x = x; rd.out = out; rd.rng = 0xD1B54A32D192ED03ull;
    size_t nheld = 0;
    for (size_t p = 0; p < l->pair_cap; p++) if (l->pair[p].key && l->pair[p].count >= 2) nheld += 2;
    rd.held = malloc((nheld + 1) * sizeof *rd.held);
    if (rd.held) {
        for (size_t p = 0; p < l->pair_cap; p++) {
            if (!l->pair[p].key || l->pair[p].count < 2) continue;
            rd.held[rd.nheld].word = l->pair[p].a; rd.held[rd.nheld].other = l->pair[p].b; rd.nheld++;
            rd.held[rd.nheld].word = l->pair[p].b; rd.held[rd.nheld].other = l->pair[p].a; rd.nheld++;
        }
        qsort(rd.held, rd.nheld, sizeof *rd.held, edge_order);
        l->reading = &rd;
    }

    uint64_t rng = 0x9E3779B97F4A7C15ull;
    uint64_t run[RUN_MAX];
    for (uint32_t i = 0; i < x->nseq; i++) {
        rd.at = i;
        /* every run that ends with the piece just read */
        for (int len = RUN_MIN; len <= RUN_MAX && (uint32_t)len <= i + 1; len++) {
            const int32_t *ids = x->seq + i + 1 - len;
            int broken = 0;
            for (int q = 0; q < len; q++) { if (ids[q] < 0) broken = 1; else run[q] = h[ids[q]]; }
            if (broken) break;
            for (int h1 = 0; h1 < len; h1++)
                for (int h2 = h1 + 1; h2 < len; h2++) {
                    uint64_t key = loom_path_key(run, len, h1, h2);
                    uint64_t f1 = run[h1], f2 = run[h2];
                    loom_sil *sl = loom_sil_at(l, key);
                    if (!sl) continue;
                    if (!sl->f1) { sl->f1 = f1; sl->f2 = f2; continue; }
                    if (!sl->born) {
                        if (sl->f1 == f1 && sl->f2 == f2) continue;
                        /* born: put together with a second different pair. Its
                           first two pairs found it; no place existed before */
                        uint64_t o1 = sl->f1, o2 = sl->f2;
                        sl->born = 1;
                        out->structures++;
                        loom_shape_of(l, key, run, len, h1, h2);
                        loom_spell(l, f1, x->lego[ids[h1]].s, x->lego[ids[h1]].len);
                        loom_spell(l, f2, x->lego[ids[h2]].s, x->lego[ids[h2]].len);
                        for (int q = 0; q < len; q++) loom_spell(l, run[q], x->lego[ids[q]].s, x->lego[ids[q]].len);
                        loom_group *g;
                        if ((g = loom_place(l, key, 0, o2))) loom_join(l, g, o1, key, out);
                        if ((g = loom_place(l, key, 1, o1))) loom_join(l, g, o2, key, out);
                        if ((g = loom_place(l, key, 0, f2))) loom_join(l, g, f1, key, out);
                        if ((g = loom_place(l, key, 1, f1))) loom_join(l, g, f2, key, out);
                        continue;
                    }
                    loom_spell(l, f1, x->lego[ids[h1]].s, x->lego[ids[h1]].len);
                    loom_spell(l, f2, x->lego[ids[h2]].s, x->lego[ids[h2]].len);
                    /* born: the word in the first hole joins beside its partner */
                    loom_group *g1 = loom_place(l, key, 0, f2);
                    if (g1) loom_join(l, g1, f1, key, out);
                    loom_group *g2 = loom_place(l, key, 1, f1);
                    if (!g2) continue;
                    /* the word just read, in the last hole: the live bar */
                    if (h2 == len - 1 && !loom_has(l, g2, f2)) {
                        out->events++;
                        uint64_t a = loom_foresees(l, g2, f2);
                        if (a) {
                            out->foreseen++;
                            /* the last ones: by then it has read the most */
                            rz_foreseen *e = &out->seen[(out->foreseen - 1) % 64];
                            if (out->nseen < 64) out->nseen++;
                            e->word = ids[h2];
                            e->associate[0] = 0;
                            const loom_word *aw = loom_spelled(l, a);
                            if (aw) { memcpy(e->associate, aw->s, aw->len); e->associate[aw->len] = 0; }
                            e->len = (uint8_t)len;
                            for (int q = 0; q < 5; q++) e->path[q] = q < len ? ids[q] : -1;
                        }
                        /* blind: a word picked from what was already read */
                        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
                        int32_t b = x->seq[rng % (i + 1)];
                        if (b >= 0 && h[b] != f2 && !loom_has(l, g2, h[b])) {
                            out->blind_events++;
                            if (loom_foresees(l, g2, h[b])) out->blind_foreseen++;
                        }
                    }
                    loom_join(l, g2, f2, key, out);
                }
        }
    }
    for (size_t s = 0; s < l->pair_cap; s++) if (l->pair[s].key && l->pair[s].count >= 2) out->associates++;
    l->reading = NULL;
    free(rd.held);
    free(h);
    return RZ_OK;
}

/* ===================== the neuron that thinks (P53) ======================== */

static int edge_order(const void *pa, const void *pb)
{
    const loom_edge *a = pa, *b = pb;
    if (a->word != b->word) return a->word < b->word ? -1 : 1;
    return (a->other > b->other) - (a->other < b->other);
}

static size_t edges_from(const loom_edge *e, size_t n, uint64_t w)
{
    size_t lo = 0, hi = n;
    while (lo < hi) { size_t mid = (lo + hi) / 2; if (e[mid].word < w) lo = mid + 1; else hi = mid; }
    return lo;
}

/* two words shared an associate in the meanings held before this book */
static int held_common(const loom_reading *rd, uint64_t a, uint64_t b)
{
    size_t i = edges_from(rd->held, rd->nheld, a), j = edges_from(rd->held, rd->nheld, b);
    while (i < rd->nheld && j < rd->nheld && rd->held[i].word == a && rd->held[j].word == b) {
        if (rd->held[i].other == rd->held[j].other) return 1;
        if (rd->held[i].other < rd->held[j].other) i++; else j++;
    }
    return 0;
}

static int held_meaning(const loom_reading *rd, uint64_t w)
{
    size_t i = edges_from(rd->held, rd->nheld, w);
    return i < rd->nheld && rd->held[i].word == w;
}

static int held_associates(const loom_reading *rd, uint64_t a, uint64_t b)
{
    for (size_t i = edges_from(rd->held, rd->nheld, a); i < rd->nheld && rd->held[i].word == a; i++)
        if (rd->held[i].other == b) return 1;
    return 0;
}

/* A new association, made by this book: did it land on a meaning held before?
   (the author: the word comes in other structures, but associated with a
   meaning it holds, and branches into the meanings kept.) Blind: the same word
   with a partner picked from what this book has read so far, that also had a
   meaning and was not already its associate -- the frequencies of the book. */
static void loom_enrich(rz_loom *l, uint64_t a, uint64_t b, loom_pair *p)
{
    loom_reading *rd = l->reading;
    if (!held_meaning(rd, a) || !held_meaning(rd, b)) return;
    rd->out->known_new++;
    if (held_common(rd, a, b)) { rd->out->landed++; p->landed = 1; }
    for (int tries = 0; tries < 16; tries++) {
        rd->rng ^= rd->rng << 13; rd->rng ^= rd->rng >> 7; rd->rng ^= rd->rng << 17;
        int32_t id = rd->x->seq[rd->rng % (rd->at + 1)];
        if (id < 0) continue;
        uint64_t c = rd->h[id];
        if (c == a || c == b || !held_meaning(rd, c) || held_associates(rd, a, c)) continue;
        rd->out->blind_new++;
        rd->out->blind_landed += (uint64_t)held_common(rd, a, c);
        break;
    }
}

static void spell_into(const rz_loom *l, uint64_t w, char *out, size_t cap)
{
    const loom_word *s = loom_spelled(l, w);
    size_t n = s ? s->len : 0;
    if (n >= cap) n = cap - 1;
    if (s) memcpy(out, s->s, n);
    out[n] = 0;
}

/* the path the idea thinks: the place's shape with the word in its hole */
static void say_idea(const rz_loom *l, const loom_group *g, uint64_t w, char *out, size_t cap)
{
    size_t k = 0, sc = 0;
    out[0] = 0;
    if (l->shape_cap) {
        LOOM_SLOT(l->shape, l->shape_cap, g->sil, sc);
    }
    if (!l->shape_cap || !l->shape[sc].key) return;
    const loom_shape *sh = &l->shape[sc];
    for (int q = 0; q < sh->len && k + 2 < cap; q++) {
        uint64_t piece = sh->path[q];
        if (q == sh->h1) piece = g->side == 0 ? w : g->partner;
        if (q == sh->h2) piece = g->side == 0 ? g->partner : w;
        char sp[RZ_LEGO_SPAN + 1];
        spell_into(l, piece, sp, sizeof sp);
        int wrote = snprintf(out + k, cap - k, "%s%s", q ? " " : "", sp);
        if (wrote < 0 || (size_t)wrote >= cap - k) break;
        k += (size_t)wrote;
    }
}

static void say_trail(const rz_loom *l, int32_t at, char *out, size_t cap)
{
    int32_t chain[64];
    int n = 0;
    for (int32_t t = at; t >= 0 && n < 64; t = l->trail[t].parent) chain[n++] = t;
    size_t k = 0;
    out[0] = 0;
    for (int i = n - 1; i >= 0 && k + 2 < cap; i--) {
        char sp[RZ_LEGO_SPAN + 1];
        spell_into(l, l->trail[chain[i]].word, sp, sizeof sp);
        int wrote = snprintf(out + k, cap - k, "%s%s", i == n - 1 ? "" : " > ", sp);
        if (wrote < 0 || (size_t)wrote >= cap - k) break;
        k += (size_t)wrote;
    }
}

static int loom_trail_push(rz_loom *l, uint64_t w, int32_t parent, uint8_t level)
{
    if (l->trail_n == l->trail_cap) {
        size_t cap = l->trail_cap ? l->trail_cap * 2 : 256;
        loom_step *t = realloc(l->trail, cap * sizeof *t);
        if (!t) return 0;
        l->trail = t;
        l->trail_cap = cap;
    }
    l->trail[l->trail_n].word = w;
    l->trail[l->trail_n].parent = parent;
    l->trail[l->trail_n].level = level;
    l->trail_n++;
    return 1;
}

int rz_loom_think_from(rz_loom *l, const char *word)
{
    size_t len = strlen(word);
    if (len > RZ_LEGO_SPAN) return RZ_ERR_MISSING;
    uint64_t w = spelling_key((const unsigned char *)word, len);
    if (!loom_spelled(l, w)) return RZ_ERR_MISSING;      /* never met: nothing to think from */
    l->trail_n = 0;
    l->front = 0;
    return loom_trail_push(l, w, -1, 0) ? RZ_OK : RZ_ERR_IO;
}

static int on_path(const rz_loom *l, int32_t at, uint64_t w)
{
    for (int32_t t = at; t >= 0; t = l->trail[t].parent) if (l->trail[t].word == w) return 1;
    return 0;
}

uint32_t rz_loom_step(rz_loom *l, rz_idea *out, uint32_t cap, uint32_t *made)
{
    *made = 0;
    if (!l->trail_n) return 0;

    /* the branches of every neuron, gathered: the places a word stands in,
       and its associates (two silhouettes shared) */
    size_t nm = 0, na = 0;
    for (size_t g = 0; g < l->grp_cap; g++) if (l->grp[g].key) nm += l->grp[g].size;
    for (size_t p = 0; p < l->pair_cap; p++) if (l->pair[p].key && l->pair[p].count >= 2) na += 2;
    loom_edge *in = malloc((nm + 1) * sizeof *in), *as = malloc((na + 1) * sizeof *as);
    if (!in || !as) { free(in); free(as); return 0; }
    size_t k = 0;
    for (size_t g = 0; g < l->grp_cap; g++) {
        if (!l->grp[g].key) continue;
        for (int32_t m = l->grp[g].head; m >= 0; m = l->mem[m].next) {
            in[k].word = l->mem[m].word;
            in[k].other = l->grp[g].key;
            k++;
        }
    }
    nm = k;
    k = 0;
    for (size_t p = 0; p < l->pair_cap; p++) {
        if (!l->pair[p].key || l->pair[p].count < 2) continue;
        as[k].word = l->pair[p].a; as[k].other = l->pair[p].b; k++;
        as[k].word = l->pair[p].b; as[k].other = l->pair[p].a; k++;
    }
    na = k;
    qsort(in, nm, sizeof *in, edge_order);
    qsort(as, na, sizeof *as, edge_order);

    /* one depth: from every path the last step reached, and only beside it */
    size_t from = l->front, to = l->trail_n;
    uint8_t level = (uint8_t)(l->trail[from].level + 1);
    uint32_t reported = 0;
    for (size_t t = from; t < to; t++) {
        uint64_t w = l->trail[t].word;
        for (size_t e = edges_from(as, na, w); e < na && as[e].word == w; e++) {
            uint64_t a = as[e].other;
            if (on_path(l, (int32_t)t, a)) continue;     /* a path never walks back onto itself */
            /* the associate stands in places w never stood in: ideas */
            for (size_t q = edges_from(in, nm, a); q < nm && in[q].word == a; q++) {
                const loom_group *g = loom_group_find(l, in[q].other);
                if (!g || loom_has(l, g, w)) continue;
                loom_idea *idea; int fresh;
                LOOM_AT(l, l->idea, l->idea_cap, l->idea_n, loom_idea_key(g->key, w, 0), idea, fresh);
                if (!idea) continue;
                g = loom_group_find(l, in[q].other);
                const loom_pair *branch = loom_pair_find(l, w, a);
                uint32_t tr = branch ? branch->traveled : 0;
                if (fresh) {
                    idea->word = w; idea->group = g->key; idea->level = level;
                    idea->via = a; idea->traveled = tr;
                    (*made)++;
                    /* its blind twin: a word met somewhere, in the same place */
                    for (int tries = 0; tries < 64 && l->word_cap; tries++) {
                        l->rng ^= l->rng << 13; l->rng ^= l->rng >> 7; l->rng ^= l->rng << 17;
                        const loom_word *bw = &l->word[l->rng & (l->word_cap - 1)];
                        if (!bw->key || bw->key == w || loom_has(l, g, bw->key)) continue;
                        loom_idea *bi; int bf;
                        uint64_t bkey = bw->key, gkey = g->key;
                        LOOM_AT(l, l->idea, l->idea_cap, l->idea_n, loom_idea_key(gkey, bkey, 1), bi, bf);
                        if (bi && bf) { bi->word = bkey; bi->group = gkey; bi->level = level; bi->blind = 1; bi->traveled = tr; }
                        g = loom_group_find(l, gkey);
                        break;
                    }
                }
                /* the cache belongs to each path: reported for this path
                   even when another path thought it first */
                if (reported < cap) {
                    rz_idea *o = &out[reported++];
                    o->level = level;
                    o->traveled = tr;
                    spell_into(l, w, o->word, sizeof o->word);
                    spell_into(l, a, o->via, sizeof o->via);
                    say_idea(l, g, w, o->text, sizeof o->text);
                    say_trail(l, (int32_t)t, o->trail, sizeof o->trail);
                }
            }
            /* the next depth thinks from the associate, along this path */
            loom_trail_push(l, a, (int32_t)t, level);
        }
    }
    l->front = to;
    free(in); free(as);
    return reported;
}

/* --- the loom on disk ------------------------------------------------------
 * Everything is kept, the silhouettes heard once too, the ideas and the cache
 * of every path: the memory comes back exactly as it was (the round trip is
 * tested). Forgetting what was heard only once is a decision of its own, not a
 * side effect of saving. */

#define LOOM_MAGIC 0x4D4F4F4Cu           /* "LOOM" */
#define LOOM_VER   4u                    /* 2: places and pairs say whose they are; shapes, ideas, the cache.
                                            3: travelled branches, and the branch each idea came by.
                                            4: associations that landed on a meaning held before */

static int put64(FILE *f, uint64_t v) { return fwrite(&v, sizeof v, 1, f) == 1; }
static int get64(FILE *f, uint64_t *v) { return fread(v, sizeof *v, 1, f) == 1; }

int rz_loom_save(const rz_loom *l, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return RZ_ERR_IO;
    int ok = put64(f, ((uint64_t)LOOM_VER << 32) | LOOM_MAGIC);
    ok = ok && put64(f, l->sil_n) && put64(f, l->grp_n) && put64(f, l->pair_n) && put64(f, l->word_n)
            && put64(f, l->shape_n) && put64(f, l->idea_n) && put64(f, l->trail_n) && put64(f, l->front)
            && put64(f, l->rng);
    for (size_t i = 0; ok && i < l->sil_cap; i++)
        if (l->sil[i].key)
            ok = put64(f, l->sil[i].key) && put64(f, l->sil[i].f1) && put64(f, l->sil[i].f2) && put64(f, l->sil[i].born);
    for (size_t i = 0; ok && i < l->grp_cap; i++) {
        const loom_group *g = &l->grp[i];
        if (!g->key) continue;
        ok = put64(f, g->key) && put64(f, g->sil) && put64(f, g->partner) && put64(f, g->side) && put64(f, g->size);
        /* members oldest first, so loading rebuilds the same order */
        uint64_t *ms = malloc(((size_t)g->size + 1) * sizeof *ms);
        if (!ms) { ok = 0; break; }
        uint32_t k = g->size;
        for (int32_t m = g->head; m >= 0 && k; m = l->mem[m].next) ms[--k] = l->mem[m].word;
        for (uint32_t q = 0; ok && q < g->size; q++) ok = put64(f, ms[q]);
        free(ms);
    }
    for (size_t i = 0; ok && i < l->pair_cap; i++)
        if (l->pair[i].key)
            ok = put64(f, l->pair[i].key) && put64(f, l->pair[i].last) && put64(f, l->pair[i].a)
                 && put64(f, l->pair[i].b) && put64(f, l->pair[i].count) && put64(f, l->pair[i].traveled)
                 && put64(f, l->pair[i].landed);
    for (size_t i = 0; ok && i < l->word_cap; i++)
        if (l->word[i].key)
            ok = put64(f, l->word[i].key) && fwrite(&l->word[i].len, 1, 1, f) == 1 &&
                 fwrite(l->word[i].s, 1, l->word[i].len, f) == l->word[i].len;
    for (size_t i = 0; ok && i < l->shape_cap; i++) {
        const loom_shape *s = &l->shape[i];
        if (!s->key) continue;
        ok = put64(f, s->key) && put64(f, (uint64_t)s->len | (uint64_t)s->h1 << 8 | (uint64_t)s->h2 << 16);
        for (int q = 0; ok && q < 5; q++) ok = put64(f, s->path[q]);
    }
    for (size_t i = 0; ok && i < l->idea_cap; i++) {
        const loom_idea *d = &l->idea[i];
        if (!d->key) continue;
        ok = put64(f, d->key) && put64(f, d->word) && put64(f, d->group) && put64(f, d->via) && put64(f, d->traveled)
             && put64(f, (uint64_t)d->level | (uint64_t)d->blind << 8 | (uint64_t)d->confirmed << 16);
    }
    for (size_t i = 0; ok && i < l->trail_n; i++)
        ok = put64(f, l->trail[i].word) && put64(f, (uint64_t)(uint32_t)l->trail[i].parent)
             && put64(f, l->trail[i].level);
    if (fclose(f) != 0) ok = 0;
    return ok ? RZ_OK : RZ_ERR_IO;
}

int rz_loom_load(rz_loom *l, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return RZ_ERR_MISSING;
    uint64_t head = 0, ns = 0, ng = 0, np = 0, nw = 0, nsh = 0, ni = 0, nt = 0, front = 0, rng = 0;
    if (!get64(f, &head) || (uint32_t)head != LOOM_MAGIC || (head >> 32) != LOOM_VER) { fclose(f); return RZ_ERR_VERSION; }
    int ok = get64(f, &ns) && get64(f, &ng) && get64(f, &np) && get64(f, &nw)
             && get64(f, &nsh) && get64(f, &ni) && get64(f, &nt) && get64(f, &front) && get64(f, &rng);
    if (ok) l->rng = rng;
    for (uint64_t i = 0; ok && i < ns; i++) {
        uint64_t k, f1, f2, born;
        ok = get64(f, &k) && get64(f, &f1) && get64(f, &f2) && get64(f, &born);
        loom_sil *s = ok ? loom_sil_at(l, k) : NULL;
        if (!s) { ok = 0; break; }
        s->f1 = f1; s->f2 = f2; s->born = (uint8_t)born;
    }
    for (uint64_t i = 0; ok && i < ng; i++) {
        uint64_t k, sil, partner, side, size;
        ok = get64(f, &k) && get64(f, &sil) && get64(f, &partner) && get64(f, &side) && get64(f, &size);
        loom_group *g = ok ? loom_group_at(l, k, sil, (int)side, partner) : NULL;
        if (!g) { ok = 0; break; }
        for (uint64_t q = 0; ok && q < size; q++) {
            uint64_t w;
            ok = get64(f, &w);
            if (ok && !loom_has(l, g, w)) loom_add_member(l, g, w);
        }
    }
    for (uint64_t i = 0; ok && i < np; i++) {
        uint64_t k, last, a, b, count, traveled, landed;
        ok = get64(f, &k) && get64(f, &last) && get64(f, &a) && get64(f, &b) && get64(f, &count) && get64(f, &traveled)
             && get64(f, &landed);
        if (!ok) break;
        loom_pair *p; int fresh;
        LOOM_AT(l, l->pair, l->pair_cap, l->pair_n, k, p, fresh);
        (void)fresh;
        if (!p) { ok = 0; break; }
        p->last = last; p->a = a; p->b = b; p->count = (uint32_t)count; p->traveled = (uint32_t)traveled; p->landed = (uint8_t)landed;
    }
    for (uint64_t i = 0; ok && i < nw; i++) {
        uint64_t k;
        uint8_t len;
        unsigned char s0[RZ_LEGO_SPAN];
        ok = get64(f, &k) && fread(&len, 1, 1, f) == 1 && len <= RZ_LEGO_SPAN && fread(s0, 1, len, f) == len;
        if (ok) loom_spell(l, k, s0, len);
    }
    for (uint64_t i = 0; ok && i < nsh; i++) {
        uint64_t k, packed;
        ok = get64(f, &k) && get64(f, &packed);
        loom_shape *s; int fresh;
        if (ok) { LOOM_AT(l, l->shape, l->shape_cap, l->shape_n, k, s, fresh); (void)fresh; } else s = NULL;
        if (!s) { ok = 0; break; }
        s->len = (uint8_t)packed; s->h1 = (uint8_t)(packed >> 8); s->h2 = (uint8_t)(packed >> 16);
        for (int q = 0; ok && q < 5; q++) ok = get64(f, &s->path[q]);
    }
    for (uint64_t i = 0; ok && i < ni; i++) {
        uint64_t k, word, group, via, traveled, packed;
        ok = get64(f, &k) && get64(f, &word) && get64(f, &group) && get64(f, &via) && get64(f, &traveled) && get64(f, &packed);
        loom_idea *d; int fresh;
        if (ok) { LOOM_AT(l, l->idea, l->idea_cap, l->idea_n, k, d, fresh); (void)fresh; } else d = NULL;
        if (!d) { ok = 0; break; }
        d->word = word; d->group = group; d->via = via; d->traveled = (uint32_t)traveled;
        d->level = (uint8_t)packed; d->blind = (uint8_t)(packed >> 8); d->confirmed = (uint8_t)(packed >> 16);
    }
    l->trail_n = 0;
    for (uint64_t i = 0; ok && i < nt; i++) {
        uint64_t w, parent, level;
        ok = get64(f, &w) && get64(f, &parent) && get64(f, &level) && loom_trail_push(l, w, (int32_t)(uint32_t)parent, (uint8_t)level);
    }
    l->front = (size_t)front;
    fclose(f);
    return ok ? RZ_OK : RZ_ERR_IO;
}

uint32_t rz_loom_traveled(const rz_loom *l, const char *word, const char *associate)
{
    size_t lw = strlen(word), la = strlen(associate);
    const loom_pair *p = loom_pair_find(l, spelling_key((const unsigned char *)word, lw),
                                        spelling_key((const unsigned char *)associate, la));
    return p ? p->traveled : 0;
}

int rz_loom_landed(const rz_loom *l, const char *a, const char *b)
{
    const loom_pair *p = loom_pair_find(l, spelling_key((const unsigned char *)a, strlen(a)),
                                        spelling_key((const unsigned char *)b, strlen(b)));
    return p && p->count >= 2 && p->landed;
}

static int by_word64(const void *pa, const void *pb)
{
    uint64_t a = *(const uint64_t *)pa, b = *(const uint64_t *)pb;
    return (a > b) - (a < b);
}

int rz_loom_meaning(const rz_loom *l, const char *word, rz_meaning *out)
{
    memset(out, 0, sizeof *out);
    size_t len = strlen(word);
    if (len > RZ_LEGO_SPAN) return RZ_ERR_MISSING;
    uint64_t w = spelling_key((const unsigned char *)word, len);
    if (!loom_spelled(l, w)) return RZ_ERR_MISSING;

    /* its associates, every one; the most shared kept to show */
    size_t na = 0;
    for (size_t i = 0; i < l->pair_cap; i++)
        if (l->pair[i].key && l->pair[i].count >= 2 && (l->pair[i].a == w || l->pair[i].b == w)) na++;
    uint64_t *who = malloc((na + 1) * sizeof *who);
    uint32_t *deg = calloc(na + 1, sizeof *deg);
    if (!who || !deg) { free(who); free(deg); return RZ_ERR_IO; }
    size_t k = 0;
    for (size_t i = 0; i < l->pair_cap; i++) {
        const loom_pair *p = &l->pair[i];
        if (!p->key || p->count < 2 || (p->a != w && p->b != w)) continue;
        who[k++] = p->a == w ? p->b : p->a;
        rz_meaning_assoc e;
        memset(&e, 0, sizeof e);
        spell_into(l, p->a == w ? p->b : p->a, e.word, sizeof e.word);
        e.forms = p->count; e.traveled = p->traveled; e.landed = p->landed;
        uint32_t j = out->nassoc < 256 ? out->nassoc++ : 256;
        if (j == 256) { if (e.forms <= out->assoc[255].forms) continue; j = 255; }
        while (j > 0 && out->assoc[j - 1].forms < e.forms) { out->assoc[j] = out->assoc[j - 1]; j--; }
        out->assoc[j] = e;
    }
    out->assoc_all = (uint32_t)k;
    /* how many associates each associate has: a word joined to everything
       says less about this one than a word joined to few */
    qsort(who, k, sizeof *who, by_word64);
    for (size_t i = 0; i < l->pair_cap; i++) {
        const loom_pair *p = &l->pair[i];
        if (!p->key || p->count < 2) continue;
        for (int side = 0; side < 2; side++) {
            uint64_t x = side ? p->b : p->a;
            size_t lo = 0, hi = k;
            while (lo < hi) { size_t mid = (lo + hi) / 2; if (who[mid] < x) lo = mid + 1; else hi = mid; }
            if (lo < k && who[lo] == x) deg[lo]++;
        }
    }
    for (uint32_t i = 0; i < out->nassoc; i++) {
        uint64_t x = spelling_key((const unsigned char *)out->assoc[i].word, strlen(out->assoc[i].word));
        size_t lo = 0, hi = k;
        while (lo < hi) { size_t mid = (lo + hi) / 2; if (who[mid] < x) lo = mid + 1; else hi = mid; }
        if (lo < k && who[lo] == x) out->assoc[i].others = deg[lo];
    }
    free(who);
    free(deg);

    /* the places it stands in, every one; the most shared kept to show */
    for (size_t i = 0; i < l->grp_cap; i++) {
        const loom_group *g = &l->grp[i];
        if (!g->key || !loom_has(l, g, w)) continue;
        out->place_all++;
        rz_meaning_place e;
        memset(&e, 0, sizeof e);
        e.company = g->size;
        say_idea(l, g, w, e.text, sizeof e.text);
        /* two silhouettes can read the same: "agua , y" with the comma fixed
           or the comma in a hole. One place, the larger company */
        int dup = 0;
        for (uint32_t q = 0; q < out->nplace && !dup; q++) {
            if (strcmp(out->place[q].text, e.text)) continue;
            dup = 1;
            if (out->place[q].company < e.company) {
                out->place[q].company = e.company;
                for (uint32_t z = q; z > 0 && out->place[z - 1].company < out->place[z].company; z--) {
                    rz_meaning_place t0 = out->place[z - 1]; out->place[z - 1] = out->place[z]; out->place[z] = t0;
                }
            }
        }
        if (dup) continue;
        uint32_t j = out->nplace < 256 ? out->nplace++ : 256;
        if (j == 256) { if (e.company <= out->place[255].company) continue; j = 255; }
        while (j > 0 && out->place[j - 1].company < e.company) { out->place[j] = out->place[j - 1]; j--; }
        out->place[j] = e;
    }
    return RZ_OK;
}
