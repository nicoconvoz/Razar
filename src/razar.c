#include "razar.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <direct.h>
  #define RZ_MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #define RZ_MKDIR(p) mkdir((p), 0755)
#endif

/* Derived from the real layout, so a struct change makes old files
   unreadable on purpose instead of silently misaligned. */
#define RZ_MIND_VER ((uint32_t)(RZ_ATOMS + 1000u * (uint32_t)sizeof(rz_module) + 1u))
/* Bumped by hand when the MEANING of a stored number changes, not only its
   size. Weight 1 used to mean "cold"; it now means "never walked". */
#define RZ_SEMANTICS 1u
#define RZ_MOD_VER  ((uint32_t)(RZ_MOD_FRAGS + 7u * (uint32_t)sizeof(rz_module) + RZ_SEMANTICS))

/* ===================== the atomic base ==================================== */

uint32_t rz_intensity(const rz_mind *m, unsigned char from, unsigned char to)
{
    return m->path[(size_t)from * RZ_ATOMS + to];
}

uint32_t rz_distinct_paths(const rz_mind *m)
{
    uint32_t n = 0;
    for (size_t i = 0; i < RZ_PATHS; i++) if (m->path[i] != 0) n++;
    return n;
}

uint32_t rz_distinct_atoms(const rz_mind *m)
{
    uint32_t n = 0;
    for (size_t i = 0; i < RZ_ATOMS; i++) if (m->atom_seen[i] != 0) n++;
    return n;
}

size_t rz_plastic_bytes(const rz_mind *m)
{
    /* What one awake module costs, plus the base that never sleeps. The point
       is that this does not grow with experience: more material means more
       modules asleep on disk, not a bigger resident footprint. */
    (void)m;
    return sizeof(((rz_mind *)0)->path)
         + sizeof(((rz_mind *)0)->atom_seen)
         + (size_t)RZ_RESIDENT * sizeof(rz_module);
}

double rz_entropy(const rz_mind *m)
{
    if (m->traversals == 0) return 0.0;

    const double total = (double)m->traversals;
    double h = 0.0;
    for (size_t i = 0; i < RZ_PATHS; i++) {
        uint32_t c = m->path[i];
        if (c == 0) continue;
        double p = (double)c / total;
        h -= p * log(p);
    }
    return h < 0.0 ? 0.0 : h;
}

double rz_effective_support(const rz_mind *m)
{
    if (m->traversals == 0) return 0.0;
    return exp(rz_entropy(m));
}

static int desc_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x < y) ? 1 : (x > y) ? -1 : 0;
}

double rz_top_share(const rz_mind *m, double pct)
{
    if (m->traversals == 0 || pct <= 0.0) return 0.0;

    static uint32_t counts[RZ_PATHS];
    size_t n = 0;
    for (size_t i = 0; i < RZ_PATHS; i++)
        if (m->path[i] != 0) counts[n++] = m->path[i];
    if (n == 0) return 0.0;

    qsort(counts, n, sizeof(counts[0]), desc_u32);

    size_t k = (size_t)ceil(pct / 100.0 * (double)n);
    if (k < 1) k = 1;
    if (k > n) k = n;

    uint64_t top = 0;
    for (size_t i = 0; i < k; i++) top += counts[i];
    return (double)top / (double)m->traversals;
}

/* ===================== modules: wake, sleep, page ========================= */

int32_t rz_atom_id(unsigned char a) { return (int32_t)a * RZ_MOD_FRAGS; }

static int32_t id_mod(int32_t id)   { return id / RZ_MOD_FRAGS; }
static int32_t id_slot(int32_t id)  { return id % RZ_MOD_FRAGS; }

void rz_set_nest(rz_mind *m, const char *dir)
{
    snprintf(m->nest, sizeof m->nest, "%s", dir ? dir : "");
}

static void mod_file(const rz_mind *m, int32_t k, char *out, size_t cap)
{
    snprintf(out, cap, "%s/mod_%03d.dat", m->nest, k);
}

static int mod_save(rz_mind *m, int32_t k)
{
    if (m->nest[0] == 0 || !m->mod[k]) return RZ_ERR_IO;

    RZ_MKDIR(m->nest);
    char p[600];
    mod_file(m, k, p, sizeof p);

    FILE *f = fopen(p, "wb");
    if (!f) return RZ_ERR_IO;

    /* Only the part that is used. A lineage holding three fragments must not
       cost what a full one does — writing the whole fixed struct turned 421 KB
       of material into 123 MB of nest. */
    const rz_module *M = m->mod[k];
    uint32_t ver = RZ_MOD_VER;
    int32_t nf = M->nfrags, np = M->npool;

    int ok = (fwrite(&ver, sizeof ver, 1, f) == 1)
          && (fwrite(&nf, sizeof nf, 1, f) == 1)
          && (fwrite(&np, sizeof np, 1, f) == 1)
          && (fwrite(&M->decayed_at, sizeof M->decayed_at, 1, f) == 1)
          && (nf == 0 || fwrite(M->frag, sizeof(rz_frag), (size_t)nf, f) == (size_t)nf)
          && (nf == 0 || fwrite(M->head, sizeof(int32_t), (size_t)nf, f) == (size_t)nf)
          && (np == 0 || fwrite(M->pool, sizeof(rz_edge), (size_t)np, f) == (size_t)np);

    if (fclose(f) != 0) ok = 0;
    if (ok) m->mod[k]->dirty = 0;
    return ok ? RZ_OK : RZ_ERR_IO;
}

static int mod_read(rz_mind *m, int32_t k, rz_module *dst)
{
    if (m->nest[0] == 0) return RZ_ERR_MISSING;

    char p[600];
    mod_file(m, k, p, sizeof p);

    FILE *f = fopen(p, "rb");
    if (!f) return RZ_ERR_MISSING;

    uint32_t ver = 0;
    int32_t nf = 0, np = 0;
    if (fread(&ver, sizeof ver, 1, f) != 1 || ver != RZ_MOD_VER
        || fread(&nf, sizeof nf, 1, f) != 1
        || fread(&np, sizeof np, 1, f) != 1
        || nf < 0 || nf > RZ_MOD_FRAGS || np < 0 || np > RZ_MOD_EDGES) {
        fclose(f);
        return RZ_ERR_VERSION;
    }

    uint64_t decayed_at = 0;
    if (fread(&decayed_at, sizeof decayed_at, 1, f) != 1) { fclose(f); return RZ_ERR_VERSION; }

    memset(dst, 0, sizeof(*dst));
    int ok = (nf == 0 || fread(dst->frag, sizeof(rz_frag), (size_t)nf, f) == (size_t)nf)
          && (nf == 0 || fread(dst->head, sizeof(int32_t), (size_t)nf, f) == (size_t)nf)
          && (np == 0 || fread(dst->pool, sizeof(rz_edge), (size_t)np, f) == (size_t)np);
    fclose(f);
    if (!ok) return RZ_ERR_IO;

    dst->nfrags = nf;
    dst->npool = np;
    dst->dirty = 0;
    dst->decayed_at = decayed_at;
    return RZ_OK;
}

/* A module starts life holding exactly its own bare atom. */
static void mod_seed(rz_module *M, int32_t k)
{
    memset(M, 0, sizeof(*M));
    M->frag[0].left  = -1;
    M->frag[0].right = -1;
    M->frag[0].span  = 1;
    M->frag[0].atoms[0] = (unsigned char)k;
    M->head[0] = -1;
    M->nfrags = 1;
    M->npool = 0;
    M->decayed_at = 0;
}

/* A module that cannot be written down cannot be forgotten. Sleeping one whose
   changes have nowhere to go would destroy them silently — so it is held open
   instead, and the resident set is allowed to exceed its target. */
static int mod_put_to_sleep(rz_mind *m, int32_t slot)
{
    int32_t k = m->resident[slot];
    rz_module *M = m->mod[k];

    if (M && M->dirty) {
        if (m->nest[0] == 0 || mod_save(m, k) != RZ_OK) return 0;
    }

    free(M);
    m->mod[k] = NULL;
    m->sleeps++;

    m->resident[slot] = m->resident[m->nresident - 1];
    m->used[slot]     = m->used[m->nresident - 1];
    m->nresident--;
    return 1;
}

static uint8_t class4(double x, double a, double b, double c);

/* The shape of a fragment computed from a module already in hand. Needed
   because forgetting runs over a module it is holding, and reaching back
   through the pager mid-pass would be both slow and circular. */
static uint8_t shape_of_local(const rz_mind *m, const rz_module *M, int32_t l)
{
    (void)m;
    if (l < 0 || l >= M->nfrags) return 0;

    /* Only what it walked. A fragment is described by what it did, not by what
       it was handed at birth. */
    uint64_t total = 0, top = 0;
    double h = 0.0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
        if (M->pool[e].weight < RZ_EDGE_FLOOR) continue;
        total += M->pool[e].weight;
        if (M->pool[e].weight > top) top = M->pool[e].weight;
    }
    if (total) {
        for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
            if (M->pool[e].weight < RZ_EDGE_FLOOR) continue;
            double p = (double)M->pool[e].weight / (double)total;
            h -= p * log(p);
        }
        if (h < 0.0) h = 0.0;
    }

    double branch = total ? exp(h) : 0.0;
    double conc   = total ? (double)top / (double)total : 0.0;

    uint8_t b = class4(branch, 1.5, 3.0, 8.0);
    uint8_t s = class4((double)M->frag[l].span, 2.0, 4.0, 9.0);
    uint8_t c = class4(conc, 0.30, 0.55, 0.85);
    return (uint8_t)((b * 4 + s) * 4 + c);
}

/* Forgetting, applied lazily. A module that slept through a hundred passes pays
   for all of them in one step the moment something reaches into it, so decay
   costs nothing for the parts nobody is touching.
   Nothing is removed: a path that fades all the way down stops at the floor and
   waits there for a cue. */
static void mod_catch_up(rz_mind *m, rz_module *M)
{
    if (!M || M->decayed_at >= m->decay_passes) return;

    uint64_t missed = m->decay_passes - M->decayed_at;
    double keep = pow(RZ_DECAY, (double)missed);

    /* Knowledge that transfers is spared. A fragment playing a role that keeps
       anticipating correctly is worth keeping sharp — that is what it means for
       something to matter here, and it is measured, not decreed. */
    static double frag_keep[RZ_MOD_FRAGS];
    for (int32_t l = 0; l < M->nfrags; l++) {
        uint8_t sh = shape_of_local(m, M, l);
        double merit = (m->shape_tried[sh] >= RZ_SHAPE_TESTED)
                     ? (double)m->shape_won[sh] / (double)m->shape_tried[sh] : 0.0;

        /* Two things earn protection: an idea that transfers, and a mark a
           dream left behind. Full merit halves the loss; so does full
           attention. Neither is decreed — one was measured, the other was
           chosen by the organism while it slept. */
        double att = (double)M->frag[l].attention / (double)RZ_ATTENTION_MAX;
        double spare = 0.5 * merit + 0.5 * att;
        if (spare > 1.0) spare = 1.0;
        frag_keep[l] = keep + (1.0 - keep) * spare;
        if (frag_keep[l] > 1.0) frag_keep[l] = 1.0;

        /* attention fades too, or everything ends up marked */
        double a = (double)M->frag[l].attention * pow(RZ_ATTENTION_KEEP, (double)missed);
        M->frag[l].attention = (uint16_t)a;
    }

    for (int32_t l = 0; l < M->nfrags; l++)
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
        if (M->pool[e].weight <= RZ_EDGE_FLOOR) continue;
        /* Floor, not round-to-nearest. Rounding leaves small weights untouched
           forever — 8 x 0.98 is 7.84, which rounds straight back to 8 — so
           anything faint would never fade and forgetting would quietly do
           nothing at exactly the scale where it matters most. */
        double w = (double)M->pool[e].weight * frag_keep[l];
        uint32_t nw = (uint32_t)w;
        M->pool[e].weight = (nw < RZ_EDGE_FLOOR) ? RZ_EDGE_FLOOR : nw;
    }

    M->decayed_at = m->decay_passes;
    M->dirty = 1;
}

