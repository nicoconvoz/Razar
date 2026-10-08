#include "lego.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================== step 1: clean legos =============================== */

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
    double mid = -HUGE_VAL;
    int two = 0;
    {
        unsigned char ord[RZ_ATOMS];
        double hv[RZ_ATOMS];
        int k = 0;
        for (int b = 0; b < RZ_ATOMS; b++) {
            if (!at[b]) continue;
            double v = rz_branching(m, (unsigned char)b);
            hv[b] = v > 0.0 ? log(v) : 0.0;
            int j = k++;
            while (j > 0 && hv[ord[j - 1]] > hv[b]) { ord[j] = ord[j - 1]; j--; }
            ord[j] = (unsigned char)b;
        }
        double W = 0, S = 0;
        for (int j = 0; j < k; j++) { W += (double)at[ord[j]]; S += (double)at[ord[j]] * hv[ord[j]]; }
        double w0 = 0, s0 = 0, best = 0;
        for (int j = 0; j + 1 < k; j++) {
            w0 += (double)at[ord[j]];
            s0 += (double)at[ord[j]] * hv[ord[j]];
            double w1 = W - w0;
            if (hv[ord[j + 1]] <= hv[ord[j]]) continue;
            double d = s0 / w0 - (S - s0) / w1;
            double between = w0 * w1 * d * d;
            if (between > best) {
                best = between;
                mid = (hv[ord[j]] + hv[ord[j + 1]]) / 2.0;
                two = 1;
            }
        }
    }

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

void rz_lexicon_read(rz_lexicon *x, const rz_mind *m,
                     const unsigned char *t, size_t n)
{
    if (n == 0) return;
    unsigned char *ends = malloc(n);
    if (!ends) return;
    rz_lego_cut(m, t, n, ends);
    drop_derived(x);                  /* new counts: the kinds are stale */

    int32_t prev = -1;
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        if (!ends[i]) continue;
        int32_t id = intern(x, t + start, i + 1 - start);
        if (id >= 0 && prev >= 0) count_pair(x, (uint32_t)prev, (uint32_t)id);
        prev = id;
        start = i + 1;
    }
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
#define RZ_LEXICON_VER   (1u + 1000u * (uint32_t)sizeof(rz_lego))

int rz_lexicon_save(const rz_lexicon *x, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return RZ_ERR_IO;
    uint32_t head[5] = { RZ_LEXICON_MAGIC, RZ_LEXICON_VER, x->n, x->npair, x->kinds };
    int ok = fwrite(head, sizeof head, 1, f) == 1
          && (x->n == 0 || fwrite(x->lego, sizeof *x->lego, x->n, f) == x->n)
          && (x->npair == 0 || fwrite(x->pair, sizeof *x->pair, x->npair, f) == x->npair);
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
    uint32_t head[5];
    if (fread(head, sizeof head, 1, f) != 1) { fclose(f); return RZ_ERR_IO; }
    if (head[0] != RZ_LEXICON_MAGIC || head[1] != RZ_LEXICON_VER) { fclose(f); return RZ_ERR_VERSION; }

    rz_lexicon y;
    rz_lexicon_init(&y);
    y.n = y.cap = head[2];
    y.npair = y.pcap = head[3];
    y.kinds = head[4];
    y.lego = malloc(((size_t)y.n + 1) * sizeof *y.lego);
    y.pair = malloc(((size_t)y.npair + 1) * sizeof *y.pair);
    int ok = y.lego && y.pair
          && fread(y.lego, sizeof *y.lego, y.n, f) == y.n
          && fread(y.pair, sizeof *y.pair, y.npair, f) == y.npair;
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