void rz_decay(rz_mind *m)
{
    m->decay_passes++;
    /* The awake ones feel it now, so the effect is visible immediately; the
       sleeping ones settle up when they wake. */
    for (int32_t i = 0; i < m->nresident; i++)
        mod_catch_up(m, m->mod[m->resident[i]]);
}

static rz_module *mod_ensure(rz_mind *m, int32_t k)
{
    m->clock++;

    if (m->mod[k]) {
        for (int32_t i = 0; i < m->nresident; i++)
            if (m->resident[i] == k) { m->used[i] = m->clock; break; }
        mod_catch_up(m, m->mod[k]);
        return m->mod[k];
    }

    if (m->nresident >= RZ_RESIDENT) {
        /* Try the least recently used first, then the next, until one of them
           can actually be let go. */
        int32_t tried = 0, freed = 0;
        while (tried < m->nresident && !freed) {
            int32_t lru = -1;
            for (int32_t i = 0; i < m->nresident; i++) {
                if (m->used[i] == UINT64_MAX) continue;   /* already refused */
                if (lru < 0 || m->used[i] < m->used[lru]) lru = i;
            }
            if (lru < 0) break;
            if (mod_put_to_sleep(m, lru)) freed = 1;
            else { m->used[lru] = UINT64_MAX; tried++; }
        }
        /* restore the marks on anything that refused */
        for (int32_t i = 0; i < m->nresident; i++)
            if (m->used[i] == UINT64_MAX) m->used[i] = m->clock;
        if (!freed) m->held_open++;
    }
    if (m->nresident >= RZ_MODULES) return NULL;

    rz_module *M = malloc(sizeof(rz_module));
    if (!M) return NULL;

    if (mod_read(m, k, M) != RZ_OK) {
        mod_seed(M, k);
        M->decayed_at = m->decay_passes;   /* born now, owes nothing */
    } else {
        mod_catch_up(m, M);                /* it slept through what it missed */
    }

    m->mod[k] = M;
    m->resident[m->nresident] = k;
    m->used[m->nresident] = m->clock;
    m->nresident++;
    m->wakes++;

    if (M->nfrags > m->mod_nfrags[k]) m->mod_nfrags[k] = M->nfrags;
    return M;
}

int32_t rz_resident_count(const rz_mind *m) { return m->nresident; }

const rz_frag *rz_frag_get(rz_mind *m, int32_t id)
{
    if (id < 0 || id >= RZ_FRAGS) return NULL;
    int32_t k = id_mod(id), l = id_slot(id);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return NULL;
    return &M->frag[l];
}

int32_t rz_frag_count(const rz_mind *m)
{
    int32_t n = 0;
    for (int32_t k = 0; k < RZ_MODULES; k++) n += m->mod_nfrags[k];
    return n;
}

size_t rz_expand(rz_mind *m, int32_t frag, unsigned char *out, size_t cap)
{
    const rz_frag *f = rz_frag_get(m, frag);
    if (!f) return 0;
    size_t n = f->span;
    if (n > cap) n = cap;
    memcpy(out, f->atoms, n);
    return n;
}

/* ===================== paths, out of a shared pool ======================== */

/* No per-fragment cap: paths come from the module's pool. When the pool is
   spent the loss is counted, never silent. */
static uint32_t *edge_slot(rz_mind *m, int32_t from, int32_t to)
{
    int32_t k = id_mod(from), l = id_slot(from);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return NULL;

    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next)
        if (M->pool[e].to == to) return &M->pool[e].weight;

    if (M->npool >= RZ_MOD_EDGES) { m->pool_full++; return NULL; }

    int32_t e = M->npool++;
    M->pool[e].to = to;
    M->pool[e].weight = 0;
    M->pool[e].next = M->head[l];
    M->head[l] = e;
    M->dirty = 1;
    return &M->pool[e].weight;
}

uint32_t rz_frag_edge(rz_mind *m, int32_t from, int32_t to)
{
    int32_t k = id_mod(from), l = id_slot(from);
    if (from < 0 || from >= RZ_FRAGS) return 0;
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return 0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next)
        if (M->pool[e].to == to) return M->pool[e].weight;
    return 0;
}

int32_t rz_frag_degree(rz_mind *m, int32_t from)
{
    if (from < 0 || from >= RZ_FRAGS) return 0;
    int32_t k = id_mod(from), l = id_slot(from);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return 0;
    int32_t d = 0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) d++;
    return d;
}

int32_t rz_frag_degree_walked(rz_mind *m, int32_t from)
{
    if (from < 0 || from >= RZ_FRAGS) return 0;
    int32_t k = id_mod(from), l = id_slot(from);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return 0;
    int32_t d = 0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next)
        if (M->pool[e].weight >= RZ_EDGE_FLOOR) d++;
    return d;
}

int32_t rz_frag_edges(rz_mind *m, int32_t from, int32_t *out, int32_t cap)
{
    if (from < 0 || from >= RZ_FRAGS || cap <= 0) return 0;
    int32_t k = id_mod(from), l = id_slot(from);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return 0;
    int32_t n = 0;
    for (int32_t e = M->head[l]; e >= 0 && n < cap; e = M->pool[e].next)
        out[n++] = M->pool[e].to;
    return n;
}

int32_t rz_frag_first_guess(rz_mind *m, int32_t from)
{
    if (from < 0 || from >= RZ_FRAGS) return -1;
    int32_t k = id_mod(from), l = id_slot(from);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return -1;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next)
        if (M->pool[e].weight == RZ_EDGE_INHERITED) return M->pool[e].to;
    return -1;
}

double rz_frag_branching(rz_mind *m, int32_t from)
{
    if (from < 0 || from >= RZ_FRAGS) return 0.0;
    int32_t k = id_mod(from), l = id_slot(from);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return 0.0;

    /* Effort is what it costs to choose among the paths it has TAKEN. Counting
       the ones it was handed at birth made every fragment look equally uncertain
       — 73 ways out inside a word, 92 at the end of one — and buried the very
       difference this measure exists to expose. */
    uint64_t total = 0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
        if (M->pool[e].weight < RZ_EDGE_FLOOR) continue;
        total += M->pool[e].weight;
    }
    if (total == 0) return 0.0;

    double h = 0.0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
        if (M->pool[e].weight < RZ_EDGE_FLOOR) continue;
        double p = (double)M->pool[e].weight / (double)total;
        h -= p * log(p);
    }
    if (h < 0.0) h = 0.0;
    return exp(h);
}

/* ===================== fragments: match, trigger, birth =================== */

/* Local slot in module k whose decomposition is exactly these atoms, or -1. */
static int32_t frag_find_local(rz_module *M, const unsigned char *atoms, uint16_t span)
{
    for (int32_t l = 0; l < M->nfrags; l++)
        if (M->frag[l].span == span && memcmp(M->frag[l].atoms, atoms, span) == 0)
            return l;
    return -1;
}

int32_t rz_longest_match(rz_mind *m, const unsigned char *buf, size_t n)
{
    if (n == 0) return -1;

    int32_t k = (int32_t)buf[0];
    rz_module *M = mod_ensure(m, k);
    if (!M) return rz_atom_id(buf[0]);

    int32_t  best = rz_atom_id(buf[0]);   /* the bare atom always matches */
    uint16_t span = 1;

    for (int32_t l = 0; l < M->nfrags; l++) {
        const rz_frag *f = &M->frag[l];
        if (f->span <= span || (size_t)f->span > n) continue;
        if (memcmp(f->atoms, buf, f->span) == 0) {
            best = k * RZ_MOD_FRAGS + l;
            span = f->span;
        }
    }
    return best;
}

int32_t rz_trigger_ex(rz_mind *m, const unsigned char *buf, size_t n, size_t *overlap)
{
    if (overlap) *overlap = 0;
    if (n == 0) return -1;

    size_t widest = n < RZ_FRAG_SPAN ? n : RZ_FRAG_SPAN;

    for (size_t lap = widest; lap >= 1; lap--) {
        const unsigned char *tail = buf + n - lap;
        int32_t k = (int32_t)tail[0];
        rz_module *M = mod_ensure(m, k);
        if (!M) continue;

        int32_t best = -1;
        uint32_t best_seen = 0;
        uint16_t best_span = 0;

        for (int32_t l = 0; l < M->nfrags; l++) {
            const rz_frag *f = &M->frag[l];
            if (f->span < lap) continue;
            if (memcmp(f->atoms, tail, lap) != 0) continue;  /* the cue covers its prefix */

            /* Nothing left to say and nowhere left to go: an orphan. Try smaller. */
            int has_more = f->span > lap;
            int has_path = M->head[l] >= 0;
            if (!has_more && !has_path) continue;

            if (best < 0 || f->seen > best_seen
                || (f->seen == best_seen && f->span > best_span)) {
                best = k * RZ_MOD_FRAGS + l;
                best_seen = f->seen;
                best_span = f->span;
            }
        }

        if (best >= 0) {
            if (overlap) *overlap = lap;
            return best;
        }
    }

    if (overlap) *overlap = 1;
    return rz_atom_id(buf[n - 1]);
}

int32_t rz_trigger(rz_mind *m, const unsigned char *buf, size_t n)
{
    return rz_trigger_ex(m, buf, n, NULL);
}

/* A pair that keeps travelling together becomes a unit of its own, and it is
   born into the lineage of its left parent — the module it starts with. */
static int32_t frag_birth(rz_mind *m, int32_t a, int32_t b)
{
    unsigned char atoms[RZ_FRAG_SPAN];
    uint16_t span_a, span;

    {   /* copy out first: waking another module can move these pages */
        const rz_frag *fa = rz_frag_get(m, a);
        if (!fa) return -1;
        span_a = fa->span;
        memcpy(atoms, fa->atoms, span_a);
    }
    {
        const rz_frag *fb = rz_frag_get(m, b);
        if (!fb) return -1;
        span = (uint16_t)(span_a + fb->span);
        if (span > RZ_FRAG_SPAN) return -1;
        memcpy(atoms + span_a, fb->atoms, fb->span);
    }

    int32_t k = (int32_t)atoms[0];
    rz_module *M = mod_ensure(m, k);
    if (!M) return -1;
    if (frag_find_local(M, atoms, span) >= 0) return -1;   /* already earned */
    if (M->nfrags >= RZ_MOD_FRAGS) return -1;

    int32_t l = M->nfrags++;
    rz_frag *f = &M->frag[l];
    memset(f, 0, sizeof(*f));
    f->left  = a;
    f->right = b;
    f->span  = span;
    memcpy(f->atoms, atoms, span);
    M->head[l] = -1;
    M->dirty = 1;

    if (M->nfrags > m->mod_nfrags[k]) m->mod_nfrags[k] = M->nfrags;
    m->births++;
    m->awake_births++;

    int32_t born = k * RZ_MOD_FRAGS + l;

    /* A conjunction ends where its tail ends, so it can go wherever its tail
       could go. Without this a newborn arrives with nowhere to walk, and since
       segmentation immediately starts preferring it, the material's paths go
       stale the moment a wider fragment appears — composition eating its own
       graph.
       It inherits the SHAPE, not the strength: each path arrives at weight 1,
       meaning "this is possible", not "this happened often". Experience still
       has to earn the weights. */
    {
        int32_t tos[RZ_MOD_EDGES > 256 ? 256 : RZ_MOD_EDGES];
        int32_t ntos = 0;
        {
            int32_t bk = id_mod(b), bl = id_slot(b);
            rz_module *B = mod_ensure(m, bk);
            if (B && bl < B->nfrags)
                for (int32_t e = B->head[bl]; e >= 0 && ntos < (int32_t)(sizeof tos / sizeof tos[0]);
                     e = B->pool[e].next)
                    tos[ntos++] = B->pool[e].to;
        }
        for (int32_t i = 0; i < ntos; i++) {
            if (tos[i] == born) continue;
            uint32_t *w = edge_slot(m, born, tos[i]);
            if (w && *w == 0) *w = RZ_EDGE_INHERITED;
        }
    }

    return born;
}

/* How far off was it? Not raw surprise — from any fragment there are hundreds
   of continuations, so even the right one carries little probability and raw
   surprise sits near 1.0 for everything, familiar or not. It has to be read
   against CHANCE from that same place: how did the probability it had already
   put on this step compare to knowing nothing at all there?

       p >= 1/degree   it did at least as well as a blind guess   → no strain
       p == 0          it had ruled this out, and it happened     → full strain

   A place it has no expectation from at all scores 1.0: it could not hook the
   step onto anything.

   Asked BEFORE the step is recorded, so the answer cannot be manufactured by
   the step itself. Summed over a meal this is strain — the honest signature of
   material arriving faster than it can be placed. */
static double step_strain(rz_mind *m, int32_t from, int32_t actual)
{
    int32_t k = id_mod(from), l = id_slot(from);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return 1.0;

    uint64_t total = 0, w = 0;
    uint32_t degree = 0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
        if (M->pool[e].weight < RZ_EDGE_FLOOR) continue;   /* experience only */
        total += M->pool[e].weight;
        degree++;
        if (M->pool[e].to == actual) w = M->pool[e].weight;
    }
    if (total == 0 || degree == 0) return 1.0;

    double p      = (double)w / (double)total;
    double chance = 1.0 / (double)degree;
    double strain = 1.0 - p / chance;
    return strain < 0.0 ? 0.0 : strain;
}

/* Segment the material into the widest conjunctions already known, then let the
   traffic between them decide what becomes a unit next. */
static void ingest_fragments(rz_mind *m, const unsigned char *buf, size_t n)
{
    int32_t prev = -1;
    size_t i = 0;

    while (i < n) {
        int32_t f = rz_longest_match(m, buf + i, n - i);
        if (f < 0) break;

        uint16_t span;
        {
            int32_t k = id_mod(f), l = id_slot(f);
            rz_module *M = mod_ensure(m, k);
            if (!M || l >= M->nfrags) break;
            M->frag[l].seen++;
            M->dirty = 1;
            span = M->frag[l].span;
        }

        if (prev >= 0) {
            /* The abstract layer is built by the same traffic, one level up:
               a fragment that LOOKED like this led to one that LOOKED like
               that. Recorded with the shapes as they stood at the time, which
               is the honest thing to record. */
            uint8_t sp = rz_shape_of(m, prev), sf = rz_shape_of(m, f);

            /* Before recording anything, ask the abstraction to call the shot.
               The answer cannot be cheated: the step has not been written yet.
               This is curiosity being satisfied or frustrated, and it is what
               tells the organism which of its ideas are worth having. */
            int32_t expected = rz_predict_shape(m, sp);
            if (expected >= 0) {
                m->shape_tried[sp]++;
                if ((uint8_t)expected == sf) {
                    m->shape_won[sp]++;
                    m->awake_anticipated++;
                }
            }

            /* Not "was the abstract layer right" — noise is easy to call in
               the abstract, where every piece looks like every other piece, and
               reading it that way reports noise as restful. What wears it down
               is how badly the specific step went against what it expected. */
            m->awake_strain += step_strain(m, prev, f);
            m->awake_steps++;

            m->shape_path[(size_t)sp * RZ_SHAPES + sf]++;
            m->shape_steps++;

            /* Every step it takes is an observation. The senses that are awake
               read where it stood and remember what followed -- which is how a
               sense earns its worth without anything ever re-reading the
               material on its behalf. */
            rz_sense_observe(m, prev, f);

            uint32_t *w = edge_slot(m, prev, f);
            if (w) {
                if (*w < RZ_EDGE_FLOOR) *w = RZ_EDGE_FLOOR;  /* a guess just became a memory */
                else if (*w < RZ_CEILING) (*w)++;   /* no path grows forever */
                m->frag_steps++;
                /* Counted in TRAVERSALS, not in stored weight: the first
                   step lands on RZ_EDGE_FLOOR, so N traversals read as
                   N + RZ_EDGE_FLOOR - 1. */
                if (*w == RZ_FRAG_BIRTH + RZ_EDGE_FLOOR - 1) frag_birth(m, prev, f);
            }
        }
        prev = f;
        i += span;
    }
}

/* ===================== life ============================================== */

void rz_init(rz_mind *m)
{
    memset(m, 0, sizeof(*m));
    /* Every atom is already a fragment: the degenerate conjunction of one.
       Its module holds it in slot 0 and is woken on demand. */
    for (int32_t k = 0; k < RZ_MODULES; k++) m->mod_nfrags[k] = 1;
}

void rz_free(rz_mind *m)
{
    for (int32_t k = 0; k < RZ_MODULES; k++) {
        free(m->mod[k]);
        m->mod[k] = NULL;
    }
    m->nresident = 0;
}

void rz_ingest(rz_mind *m, const unsigned char *buf, size_t n)
{
    if (n == 0) return;

    for (size_t i = 0; i < n; i++) m->atom_seen[buf[i]]++;
    m->atoms_ingested += (uint64_t)n;

    /* A buffer boundary is not a transition: the walk starts and ends here. */
    for (size_t i = 0; i + 1 < n; i++) {
        m->path[(size_t)buf[i] * RZ_ATOMS + buf[i + 1]]++;
        m->traversals++;
    }

    ingest_fragments(m, buf, n);

    /* Decay is paid in experience, not in time: a mind that spent the interval
       thinking should not forget as much as one that spent it switched off. */
    while (m->frag_steps >= (m->decay_passes + 1) * (uint64_t)RZ_DECAY_EVERY)
        rz_decay(m);
}

uint64_t rz_cold_edges(rz_mind *m)
{
    uint64_t cold = 0;
    for (int32_t k = 0; k < RZ_MODULES; k++) {
        if (m->mod_nfrags[k] <= 1) continue;
        rz_module *M = mod_ensure(m, k);
        if (!M) continue;
        for (int32_t e = 0; e < M->npool; e++)
            if (M->pool[e].weight <= RZ_EDGE_FLOOR) cold++;
    }
    return cold;
}

/* ===================== recall ============================================ */

static int strongest_next_atom(const rz_mind *m, unsigned char from, unsigned char *next)
{
    const uint32_t *row = &m->path[(size_t)from * RZ_ATOMS];
    uint32_t best = 0;
    int found = -1;
    for (size_t i = 0; i < RZ_ATOMS; i++)
        if (row[i] > best) { best = row[i]; found = (int)i; }
    if (found < 0) return 0;
    *next = (unsigned char)found;
    return 1;
}

double rz_branching(const rz_mind *m, unsigned char from)
{
    const uint32_t *row = &m->path[(size_t)from * RZ_ATOMS];

    uint64_t total = 0;
    for (size_t i = 0; i < RZ_ATOMS; i++) total += row[i];
    if (total == 0) return 0.0;

    double h = 0.0;
    for (size_t i = 0; i < RZ_ATOMS; i++) {
        if (row[i] == 0) continue;
        double p = (double)row[i] / (double)total;
        h -= p * log(p);
    }
    if (h < 0.0) h = 0.0;
    return exp(h);
}

void rz_recall_from(const rz_mind *m,
                    const unsigned char *cue, size_t cue_len,
                    size_t want, uint64_t effort_budget,
                    rz_recall *out)
{
    memset(out, 0, sizeof(*out));

    if (cue_len == 0 || want == 0) { out->gave_up = 1; return; }
    if (want > RZ_RECALL_MAX) want = RZ_RECALL_MAX;

    unsigned char cur = cue[cue_len - 1];

    while (out->len < want) {
        double b = rz_branching(m, cur);
        if (b <= 0.0) { out->gave_up = 1; return; }

        uint64_t cost = (uint64_t)ceil(b - 1e-9);
        if (cost < 1) cost = 1;
        if (out->effort + cost > effort_budget) { out->gave_up = 1; return; }

        unsigned char next;
        if (!strongest_next_atom(m, cur, &next)) { out->gave_up = 1; return; }

        out->effort += cost;
        out->atoms[out->len++] = next;
        cur = next;
    }
}

/* --- vertical composition (P18) ------------------------------------------ */

static uint8_t class4(double x, double a, double b, double c)
{
    if (x < a) return 0;
    if (x < b) return 1;
    if (x < c) return 2;
    return 3;
}

uint8_t rz_shape_of(rz_mind *m, int32_t frag)
{
    if (frag < 0 || frag >= RZ_FRAGS) return 0;
    int32_t k = id_mod(frag), l = id_slot(frag);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return 0;

    /* One definition, in shape_of_local: how decided it is, how wide, how
       concentrated. Identity plays no part — two fragments with nothing in
       common can carry the same shape. */
    return shape_of_local(m, M, l);
}

uint32_t rz_shape_members(rz_mind *m, uint8_t shape)
{
    uint32_t n = 0;
    for (int32_t k = 0; k < RZ_MODULES; k++) {
        if (m->mod_nfrags[k] <= 1) continue;
        rz_module *M = mod_ensure(m, k);
        if (!M) continue;
        int32_t nf = M->nfrags;
        for (int32_t l = 0; l < nf; l++)
            if (rz_shape_of(m, k * RZ_MOD_FRAGS + l) == shape) n++;
    }
    return n;
}

double rz_shape_merit(const rz_mind *m, uint8_t shape)
{
    if (shape >= RZ_SHAPES || m->shape_tried[shape] == 0) return 0.0;
    return (double)m->shape_won[shape] / (double)m->shape_tried[shape];
}

uint32_t rz_shapes_that_transfer(const rz_mind *m)
{
    uint32_t n = 0;
    for (uint8_t s = 0; s < RZ_SHAPES; s++)
        if (m->shape_tried[s] >= RZ_SHAPE_TESTED
            && rz_shape_merit(m, s) > 1.0 / (double)RZ_SHAPES) n++;
    return n;
}

int32_t rz_predict_shape(const rz_mind *m, uint8_t from)
{
    if (from >= RZ_SHAPES) return -1;
    const uint32_t *row = &m->shape_path[(size_t)from * RZ_SHAPES];

    int32_t best = -1;
    uint32_t w = 0;
    for (int32_t t = 0; t < RZ_SHAPES; t++)
        if (row[t] > w) { w = row[t]; best = t; }
    return best;
}

/* --- the fixed external bar (D23) ---------------------------------------- */

int32_t rz_predict_next(rz_mind *m, int32_t from)
{
    if (from < 0 || from >= RZ_FRAGS) return -1;
    int32_t k = id_mod(from), l = id_slot(from);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return -1;

    int32_t next = -1;
    uint32_t best = 0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
        if (M->pool[e].weight == 0) continue;
        if (next < 0 || M->pool[e].weight > best
            || (M->pool[e].weight == best && M->pool[e].to < next)) {
            best = M->pool[e].weight;
            next = M->pool[e].to;
        }
    }
    return next;
}

enum { RZ_STEP_OPAQUE, RZ_STEP_FRONTIER, RZ_STEP_MASTERED };

/* One step, read two ways: how surprising it was, and where it sits relative to
   what the organism already knows. `*novel` is set when it knew the context but
   had never taken this exact step — the most interesting case there is. */
static int step_appraise(rz_mind *m, int32_t from, int32_t actual,
                         double *surprise, int *novel)
{
    *novel = 0;

    int32_t k = id_mod(from), l = id_slot(from);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) { *surprise = 1.0; return RZ_STEP_OPAQUE; }

    /* Measured against EXPERIENCE, not against possibility. A path it was handed
       at birth says "this could happen"; it never said "this happens". Counting
       those ~90 gifts alongside real traffic diluted every probability and left
       no fragment able to report that it was genuinely blind. */
    uint64_t total = 0, w = 0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
        if (M->pool[e].weight < RZ_EDGE_FLOOR) continue;
        total += M->pool[e].weight;
        if (M->pool[e].to == actual) w = M->pool[e].weight;
    }

    if (total == 0) {                    /* no experience at all: nothing to hook onto */
        *surprise = 1.0;
        return RZ_STEP_OPAQUE;
    }

    double p = (double)w / (double)total;
    *surprise = 1.0 - p;

    if (p >= RZ_MASTERY) return RZ_STEP_MASTERED;

    *novel = (w == 0);                   /* knew where it was, had never gone here */
    return RZ_STEP_FRONTIER;
}

void rz_measure(rz_mind *m, const unsigned char *buf, size_t n,
                rz_measure_result *out)
{
    memset(out, 0, sizeof(*out));
    if (n == 0) return;
    out->atoms = (uint64_t)n;

    int32_t prev = -1;
    size_t i = 0;

    while (i < n) {
        int32_t f = rz_longest_match(m, buf + i, n - i);
        if (f < 0) break;

        uint16_t span;
        {   /* copy out before anything else can page this module away */
            const rz_frag *fp = rz_frag_get(m, f);
            if (!fp) break;
            span = fp->span;
        }

        if (prev >= 0) {
            /* The guess is committed before the answer is looked at, and
               nothing here writes: the bar measures and never teaches. */
            int32_t guess = rz_predict_next(m, prev);
            out->predictions++;
            if (guess == f) out->hits++;

            double s = 1.0;
            int novel = 0;
            int place = step_appraise(m, prev, f, &s, &novel);

            if (place == RZ_STEP_OPAQUE) {
                /* It has never walked out of here. The specific knows nothing —
                   so ask what a place SHAPED like this usually leads to. This is
                   the only thing in the system that carries what was learned
                   into somewhere it has never been. */
                int32_t guessed = rz_predict_shape(m, rz_shape_of(m, prev));
                if (guessed >= 0) {
                    out->shape_tried++;
                    if ((uint8_t)guessed == rz_shape_of(m, f)) out->shape_hits++;
                }
            }

            switch (place) {
            case RZ_STEP_OPAQUE:   out->opaque++;   break;
            case RZ_STEP_MASTERED: out->mastered++; break;
            default:               out->frontier++; if (novel) out->novel++; break;
            }
            out->surprise += s;
        }

        prev = f;
        i += span;
    }
}

/* Deterministic wandering, and deterministic drawing: the same seed replays the
   same lateral walk, and draws the same sense. */
static uint64_t xorshift(uint64_t *s)
{
    uint64_t x = *s ? *s : 0x9E3779B97F4A7C15ull;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

/* --- senses --------------------------------------------------------------- */

static const uint8_t SENSE_RANGE[RZ_WHAT_COUNT] = {
    RZ_SHAPES,   /* SHAPE   — which of the shape classes */
    8,           /* WIDER   — spans more than 0..7 atoms */
    8,           /* BUSIER  — walked more than 2^0..2^7 times */
    8,           /* FORKED  — more than 0..7 walked ways out */
    1,           /* GUESSED — a yes/no question */
    1,           /* MARKED  — a yes/no question */
};

/* Which shapes anybody actually carries. Drawing a sense that asks about a
   class nobody occupies is drawing a sense born dead, and with 64 classes and
   ~28 occupied that was half the draws. The vocabulary of a sense has to be the
   vocabulary of the organism. */
static uint8_t sense_pick_shape(rz_mind *m, uint64_t *r)
{
    uint8_t live[RZ_SHAPES];
    uint8_t n = 0;
    for (uint8_t sh = 0; sh < RZ_SHAPES && n < RZ_SHAPES; sh++)
        if (rz_shape_members(m, sh) > 0) live[n++] = sh;
    if (n == 0) return (uint8_t)(xorshift(r) % RZ_SHAPES);
    return live[xorshift(r) % n];
}

rz_sense rz_sense_draw(rz_mind *m, uint64_t seed)
{
    uint64_t r = seed ? seed : 1;
    rz_sense s;
    s.what   = (uint8_t)(xorshift(&r) % RZ_WHAT_COUNT);
    s.value  = (s.what == RZ_WHAT_SHAPE) ? sense_pick_shape(m, &r)
                                         : (uint8_t)(xorshift(&r) % SENSE_RANGE[s.what]);
    s.dir    = (uint8_t)(xorshift(&r) % RZ_DIR_COUNT);
    s.radius = (uint8_t)(1 + xorshift(&r) % RZ_SENSE_RADIUS);
    s.src    = 0;
    return s;
}

rz_sense rz_sense_mutate(rz_mind *m, const rz_sense *parent, uint64_t seed)
{
    uint64_t r = seed ? seed : 1;
    rz_sense s = *parent;
    switch (xorshift(&r) % 5) {
    case 0:
        s.what = (uint8_t)((parent->what + 1 + xorshift(&r) % (RZ_WHAT_COUNT - 1))
                           % RZ_WHAT_COUNT);
        s.value = (s.what == RZ_WHAT_SHAPE) ? sense_pick_shape(m, &r)
                                            : (uint8_t)(xorshift(&r) % SENSE_RANGE[s.what]);
        break;
    case 1: {
        if (s.what == RZ_WHAT_SHAPE) { s.value = sense_pick_shape(m, &r); break; }
        uint8_t n = SENSE_RANGE[s.what];
        s.value = (uint8_t)(n > 1 ? (parent->value + 1 + xorshift(&r) % (n - 1)) % n
                                  : parent->value);
        if (n <= 1) s.dir = (uint8_t)((parent->dir + 1) % RZ_DIR_COUNT);
        break;
    }
    case 2:
        s.dir = (uint8_t)((parent->dir + 1) % RZ_DIR_COUNT);
        break;
    case 3:
        s.radius = (uint8_t)(1 + (parent->radius % RZ_SENSE_RADIUS));
        break;
    default:
        /* build on another sense, if there is one to build on. This is the
           mutation that makes the stack possible at all. */
        if (m->nsenses > 0)
            s.src = (uint8_t)(1 + xorshift(&r) % (uint64_t)m->nsenses);
        else
            s.radius = (uint8_t)(1 + (parent->radius % RZ_SENSE_RADIUS));
        break;
    }
    return s;
}

int rz_sense_same(const rz_sense *a, const rz_sense *b)
{
    return a->what == b->what && a->value == b->value
        && a->dir == b->dir && a->radius == b->radius;
}

uint32_t rz_sense_distance(const rz_sense *a, const rz_sense *b)
{
    uint32_t d = 0;
    if (a->what != b->what) d++;
    else if (a->value != b->value) d++;   /* value belongs to what: one change */
    if (a->dir != b->dir) d++;
    if (a->radius != b->radius) d++;
    if (a->src != b->src) d++;
    return d;
}

/* Already at hand, or nothing. A glance may not wake a module from disk: with a
   handful resident they evict each other on every look, and the senses were
   paying seven disk loads per reading. What is asleep is simply not seen. */
static rz_module *mod_at_hand(rz_mind *m, int32_t k)
{
    if (k < 0 || k >= RZ_MODULES) return NULL;
    return m->mod[k];
}

/* The three questions a glance asks, answered only from what is resident. */
static const rz_frag *frag_at_hand(rz_mind *m, int32_t f)
{
    if (f < 0 || f >= RZ_FRAGS) return NULL;
    rz_module *M = mod_at_hand(m, id_mod(f));
    int32_t l = id_slot(f);
    if (!M || l >= M->nfrags) return NULL;
    return &M->frag[l];
}

static uint8_t shape_at_hand(rz_mind *m, int32_t f)
{
    if (f < 0 || f >= RZ_FRAGS) return 0;
    rz_module *M = mod_at_hand(m, id_mod(f));
    int32_t l = id_slot(f);
    if (!M || l >= M->nfrags) return 0;
    return shape_of_local(m, M, l);
}

static int32_t degree_at_hand(rz_mind *m, int32_t f, int walked_only)
{
    if (f < 0 || f >= RZ_FRAGS) return 0;
    rz_module *M = mod_at_hand(m, id_mod(f));
    int32_t l = id_slot(f);
    if (!M || l >= M->nfrags) return 0;
    int32_t d = 0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next)
        if (!walked_only || M->pool[e].weight >= RZ_EDGE_FLOOR) d++;
    return d;
}

static int32_t edges_at_hand(rz_mind *m, int32_t f, int32_t *out, int32_t cap)
{
    if (f < 0 || f >= RZ_FRAGS || cap <= 0) return 0;
    rz_module *M = mod_at_hand(m, id_mod(f));
    int32_t l = id_slot(f);
    if (!M || l >= M->nfrags) return 0;
    int32_t n = 0;
    for (int32_t e = M->head[l]; e >= 0 && n < cap; e = M->pool[e].next)
        out[n++] = M->pool[e].to;
    return n;
}

/* Does this fragment answer the question the way the sense is looking for?
   Every question here is one the organism can already ask about itself. */
static int sense_holds(rz_mind *m, const rz_sense *s, int32_t f)
{
    const rz_frag *fg = frag_at_hand(m, f);
    if (!fg) return 0;                       /* asleep is simply not seen */
    switch (s->what) {
    case RZ_WHAT_SHAPE:   return shape_at_hand(m, f) == s->value;
    case RZ_WHAT_WIDER:   return fg->span > s->value;
    case RZ_WHAT_BUSIER:  return fg->seen > (uint32_t)(1u << s->value);
    case RZ_WHAT_FORKED:  return degree_at_hand(m, f, 1) > (int32_t)s->value;
    case RZ_WHAT_GUESSED: return degree_at_hand(m, f, 0) > degree_at_hand(m, f, 1);
    case RZ_WHAT_MARKED:  return fg->attention > 0;
    default:              return 0;
    }
}

uint32_t rz_sense_read(rz_mind *m, const rz_sense *s, int32_t here)
{
    if (here < 0 || here >= RZ_FRAGS) return 0;

    /* One step. Not a radius.

       Looking out to radius 3 in a graph where a place has dozens of ways out
       meant a single reading touched thousands of fragments, and the faculty
       cost more than the organism it served. A sense sees the NEXT MOVE, and
       its reach comes from watching many moves rather than from sweeping a
       neighbourhood on each one.

       Downward is the exception and it is free: a fragment decomposes into two,
       so the whole descent to `radius` is at most a handful of nodes. */
    uint32_t count = 0;

    if (s->dir == RZ_DIR_DOWN) {
        int32_t front[8], next[8];
        int32_t nf = 0;
        front[nf++] = here;
        for (uint8_t step = 0; step < s->radius && nf > 0; step++) {
            int32_t nn = 0;
            for (int32_t i = 0; i < nf; i++) {
                const rz_frag *fg = frag_at_hand(m, front[i]);
                if (!fg) continue;
                int32_t kids[2] = { fg->left, fg->right };
                for (int k = 0; k < 2; k++) {
                    if (kids[k] < 0) continue;
                    if (sense_holds(m, s, kids[k])) count++;
                    if (nn < 8) next[nn++] = kids[k];
                }
            }
            for (int32_t i = 0; i < nn; i++) front[i] = next[i];
            nf = nn;
        }
        return count;
    }

    int32_t tos[16];
    int32_t nt = edges_at_hand(m, here, tos, 16);
    for (int32_t k = 0; k < nt; k++)
        if (sense_holds(m, s, tos[k])) count++;
    return count;
}

/* --- what a sense is worth, and the life of a population ------------------ */

static uint32_t sense_bucket(uint32_t v)
{
    /* the reading, coarsened. A sense is worth something for the DISTINCTIONS it
       draws, not for the exact number it happens to land on. */
    if (v == 0) return 0;
    if (v == 1) return 1;
    if (v <= 3) return 2;
    if (v <= 7) return 3;
    if (v <= 20) return 4;
    return 5;
}

/* Reading a sense that stands on another sense. Composition is this one branch:
   what it counts is places where the sense below it reads above a bar. The
   output of one becomes the material of the next, which is the only reason a
   population of senses is more than a list of detectors. */
static uint32_t sense_read_at(rz_mind *m, const rz_sense *s, int32_t here, int depth)
{
    if (s->src == 0 || depth > 2) return rz_sense_read(m, s, here);
    int32_t i = (int32_t)s->src - 1;
    if (i < 0 || i >= m->nsenses) return rz_sense_read(m, s, here);

    int32_t tos[12];
    int32_t nt = rz_frag_edges(m, here, tos, 12);
    uint32_t count = 0;
    for (int32_t k = 0; k < nt; k++)
        if (sense_read_at(m, &m->senses[i], tos[k], depth + 1) > s->value) count++;
    return count;
}

int32_t rz_sense_add(rz_mind *m, rz_sense s)
{
    if (m->nsenses >= RZ_SENSES) return -1;
    int32_t i = m->nsenses++;
    m->senses[i] = s;
    m->sense_worth[i] = 0.0;
    m->sense_n[i] = 0;
    memset(m->sense_obs[i], 0, sizeof m->sense_obs[i]);
    memset(m->sense_row[i], 0, sizeof m->sense_row[i]);
    return i;
}

uint64_t rz_sense_observations(const rz_mind *m, int32_t i)
{
    return (i >= 0 && i < m->nsenses) ? m->sense_n[i] : 0;
}

/* What this sense is worth, read off what it has already watched. Nothing walks
   here: the information is in the counters the steps left behind. */
static double sense_worth_now(const rz_mind *m, int32_t i)
{
    if (i < 0 || i >= m->nsenses) return 0.0;
    uint64_t total = m->sense_n[i];
    if (total < 32) return 0.0;

    double info = 0.0;
    for (uint32_t b = 0; b < RZ_SENSE_BUCKETS; b++) {
        uint32_t rn = m->sense_row[i][b];
        if (!rn) continue;
        for (uint32_t sh = 0; sh < RZ_SHAPES; sh++) {
            uint32_t k = m->sense_obs[i][b][sh];
            if (!k) continue;
            double pxy = (double)k / (double)total;
            double px = (double)rn / (double)total;
            double py = (double)m->sense_col[sh] / (double)m->sense_col_n;
            if (py <= 0.0) continue;
            info += pxy * log(pxy / (px * py));
        }
    }
    return info < 0.0 ? 0.0 : info;
}

/* Who is awake. The hot set is simply the best it has, and a sleeping sense
   still gets a turn in rotation -- asleep must not mean buried, or the pool
   freezes into whatever happened to be hot first and can never be corrected. */
static void sense_watch(rz_mind *m, int32_t idx, int32_t from, uint8_t next_shape)
{
    uint32_t b = sense_bucket(sense_read_at(m, &m->senses[idx], from, 0));
    if (b >= RZ_SENSE_BUCKETS || next_shape >= RZ_SHAPES) return;
    m->sense_obs[idx][b][next_shape]++;
    m->sense_row[idx][b]++;
    m->sense_n[idx]++;
    m->sense_reads++;
}

void rz_sense_observe(rz_mind *m, int32_t from, int32_t to)
{
    if (m->nsenses <= 0 || from < 0 || to < 0) return;
    uint8_t next_shape = shape_at_hand(m, to);
    if (next_shape >= RZ_SHAPES) return;

    m->sense_col[next_shape]++;
    m->sense_col_n++;

    /* the awake ones: the best it currently holds */
    int32_t hot[RZ_SENSE_HOT];
    int32_t nh = 0;
    for (int32_t i = 0; i < m->nsenses; i++) {
        if (nh < RZ_SENSE_HOT) {
            int32_t j = nh++;
            while (j > 0 && m->sense_worth[hot[j - 1]] < m->sense_worth[i]) {
                hot[j] = hot[j - 1];
                j--;
            }
            hot[j] = i;
        } else if (m->sense_worth[i] > m->sense_worth[hot[RZ_SENSE_HOT - 1]]) {
            int32_t j = RZ_SENSE_HOT - 1;
            while (j > 0 && m->sense_worth[hot[j - 1]] < m->sense_worth[i]) {
                hot[j] = hot[j - 1];
                j--;
            }
            hot[j] = i;
        }
    }
    for (int32_t k = 0; k < nh; k++) sense_watch(m, hot[k], from, next_shape);

    /* and one sleeping one, by rotation, so it can earn its way back */
    if (m->nsenses > nh) {
        int32_t cold = (int32_t)(m->sense_turn % (uint32_t)m->nsenses);
        m->sense_turn++;
        int awake = 0;
        for (int32_t k = 0; k < nh; k++) if (hot[k] == cold) { awake = 1; break; }
        if (!awake) sense_watch(m, cold, from, next_shape);
    }

    /* worth is refreshed cheaply, from the counters, never by re-reading */
    for (int32_t k = 0; k < nh; k++) m->sense_worth[hot[k]] = sense_worth_now(m, hot[k]);
}

double rz_sense_worth(rz_mind *m, const rz_sense *s,
                      const unsigned char *buf, size_t n)
{
    /* How much does knowing this sense's reading tell the organism about what is
       coming next? Information, in nats, measured over material. It learns
       nothing while being asked -- the same discipline as the external bar. */
    static uint32_t joint[RZ_SENSE_BUCKETS][RZ_SHAPES];
    static uint32_t rowc[RZ_SENSE_BUCKETS], colc[RZ_SHAPES];
    memset(joint, 0, sizeof joint);
    memset(rowc, 0, sizeof rowc);
    memset(colc, 0, sizeof colc);

    uint64_t total = 0;
    int32_t prev = -1;
    size_t i = 0;
    while (i < n) {
        int32_t f = rz_longest_match(m, buf + i, n - i);
        if (f < 0) break;
        const rz_frag *fg = rz_frag_get(m, f);
        if (!fg) break;
        uint16_t span = fg->span;
        if (prev >= 0) {
            uint32_t b = sense_bucket(sense_read_at(m, s, prev, 0));
            uint8_t sh = rz_shape_of(m, f);
            if (b < RZ_SENSE_BUCKETS && sh < RZ_SHAPES) {
                joint[b][sh]++;
                rowc[b]++;
                colc[sh]++;
                total++;
            }
        }
        prev = f;
        i += span;
    }
    if (total < 32) return 0.0;

    double info = 0.0;
    for (uint32_t b = 0; b < RZ_SENSE_BUCKETS; b++) {
        if (!rowc[b]) continue;
        for (uint32_t sh = 0; sh < RZ_SHAPES; sh++) {
            if (!joint[b][sh]) continue;
            double pxy = (double)joint[b][sh] / (double)total;
            double px = (double)rowc[b] / (double)total;
            double py = (double)colc[sh] / (double)total;
            info += pxy * log(pxy / (px * py));
        }
    }
    return info < 0.0 ? 0.0 : info;
}

int32_t rz_senses_count(const rz_mind *m) { return m->nsenses; }

const rz_sense *rz_sense_at(const rz_mind *m, int32_t i)
{
    return (i >= 0 && i < m->nsenses) ? &m->senses[i] : NULL;
}

double rz_sense_score(const rz_mind *m, int32_t i)
{
    return (i >= 0 && i < m->nsenses) ? m->sense_worth[i] : 0.0;
}

void rz_sense_describe(const rz_mind *m, int32_t i, char *out, size_t cap)
{
    static const char *WHAT[] = { "shape", "wider than", "busier than",
                                  "forked more than", "given not taken", "marked" };
    if (i < 0 || i >= m->nsenses || cap == 0) { if (cap) out[0] = 0; return; }
    const rz_sense *s = &m->senses[i];
    if (s->src)
        snprintf(out, cap, "where #%u reads over %u, %s within %u",
                 (unsigned)s->src - 1u, (unsigned)s->value,
                 s->dir == RZ_DIR_DOWN ? "downward" : "ahead", (unsigned)s->radius);
    else
        snprintf(out, cap, "%s %u, %s within %u",
                 WHAT[s->what % RZ_WHAT_COUNT], (unsigned)s->value,
                 s->dir == RZ_DIR_DOWN ? "downward" : "ahead", (unsigned)s->radius);
}

/* What this sense adds BEYOND the ones already held. A sense that repeats what a
   near neighbour already says is occupying a slot for nothing -- and that, not
   uselessness, is the only thing worth pruning for. */
#define SENSE_BIRTH_GATE   0.0000   /* born unless it says literally nothing */
#define SENSE_PRUNE_GATE   0.0005   /* dropped only if a neighbour already says it */
#define SENSE_SWAP_GATE    1.5      /* replaced only if clearly better */

static int sense_is_redundant(rz_mind *m, int32_t idx)
{
    /* Redundant means ANOTHER sense already says what this one says. Weak does
       NOT mean redundant, and the difference is the whole design: an earlier
       version returned the sense own worth when it had no neighbour, so a quiet
       sense with nothing like it was dropped for being quiet -- pruning for
       usefulness, the exact thing this gate exists to prevent. With no near
       neighbour there is nothing to be redundant WITH, so it stays. */
    double mine = m->sense_worth[idx];
    for (int32_t j = 0; j < m->nsenses; j++) {
        if (j == idx) continue;
        /* Only an EXACT twin counts. Distance 1 is how a newborn always
           relates to its parent, so treating that as sameness made every birth
           redundant by construction -- and the pool could never fill. The
           reference organism keeps a deliberately enormous cold pool; the
           hot/cold split is what makes holding them affordable, not pruning. */
        if (rz_sense_same(&m->senses[idx], &m->senses[j])
            && m->sense_worth[j] >= mine - SENSE_PRUNE_GATE)
            return 1;
    }
    return 0;
}

/* The gates, asymmetric on purpose, taken from the reference organism: born
   easily, pruned only when it actively costs something, replaced only by a
   clearly better challenger -- and a merely NEUTRAL sense is kept.
   Keeping the neutral ones is the whole point. A sense that says little today is
   what another sense is built from tomorrow, and an organism that prunes for
   usefulness starves its own composition. */

/* A round of living. It walks nothing.
 *
 * Worth is already there, left behind by the steps the organism took while it
 * was eating. What happens here is only what cannot happen mid-step: a birth,
 * and an audit of one old sense.
 *
 * Birth is INSTANT, as in the reference organism -- a newborn enters expressed
 * and in use, with no trial period and nothing to prove yet. It has watched
 * nothing, so it is worth nothing, and gating on that would mean no sense could
 * ever be born. Quality is looked after afterwards, by the audit, without
 * holding up the learning. */
void rz_senses_live(rz_mind *m, uint64_t seed)
{
    uint64_t r = seed ? seed : 1;

    /* --- a newborn, as a divergent copy of one it already holds --- */
    if (m->nsenses < RZ_SENSES) {
        rz_sense baby;
        if (m->nsenses == 0) {
            baby = rz_sense_draw(m, xorshift(&r));
        } else {
            int32_t parent = (int32_t)(xorshift(&r) % (uint64_t)m->nsenses);
            baby = rz_sense_mutate(m, &m->senses[parent], xorshift(&r));
        }
        if (rz_sense_add(m, baby) >= 0) m->senses_born++;
    }

    if (m->nsenses < 3) return;

    /* --- and an audit of one old one, chosen at random --- */
    int32_t victim = (int32_t)(xorshift(&r) % (uint64_t)m->nsenses);

    /* A sense that has not watched enough is not judged. Dropping one for
       saying little before it has had the chance to say anything is how a
       population stops accumulating. */
    if (m->sense_n[victim] < 64) return;

    if (xorshift(&r) % 100 < 55) {
        if (sense_is_redundant(m, victim)) {
            for (int32_t j = victim; j + 1 < m->nsenses; j++) {
                m->senses[j] = m->senses[j + 1];
                m->sense_worth[j] = m->sense_worth[j + 1];
                m->sense_n[j] = m->sense_n[j + 1];
                memcpy(m->sense_obs[j], m->sense_obs[j + 1], sizeof m->sense_obs[j]);
                memcpy(m->sense_row[j], m->sense_row[j + 1], sizeof m->sense_row[j]);
            }
            m->nsenses--;
            m->senses_pruned++;
        } else {
            m->senses_kept_neutral++;   /* it says little, and it stays */
        }
    } else {
        /* A challenger cannot be compared yet -- it has watched nothing. So it
           replaces nobody here; it is added, and the two are left to earn their
           worth against the same steps. Whichever turns out redundant is the
           one the audit drops later. */
        rz_sense chal = rz_sense_mutate(m, &m->senses[victim], xorshift(&r));
        if (rz_sense_add(m, chal) >= 0) m->senses_born++;
    }
}


void rz_measure_harmony(rz_mind *m, const unsigned char *buf, size_t n,
                        rz_harmony *out)
{
    out->cost = 0.0;
    out->unaccounted = 0.0;
    out->steps = 0;

    if (!buf || n == 0) return;

    uint64_t lost = 0;
    double cost = 0.0;
    int32_t prev = -1;
    size_t i = 0;

    while (i < n) {
        int32_t f = rz_longest_match(m, buf + i, n - i);
        if (f < 0) break;

        uint16_t span;
        {
            const rz_frag *fg = rz_frag_get(m, f);
            if (!fg) break;
            span = fg->span;
        }

        if (prev >= 0) {
            /* What the step cost: the effective branching it had to choose
               among. Logged, so a walk is the sum of its choices rather than
               the product.

               A step it has NO experience for is not free. Reading it as free
               made ignorance look cheap and handed the collapse the very
               shortcut this measure exists to close: a mind that never met the
               material would win by describing nothing. Unknown material costs
               what it costs to spell out — the whole alphabet. */
            double b = rz_frag_branching(m, prev);
            if (b < 1.0) { lost++; b = (double)RZ_ATOMS; }
            cost += log(b);
            out->steps++;
        }

        prev = f;
        i += span;
    }

    /* The TOTAL, not a mean. D39 says describe the same material with LESS,
       and less is an amount. A mean per step would reward chopping the walk
       into many cheap steps, which is a different organisation of the same
       material — exactly the thing being compared. */
    out->cost = cost;
    if (out->steps) out->unaccounted = (double)lost / (double)out->steps;
}

int rz_more_harmonious(const rz_harmony *a, const rz_harmony *b)
{
    /* Cheaper is only better if nothing more was dropped to buy it. */
    if (a->unaccounted > b->unaccounted) return 0;
    return a->cost < b->cost;
}

double rz_shape_rescue(const rz_measure_result *r)
{
    return r->shape_tried ? (double)r->shape_hits / (double)r->shape_tried : 0.0;
}

double rz_opaque_share(const rz_measure_result *r)
{
    return r->predictions ? (double)r->opaque / (double)r->predictions : 0.0;
}
double rz_frontier_share(const rz_measure_result *r)
{
    return r->predictions ? (double)r->frontier / (double)r->predictions : 0.0;
}
double rz_mastered_share(const rz_measure_result *r)
{
    return r->predictions ? (double)r->mastered / (double)r->predictions : 0.0;
}
double rz_novel_share(const rz_measure_result *r)
{
    return r->predictions ? (double)r->novel / (double)r->predictions : 0.0;
}

/* --- the appetite ledger -------------------------------------------------- */

uint64_t rz_fingerprint(const unsigned char *buf, size_t n)
{
    uint64_t h = 1469598103934665603ull;          /* FNV-1a, 64 bit */
    for (size_t i = 0; i < n; i++) {
        h ^= (uint64_t)buf[i];
        h *= 1099511628211ull;
    }
    h ^= (uint64_t)n;                             /* length matters too */
    h *= 1099511628211ull;
    return h;
}

void rz_ledger_init(rz_ledger *L)
{
    memset(L, 0, sizeof(*L));
}

const rz_appraisal *rz_ledger_find(const rz_ledger *L, const char *path,
                                   uint64_t fingerprint)
{
    for (int32_t i = 0; i < L->n; i++)
        if (L->item[i].fingerprint == fingerprint
            && strcmp(L->item[i].path, path) == 0)
            return &L->item[i];
    return NULL;   /* new, or the material changed — not the same thing */
}

void rz_ledger_record(rz_ledger *L, const char *path, uint64_t fingerprint,
                      double surprise, double frontier)
{
    rz_appraisal *slot = NULL;
    for (int32_t i = 0; i < L->n; i++)
        if (strcmp(L->item[i].path, path) == 0) { slot = &L->item[i]; break; }

    if (!slot) {
        if (L->n >= RZ_APPRAISALS) return;
        slot = &L->item[L->n++];
    }

    snprintf(slot->path, sizeof slot->path, "%s", path);
    slot->fingerprint = fingerprint;
    slot->surprise = surprise;
    slot->frontier = frontier;
    slot->run = L->run;
}

#define RZ_LEDGER_VER ((uint32_t)(7u + 1000u * (uint32_t)sizeof(rz_appraisal)))

int rz_ledger_save(rz_ledger *L, const char *path)
{
    L->run++;
    for (int32_t i = 0; i < L->n; i++)
        if (L->item[i].run == 0) L->item[i].run = L->run;

    FILE *f = fopen(path, "wb");
    if (!f) return RZ_ERR_IO;

    uint32_t ver = RZ_LEDGER_VER;
    int ok = (fwrite(&ver, sizeof ver, 1, f) == 1)
          && (fwrite(&L->run, sizeof L->run, 1, f) == 1)
          && (fwrite(&L->n, sizeof L->n, 1, f) == 1)
          && (L->n == 0 || fwrite(L->item, sizeof(rz_appraisal), (size_t)L->n, f) == (size_t)L->n);
    if (fclose(f) != 0) ok = 0;
    return ok ? RZ_OK : RZ_ERR_IO;
}

int rz_ledger_load(rz_ledger *L, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return RZ_ERR_MISSING;

    uint32_t ver = 0;
    uint64_t run = 0;
    int32_t n = 0;
    if (fread(&ver, sizeof ver, 1, f) != 1 || ver != RZ_LEDGER_VER
        || fread(&run, sizeof run, 1, f) != 1
        || fread(&n, sizeof n, 1, f) != 1
        || n < 0 || n > RZ_APPRAISALS) {
        fclose(f);
        return RZ_ERR_VERSION;
    }

    rz_ledger tmp;
    rz_ledger_init(&tmp);
    int ok = (n == 0 || fread(tmp.item, sizeof(rz_appraisal), (size_t)n, f) == (size_t)n);
    fclose(f);
    if (!ok) return RZ_ERR_IO;

    tmp.n = n;
    tmp.run = run;
    *L = tmp;
    return RZ_OK;
}

double rz_hit_rate(const rz_measure_result *r)
{
    if (r->predictions == 0) return 0.0;
    return (double)r->hits / (double)r->predictions;
}

double rz_mean_surprise(const rz_measure_result *r)
{
    if (r->predictions == 0) return 0.0;
    return r->surprise / (double)r->predictions;
}

int32_t rz_descend(rz_mind *m, int32_t id)
{
    const rz_frag *f = rz_frag_get(m, id);
    if (!f) return -1;
    return f->right;   /* -1 for a bare atom */
}


/* A fragment used a moment ago is briefly harder to reach again — the way a
   neuron that just fired is. Not forbidden: dampened. Without this a text turns
   into a chant no matter how much it wanders. */
typedef struct {
    int32_t  id[RZ_REFRACTORY];
    uint32_t n;
    uint32_t head;
} rz_refractory;

static int in_refractory(const rz_refractory *r, int32_t id)
{
    if (!r) return 0;
    for (uint32_t i = 0; i < r->n; i++) if (r->id[i] == id) return 1;
    return 0;
}

static void refractory_push(rz_refractory *r, int32_t id)
{
    if (!r) return;
    r->id[r->head] = id;
    r->head = (r->head + 1) % RZ_REFRACTORY;
    if (r->n < RZ_REFRACTORY) r->n++;
}

static double edge_pull(uint32_t weight, int damped, double temperature)
{
    double w = (double)weight;
    if (damped) w /= (double)RZ_REFRACTORY_D;
    if (w < 1e-9) w = 1e-9;
    return pow(w, 1.0 / temperature);
}

/* Pick a path out of `cur`. Temperature 0 takes the strongest; above 0 it
   samples, weighting each path by how travelled it is. `ref` may be NULL. */
static int32_t pick_next_ex(rz_mind *m, int32_t cur, double temperature,
                            uint64_t *rng, const rz_refractory *ref,
                            int *was_strongest, int *was_damped)
{
    if (was_strongest) *was_strongest = 1;
    if (was_damped) *was_damped = 0;

    int32_t k = id_mod(cur), l = id_slot(cur);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return -1;

    /* the plain strongest, for reporting whether the walk deviated */
    int32_t strongest = -1;
    uint32_t best = 0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
        if (M->pool[e].weight == 0) continue;
        if (strongest < 0 || M->pool[e].weight > best
            || (M->pool[e].weight == best && M->pool[e].to < strongest)) {
            best = M->pool[e].weight;
            strongest = M->pool[e].to;
        }
    }
    if (strongest < 0) return -1;

    if (temperature <= 0.0 && !ref) return strongest;

    double total = 0.0;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
        if (M->pool[e].weight == 0) continue;
        total += edge_pull(M->pool[e].weight, in_refractory(ref, M->pool[e].to),
                           temperature > 0.0 ? temperature : 0.001);
    }
    if (total <= 0.0) return strongest;

    double r = (double)(xorshift(rng) >> 11) / 9007199254740992.0 * total;
    int32_t chosen = -1;
    for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) {
        if (M->pool[e].weight == 0) continue;
        chosen = M->pool[e].to;
        r -= edge_pull(M->pool[e].weight, in_refractory(ref, M->pool[e].to),
                       temperature > 0.0 ? temperature : 0.001);
        if (r <= 0.0) break;
    }
    if (chosen < 0) chosen = strongest;

    if (was_strongest) *was_strongest = (chosen == strongest);
    if (was_damped) *was_damped = in_refractory(ref, strongest) && chosen != strongest;
    return chosen;
}

static int32_t pick_next(rz_mind *m, int32_t cur, double temperature, uint64_t *rng)
{
    return pick_next_ex(m, cur, temperature, rng, NULL, NULL, NULL);
}

static void recall_frag_ex(rz_mind *m,
                           const unsigned char *cue, size_t cue_len,
                           size_t want_atoms, uint64_t effort_budget,
                           double temperature, uint64_t seed,
                           rz_recall *out)
{
    memset(out, 0, sizeof(*out));

    if (cue_len == 0 || want_atoms == 0) { out->gave_up = 1; return; }
    if (want_atoms > RZ_RECALL_MAX) want_atoms = RZ_RECALL_MAX;

    uint64_t rng = seed;
    size_t lap = 0;
    int32_t cur = rz_trigger_ex(m, cue, cue_len, &lap);
    if (cur < 0) { out->gave_up = 1; return; }

    /* Whatever of the fired fragment the cue had not covered is the first thing
       it has to say. */
    {
        unsigned char buf[RZ_FRAG_SPAN];
        size_t fn = rz_expand(m, cur, buf, sizeof buf);
        for (size_t k = lap; k < fn && out->len < want_atoms; k++)
            out->atoms[out->len++] = buf[k];
    }

    while (out->len < want_atoms) {
        /* No way forward from here? Look in another direction before giving up.
           A wide conjunction may lead nowhere while its tail leads everywhere,
           so drop into it and carry on from inside. Each descent is work. */
        double b = rz_frag_branching(m, cur);
        while (b <= 0.0) {
            int32_t down = rz_descend(m, cur);
            if (down < 0) { out->gave_up = 1; return; }   /* an atom: nowhere left */
            if (out->effort + 1 > effort_budget) { out->gave_up = 1; return; }
            out->effort++;
            out->descents++;
            cur = down;
            b = rz_frag_branching(m, cur);
        }

        uint64_t cost = (uint64_t)ceil(b - 1e-9);
        if (cost < 1) cost = 1;
        if (out->effort + cost > effort_budget) { out->gave_up = 1; return; }

        int32_t next = pick_next(m, cur, temperature, &rng);
        if (next < 0) { out->gave_up = 1; return; }

        out->effort += cost;

        unsigned char buf[RZ_FRAG_SPAN];
        size_t fn = rz_expand(m, next, buf, sizeof buf);
        if (fn == 0) { out->gave_up = 1; return; }
        for (size_t k = 0; k < fn && out->len < want_atoms; k++)
            out->atoms[out->len++] = buf[k];
        cur = next;
    }
}

void rz_recall_frag(rz_mind *m,
                    const unsigned char *cue, size_t cue_len,
                    size_t want_atoms, uint64_t effort_budget,
                    rz_recall *out)
{
    recall_frag_ex(m, cue, cue_len, want_atoms, effort_budget, 0.0, 0, out);
}

void rz_recall_lateral(rz_mind *m,
                       const unsigned char *cue, size_t cue_len,
                       size_t want_atoms, uint64_t effort_budget,
                       double temperature, uint64_t seed,
                       rz_recall *out)
{
    recall_frag_ex(m, cue, cue_len, want_atoms, effort_budget, temperature, seed, out);
}

/* --- dreaming (P14) ------------------------------------------------------- */

rz_dream_kind rz_dream_needed(const rz_mind *m)
{
    if (m->awake_steps < RZ_DREAM_MIN_STEPS) return RZ_DREAM_NONE;

    double strain  = m->awake_strain / (double)m->awake_steps;
    double fertile = (double)m->awake_births / (double)m->awake_steps;

    /* Order matters. You cannot consolidate or explore while drowning, and
       there is no point loosening something that has not settled yet. */
    if (strain  > RZ_OVERWHELMED) return RZ_DREAM_CALM;
    if (fertile > RZ_FERTILE)     return RZ_DREAM_SETTLE;
    return RZ_DREAM_LOOSEN;
}

const char *rz_dream_reason(rz_dream_kind k)
{
    switch (k) {
    case RZ_DREAM_NONE:   return "nothing has happened worth sleeping on.";
    case RZ_DREAM_CALM:   return "it was overwhelmed — too much of what it met was a wall.";
    case RZ_DREAM_SETTLE: return "it built a lot that has not been exercised yet.";
    case RZ_DREAM_LOOSEN: return "nothing new and nothing hurting: it is circling.";
    }
    return "";
}

uint16_t rz_attention_of(rz_mind *m, int32_t frag)
{
    const rz_frag *f = rz_frag_get(m, frag);
    return f ? f->attention : 0;
}

uint64_t rz_attention_total(rz_mind *m)
{
    uint64_t total = 0;
    for (int32_t k = 0; k < RZ_MODULES; k++) {
        if (m->mod_nfrags[k] <= 1) continue;
        rz_module *M = mod_ensure(m, k);
        if (!M) continue;
        for (int32_t l = 0; l < M->nfrags; l++) total += M->frag[l].attention;
    }
    return total;
}

/* The one thing a dream may write. Not a record of anything that happened. */
static void mark(rz_mind *m, int32_t frag, uint16_t amount)
{
    if (frag < 0 || frag >= RZ_FRAGS) return;
    int32_t k = id_mod(frag), l = id_slot(frag);
    rz_module *M = mod_ensure(m, k);
    if (!M || l >= M->nfrags) return;

    uint32_t a = (uint32_t)M->frag[l].attention + amount;
    M->frag[l].attention = (uint16_t)(a > RZ_ATTENTION_MAX ? RZ_ATTENTION_MAX : a);
    M->dirty = 1;
}

/* Where each dream starts. The state decides the kind; the kind decides where
   to stand. */
static int32_t dream_seed_frag(rz_mind *m, rz_dream_kind k)
{
    int32_t best = -1;
    double  score = -1.0;

    for (int32_t mod = 0; mod < RZ_MODULES; mod++) {
        if (m->mod_nfrags[mod] <= 1) continue;
        rz_module *M = mod_ensure(m, mod);
        if (!M) continue;

        for (int32_t l = 1; l < M->nfrags; l++) {
            int32_t id = mod * RZ_MOD_FRAGS + l;
            double s;
            switch (k) {
            case RZ_DREAM_LOOSEN:
                s = (double)M->frag[l].seen;                 /* deepest rut */
                break;
            case RZ_DREAM_SETTLE:
                s = (double)l;                               /* newest structure */
                break;
            case RZ_DREAM_CALM: {
                uint8_t sh = shape_of_local(m, M, l);        /* where it is most lost */
                double merit = m->shape_tried[sh] >= RZ_SHAPE_TESTED
                             ? (double)m->shape_won[sh] / (double)m->shape_tried[sh] : 0.0;
                s = (1.0 - merit) * (double)M->frag[l].seen;
                break;
            }
            default:
                return -1;
            }
            if (s > score) { score = s; best = id; }
        }
    }
    return best;
}

rz_dream_kind rz_sleep(rz_mind *m, uint64_t seed)
{
    rz_dream_kind kind = rz_dream_needed(m);
    if (kind == RZ_DREAM_NONE) return kind;

    int32_t start = dream_seed_frag(m, kind);
    if (start < 0) { m->awake_steps = 0; return RZ_DREAM_NONE; }

    /* How wildly it wanders is the difference between the three dreams. To
       loosen, it must deform; to settle, it must stay close; to calm, it must
       go back over the same ground without being dragged by it. */
    double temperature = (kind == RZ_DREAM_LOOSEN) ? 1.6
                       : (kind == RZ_DREAM_SETTLE) ? 0.2 : 0.8;

    unsigned char cue[RZ_FRAG_SPAN];
    size_t cue_len = rz_expand(m, start, cue, sizeof cue);
    if (cue_len == 0) { m->awake_steps = 0; return RZ_DREAM_NONE; }

    rz_recall dream;
    rz_write(m, cue, cue_len, 600, 40000, temperature, seed ? seed : 1, &dream);

    /* The episode never happened and is thrown away. Only the marks survive. */
    for (uint32_t i = 0; i < dream.trace_len; i++) mark(m, dream.trace[i], 4);
    mark(m, start, 8);

    m->sleeps_had++;
    m->dreams[kind]++;
    m->awake_steps = m->awake_births = m->awake_anticipated = 0;
    m->awake_strain = 0.0;
    return kind;
}

/* --- the pen (P17) -------------------------------------------------------- */

static void trace_push(rz_recall *out, int32_t id)
{
    if (out->trace_len < RZ_TRACE_MAX) out->trace[out->trace_len++] = id;
}

static size_t emit(rz_mind *m, rz_recall *out, int32_t frag, size_t from, size_t want)
{
    unsigned char buf[RZ_FRAG_SPAN];
    size_t n = rz_expand(m, frag, buf, sizeof buf);
    size_t wrote = 0;
    for (size_t k = from; k < n && out->len < want; k++) {
        out->atoms[out->len++] = buf[k];
        wrote++;
    }
    return wrote;
}

void rz_write(rz_mind *m,
              const unsigned char *cue, size_t cue_len,
              size_t want_atoms, uint64_t effort_budget,
              double temperature, uint64_t seed,
              rz_recall *out)
{
    memset(out, 0, sizeof(*out));

    if (cue_len == 0 || want_atoms == 0) { out->gave_up = 1; return; }
    if (want_atoms > RZ_RECALL_MAX) want_atoms = RZ_RECALL_MAX;

    uint64_t rng = seed ? seed : 0x9E3779B97F4A7C15ull;
    rz_refractory ref;
    memset(&ref, 0, sizeof ref);

    size_t lap = 0;
    int32_t cur = rz_trigger_ex(m, cue, cue_len, &lap);
    if (cur < 0) { out->gave_up = 1; return; }

    trace_push(out, cur);
    refractory_push(&ref, cur);
    emit(m, out, cur, lap, want_atoms);   /* whatever the cue had not covered */

    while (out->len < want_atoms) {
        /* nowhere forward? go down and carry on from inside */
        double b = rz_frag_branching(m, cur);
        while (b <= 0.0) {
            int32_t down = rz_descend(m, cur);
            if (down < 0) { out->gave_up = 1; return; }
            if (out->effort + 1 > effort_budget) { out->gave_up = 1; return; }
            out->effort++;
            out->descents++;
            cur = down;
            b = rz_frag_branching(m, cur);
        }

        uint64_t cost = (uint64_t)ceil(b - 1e-9);
        if (cost < 1) cost = 1;
        if (out->effort + cost > effort_budget) { out->gave_up = 1; return; }

        int strongest = 1, damped = 0;
        int32_t next = pick_next_ex(m, cur, temperature, &rng, &ref, &strongest, &damped);
        if (next < 0) { out->gave_up = 1; return; }

        out->effort += cost;
        if (!strongest) out->wanders++;
        if (damped) out->damped++;

        if (emit(m, out, next, 0, want_atoms) == 0) { out->gave_up = 1; return; }
        trace_push(out, next);
        refractory_push(&ref, next);
        cur = next;
    }
}

/* ===================== the instrument ==================================== */

uint32_t rz_orphans(rz_mind *m)
{
    uint32_t n = 0;
    for (int32_t k = 0; k < RZ_MODULES; k++) {
        if (m->mod_nfrags[k] <= 1) continue;
        rz_module *M = mod_ensure(m, k);
        if (!M) continue;
        for (int32_t l = 1; l < M->nfrags; l++)
            if (M->head[l] < 0 && M->frag[l].seen == 0) n++;
    }
    return n;
}

void rz_examine(rz_mind *m, rz_autopsy *out)
{
    memset(out, 0, sizeof(*out));

    out->distinct_paths    = rz_distinct_paths(m);
    out->distinct_atoms    = rz_distinct_atoms(m);
    out->traversals        = m->traversals;
    out->atoms_ingested    = m->atoms_ingested;
    out->plastic_bytes     = (uint64_t)rz_plastic_bytes(m);
    out->entropy           = rz_entropy(m);
    out->effective_support = rz_effective_support(m);
    out->top1_share        = rz_top_share(m, 1.0);

    out->frags      = (uint32_t)rz_frag_count(m);
    out->births     = m->births;
    out->frag_steps = m->frag_steps;
    out->pool_full  = m->pool_full;

    for (int32_t k = 0; k < RZ_MODULES; k++) {
        if (m->mod_nfrags[k] > 1) out->modules_used++;
        if ((uint32_t)m->mod_nfrags[k] > out->widest_module)
            out->widest_module = (uint32_t)m->mod_nfrags[k];
    }

    /* Busiest fragment: the whole point of the pool is that this may exceed
       what a fixed per-fragment array would have allowed. */
    for (int32_t k = 0; k < RZ_MODULES; k++) {
        if (m->mod_nfrags[k] <= 1) continue;
        rz_module *M = mod_ensure(m, k);
        if (!M) continue;
        for (int32_t l = 0; l < M->nfrags; l++) {
            uint32_t d = 0;
            for (int32_t e = M->head[l]; e >= 0; e = M->pool[e].next) d++;
            if (d > out->busiest_frag) out->busiest_frag = d;
        }
    }

    out->orphans = rz_orphans(m);
    out->wakes   = m->wakes;
    out->sleeps  = m->sleeps;
    out->decay_passes = m->decay_passes;
    out->shapes_transfer = rz_shapes_that_transfer(m);
    out->best_shape = 0.0;
    for (uint8_t sh = 0; sh < RZ_SHAPES; sh++)
        if (m->shape_tried[sh] >= RZ_SHAPE_TESTED) {
            double mt = rz_shape_merit(m, sh);
            if (mt > out->best_shape) out->best_shape = mt;
        }
    out->cold_edges   = rz_cold_edges(m);
}

/* ===================== persistence ======================================= */

int rz_save(const rz_mind *m, const char *path)
{
    rz_mind *mm = (rz_mind *)m;   /* flushing is not a change of content */

    if (mm->nest[0] == 0) return RZ_ERR_IO;

    RZ_MKDIR(mm->nest);
    for (int32_t i = 0; i < mm->nresident; i++) {
        int32_t k = mm->resident[i];
        if (mm->mod[k] && mm->mod[k]->dirty) mod_save(mm, k);
    }

    FILE *f = fopen(path, "wb");
    if (!f) return RZ_ERR_IO;

    uint32_t ver = RZ_MIND_VER;
    int ok = (fwrite(&ver, sizeof ver, 1, f) == 1)
          && (fwrite(m->path, sizeof m->path, 1, f) == 1)
          && (fwrite(m->atom_seen, sizeof m->atom_seen, 1, f) == 1)
          && (fwrite(&m->traversals, sizeof m->traversals, 1, f) == 1)
          && (fwrite(&m->atoms_ingested, sizeof m->atoms_ingested, 1, f) == 1)
          && (fwrite(m->mod_nfrags, sizeof m->mod_nfrags, 1, f) == 1)
          && (fwrite(&m->frag_steps, sizeof m->frag_steps, 1, f) == 1)
          && (fwrite(&m->births, sizeof m->births, 1, f) == 1)
          && (fwrite(&m->pool_full, sizeof m->pool_full, 1, f) == 1)
          && (fwrite(&m->decay_passes, sizeof m->decay_passes, 1, f) == 1)
          && (fwrite(m->shape_path, sizeof m->shape_path, 1, f) == 1)
          && (fwrite(&m->shape_steps, sizeof m->shape_steps, 1, f) == 1)
          && (fwrite(m->shape_tried, sizeof m->shape_tried, 1, f) == 1)
          && (fwrite(m->shape_won, sizeof m->shape_won, 1, f) == 1)
          /* the night, and the sleeping — a field added to the mind is a field
             added here, in the same movement. Three times it was not. */
          && (fwrite(&m->awake_steps, sizeof m->awake_steps, 1, f) == 1)
          && (fwrite(&m->awake_births, sizeof m->awake_births, 1, f) == 1)
          && (fwrite(&m->awake_strain, sizeof m->awake_strain, 1, f) == 1)
          && (fwrite(&m->awake_anticipated, sizeof m->awake_anticipated, 1, f) == 1)
          && (fwrite(&m->sleeps_had, sizeof m->sleeps_had, 1, f) == 1)
          && (fwrite(m->dreams, sizeof m->dreams, 1, f) == 1);
    if (fclose(f) != 0) ok = 0;
    return ok ? RZ_OK : RZ_ERR_IO;
}

int rz_load(rz_mind *m, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return RZ_ERR_MISSING;

    uint32_t ver = 0;
    if (fread(&ver, sizeof ver, 1, f) != 1 || ver != RZ_MIND_VER) {
        fclose(f);
        return RZ_ERR_VERSION;
    }

    /* Read aside and commit only on success: a failed load never damages a
       living mind. */
    static uint32_t tpath[RZ_PATHS];
    uint32_t tatom[RZ_ATOMS];
    uint64_t ttrav, tingest, tsteps, tbirths, tfull, tdecay, tshsteps;
    static uint32_t tshape[RZ_SHAPES * RZ_SHAPES];
    static uint32_t ttried[RZ_SHAPES], twon[RZ_SHAPES];
    uint64_t taw[4], tslept, tdreams[4];
    double   tstrain;
    int32_t  tnfrags[RZ_MODULES];

    int ok = (fread(tpath, sizeof tpath, 1, f) == 1)
          && (fread(tatom, sizeof tatom, 1, f) == 1)
          && (fread(&ttrav, sizeof ttrav, 1, f) == 1)
          && (fread(&tingest, sizeof tingest, 1, f) == 1)
          && (fread(tnfrags, sizeof tnfrags, 1, f) == 1)
          && (fread(&tsteps, sizeof tsteps, 1, f) == 1)
          && (fread(&tbirths, sizeof tbirths, 1, f) == 1)
          && (fread(&tfull, sizeof tfull, 1, f) == 1)
          && (fread(&tdecay, sizeof tdecay, 1, f) == 1)
          && (fread(tshape, sizeof tshape, 1, f) == 1)
          && (fread(&tshsteps, sizeof tshsteps, 1, f) == 1)
          && (fread(ttried, sizeof ttried, 1, f) == 1)
          && (fread(twon, sizeof twon, 1, f) == 1)
          && (fread(&taw[0], sizeof taw[0], 1, f) == 1)
          && (fread(&taw[1], sizeof taw[1], 1, f) == 1)
          && (fread(&tstrain, sizeof tstrain, 1, f) == 1)
          && (fread(&taw[3], sizeof taw[3], 1, f) == 1)
          && (fread(&tslept, sizeof tslept, 1, f) == 1)
          && (fread(tdreams, sizeof tdreams, 1, f) == 1);
    fclose(f);
    if (!ok) return RZ_ERR_IO;

    char nest[512];
    memcpy(nest, m->nest, sizeof nest);
    rz_free(m);
    rz_init(m);
    memcpy(m->nest, nest, sizeof m->nest);

    memcpy(m->path, tpath, sizeof tpath);
    memcpy(m->atom_seen, tatom, sizeof tatom);
    m->traversals = ttrav;
    m->atoms_ingested = tingest;
    memcpy(m->mod_nfrags, tnfrags, sizeof tnfrags);
    m->frag_steps = tsteps;
    m->births = tbirths;
    m->pool_full = tfull;
    m->decay_passes = tdecay;
    memcpy(m->shape_path, tshape, sizeof tshape);
    m->shape_steps = tshsteps;
    memcpy(m->shape_tried, ttried, sizeof ttried);
    memcpy(m->shape_won, twon, sizeof twon);
    m->awake_steps = taw[0];
    m->awake_births = taw[1];
    m->awake_strain = tstrain;
    m->awake_anticipated = taw[3];
    m->sleeps_had = tslept;
    memcpy(m->dreams, tdreams, sizeof tdreams);
    return RZ_OK;
}

int rz_autopsy_save(const rz_autopsy *a, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return RZ_ERR_IO;

    fprintf(f, "razar_autopsy 5\n");
    fprintf(f, "distinct_paths %lu\n",      (unsigned long)a->distinct_paths);
    fprintf(f, "distinct_atoms %lu\n",      (unsigned long)a->distinct_atoms);
    fprintf(f, "traversals %llu\n",         (unsigned long long)a->traversals);
    fprintf(f, "atoms_ingested %llu\n",     (unsigned long long)a->atoms_ingested);
    fprintf(f, "plastic_bytes %llu\n",      (unsigned long long)a->plastic_bytes);
    fprintf(f, "entropy %.12g\n",           a->entropy);
    fprintf(f, "effective_support %.12g\n", a->effective_support);
    fprintf(f, "top1_share %.12g\n",        a->top1_share);
    fprintf(f, "frags %lu\n",               (unsigned long)a->frags);
    fprintf(f, "births %llu\n",             (unsigned long long)a->births);
    fprintf(f, "frag_steps %llu\n",         (unsigned long long)a->frag_steps);
    fprintf(f, "orphans %lu\n",             (unsigned long)a->orphans);
    fprintf(f, "pool_full %llu\n",          (unsigned long long)a->pool_full);
    fprintf(f, "modules_used %lu\n",        (unsigned long)a->modules_used);
    fprintf(f, "widest_module %lu\n",       (unsigned long)a->widest_module);
    fprintf(f, "busiest_frag %lu\n",        (unsigned long)a->busiest_frag);
    fprintf(f, "wakes %llu\n",              (unsigned long long)a->wakes);
    fprintf(f, "sleeps %llu\n",             (unsigned long long)a->sleeps);
    fprintf(f, "decay_passes %llu\n",       (unsigned long long)a->decay_passes);
    fprintf(f, "cold_edges %llu\n",         (unsigned long long)a->cold_edges);
    fprintf(f, "shapes_transfer %lu\n",     (unsigned long)a->shapes_transfer);
    fprintf(f, "best_shape %.12g\n",        a->best_shape);

    return (fclose(f) == 0) ? RZ_OK : RZ_ERR_IO;
}

int rz_autopsy_load(rz_autopsy *a, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return RZ_ERR_MISSING;

    char tag[32] = {0};
    int fmt = 0;
    if (fscanf(f, "%31s %d", tag, &fmt) != 2
        || strcmp(tag, "razar_autopsy") != 0 || fmt != 5) {
        fclose(f);
        return RZ_ERR_VERSION;   /* an older instrument's readings do not compare */
    }

    rz_autopsy tmp;
    memset(&tmp, 0, sizeof tmp);
    char key[64];
    int fields = 0;

    while (fscanf(f, "%63s", key) == 1) {
        unsigned long long u = 0;
        double d = 0.0;
        if      (!strcmp(key, "distinct_paths")    && fscanf(f, "%llu", &u) == 1) { tmp.distinct_paths = (uint32_t)u; fields++; }
        else if (!strcmp(key, "distinct_atoms")    && fscanf(f, "%llu", &u) == 1) { tmp.distinct_atoms = (uint32_t)u; fields++; }
        else if (!strcmp(key, "traversals")        && fscanf(f, "%llu", &u) == 1) { tmp.traversals = u; fields++; }
        else if (!strcmp(key, "atoms_ingested")    && fscanf(f, "%llu", &u) == 1) { tmp.atoms_ingested = u; fields++; }
        else if (!strcmp(key, "plastic_bytes")     && fscanf(f, "%llu", &u) == 1) { tmp.plastic_bytes = u; fields++; }
        else if (!strcmp(key, "entropy")           && fscanf(f, "%lf", &d) == 1)  { tmp.entropy = d; fields++; }
        else if (!strcmp(key, "effective_support") && fscanf(f, "%lf", &d) == 1)  { tmp.effective_support = d; fields++; }
        else if (!strcmp(key, "top1_share")        && fscanf(f, "%lf", &d) == 1)  { tmp.top1_share = d; fields++; }
        else if (!strcmp(key, "frags")             && fscanf(f, "%llu", &u) == 1) { tmp.frags = (uint32_t)u; fields++; }
        else if (!strcmp(key, "births")            && fscanf(f, "%llu", &u) == 1) { tmp.births = u; fields++; }
        else if (!strcmp(key, "frag_steps")        && fscanf(f, "%llu", &u) == 1) { tmp.frag_steps = u; fields++; }
        else if (!strcmp(key, "orphans")           && fscanf(f, "%llu", &u) == 1) { tmp.orphans = (uint32_t)u; fields++; }
        else if (!strcmp(key, "pool_full")         && fscanf(f, "%llu", &u) == 1) { tmp.pool_full = u; fields++; }
        else if (!strcmp(key, "modules_used")      && fscanf(f, "%llu", &u) == 1) { tmp.modules_used = (uint32_t)u; fields++; }
        else if (!strcmp(key, "widest_module")     && fscanf(f, "%llu", &u) == 1) { tmp.widest_module = (uint32_t)u; fields++; }
        else if (!strcmp(key, "busiest_frag")      && fscanf(f, "%llu", &u) == 1) { tmp.busiest_frag = (uint32_t)u; fields++; }
        else if (!strcmp(key, "wakes")             && fscanf(f, "%llu", &u) == 1) { tmp.wakes = u; fields++; }
        else if (!strcmp(key, "sleeps")            && fscanf(f, "%llu", &u) == 1) { tmp.sleeps = u; fields++; }
        else if (!strcmp(key, "decay_passes")      && fscanf(f, "%llu", &u) == 1) { tmp.decay_passes = u; fields++; }
        else if (!strcmp(key, "cold_edges")        && fscanf(f, "%llu", &u) == 1) { tmp.cold_edges = u; fields++; }
        else if (!strcmp(key, "shapes_transfer")   && fscanf(f, "%llu", &u) == 1) { tmp.shapes_transfer = (uint32_t)u; fields++; }
        else if (!strcmp(key, "best_shape")        && fscanf(f, "%lf", &d) == 1)  { tmp.best_shape = d; fields++; }
        else if (!strcmp(key, "decay_passes")      && fscanf(f, "%llu", &u) == 1) { tmp.decay_passes = u; fields++; }
        else break;
    }
    fclose(f);

    if (fields != 22) return RZ_ERR_VERSION;
    *a = tmp;
    return RZ_OK;
}

/* ===================== the diagnosis ===================================== */

rz_verdict rz_diagnose(const rz_autopsy *now, const rz_autopsy *was, int have_prev)
{
    if (now->traversals == 0) return RZ_V_EMPTY;
    if (!have_prev)           return RZ_V_BASELINE;

    /* The alarm first. Obsessive material also looks like consolidation from
       the outside: no new paths, more traversals. The difference is what
       happened to the share of the map still in use. */
    if (was->effective_support > 0.0) {
        double drop = (was->effective_support - now->effective_support)
                    / was->effective_support;
        if (drop > RZ_COLLAPSE_DROP) return RZ_V_CONCENTRATING;
    }

    if (now->distinct_paths == was->distinct_paths
        && now->traversals > was->traversals) {
        return RZ_V_CONSOLIDATING;
    }

    return RZ_V_HEALTHY;
}

const char *rz_verdict_text(rz_verdict v)
{
    switch (v) {
    case RZ_V_EMPTY:
        return "empty. It has not eaten anything yet.";
    case RZ_V_BASELINE:
        return "baseline. Nothing to compare against yet —\n"
               "         this is the least useful autopsy there will ever be.";
    case RZ_V_CONCENTRATING:
        return "CONCENTRATING. Effective support fell: it is using less of\n"
               "         the map it has. This is the collapse signature.";
    case RZ_V_CONSOLIDATING:
        return "consolidating. It discovered nothing new and lived it again.";
    case RZ_V_HEALTHY:
        return "healthy. Reach and use are moving together.";
    }
    return "unknown.";
}
