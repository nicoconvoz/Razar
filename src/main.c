/* razar — spike 1 CLI
 *
 *   razar feed [path...]   walk material (default: feed/) and remember having read it
 *   razar autopsy          examine the organism and report the diff since last time
 *
 * The autopsy reads a saved mind from disk in its own invocation. It never runs
 * while the organism is being fed, so measuring cannot strengthen what it
 * measures.
 */
#include "razar.h"
#include "lego.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #define MKDIR(p) mkdir((p), 0755)
#endif

#define NEST      "nido"
#define MIND_PATH NEST "/mind.dat"
#define MODULES   NEST "/modules"
#define AUTOPSIES "autopsies"
#define FEED_DIR  "feed"
#define LEXICON   NEST "/lexicon.dat"
#define MELODY    NEST "/melody.dat"

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFDIR);
}

/* --- feeding -------------------------------------------------------------- */

static int eat_file(rz_mind *m, const char *path, unsigned long long *bytes)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "  skipped (unreadable): %s\n", path);
        return 0;
    }

    static unsigned char buf[1 << 16];
    size_t n;
    unsigned long long total = 0;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        rz_ingest(m, buf, n);
        total += (unsigned long long)n;
    }
    fclose(f);

    printf("  ate %-52s %10llu bytes\n", path, total);
    *bytes += total;
    return 1;
}

static int eat_path(rz_mind *m, const char *path, unsigned long long *bytes)
{
    if (!is_dir(path)) return eat_file(m, path, bytes);

    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "  skipped (unreadable dir): %s\n", path);
        return 0;
    }

    int eaten = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;                 /* . .. .gitignore */
        if (strcmp(e->d_name, "README.md") == 0) continue; /* the folder's own contract */
        char child[1024];
        snprintf(child, sizeof child, "%s/%s", path, e->d_name);
        eaten += eat_path(m, child, bytes);
    }
    closedir(d);
    return eaten;
}

/* --- autopsy archive ------------------------------------------------------ */

static int latest_autopsy_number(void)
{
    DIR *d = opendir(AUTOPSIES);
    if (!d) return 0;

    int best = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        int n = atoi(e->d_name);
        if (n > best && strstr(e->d_name, ".txt")) best = n;
    }
    closedir(d);
    return best;
}

static void autopsy_path(int n, char *out, size_t cap)
{
    snprintf(out, cap, "%s/%04d.txt", AUTOPSIES, n);
}

/* --- reporting ------------------------------------------------------------ */

static void line_u(const char *label, unsigned long long now,
                   unsigned long long was, int have_prev)
{
    printf("  %-22s %14llu", label, now);
    if (have_prev) {
        long long d = (long long)now - (long long)was;
        printf("   %+14lld", d);
    }
    printf("\n");
}

static void line_f(const char *label, double now, double was, int have_prev,
                   const char *fmt)
{
    printf("  %-22s ", label);
    printf(fmt, now);
    if (have_prev) {
        printf("   ");
        char withsign[32];
        snprintf(withsign, sizeof withsign, "%%+%s", fmt + 1);
        printf(withsign, now - was);
    }
    printf("\n");
}

static void report(const rz_autopsy *now, const rz_autopsy *was, int have_prev, int number)
{
    printf("\n=== RAZAR AUTOPSY #%04d ===\n\n", number);
    printf("VERDICT  %s\n", rz_verdict_text(rz_diagnose(now, was, have_prev)));
    printf("\n");
    if (have_prev) printf("  %-22s %14s   %14s\n", "", "now", "since last");

    line_u("distinct paths",  now->distinct_paths, was->distinct_paths, have_prev);
    line_u("distinct atoms",  now->distinct_atoms, was->distinct_atoms, have_prev);
    line_u("traversals",      now->traversals,     was->traversals,     have_prev);
    line_u("atoms ingested",  now->atoms_ingested, was->atoms_ingested, have_prev);
    line_f("entropy (nats)",  now->entropy,           was->entropy,           have_prev, "%14.6f");
    line_f("effective support", now->effective_support, was->effective_support, have_prev, "%14.2f");
    line_f("top 1% share",    now->top1_share,        was->top1_share,        have_prev, "%14.4f");
    printf("\n  -- the plastic layer: what it built out of the atoms --\n");
    line_u("fragments known", now->frags,          was->frags,          have_prev);
    line_u("earned their place", now->births,      was->births,         have_prev);
    line_u("fragment steps",  now->frag_steps,     was->frag_steps,     have_prev);
    line_u("orphaned",        now->orphans,        was->orphans,        have_prev);
    line_u("paths refused",   now->pool_full,      was->pool_full,      have_prev);

    printf("\n  -- modules: which lineages are awake --\n");
    line_u("lineages in use",  now->modules_used,  was->modules_used,   have_prev);
    line_u("widest lineage",   now->widest_module, was->widest_module,  have_prev);
    line_u("busiest fragment", now->busiest_frag,  was->busiest_frag,   have_prev);
    line_u("woken",            now->wakes,         was->wakes,          have_prev);
    line_u("put to sleep",     now->sleeps,        was->sleeps,         have_prev);

    printf("\n  -- forgetting: nothing is deleted, paths go cold --\n");
    line_u("passes of decay",  now->decay_passes,  was->decay_passes,   have_prev);
    line_u("paths at the floor", now->cold_edges,  was->cold_edges,     have_prev);

    printf("\n  -- what matters to it: ideas that carry into new ground --\n");
    line_u("ideas that transfer", now->shapes_transfer, was->shapes_transfer, have_prev);
    line_f("best idea",           now->best_shape,      was->best_shape,      have_prev, "%14.4f");

    printf("\n");
    line_u("plastic bytes",   now->plastic_bytes,  was->plastic_bytes,  have_prev);

    if (now->atoms_ingested > 0) {
        double per = (double)now->plastic_bytes / (double)now->atoms_ingested;
        printf("  %-22s %14.6f   bytes of memory per byte eaten\n", "cost ratio", per);
    }
    printf("\n");
}

/* --- commands ------------------------------------------------------------- */

static int cmd_feed(int argc, char **argv)
{
    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    MKDIR(NEST);
    rz_set_nest(m, MODULES);

    int rc = rz_load(m, MIND_PATH);
    if (rc == RZ_OK) {
        printf("woke up: %lu paths already walked\n", (unsigned long)rz_distinct_paths(m));
    } else if (rc == RZ_ERR_MISSING) {
        printf("born: nothing remembered yet\n");
    } else {
        fprintf(stderr, "refusing to run on an incompatible mind (%s).\n", MIND_PATH);
        fprintf(stderr, "delete it to start fresh.\n");
        free(m);
        return 1;
    }

    unsigned long long bytes = 0;
    int eaten = 0;
    if (argc > 0) {
        for (int i = 0; i < argc; i++) eaten += eat_path(m, argv[i], &bytes);
    } else {
        eaten = eat_path(m, FEED_DIR, &bytes);
    }

    if (eaten == 0) {
        printf("nothing to eat. Put material in %s/\n", FEED_DIR);
        free(m);
        return 0;
    }

    MKDIR(NEST);
    if (rz_save(m, MIND_PATH) != RZ_OK) {
        fprintf(stderr, "could not write %s\n", MIND_PATH);
        free(m);
        return 1;
    }

    printf("\nate %d file(s), %llu bytes.\n", eaten, bytes);
    printf("run `razar autopsy` to see what it did.\n");
    rz_free(m);
    free(m);
    return 0;
}

static int cmd_autopsy(void)
{
    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    MKDIR(NEST);
    rz_set_nest(m, MODULES);

    int rc = rz_load(m, MIND_PATH);
    if (rc == RZ_ERR_MISSING) {
        fprintf(stderr, "no mind at %s — feed it first.\n", MIND_PATH);
        free(m);
        return 1;
    }
    if (rc != RZ_OK) {
        fprintf(stderr, "incompatible mind at %s — refusing to read it.\n", MIND_PATH);
        free(m);
        return 1;
    }

    rz_autopsy now;
    rz_examine(m, &now);
    free(m);

    int prev_n = latest_autopsy_number();
    rz_autopsy was;
    memset(&was, 0, sizeof was);
    int have_prev = 0;
    if (prev_n > 0) {
        char p[512];
        autopsy_path(prev_n, p, sizeof p);
        have_prev = (rz_autopsy_load(&was, p) == RZ_OK);
    }

    int number = prev_n + 1;
    report(&now, &was, have_prev, number);

    MKDIR(AUTOPSIES);
    char p[512];
    autopsy_path(number, p, sizeof p);
    if (rz_autopsy_save(&now, p) != RZ_OK) {
        fprintf(stderr, "could not archive the autopsy to %s\n", p);
        return 1;
    }
    printf("archived: %s\n", p);
    return 0;
}

static void print_trace(const unsigned char *a, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char c = a[i];
        if (c == '\n')      printf("\\n");
        else if (c == '\t') printf("\\t");
        else if (c == '\r') printf("\\r");
        else if (c < 32 || c == 127) printf("\\x%02x", c);
        else                putchar((int)c);
    }
}

static int cmd_recall(int argc, char **argv, int by_atoms)
{
    if (argc < 1) {
        fprintf(stderr, "usage: razar recall <cue> [steps] [effort]\n");
        return 1;
    }
    const char *cue = argv[0];
    size_t want    = (argc > 1) ? (size_t)strtoul(argv[1], NULL, 10) : 80;
    uint64_t budget = (argc > 2) ? strtoull(argv[2], NULL, 10) : 400;

    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    MKDIR(NEST);
    rz_set_nest(m, MODULES);

    int rc = rz_load(m, MIND_PATH);
    if (rc != RZ_OK) {
        fprintf(stderr, rc == RZ_ERR_MISSING
                ? "no mind at %s — feed it first.\n"
                : "incompatible mind at %s — refusing to read it.\n", MIND_PATH);
        free(m);
        return 1;
    }

    rz_recall r;
    if (by_atoms) rz_recall_from(m, (const unsigned char *)cue, strlen(cue), want, budget, &r);
    else          rz_recall_frag(m, (const unsigned char *)cue, strlen(cue), want, budget, &r);

    if (!by_atoms) {
        size_t lap = 0;
        int32_t t = rz_trigger_ex(m, (const unsigned char *)cue, strlen(cue), &lap);
        unsigned char fb[RZ_FRAG_SPAN];
        size_t fn = rz_expand(m, t, fb, sizeof fb);
        printf("\ntrigger  \"");
        print_trace(fb, fn);
        printf("\"  (%zu atoms of it were already in the cue)\n", lap);
    }
    free(m);

    printf("\ncue      \"%s\"\n", cue);
    printf("recalled \"");
    print_trace(r.atoms, r.len);
    printf("\"\n\n");

    printf("  atoms returned   %14zu  of %zu asked\n", r.len, want);
    printf("  effort spent     %14llu  of %llu budget\n",
           (unsigned long long)r.effort, (unsigned long long)budget);
    if (r.len > 0) {
        printf("  effort per atom  %14.2f\n", (double)r.effort / (double)r.len);
    }
    printf("  outcome          %14s\n", r.gave_up ? "GAVE UP" : "arrived");
    if (r.gave_up && r.len == 0) {
        printf("\n  It has never walked out of that cue. Nothing to revive.\n");
    } else if (r.gave_up) {
        printf("\n  It stopped short — dead end, or it ran out of effort.\n");
    }
    printf("\n");
    return 0;
}

static int cmd_predict(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "usage: razar predict <file>   (material it has NOT eaten)\n");
        return 1;
    }

    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    MKDIR(NEST);
    rz_set_nest(m, MODULES);

    if (rz_load(m, MIND_PATH) != RZ_OK) {
        fprintf(stderr, "no usable mind at %s — feed it first.\n", MIND_PATH);
        free(m);
        return 1;
    }

    FILE *f = fopen(argv[0], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[0]); rz_free(m); free(m); return 1; }

    static unsigned char buf[1 << 20];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);

    rz_measure_result r;
    rz_measure(m, buf, n, &r);
    rz_free(m);
    free(m);

    printf("\n=== THE BAR ===  %s\n\n", argv[0]);
    printf("  Every guess was committed before the next piece was read, and\n");
    printf("  nothing was learned from it. The bar measures and never teaches.\n\n");
    printf("  %-22s %14llu\n", "material (bytes)",  (unsigned long long)r.atoms);
    printf("  %-22s %14llu\n", "predictions made",  (unsigned long long)r.predictions);
    printf("  %-22s %14llu\n", "right",             (unsigned long long)r.hits);
    printf("  %-22s %14.4f\n", "hit rate",          rz_hit_rate(&r));
    printf("  %-22s %14.4f   0 = expected, 1 = never seen\n",
           "mean surprise", rz_mean_surprise(&r));

    printf("\n  -- where the specific was blind, the general answered --\n");
    printf("  %-22s %14llu\n", "never taken this step", (unsigned long long)r.shape_tried);
    printf("  %-22s %14llu\n", "the shape got it right", (unsigned long long)r.shape_hits);
    printf("  %-22s %14.4f   chance alone would be %.4f\n", "rescue rate",
           rz_shape_rescue(&r), 1.0 / (double)RZ_SHAPES);
    printf("\n");
    return 0;
}

/* --- what it wants to read ------------------------------------------------ */

#define LEDGER NEST "/interest.dat"
#define MAX_DISHES 128
#define RZ_PATHLEN 256

typedef struct {
    char   path[RZ_PATHLEN];
    double frontier, opaque, mastered, novel, surprise;
    double trend;        /* fall in surprise since last time; +inf marker below */
    int    had_history;
    uint64_t bytes;
} rz_dish;

static int by_frontier(const void *a, const void *b)
{
    const rz_dish *x = a, *y = b;
    if (x->frontier < y->frontier) return 1;
    if (x->frontier > y->frontier) return -1;
    /* Same share of edge: the one it has more traction on comes first. */
    if (x->surprise > y->surprise) return 1;
    if (x->surprise < y->surprise) return -1;
    return 0;
}

static void taste_dish(rz_mind *m, rz_ledger *L, const char *path, rz_dish *d)
{
    memset(d, 0, sizeof(*d));
    snprintf(d->path, sizeof d->path, "%.*s", (int)(sizeof d->path) - 1, path);

    FILE *f = fopen(path, "rb");
    if (!f) return;
    static unsigned char buf[1 << 20];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    d->bytes = (uint64_t)n;
    if (n == 0) return;

    rz_measure_result r;
    rz_measure(m, buf, n, &r);

    d->frontier = rz_frontier_share(&r);
    d->opaque   = rz_opaque_share(&r);
    d->mastered = rz_mastered_share(&r);
    d->novel    = rz_novel_share(&r);
    d->surprise = rz_mean_surprise(&r);

    uint64_t fp = rz_fingerprint(buf, n);
    const rz_appraisal *before = rz_ledger_find(L, path, fp);
    if (before) {
        d->had_history = 1;
        d->trend = before->surprise - d->surprise;   /* positive = getting better */
    }
    rz_ledger_record(L, path, fp, d->surprise, d->frontier);
}

static int gather(const char *dir, char list[][RZ_PATHLEN], int cap)
{
    DIR *dd = opendir(dir);
    if (!dd) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(dd)) != NULL && n < cap) {
        if (e->d_name[0] == '.') continue;
        if (strcmp(e->d_name, "README.md") == 0) continue;
        char child[RZ_PATHLEN];
        snprintf(child, sizeof child, "%.120s/%.120s", dir, e->d_name);
        if (is_dir(child)) { n += gather(child, list + n, cap - n); continue; }
        snprintf(list[n], RZ_PATHLEN, "%.*s", RZ_PATHLEN - 1, child);
        n++;
    }
    closedir(dd);
    return n;
}

/* Thresholds calibrated against real readings, not guessed: prose and source
   sit under 1% opaque, random bytes over 50%. */
static const char *appetite(const rz_dish *d)
{
    if (d->bytes == 0)          return "empty";
    if (d->opaque   > 0.20)     return "little to grab";
    if (d->mastered > 0.60)     return "done with it";
    if (d->frontier > 0.25)     return "the edge";
    return "chewing";
}

/* Whether reading it is actually working. Only meaningful once there is a
   previous reading of the same material to compare against. */
static const char *trend_word(const rz_dish *d)
{
    if (!d->had_history)   return "";
    if (d->trend >  0.001) return "learning";
    if (d->trend < -0.001) return "slipping";
    return "flat";
}

static int cmd_interest(int argc, char **argv)
{
    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    MKDIR(NEST);
    rz_set_nest(m, MODULES);

    if (rz_load(m, MIND_PATH) != RZ_OK) {
        fprintf(stderr, "no usable mind at %s — feed it first.\n", MIND_PATH);
        free(m);
        return 1;
    }

    rz_ledger L;
    rz_ledger_init(&L);
    rz_ledger_load(&L, LEDGER);          /* missing is fine: nothing to compare yet */

    static char list[MAX_DISHES][RZ_PATHLEN];
    int n = 0;
    if (argc > 0) {
        for (int i = 0; i < argc && n < MAX_DISHES; i++) {
            if (is_dir(argv[i])) n += gather(argv[i], list + n, MAX_DISHES - n);
            else snprintf(list[n++], RZ_PATHLEN, "%.*s", RZ_PATHLEN - 1, argv[i]);
        }
    } else {
        n = gather(FEED_DIR, list, MAX_DISHES);
    }

    if (n == 0) {
        printf("nothing on the table. Put material in %s/\n", FEED_DIR);
        rz_free(m); free(m);
        return 0;
    }

    static rz_dish dish[MAX_DISHES];
    for (int i = 0; i < n; i++) taste_dish(m, &L, list[i], &dish[i]);
    rz_free(m);
    free(m);

    qsort(dish, (size_t)n, sizeof dish[0], by_frontier);

    printf("\n=== WHAT IT WANTS TO READ ===\n\n");
    printf("  Nothing here was learned from. It only looked, and said where each\n");
    printf("  piece sits: outside its reach, on its edge, or already behind it.\n\n");
    printf("  %-38s %9s %8s %9s %9s %8s  %s\n",
           "material", "edge", "opaque", "known", "surprise", "trend", "");
    printf("  ");
    for (int i = 0; i < 92; i++) putchar('-');
    printf("\n");

    for (int i = 0; i < n; i++) {
        rz_dish *d = &dish[i];
        const char *shown = d->path;
        size_t len = strlen(shown);
        if (len > 38) shown += len - 38;

        printf("  %-38s %8.1f%% %7.1f%% %8.1f%% %9.4f ",
               shown, d->frontier * 100.0, d->opaque * 100.0,
               d->mastered * 100.0, d->surprise);
        if (d->had_history) printf("%+8.4f", d->trend);
        else                printf("%8s", "new");
        printf("  %-15s %s\n", appetite(d), trend_word(d));
    }

    printf("\n");
    if (dish[0].frontier > 0.05) {
        printf("  It wants %s: it has hooks there and is not done.\n", dish[0].path);
    } else {
        printf("  Nothing on the table is on its edge. Everything is either out of\n");
        printf("  reach or already behind it — feed it something in between.\n");
    }
    printf("\n");

    if (rz_ledger_save(&L, LEDGER) != RZ_OK)
        fprintf(stderr, "could not write %s — no trend next time.\n", LEDGER);
    return 0;
}

static int cmd_write(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "usage: razar write <cue> [atoms] [effort] [temperature] [seed]\n");
        return 1;
    }
    const char *cue     = argv[0];
    size_t   want       = (argc > 1) ? (size_t)strtoul(argv[1], NULL, 10) : 400;
    uint64_t budget     = (argc > 2) ? strtoull(argv[2], NULL, 10) : 20000;
    /* 0.25 is where structured corruption lives for source code: low enough to
       keep the idioms, high enough not to chant. The band is material-dependent
       and was found by sweeping, not guessed. */
    double   temperature= (argc > 3) ? strtod(argv[3], NULL) : 0.25;
    uint64_t seed       = (argc > 4) ? strtoull(argv[4], NULL, 10) : 1;

    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    MKDIR(NEST);
    rz_set_nest(m, MODULES);

    if (rz_load(m, MIND_PATH) != RZ_OK) {
        fprintf(stderr, "no usable mind at %s — feed it first.\n", MIND_PATH);
        free(m);
        return 1;
    }

    rz_recall t;
    rz_write(m, (const unsigned char *)cue, strlen(cue), want, budget, temperature, seed, &t);

    printf("\n=== RAZAR WRITES ===\n\n  cue \"%s\"   temperature %.2f   seed %llu\n\n",
           cue, temperature, (unsigned long long)seed);
    printf("  \"");
    print_trace(t.atoms, t.len);
    printf("\"\n\n");

    printf("  %-26s %10zu of %zu\n", "atoms written", t.len, want);
    printf("  %-26s %10llu of %llu\n", "effort spent",
           (unsigned long long)t.effort, (unsigned long long)budget);
    if (t.len) printf("  %-26s %10.2f\n", "effort per atom", (double)t.effort / (double)t.len);
    printf("  %-26s %10lu\n", "left the strongest path", (unsigned long)t.wanders);
    printf("  %-26s %10lu\n", "  of those, dampened", (unsigned long)t.damped);
    printf("  %-26s %10lu\n", "walked around a dead end", (unsigned long)t.descents);
    printf("  %-26s %10lu\n", "pieces used", (unsigned long)t.trace_len);
    printf("  %-26s %10s\n", "outcome", t.gave_up ? "ran out" : "finished");

    /* Nothing it produces is opaque: this is where each piece came from. */
    printf("\n  it was assembled from: ");
    unsigned char buf[RZ_FRAG_SPAN];
    for (uint32_t i = 0; i < t.trace_len && i < 8; i++) {
        size_t n = rz_expand(m, t.trace[i], buf, sizeof buf);
        printf("%s\"", i ? " | " : "");
        print_trace(buf, n);
        printf("\"");
    }
    if (t.trace_len > 8) printf(" | ... (%lu more)", (unsigned long)(t.trace_len - 8));
    printf("\n\n");

    rz_free(m);
    free(m);
    return 0;
}

static const char *dream_name(rz_dream_kind k)
{
    switch (k) {
    case RZ_DREAM_CALM:   return "CALM   — go back over the wall";
    case RZ_DREAM_SETTLE: return "SETTLE — make the new solid";
    case RZ_DREAM_LOOSEN: return "LOOSEN — shake it out of the rut";
    default:              return "none";
    }
}

static int cmd_sleep(int argc, char **argv)
{
    uint64_t seed = (argc > 0) ? strtoull(argv[0], NULL, 10) : 1;

    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    MKDIR(NEST);
    rz_set_nest(m, MODULES);

    if (rz_load(m, MIND_PATH) != RZ_OK) {
        fprintf(stderr, "no usable mind at %s — feed it first.\n", MIND_PATH);
        free(m);
        return 1;
    }

    printf("\n=== RAZAR SLEEPS ===\n\n");
    printf("  since it last slept\n");
    printf("    %-24s %12llu\n", "steps walked", (unsigned long long)m->awake_steps);
    printf("    %-24s %12llu\n", "new structure built", (unsigned long long)m->awake_births);
    printf("    %-24s %12.4f\n", "strain per step",
           m->awake_steps ? m->awake_strain / (double)m->awake_steps : 0.0);
    printf("    %-24s %12llu\n", "its ideas called it", (unsigned long long)m->awake_anticipated);

    rz_dream_kind want = rz_dream_needed(m);
    printf("\n  it needs: %s\n", dream_name(want));
    printf("  because %s\n", rz_dream_reason(want));

    uint64_t before = rz_attention_total(m);
    rz_dream_kind had = rz_sleep(m, seed);

    if (had == RZ_DREAM_NONE) {
        printf("\n  it stayed awake.\n\n");
        rz_free(m); free(m);
        return 0;
    }

    uint64_t after = rz_attention_total(m);
    printf("\n  It dreamt. The episode is gone — a dream records nothing that\n");
    printf("  happened. What it left behind is where to look again.\n\n");
    printf("  %-26s %12llu\n", "attention laid down", (unsigned long long)(after - before));
    printf("  %-26s %12llu\n", "nights slept in all", (unsigned long long)m->sleeps_had);
    printf("  %-26s %5llu loosen  %5llu settle  %5llu calm\n", "of which",
           (unsigned long long)m->dreams[RZ_DREAM_LOOSEN],
           (unsigned long long)m->dreams[RZ_DREAM_SETTLE],
           (unsigned long long)m->dreams[RZ_DREAM_CALM]);

    if (rz_save(m, MIND_PATH) != RZ_OK)
        fprintf(stderr, "could not write %s — the night is lost.\n", MIND_PATH);
    printf("\n");

    rz_free(m);
    free(m);
    return 0;
}

/* --- senses ---------------------------------------------------------------
 *
 * Watch it grow its own ways of looking. Nothing here names a single one of
 * them: each is drawn from a template, born by mutating one it already holds,
 * and kept or dropped by what it turns out to be worth.
 */
static int cmd_senses(int argc, char **argv)
{
    int rounds = (argc > 0) ? atoi(argv[0]) : 120;
    if (rounds < 1) rounds = 1;

    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    MKDIR(NEST);
    rz_set_nest(m, MODULES);
    if (rz_load(m, MIND_PATH) != RZ_OK) {
        fprintf(stderr, "nothing remembered yet. Feed it first.\n");
        free(m);
        return 1;
    }

    /* a slice is enough to score a sense: it is measuring information, not
       reading the library. Scoring every sense over every byte, every round,
       made the faculty cost more than the organism. */
    /* Say which material, or it silently measures whatever happens to be
       first in the folder -- and then the panel is about something other than
       what the reader thinks. */
    const char *path = (argc > 1) ? argv[1] : FEED_DIR "/organismo.c";
    static unsigned char buf[6000];
    size_t n = 0;
    {
        FILE *f = fopen(path, "rb");
        if (f) { n = fread(buf, 1, sizeof buf, f); fclose(f); }
    }
    if (n == 0) {
        fprintf(stderr, "no material in %s/\n", FEED_DIR);
        free(m);
        return 1;
    }

    printf("\n=== RAZAR GROWS ITS OWN WAYS OF LOOKING ===\n\n");
    printf("  material %zu bytes, %d rounds\n\n", n, rounds);
    printf("  %6s %7s %7s %8s %7s   %s\n",
           "round", "held", "born", "pruned", "kept", "the newest way of looking");

    for (int r = 0; r < rounds; r++) {
        /* it lives by EATING: every step it takes is what scores its senses.
           Nothing re-reads the material on their behalf. */
        rz_ingest(m, buf, n);
        rz_senses_live(m, (uint64_t)r * 2654435761u + 1u);
        if (r % 5 == 4 || r == rounds - 1) {
            char what[160];
            rz_sense_describe(m, rz_senses_count(m) - 1, what, sizeof what);
            printf("  %6d %7d %7llu %8llu %7llu   %s\n", r + 1,
                   rz_senses_count(m),
                   (unsigned long long)m->senses_born,
                   (unsigned long long)m->senses_pruned,
                   (unsigned long long)m->senses_kept_neutral, what);
        }
    }

    printf("\n  -- what it ended up looking for, best first --\n");
    int32_t order[RZ_SENSES];
    int32_t ns = rz_senses_count(m);
    for (int32_t i = 0; i < ns; i++) order[i] = i;
    for (int32_t a = 1; a < ns; a++) {
        int32_t v = order[a], b = a - 1;
        while (b >= 0 && rz_sense_score(m, order[b]) < rz_sense_score(m, v)) {
            order[b + 1] = order[b];
            b--;
        }
        order[b + 1] = v;
    }

    int stacked = 0;
    for (int32_t i = 0; i < ns; i++) {
        const rz_sense *sn = rz_sense_at(m, i);
        if (sn && sn->src) stacked++;
    }
    for (int32_t k = 0; k < ns && k < 14; k++) {
        char what[160];
        rz_sense_describe(m, order[k], what, sizeof what);
        const rz_sense *sn = rz_sense_at(m, order[k]);
        printf("     %9.6f nats  %s%s\n", rz_sense_score(m, order[k]), what,
               (sn && sn->src) ? "   <- built on another sense" : "");
    }

    printf("\n  senses standing on other senses      %d of %d\n", stacked, ns);
    printf("  neutral ones kept rather than pruned %llu\n",
           (unsigned long long)m->senses_kept_neutral);
    printf("\n  A sense that says little today is what another is built from\n");
    printf("  tomorrow. Pruning for usefulness starves the composition.\n\n");

    /* what it grew is kept. Growing senses and dropping them at exit is how
       an organism relearns the same thing forever. */
    if (rz_save(m, MIND_PATH) != RZ_OK)
        fprintf(stderr, "warning: could not write %s\n", MIND_PATH);

    rz_free(m);
    free(m);
    return 0;
}


/* --- dash ------------------------------------------------------------------
 *
 * The whole organism as one JSON object, so an instrument outside C can read
 * it. It reads a saved mind and changes nothing: measuring may not strengthen
 * what it measures. */
static void j_str(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p < 0x20) printf("\\u%04x", *p);
        else putchar(*p);
    }
    putchar('"');
}

static int cmd_dash(int argc, char **argv)
{
    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    rz_set_nest(m, MODULES);
    if (rz_load(m, MIND_PATH) != RZ_OK) {
        fprintf(stderr, "nothing remembered yet. Feed it first.\n");
        free(m);
        return 1;
    }

    rz_autopsy a;
    rz_examine(m, &a);
    uint64_t rhit = 0, rmiss = 0;
    for (uint32_t ri = 0; ri < rz_rules_count(m); ri++) {
        rhit += m->rules[ri].hits;
        rmiss += m->rules[ri].misses;
    }

    printf("{\n");
    printf("  \"memory\": {\n");
    printf("    \"atomsEaten\": %llu,\n",   (unsigned long long)a.atoms_ingested);
    printf("    \"paths\": %llu,\n",        (unsigned long long)a.distinct_paths);
    printf("    \"atomsSeen\": %llu,\n",    (unsigned long long)a.distinct_atoms);
    printf("    \"fragments\": %llu,\n",    (unsigned long long)a.frags);
    printf("    \"births\": %llu,\n",       (unsigned long long)a.births);
    printf("    \"fragSteps\": %llu,\n",    (unsigned long long)a.frag_steps);
    printf("    \"orphans\": %llu,\n",      (unsigned long long)a.orphans);
    printf("    \"entropy\": %.6f,\n",      a.entropy);
    printf("    \"support\": %.2f,\n",      a.effective_support);
    printf("    \"top1Share\": %.4f,\n",    a.top1_share);
    printf("    \"plasticBytes\": %llu,\n", (unsigned long long)a.plastic_bytes);
    printf("    \"costRatio\": %.6f\n",
           a.atoms_ingested ? (double)a.plastic_bytes / (double)a.atoms_ingested : 0.0);
    printf("  },\n");

    printf("  \"modules\": { \"used\": %llu, \"widest\": %llu, \"busiest\": %llu,"
           " \"wakes\": %llu, \"sleeps\": %llu },\n",
           (unsigned long long)a.modules_used, (unsigned long long)a.widest_module,
           (unsigned long long)a.busiest_frag, (unsigned long long)a.wakes,
           (unsigned long long)a.sleeps);

    printf("  \"forgetting\": { \"passes\": %llu, \"coldPaths\": %llu },\n",
           (unsigned long long)a.decay_passes, (unsigned long long)a.cold_edges);

    /* the shape alphabet: how many of the sixty-four roles anybody occupies.
       It collapsed to four once, silently, so it is on the dashboard now. */
    {
        uint32_t occupied = 0, biggest = 0, total = 0;
        printf("  \"shapes\": { \"occupancy\": [");
        for (uint8_t sh = 0; sh < RZ_SHAPES; sh++) {
            uint32_t n = rz_shape_members(m, sh);
            total += n;
            if (n) occupied++;
            if (n > biggest) biggest = n;
            printf("%s%u", sh ? "," : "", n);
        }
        printf("], \"occupied\": %u, \"of\": %d, \"largestShare\": %.4f },\n",
               occupied, RZ_SHAPES, total ? (double)biggest / (double)total : 0.0);
    }

    printf("  \"night\": { \"steps\": %llu, \"built\": %llu, \"strain\": %.4f,"
           " \"anticipated\": %llu, \"slept\": %llu,"
           " \"needs\": ", (unsigned long long)m->awake_steps,
           (unsigned long long)m->awake_births,
           m->awake_steps ? m->awake_strain / (double)m->awake_steps : 0.0,
           (unsigned long long)m->awake_anticipated,
           (unsigned long long)m->sleeps_had);
    j_str(rz_dream_reason(rz_dream_needed(m)));
    printf(", \"dreams\": [%llu,%llu,%llu,%llu] },\n",
           (unsigned long long)m->dreams[0], (unsigned long long)m->dreams[1],
           (unsigned long long)m->dreams[2], (unsigned long long)m->dreams[3]);

    printf("  \"transfer\": { \"shapesThatCarry\": %llu, \"bestShape\": %.4f },\n",
           (unsigned long long)a.shapes_transfer, a.best_shape);

    /* the senses it grew, each with what it is worth and whether it stands on
       another sense */
    printf("  \"senses\": [");
    for (int32_t i = 0; i < rz_senses_count(m); i++) {
        char what[160];
        rz_sense_describe(m, i, what, sizeof what);
        const rz_sense *s = rz_sense_at(m, i);
        printf("%s\n    { \"what\": ", i ? "," : "");
        j_str(what);
        printf(", \"worth\": %.6f, \"watched\": %llu, \"stacked\": %s }",
               rz_sense_score(m, i), (unsigned long long)rz_sense_observations(m, i),
               (s && s->src) ? "true" : "false");
    }
    printf("%s  ],\n", rz_senses_count(m) ? "\n" : "");

    printf("  \"rules\": { \"held\": %u, \"born\": %llu, \"used\": %llu,"
           " \"right\": %llu, \"wrong\": %llu },\n",
           rz_rules_count(m), (unsigned long long)m->rules_born,
           (unsigned long long)m->rules_used,
           (unsigned long long)rhit, (unsigned long long)rmiss);

    /* The conversation it has with itself: what it expected, what came,
       and whether a rule was the one that spoke. */
    printf("  \"inner\": { \"said\": %llu, \"right\": %llu, \"lately\": [",
           (unsigned long long)rz_inner_said(m),
           (unsigned long long)rz_inner_right(m));
    {
        uint32_t have = rz_inner_count(m);
        uint32_t show = have < 20 ? have : 20;
        for (uint32_t k = 0; k < show; k++) {
            rz_inner e;
            if (!rz_inner_at(m, k, &e)) break;
            unsigned char here[RZ_FRAG_SPAN + 1], came[RZ_FRAG_SPAN + 1];
            size_t hn = rz_expand(m, e.from, here, sizeof here - 1);
            size_t cn = e.actual >= 0
                      ? rz_expand(m, e.actual, came, sizeof came - 1) : 0;
            here[hn] = 0; came[cn] = 0;
            printf("%s\n    { \"at\": ", k ? "," : "");
            j_str((const char *)here);
            printf(", \"came\": ");
            j_str((const char *)came);
            printf(", \"right\": %s, \"byRule\": %s }",
                   e.right ? "true" : "false",
                   e.rule >= 0 ? "true" : "false");
        }
        printf("%s  ] }\n", show ? "\n" : "");
    }
    printf("}\n");

    rz_free(m);
    free(m);
    (void)argc; (void)argv;
    return 0;
}


/* --- think ----------------------------------------------------------------
 *
 * The same eating, narrated. It reads the material in mouthfuls and after each
 * one says a little of what it has been telling itself -- what it expected,
 * what came, and whether it was right.
 *
 * Every line is flushed as it is written, so something watching from outside
 * sees it thinking rather than a transcript handed over at the end.
 */
static void say_piece(rz_mind *m, int32_t frag, char *out, size_t cap)
{
    unsigned char raw[RZ_FRAG_SPAN + 1];
    size_t n = (frag >= 0) ? rz_expand(m, frag, raw, sizeof raw - 1) : 0;
    raw[n] = 0;
    size_t w = 0;
    for (size_t i = 0; i < n && w + 5 < cap; i++) {
        unsigned char c = raw[i];
        if (c == '\n')      { out[w++] = '\\'; out[w++] = 'n'; }
        else if (c == '\r') { out[w++] = '\\'; out[w++] = 'r'; }
        else if (c == '\t') { out[w++] = '\\'; out[w++] = 't'; }
        else if (c < 0x20)  { out[w++] = '.'; }
        else                { out[w++] = (char)c; }
    }
    out[w] = 0;
    if (w == 0) snprintf(out, cap, "(nothing)");
}

static int cmd_think(int argc, char **argv)
{
    const char *path = (argc > 0) ? argv[0] : FEED_DIR "/nuevo-testamento.txt";
    size_t mouthful = (argc > 1) ? (size_t)strtoul(argv[1], NULL, 10) : 4000;
    if (mouthful < 256) mouthful = 256;

    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    MKDIR(NEST);
    rz_set_nest(m, MODULES);
    int rc = rz_load(m, MIND_PATH);
    if (rc != RZ_OK && rc != RZ_ERR_MISSING) {
        fprintf(stderr, "refusing to run on an incompatible mind\n");
        free(m);
        return 1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); free(m); return 1; }

    printf("thinking out loud over %s\n\n", path);
    fflush(stdout);

    unsigned char *buf = malloc(mouthful);
    if (!buf) { fclose(f); free(m); return 1; }

    size_t n;
    uint64_t said_before = rz_inner_said(m);
    unsigned long mouthfuls = 0;
    while ((n = fread(buf, 1, mouthful, f)) > 0) {
        uint64_t before = rz_inner_said(m);
        rz_ingest(m, buf, n);
        mouthfuls++;

        /* a couple of exchanges from the mouthful just swallowed. Not all of
           them: a flood is not a conversation. */
        uint32_t have = rz_inner_count(m);
        uint32_t show = have < 3 ? have : 3;
        for (uint32_t i = 0; i < show; i++) {
            rz_inner e;
            if (!rz_inner_at(m, i, &e)) break;
            char at[80], came[80], want[80];
            say_piece(m, e.from, at, sizeof at);
            say_piece(m, e.actual, came, sizeof came);
            say_piece(m, e.expected, want, sizeof want);
            if (e.right)
                printf("  at %-18s it saw %-18s coming.%s\n", at, came,
                       e.rule >= 0 ? "   (a rule said so)" : "");
            else
                printf("  at %-18s it expected %-14s and got %s\n", at, want, came);
        }
        uint64_t said = rz_inner_said(m) - before;
        printf("  — mouthful %lu: %llu exchanges, right %.1f%% in all\n\n",
               mouthfuls, (unsigned long long)said,
               100.0 * (double)rz_inner_right(m) / (double)(rz_inner_said(m) | 1));
        fflush(stdout);
    }
    fclose(f);
    free(buf);

    printf("it said %llu things to itself, and was right %llu times.\n",
           (unsigned long long)(rz_inner_said(m) - said_before),
           (unsigned long long)rz_inner_right(m));
    fflush(stdout);

    MKDIR(NEST);
    if (rz_save(m, MIND_PATH) != RZ_OK)
        fprintf(stderr, "warning: could not write %s\n", MIND_PATH);

    rz_free(m);
    free(m);
    return 0;
}


/* Line breaks are read as continuous writing: every run of line breaks is one
   space (none if a space is already there). The author's decision
   (2026-10-08): a line break is layout, not language. Applied where material
   is read into words and legos; `feed` still eats the raw bytes. */
static size_t continuous(unsigned char *t, size_t n)
{
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = t[i];
        if (c == 13 || c == 10) {      /* carriage return, line feed */
            if (k > 0 && t[k - 1] == ' ') continue;
            c = ' ';
        }
        t[k++] = c;
    }
    return k;
}

/* --- the legos (P33) ------------------------------------------------------ */

static void say_lego(const rz_lexicon *x, int32_t id, char *out, size_t cap)
{
    size_t k = 0;
    if (id < 0 || (uint32_t)id >= x->n) { snprintf(out, cap, "(none)"); return; }
    out[k++] = '[';
    for (uint8_t i = 0; i < x->lego[id].len && k + 2 < cap; i++) {
        unsigned char c = x->lego[id].s[i];
        out[k++] = (c == '\n' || c == '\r' || c == '\t') ? ' ' : (char)c;
    }
    out[k++] = ']';
    out[k] = 0;
}

typedef struct { uint64_t steps, kind_ok, kind_base, lego_ok, memory_ok, common_ok; } exam_row;

static void exam_print(const char *label, const exam_row *r)
{
    if (!r->steps) return;
    double n = (double)r->steps;
    printf("  %-28s %7llu   kind %5.1f%% (always the same kind %5.1f%%)   lego %5.1f%% "
           "(memory alone %5.1f%%, commonest %4.1f%%)\n",
           label, (unsigned long long)r->steps,
           100.0 * (double)r->kind_ok / n, 100.0 * (double)r->kind_base / n,
           100.0 * (double)r->lego_ok / n, 100.0 * (double)r->memory_ok / n,
           100.0 * (double)r->common_ok / n);
}

static int cmd_legos(int argc, char **argv)
{
    const char *path = (argc > 0) ? argv[0] : FEED_DIR "/nuevo-testamento.txt";
    uint32_t kinds = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 40;
    if (kinds < 2) kinds = 2;

    rz_mind *m = malloc(sizeof(*m));
    if (!m) { fprintf(stderr, "out of memory\n"); return 1; }
    rz_init(m);
    MKDIR(NEST);

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); rz_free(m); free(m); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);
    if (n < 1000) { fprintf(stderr, "not enough material in %s\n", path); free(t); rz_free(m); free(m); return 1; }

    /* learn on four fifths, examine on the fifth it never read */
    size_t split = n / 5 * 4;
    while (split < n && t[split] != ' ' && t[split] != '\n') split++;

    /* The mind that cuts has read the four fifths and nothing else: the one in
       the nest has eaten other material (and maybe this fifth), which moves
       where it is uncertain and would leak the exam. Twice, as the probes did.
       It has no nest, so nothing it learns here is written anywhere. */
    for (int pass = 0; pass < 2; pass++) rz_ingest(m, t, split);

    rz_lexicon x;
    rz_lexicon_init(&x);
    rz_lexicon_read(&x, m, t, split);
    rz_lexicon_sort(&x, kinds, 7);
    printf("cut %zu bytes into %u different legos, sorted into %u kinds by their company\n\n",
           split, x.n, x.kinds);

    /* what the kinds look like */
    printf("the kinds it found, by their commonest members:\n\n");
    for (uint32_t k = 0; k < x.kinds && k < 14; k++) {
        printf("  kind %2u ", k);
        int32_t shown[8];
        int ns = 0;
        for (int s = 0; s < 8; s++) {
            int32_t best = -1;
            for (uint32_t i = 0; i < x.n; i++) {
                if (x.lego[i].kind != (int32_t)k) continue;
                int dup = 0;
                for (int q = 0; q < ns; q++) if (shown[q] == (int32_t)i) dup = 1;
                if (!dup && (best < 0 || x.lego[i].count > x.lego[best].count)) best = (int32_t)i;
            }
            if (best < 0) break;
            shown[ns++] = best;
            char w[80];
            say_lego(&x, best, w, sizeof w);
            printf(" %s", w);
        }
        printf("\n");
    }

    /* the exam */
    const unsigned char *h = t + split;
    size_t hn = n - split;
    unsigned char *ends = malloc(hn);
    int32_t *ids = malloc(hn * sizeof *ids);
    size_t nl = 0, start = 0, never = 0;
    rz_lego_cut(m, h, hn, ends);
    for (size_t i = 0; i < hn; i++) {
        if (!ends[i]) continue;
        ids[nl] = rz_lexicon_find(&x, h + start, i + 1 - start);
        if (ids[nl] < 0) never++;
        nl++;
        start = i + 1;
    }

    /* baselines, each from the four fifths only */
    uint64_t *into = calloc(x.kinds, sizeof *into);
    for (uint32_t a = 0; a < x.kinds; a++)
        for (uint32_t b = 0; b < x.kinds; b++) into[b] += x.follow[(size_t)a * x.kinds + b];
    uint32_t majority = 0;
    for (uint32_t k = 1; k < x.kinds; k++) if (into[k] > into[majority]) majority = k;
    int32_t commonest = 0;
    for (uint32_t i = 1; i < x.n; i++) if (x.lego[i].count > x.lego[commonest].count) commonest = (int32_t)i;

    exam_row seen = {0}, unseen = {0}, guess = {0};
    for (size_t i = 1; i < nl; i++) {
        int32_t a = ids[i - 1], b = ids[i];
        if (a < 0 || b < 0) continue;
        int32_t ka = rz_lexicon_kind(&x, a), kb = rz_lexicon_kind(&x, b);
        if (ka < 0 || kb < 0) continue;

        int32_t mem = -1;
        uint32_t most = 0, had = 0;
        for (uint32_t o = x.out_start[a]; o < x.out_start[a + 1]; o++) {
            const rz_lego_pair *q = &x.pair[x.out_pair[o]];
            if (q->count > most) { most = q->count; mem = (int32_t)q->b; }
            if ((int32_t)q->b == b) had = 1;
        }
        exam_row *r = had ? &seen : &unseen;
        r->steps++;
        r->kind_ok += rz_lexicon_next_kind(&x, ka) == kb;
        r->kind_base += (int32_t)majority == kb;
        r->lego_ok += rz_lexicon_next(&x, a) == b;
        r->memory_ok += mem == b;
        r->common_ok += commonest == b;

        /* recognise a lego by where it sits: hide it, read its neighbours */
        if (i + 1 < nl) {
            int32_t g = rz_lexicon_guess_kind(&x, a, ids[i + 1]);
            guess.steps++;
            guess.kind_ok += g == kb;
            guess.kind_base += (int32_t)majority == kb;
        }
    }

    printf("\non the fifth it never read: %zu legos, %zu of them never met before\n\n", nl, never);
    exam_print("it had seen this pair", &seen);
    exam_print("it had NEVER seen this pair", &unseen);
    if (guess.steps)
        printf("\n  a lego hidden, its kind read from its two neighbours: %.1f%% (always the same kind %.1f%%)\n",
               100.0 * (double)guess.kind_ok / (double)guess.steps,
               100.0 * (double)guess.kind_base / (double)guess.steps);

    if (rz_lexicon_save(&x, LEXICON) != RZ_OK)
        fprintf(stderr, "warning: could not write %s\n", LEXICON);
    else
        printf("\n  kept in %s\n", LEXICON);

    free(into); free(ids); free(ends); free(t);
    rz_lexicon_free(&x);
    rz_free(m);
    free(m);
    return 0;
}


/* --- curiosity (P34) ------------------------------------------------------ */

/* The person types a word; the lexicon holds it as several legos ("Cristo ",
   "Cristo, ", "Cristo."). Take the commonest one that is the word plus only
   its edge -- this is the typing interface, not something the organism is told. */
static int32_t lego_for_word(const rz_lexicon *x, const char *w)
{
    size_t wl = strlen(w);
    int32_t best = -1;
    for (uint32_t i = 0; i < x->n; i++) {
        const rz_lego *g = &x->lego[i];
        if (g->len < wl || memcmp(g->s, w, wl) != 0) continue;
        int edge = 1;
        for (size_t k = wl; k < g->len; k++) {
            unsigned char c = g->s[k];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80) edge = 0;
        }
        if (edge && (best < 0 || g->count > x->lego[best].count)) best = (int32_t)i;
    }
    if (best >= 0) return best;

    /* A Windows console hands "Jesús" over in Latin-1 while the text is UTF-8:
       try the word again re-encoded. */
    char u[128];
    size_t k = 0, high = 0;
    for (size_t i = 0; i < wl && k + 2 < sizeof u; i++) {
        unsigned char c = (unsigned char)w[i];
        if (c < 0x80) { u[k++] = (char)c; continue; }
        u[k++] = (char)(0xC0 | (c >> 6));
        u[k++] = (char)(0x80 | (c & 0x3F));
        high++;
    }
    u[k] = 0;
    return high ? lego_for_word(x, u) : -1;
}

static int cmd_ask(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "usage: razar ask <word> [relation] [path]\n");
        return 1;
    }
    const char *word = argv[0];
    const char *rel = (argc > 1) ? argv[1] : "es";
    const char *path = (argc > 2) ? argv[2] : FEED_DIR "/nuevo-testamento.txt";

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);

    rz_mind *m = malloc(sizeof(*m));
    if (!m || !t) { fprintf(stderr, "out of memory\n"); free(t); free(m); return 1; }
    rz_init(m);
    for (int pass = 0; pass < 2; pass++) rz_ingest(m, t, n);
    rz_lexicon x;
    rz_lexicon_init(&x);
    rz_lexicon_read_words(&x, m, t, n);   /* a question is asked in words */

    int32_t subject = lego_for_word(&x, word), relation = lego_for_word(&x, rel);
    if (subject < 0 || relation < 0) {
        printf("it has never met %s\n", subject < 0 ? word : rel);
    } else {
        int32_t chain[12];
        rz_wonder_end end;
        uint32_t k = rz_lexicon_wonder(&x, subject, relation, chain, 12, &end);
        for (uint32_t i = 0; i < k; i++) {
            char w[80];
            say_lego(&x, chain[i], w, sizeof w);
            rz_answer a[4];
            uint32_t got = rz_lexicon_ask(&x, chain[i], relation, a, 4);
            printf("%s %s %s ?\n", i == 0 ? "asked:" : "  why?", w, rel);
            for (uint32_t j = 0; j < got; j++) {
                char ans[80];
                say_lego(&x, a[j].lego, ans, sizeof ans);
                printf("        %s %-22s said %u time%s\n",
                       a[j].dir > 0 ? "it is  " : "it is said of", ans,
                       a[j].support, a[j].support == 1 ? "" : "s");
            }
        }
        printf("\n%s\n", end == RZ_WONDER_CIRCLE ? "the answers lead back to what it asked: a circle closed"
                       : end == RZ_WONDER_GAP    ? "nothing answers the last one: the question stays open"
                                                 : "still asking when it ran out of room");
    }
    rz_lexicon_free(&x);
    rz_free(m);
    free(m);
    free(t);
    return 0;
}

/* --- behaviours (P35) ----------------------------------------------------- */

static int cmd_play(int argc, char **argv)
{
    uint32_t acts = (argc > 0) ? (uint32_t)strtoul(argv[0], NULL, 10) : 300;
    const char *shown = (argc > 1) ? argv[1] : "es";
    const char *path = FEED_DIR "/nuevo-testamento.txt";

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);

    /* it lives four fifths; the last fifth is what comes later, which it has
       not read -- an answer is worth what it carries into it */
    size_t split = n / 5 * 4;
    while (split < n && t[split] != ' ') split++;
    rz_mind *m = malloc(sizeof(*m));
    if (!m || !t) { fprintf(stderr, "out of memory\n"); free(t); free(m); return 1; }
    rz_init(m);
    for (int pass = 0; pass < 2; pass++) rz_ingest(m, t, split);
    rz_lexicon x;
    rz_lexicon_init(&x);
    rz_lexicon_read_words(&x, m, t, split);
    int32_t *later = malloc((n - split) * sizeof *later);
    uint32_t nlater = later ? rz_lexicon_words_of(&x, m, t + split, n - split,
                                                  later, (uint32_t)(n - split)) : 0;

    int32_t form = lego_for_word(&x, shown);
    printf("it lived %zu bytes; shown how to ask with [%s]; %u acts\n\n", split, shown, acts);

    double total[2] = {0, 0};
    for (int run = 0; run < 2; run++) {
        rz_player p;
        rz_player_init(&p);
        if (run == 1) p.blind = 0x9E3779B97F4A7C15ull;    /* the bar */
        rz_player_imitate(&p, form);
        rz_act a;
        uint32_t done = 0, shown_acts = 0;
        while (done < acts && rz_player_act(&p, &x, later, nlater, &a)) {
            done++;
            total[run] += a.pay;
            if (run == 0 && a.answer >= 0 && a.pay > 0.2 && shown_acts < 15) {
                char s[80], r[80], w[80];
                say_lego(&x, a.subject, s, sizeof s);
                say_lego(&x, a.relation, r, sizeof r);
                say_lego(&x, a.answer, w, sizeof w);
                printf("  %s %s ?  ->  %-16s harmony %+.2f  transfer %+.2f%s\n",
                       s, r, w, a.harmony, a.transfer, a.new_form ? "   (a new way of asking)" : "");
                shown_acts++;
            }
        }
        if (run == 0) {
            printf("\n  ways of asking, by what they paid:\n");
            for (uint32_t i = 0; i < p.nhabit; i++) {
                const rz_habit *h = &p.habit[i];
                if (!h->tries) continue;
                char r[80];
                say_lego(&x, h->relation, r, sizeof r);
                printf("    %-12s tried %3u   pays %+.3f%s\n", r, h->tries, h->paid / h->tries,
                       h->imitated ? "   (shown to it)" : "");
            }
        }
        printf("\n  %s: %u acts, mean pay %+.4f\n", run == 0 ? "choosing" : "blind (the bar)",
               done, done ? total[run] / done : 0.0);
        total[run] = done ? total[run] / done : 0.0;
        rz_player_free(&p);
    }

    free(later);
    rz_lexicon_free(&x);
    rz_free(m);
    free(m);
    free(t);
    return 0;
}

/* --- concepts (P36) ------------------------------------------------------- */

static int cmd_concepts(int argc, char **argv)
{
    const char *rel = (argc > 0) ? argv[0] : "es";
    const char *path = (argc > 1) ? argv[1] : FEED_DIR "/nuevo-testamento.txt";

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);

    /* four fifths lived; the last fifth is what comes later, not read */
    size_t split = n / 5 * 4;
    while (split < n && t[split] != ' ') split++;
    rz_mind *m = malloc(sizeof(*m));
    if (!m || !t) { fprintf(stderr, "out of memory\n"); free(t); free(m); return 1; }
    rz_init(m);
    for (int pass = 0; pass < 2; pass++) rz_ingest(m, t, split);
    rz_lexicon x;
    rz_lexicon_init(&x);
    rz_lexicon_read_words(&x, m, t, split);
    int32_t *later = malloc((n - split) * sizeof *later);
    uint32_t nlater = later ? rz_lexicon_words_of(&x, m, t + split, n - split,
                                                  later, (uint32_t)(n - split)) : 0;

    int32_t relation = lego_for_word(&x, rel);
    rz_concept *c = malloc(512 * sizeof *c);

    /* the question's silhouette: the words that keep the company it keeps */
    int32_t rels[64];
    uint32_t nrel = 0;
    if (relation >= 0) {
        rels[nrel++] = relation;
        nrel += rz_lexicon_silhouette(&x, relation, rels + 1, 63);
    }
    printf("the silhouette of \"X %s ?\":", rel);
    for (uint32_t i = 0; i < nrel; i++) {
        char w[80];
        say_lego(&x, rels[i], w, sizeof w);
        printf(" %s", w);
    }
    printf("\n");

    /* the same question asked with the word alone, as the bar for the silhouette */
    uint32_t k = (relation >= 0 && c) ? rz_concepts_find(&x, relation, later, nlater, c, 512) : 0;
    {
        uint32_t adopted = 0;
        double th = 0, tt = 0;
        for (uint32_t i = 0; i < k; i++) { adopted += c[i].adopted; th += c[i].harmony; tt += c[i].transfer; }
        printf("  asked with [%s] alone:      %3u concepts, %3u adopted, mean harmony %+.4f, transfer %+.4f\n",
               rel, k, adopted, k ? th / k : 0.0, k ? tt / k : 0.0);
    }
    k = (nrel && c) ? rz_concepts_find_any(&x, rels, nrel, later, nlater, c, 512) : 0;

    uint32_t adopted = 0;
    double th = 0, tt = 0;
    for (uint32_t i = 0; i < k; i++) { adopted += c[i].adopted; th += c[i].harmony; tt += c[i].transfer; }
    printf("  asked with the silhouette: %3u concepts, %3u adopted, mean harmony %+.4f, transfer %+.4f\n\n",
           k, adopted, k ? th / k : 0.0, k ? tt / k : 0.0);
    printf("of everything it lived (%zu bytes), asked with the silhouette:\n\n", split);
    for (int pass = 1; pass >= 0; pass--) {
        printf("  %s\n", pass ? "adopted:" : "not adopted:");
        uint32_t shown = 0;
        for (uint32_t i = 0; i < k && shown < 15; i++) {
            if (c[i].adopted != pass) continue;
            char h[80], w[80];
            say_lego(&x, c[i].head, h, sizeof h);
            printf("    %-16s harmony %+.3f  transfer %+.3f   ", h, c[i].harmony, c[i].transfer);
            for (uint32_t j = 0; j < c[i].nmember && j < 6; j++) {
                say_lego(&x, c[i].member[j], w, sizeof w);
                printf(" %s", w);
            }
            printf("%s\n", c[i].nmember > 6 ? " ..." : "");
            shown++;
        }
    }
    if (k) printf("\n  mean gain over the blind groups: harmony %+.4f, transfer %+.4f\n", th / k, tt / k);

    free(c);
    free(later);
    rz_lexicon_free(&x);
    rz_free(m);
    free(m);
    free(t);
    return 0;
}

/* --- types and identities (P37) ------------------------------------------- */

/* the bar only: a word that starts with a capital, ASCII or the Latin-1 range
   as UTF-8 writes it */
static int capital(const rz_lego *g)
{
    unsigned char c = g->s[0];
    if (c >= 'A' && c <= 'Z') return 1;
    return c == 0xC3 && g->len > 1 && g->s[1] >= 0x80 && g->s[1] <= 0x9E;
}

static int cmd_roles(int argc, char **argv)
{
    const char *seed_word = (argc > 0) ? argv[0] : "hijo";
    const char *path = (argc > 1) ? argv[1] : FEED_DIR "/nuevo-testamento.txt";

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);

    rz_mind *m = malloc(sizeof(*m));
    if (!m || !t) { fprintf(stderr, "out of memory\n"); free(t); free(m); return 1; }
    rz_init(m);
    for (int pass = 0; pass < 2; pass++) rz_ingest(m, t, n);
    rz_lexicon x;
    rz_lexicon_init(&x);
    rz_lexicon_read_words(&x, m, t, n);

    int32_t seed = lego_for_word(&x, seed_word), bind = -1;
    uint8_t *ty = calloc(x.n, 1), *id = calloc(x.n, 1);
    uint32_t ids = (seed >= 0 && ty && id) ? rz_lexicon_roles(&x, seed, &bind, ty, id) : 0;
    char w[80];
    if (bind < 0) { printf("[%s] binds to nothing it can find\n", seed_word); goto done; }
    say_lego(&x, bind, w, sizeof w);
    printf("[%s] asks for an owner through %s\n\n", seed_word, w);

    for (int pass = 0; pass < 2; pass++) {
        uint8_t *flag = pass ? id : ty;
        uint32_t total = 0, caps = 0, shown = 0;
        for (uint32_t i = 0; i < x.n; i++) if (flag[i]) { total++; caps += capital(&x.lego[i]); }
        printf("  %s: %u, %.0f%% with a capital\n    ", pass ? "identities" : "types", total,
               100.0 * caps / (total ? total : 1));
        uint8_t *done = calloc(x.n, 1);
        while (shown < 40) {
            int32_t b = -1;
            for (uint32_t i = 0; i < x.n; i++)
                if (flag[i] && !done[i] && (b < 0 || x.lego[i].count > x.lego[b].count)) b = (int32_t)i;
            if (b < 0) break;
            done[b] = 1;
            say_lego(&x, b, w, sizeof w);
            printf(" %s", w);
            shown++;
        }
        free(done);
        printf("\n\n");
    }
    {
        uint32_t base = 0, nb = 0;
        for (uint32_t i = 0; i < x.n; i++) if (x.lego[i].count >= 2) { nb++; base += capital(&x.lego[i]); }
        printf("  bar: of all words seen twice or more, %.0f%% start with a capital\n", 100.0 * base / (nb ? nb : 1));
    }
    (void)ids;
done:
    free(ty); free(id);
    rz_lexicon_free(&x);
    rz_free(m);
    free(m);
    free(t);
    return 0;
}

/* --- weaving (P38) -------------------------------------------------------- */

static int cmd_weave(int argc, char **argv)
{
    const char *seed_word = (argc > 0) ? argv[0] : "hijo";
    const char *path = (argc > 1) ? argv[1] : FEED_DIR "/nuevo-testamento.txt";

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);

    rz_mind *m = malloc(sizeof(*m));
    if (!m || !t) { fprintf(stderr, "out of memory\n"); free(t); free(m); return 1; }
    rz_init(m);
    for (int pass = 0; pass < 2; pass++) rz_ingest(m, t, n);
    rz_lexicon x;
    rz_lexicon_init(&x);
    rz_lexicon_read_words(&x, m, t, n);

    int32_t seed = lego_for_word(&x, seed_word);
    uint8_t *ty = calloc(x.n, 1);
    rz_binding b[RZ_BINDINGS];
    uint32_t nb = (seed >= 0 && ty) ? rz_lexicon_weave(&x, seed, b, RZ_BINDINGS, ty) : 0;
    char w[80];
    printf("woven from [%s]: %u bindings\n", seed_word, nb);
    uint8_t last = 255;
    for (uint32_t i = 0; i < nb; i++) {
        if (b[i].round != last) { printf("\n  round %u:", b[i].round); last = b[i].round; }
        say_lego(&x, b[i].word, w, sizeof w);
        printf("  %s%s%s", b[i].side < 0 ? "" : "___ ", w, b[i].side < 0 ? " ___" : "");
    }
    uint32_t nt = 0;
    for (uint32_t i = 0; i < x.n; i++) nt += ty[i];
    printf("\n\n  types: %u\n  ", nt);
    {
        static const char *look[] = { "hijo", "hija", "padre", "madre", "hermano", "hermana",
                                      "hermanos", "esposa", "marido", "amigo", "rey",
                                      "perro", "Jesús", "dijo", "y", 0 };
        for (int i = 0; look[i]; i++) {
            int32_t k = lego_for_word(&x, look[i]);
            if (k >= 0) printf(" %s%s", look[i], ty[k] ? "+" : "-");
        }
        printf("\n");
    }
    free(ty);
    rz_lexicon_free(&x);
    rz_free(m);
    free(m);
    free(t);
    return 0;
}

/* --- the melody (P39) ----------------------------------------------------- */

/* how much more often the types a melody hears in x (the first half) take an
   owner in `later` (the second half, not read) than every other word */
static double melody_lift(rz_lexicon *x, const rz_binding *b, uint32_t nb,
                          const int32_t *later, uint32_t nlater, uint32_t *cases)
{
    uint8_t *ty = calloc(x->n, 1);
    rz_binding *tmp = malloc((nb + 1) * sizeof *tmp);
    if (!ty || !tmp) { free(ty); free(tmp); return 0; }
    rz_lexicon_weave_from(x, b, nb, tmp, nb, ty);     /* types only: no room to grow */
    uint32_t n1 = 0, h1 = 0, n0 = 0, h0 = 0;
    for (uint32_t i = 0; i < nlater; i++) {
        int32_t a = later[i];
        if (a < 0) continue;
        int bound = 0;
        for (uint32_t k = 0; k < nb && !bound; k++) {
            int64_t j = (int64_t)i + b[k].side;
            bound = j >= 0 && (uint64_t)j < nlater && later[j] == b[k].word;
        }
        if (ty[a]) { n1++; h1 += bound; } else { n0++; h0 += bound; }
    }
    free(ty);
    free(tmp);
    *cases = n1;
    double r1 = n1 ? (double)h1 / n1 : 0, r0 = n0 ? (double)h0 / n0 : 0;
    return r0 > 0 ? r1 / r0 : 0;
}

/* the same, for blind melodies: every binding swapped for the word of the
   nearest frequency in this text, on the same side; the average of eight */
static double blind_lift(rz_lexicon *x, const rz_binding *b, uint32_t nb,
                         const int32_t *later, uint32_t nlater)
{
    uint32_t *ord = malloc(x->n * sizeof *ord), *rank = malloc(x->n * sizeof *rank);
    rz_binding *blind = malloc((nb + 1) * sizeof *blind);
    double sum = 0;
    int runs = 0;
    if (ord && rank && blind && nb) {
        for (uint32_t i = 0; i < x->n; i++) ord[i] = i;
        for (uint32_t gap = x->n / 2; gap > 0; gap /= 2)
            for (uint32_t i = gap; i < x->n; i++) {
                uint32_t v = ord[i], j = i;
                while (j >= gap && x->lego[ord[j - gap]].count < x->lego[v].count) { ord[j] = ord[j - gap]; j -= gap; }
                ord[j] = v;
            }
        for (uint32_t i = 0; i < x->n; i++) rank[ord[i]] = i;
        for (int j = 1; j <= 8; j++) {
            for (uint32_t k = 0; k < nb; k++) {
                int64_t r = (int64_t)rank[b[k].word] + ((j & 1) ? (j + 1) / 2 : -(j / 2));
                if (r < 0) r = 0;
                if (r >= (int64_t)x->n) r = (int64_t)x->n - 1;
                blind[k] = (rz_binding){ b[k].side, (int32_t)ord[r], 0.0, 0 };
            }
            uint32_t bc;
            sum += melody_lift(x, blind, nb, later, nlater, &bc);
            runs++;
        }
    }
    free(ord); free(rank); free(blind);
    return runs ? sum / runs : 0.0;
}

static int cmd_melody(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "usage: razar melody <path> [seed]   (seed only when no melody is kept yet)\n");
        return 1;
    }
    const char *path = argv[0];
    const char *seed_word = (argc > 1) ? argv[1] : "hijo";

    static rz_note notes[RZ_MELODY];
    uint32_t nn = 0;
    int rc = rz_melody_load(notes, RZ_MELODY, &nn, MELODY);
    if (rc != RZ_OK && rc != RZ_ERR_MISSING) { fprintf(stderr, "refusing an unreadable melody\n"); return 1; }

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);
    size_t half = n / 2;
    while (half < n && t[half] != ' ') half++;

    rz_mind *m = malloc(sizeof(*m));
    if (!m || !t) { fprintf(stderr, "out of memory\n"); free(t); free(m); return 1; }
    rz_init(m);
    for (int pass = 0; pass < 2; pass++) rz_ingest(m, t, half);
    rz_lexicon x;
    rz_lexicon_init(&x);
    rz_lexicon_read_words(&x, m, t, half);
    int32_t *later = malloc((n - half + 1) * sizeof *later);
    uint32_t nlater = later ? rz_lexicon_words_of(&x, m, t + half, n - half, later, (uint32_t)(n - half)) : 0;

    static rz_binding carried[RZ_MELODY], woven[RZ_MELODY];
    uint8_t *ty = calloc(x.n, 1);
    uint32_t nc = 0, nw = 0, nf = 0;

    /* the text's name: keys special to it carry it */
    char where[24] = "";
    {
        const char *base = path;
        for (const char *q = path; *q; q++) if (*q == '/' || *q == '\\') base = q + 1;
        size_t bl = 0;
        while (base[bl] && base[bl] != '.' && bl + 1 < sizeof where) { where[bl] = base[bl]; bl++; }
        where[bl] = 0;
    }
    if (nn == 0) {
        int32_t seed = lego_for_word(&x, seed_word);
        printf("no melody kept yet: weaving one from [%s]\n", seed_word);
        nw = seed >= 0 ? rz_lexicon_weave(&x, seed, woven, RZ_MELODY, ty) : 0;
    } else {
        /* it weaves only with the notes it trusts: they held at least as often
           as they failed, in the texts they sounded in */
        static uint32_t from[RZ_MELODY];
        static rz_binding all_c[RZ_MELODY];
        uint32_t na = rz_melody_carry(&x, notes, nn, all_c, RZ_MELODY, from);
        /* the general melody: trusted notes that belong to no one text */
        for (uint32_t k = 0; k < na; k++)
            if (rz_melody_trusted(&notes[from[k]]) && notes[from[k]].where[0] == 0) carried[nc++] = all_c[k];
        printf("the melody kept: %u notes, %u sound in this text, %u of those trusted\n", nn, na, nc);

        /* fresh eyes first: the text woven alone from the seed, with no melody;
           then compared with what the melody says of it. What coincides the
           melody already knew; what only the fresh reading found is the
           difference -- a key special to this text (the author's answer to
           riddle 8) */
        int32_t seed = lego_for_word(&x, seed_word);
        static rz_binding fresh[RZ_MELODY], diff[RZ_MELODY];
        uint8_t *fty = calloc(x.n, 1);
        nf = (seed >= 0 && fty) ? rz_lexicon_weave(&x, seed, fresh, RZ_MELODY, fty) : 0;
        free(fty);
        uint32_t agree = 0;
        uint32_t nd = rz_melody_compare(fresh, nf, carried, nc, diff, RZ_MELODY, &agree);
        printf("read with fresh eyes from [%s]: %u bindings; %u coincide with the melody, %u are new here\n",
               seed_word, nf, agree, nd);
        /* Two melodies, kept apart. Pooled into one set of types they were
           worse than either (2.68x against 3.14x and 5.22x on Quiroga): they
           are different music, and their types mixed. */
        uint32_t cc, fc;
        double lc = melody_lift(&x, carried, nc, later, nlater, &cc);
        printf("  the general melody     %.2fx  (%u cases), blind %.2fx\n", lc, cc,
               blind_lift(&x, carried, nc, later, nlater));
        if (nf) {
            double lf = melody_lift(&x, fresh, nf, later, nlater, &fc);
            printf("  its own melody         %.2fx  (%u cases), blind %.2fx\n", lf, fc,
                   blind_lift(&x, fresh, nf, later, nlater));
        }
        printf("  new keys, special to [%s]:", where);
        char kw[80];
        for (uint32_t k = 0; k < nd; k++) {
            say_lego(&x, diff[k].word, kw, sizeof kw);
            printf("  %s%s%s", diff[k].side < 0 ? "" : "___ ", kw, diff[k].side < 0 ? " ___" : "");
        }
        printf("%s\n", nd ? "" : " none");
        for (uint32_t k = 0; k < nd && nw < RZ_MELODY; k++) woven[nw++] = diff[k];
    }

    char w[80];
    if (!nn) printf("woven here: %u bindings, new:", nw);
    uint32_t fresh = 0;
    for (uint32_t i = 0; i < nw && !nn; i++) {
        say_lego(&x, woven[i].word, w, sizeof w);
        printf("  %s%s%s", woven[i].side < 0 ? "" : "___ ", w, woven[i].side < 0 ? " ___" : "");
        fresh++;
    }
    if (!nn) printf("%s\n\n", fresh ? "" : " none");

    /* the bar: the second half, not read (a first melody only; the special
       and general ones were measured above, each against its own blind) */
    uint32_t cs = 0;
    if (!nn) {
    if (nc) {
        double lc = melody_lift(&x, carried, nc, later, nlater, &cs);
        printf("  carried melody alone   %.2fx  (%u cases)\n", lc, cs);
    }
    double lw = melody_lift(&x, woven, nw, later, nlater, &cs);
    printf("  %s %.2fx  (%u cases)\n", nn ? "melody + special keys " : "woven                 ", lw, cs);
    {
        /* blind melodies: every binding swapped for the word of the nearest
           frequency in this text, on the same side */
        uint32_t *ord = malloc(x.n * sizeof *ord), *rank = malloc(x.n * sizeof *rank);
        static rz_binding blind[RZ_MELODY];
        double sum = 0;
        int runs = 0;
        if (ord && rank) {
            for (uint32_t i = 0; i < x.n; i++) ord[i] = i;
            for (uint32_t gap = x.n / 2; gap > 0; gap /= 2)
                for (uint32_t i = gap; i < x.n; i++) {
                    uint32_t v = ord[i], j = i;
                    while (j >= gap && x.lego[ord[j - gap]].count < x.lego[v].count) { ord[j] = ord[j - gap]; j -= gap; }
                    ord[j] = v;
                }
            for (uint32_t i = 0; i < x.n; i++) rank[ord[i]] = i;
            for (int j = 1; j <= 8; j++) {
                for (uint32_t k = 0; k < nw; k++) {
                    int64_t r = (int64_t)rank[woven[k].word] + ((j & 1) ? (j + 1) / 2 : -(j / 2));
                    if (r < 0) r = 0;
                    if (r >= (int64_t)x.n) r = (int64_t)x.n - 1;
                    blind[k] = (rz_binding){ woven[k].side, (int32_t)ord[r], 0.0, 0 };
                }
                uint32_t bc;
                sum += melody_lift(&x, blind, nw, later, nlater, &bc);
                runs++;
            }
        }
        printf("  blind melodies         %.2fx  (average of %d)\n\n", runs ? sum / runs : 0.0, runs);
        free(ord);
        free(rank);
    }
    }

    /* the keys found here are special to this text; the first melody is general */
    uint32_t grown = rz_melody_learn(&x, woven, nw, notes, nn, RZ_MELODY, nn ? where : NULL);
    (void)nf;

    /* Why: every note that sounds is judged here, against the rest of the
       melody and a blind word of its frequency. The verdicts are lessons for
       the next text -- the bar above was read before them. */
    {
        static uint32_t from[RZ_MELODY];
        static rz_binding every[RZ_MELODY];
        static rz_note_verdict v[RZ_MELODY];
        uint32_t ne = rz_melody_carry(&x, notes, grown, every, RZ_MELODY, from);
        rz_melody_judge(&x, every, ne, later, nlater, v);
        uint32_t count[4] = {0, 0, 0, 0};
        printf("\n  why, note by note (%u that sound here):\n", ne);
        for (uint32_t k = 0; k < ne; k++) {
            rz_note *note = &notes[from[k]];
            rz_why why = rz_melody_why(note, &v[k]);
            count[why]++;
            int was_trusted = rz_melody_trusted(note);
            int changed = (note->held > 0 && why != RZ_WHY_HOLDS) || (note->heard > note->held && why == RZ_WHY_HOLDS);
            if (changed) {
                say_lego(&x, every[k].word, w, sizeof w);
                const char *shape = every[k].side < 0 ? "%s ___" : "___ %s";
                char named[96];
                snprintf(named, sizeof named, shape, w);
                if (why == RZ_WHY_HOLDS)
                    printf("    %-20s failed before, holds here (%u of %u texts)\n", named, note->held, note->heard);
                else if (why == RZ_WHY_OTHER_WORK)
                    printf("    %-20s held before, fails here: other work -- next to a type %.0f%% here, %.0f%% where it held\n",
                           named, 100 * v[k].role, 100.0 * note->role);
                else
                    printf("    %-20s held before, fails here: same work (%.0f%%), but here it tells nothing\n",
                           named, 100 * v[k].role);
            }
            rz_melody_remember(note, &v[k]);
            if (was_trusted && !rz_melody_trusted(note)) {
                say_lego(&x, every[k].word, w, sizeof w);
                printf("    %-20s now trusted no more (%u of %u)\n", w, note->held, note->heard);
            }
        }
        printf("  holds %u, does other work %u, tells nothing %u\n", count[RZ_WHY_HOLDS],
               count[RZ_WHY_OTHER_WORK], count[RZ_WHY_SAYS_NOTHING]);
        uint32_t trusted = 0;
        for (uint32_t i = 0; i < grown; i++) trusted += rz_melody_trusted(&notes[i]);
        printf("  the melody rewrites itself: %u notes, %u trusted for the next text\n\n", grown, trusted);
    }
    MKDIR(NEST);
    if (rz_melody_save(notes, grown, MELODY) != RZ_OK) fprintf(stderr, "warning: could not write %s\n", MELODY);
    else printf("  the melody kept: %u notes -> %u (%s)\n", nn, grown, MELODY);

    free(ty);
    free(later);
    rz_lexicon_free(&x);
    rz_free(m);
    free(m);
    free(t);
    return 0;
}

/* --- the instruments of a word (P43) -------------------------------------- */

static int cmd_instruments(int argc, char **argv)
{
    if (argc < 1) { fprintf(stderr, "usage: razar instruments <word> [path]\n"); return 1; }
    const char *word = argv[0];
    const char *path = (argc > 1) ? argv[1] : FEED_DIR "/nuevo-testamento.txt";

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);

    rz_mind *m = malloc(sizeof(*m));
    if (!m || !t) { fprintf(stderr, "out of memory\n"); free(t); free(m); return 1; }
    rz_init(m);
    for (int pass = 0; pass < 2; pass++) rz_ingest(m, t, n);
    rz_lexicon x;
    rz_lexicon_init(&x);
    rz_lexicon_read_words(&x, m, t, n);

    int32_t w = lego_for_word(&x, word);
    rz_instrument in[RZ_INSTRUMENTS];
    uint32_t k = w >= 0 ? rz_word_instruments(&x, w, in, RZ_INSTRUMENTS, NULL, 0) : 0;
    char s1[80];
    say_lego(&x, w, s1, sizeof s1);
    printf("%s sounds %u times in %s; it plays %u instrument%s\n\n", s1, w >= 0 ? x.lego[w].count : 0,
           path, k, k == 1 ? "" : "s");
    for (uint32_t i = 0; i < k; i++) {
        printf("  instrument %u: %u uses\n    before:", i + 1, in[i].uses);
        for (int b = 0; b < 6 && in[i].before[b] >= 0; b++) { say_lego(&x, in[i].before[b], s1, sizeof s1); printf(" %s", s1); }
        printf("\n    after: ");
        for (int b = 0; b < 6 && in[i].after[b] >= 0; b++) { say_lego(&x, in[i].after[b], s1, sizeof s1); printf(" %s", s1); }
        printf("\n    e.g.  ");
        for (int d = -3; d <= 3; d++) {
            int64_t j = (int64_t)in[i].example + d;
            if (j < 0 || (uint64_t)j >= x.nseq || x.seq[j] < 0) continue;
            const rz_lego *g = &x.lego[x.seq[j]];
            printf("%s%.*s%s ", d == 0 ? "[" : "", g->len, g->s, d == 0 ? "]" : "");
        }
        printf("\n\n");
    }
    rz_lexicon_free(&x);
    rz_free(m);
    free(m);
    free(t);
    return 0;
}

/* --- PENZAR: thinking in real time (P45) ---------------------------------- */

static int cmd_penzar(int argc, char **argv)
{
    const char *path = (argc > 0) ? argv[0] : FEED_DIR "/nuevo-testamento.txt";
    size_t bite = (argc > 1) ? (size_t)strtoul(argv[1], NULL, 10) : 40000;
    const char *shown = (argc > 2) ? argv[2] : "es";       /* the question it was taught */
    if (bite < 1000) bite = 1000;

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);

    rz_penzar *p = malloc(sizeof *p);
    if (!p || !t) { fprintf(stderr, "out of memory\n"); free(p); free(t); return 1; }
    rz_penzar_init(p);
    printf("thinking while reading %s, %zu bytes a bite\n\n", path, bite);
    fflush(stdout);

    static uint8_t was_lent[RZ_HUNCHES];
    uint64_t bets = 0, won = 0, base = 0;
    uint32_t *seen_before = NULL, seen_n = 0;
    uint8_t *wondered = NULL;
    size_t at2 = 0;
    char w[80], v[80];
    while (at2 < n) {
        size_t len = n - at2 < bite ? n - at2 : bite;
        while (at2 + len < n && t[at2 + len] != ' ') len++;
        uint32_t before = p->nhunch;
        for (uint32_t i = 0; i < p->nhunch; i++) was_lent[i] = p->hunch[i].lent;
        rz_thought th;
        rz_penzar_bite(p, t + at2, len, &th);
        at2 += len;
        bets += th.bets; won += th.won; base += th.base_won;

        /* What caught its attention: the word that grew the most in this bite,
           seen twice, not wondered about before. Who sits in its chair, and
           what family are they? (P46, riddle 3) -- asked with the question it
           was taught and that question's silhouette */
        int32_t family_a = -1, family_b = -1;
        rz_family fam;
        memset(&fam, 0, sizeof fam);
        {
            uint32_t *grown = realloc(seen_before, p->lex.n * sizeof *grown);
            uint8_t *wd = grown ? realloc(wondered, p->lex.n) : NULL;
            if (grown) seen_before = grown;
            if (wd) wondered = wd;
            if (grown && wd) {
                for (uint32_t i = seen_n; i < p->lex.n; i++) { seen_before[i] = 0; wondered[i] = 0; }
                uint32_t best = 0;
                int32_t pick = -1;
                for (uint32_t i = 0; i < p->lex.n; i++) {
                    uint32_t gain = p->lex.lego[i].count - seen_before[i];
                    if (p->lex.lego[i].count < 2 || wondered[i] || gain <= best) continue;
                    if (p->have_keys && p->keys.marks[i] >= 0) continue;   /* the glue of the key */
                    if (p->lex.lego[i].len < 3) continue;
                    best = gain;
                    pick = (int32_t)i;
                }
                for (uint32_t i = 0; i < p->lex.n; i++) seen_before[i] = p->lex.lego[i].count;
                seen_n = p->lex.n;
                if (pick >= 0) {
                    wondered[pick] = 1;
                    int32_t sil[8];
                    uint32_t ks = rz_lexicon_silhouette(&p->lex, pick, sil, 8);
                    int32_t rel = lego_for_word(&p->lex, shown);
                    if (ks && rel >= 0) {
                        int32_t rels[16];
                        uint32_t nr = 1;
                        rels[0] = rel;
                        nr += rz_lexicon_silhouette(&p->lex, rel, rels + 1, 15);
                        rz_lexicon_family(&p->lex, pick, sil[0], rels, nr, &fam);
                        family_a = pick;
                        family_b = sil[0];
                    }
                }
            }
        }

        printf("bite %u", p->bites);
        if (th.bets)
            printf(": bet %u times, won %u (chance would win %u)", th.bets, th.won, th.base_won);
        printf("\n");
        for (uint32_t i = 0; i < before; i++)
            if (!was_lent[i] && p->hunch[i].lent) {
                say_lego(&p->lex, p->hunch[i].word, w, sizeof w);
                printf("  holds: after %s comes side %d (%u of %u)\n", w, p->hunch[i].side,
                       p->hunch[i].right, p->hunch[i].tried);
            }
        for (uint32_t i = before; i < p->nhunch; i++) {
            say_lego(&p->lex, p->hunch[i].word, w, sizeof w);
            if (p->hunch[i].from >= 0) {
                say_lego(&p->lex, p->hunch[p->hunch[i].from].word, v, sizeof v);
                printf("  sideways: %s sits where %s sits -- maybe it marks side %d too\n", w, v, p->hunch[i].side);
            } else {
                printf("  a hunch: after %s comes side %d\n", w, p->hunch[i].side);
            }
        }
        if (family_a >= 0) {
            say_lego(&p->lex, family_a, w, sizeof w);
            say_lego(&p->lex, family_b, v, sizeof v);
            printf("  wonders: %s sits where %s sits -- ", w, v);
            if (fam.kin == RZ_KIN_KEY) printf("the same note in another key\n");
            else if (fam.kin == RZ_KIN_CHAIR) printf("they share only the chair\n");
            else {
                printf("one family:");
                for (uint32_t i = 0; i < fam.nfamily; i++) { say_lego(&p->lex, fam.family[i], v, sizeof v); printf(" %s", v); }
                printf("\n");
            }
            if (fam.nonly_a || fam.nonly_b) {
                printf("           apart:");
                for (uint32_t i = 0; i < fam.nonly_a && i < 4; i++) { say_lego(&p->lex, fam.only_a[i], v, sizeof v); printf(" %s", v); }
                printf(" |");
                for (uint32_t i = 0; i < fam.nonly_b && i < 4; i++) { say_lego(&p->lex, fam.only_b[i], v, sizeof v); printf(" %s", v); }
                printf("\n");
            }
            if (fam.nnames) {
                printf("           names said of both (an identity, not a kind):");
                for (uint32_t i = 0; i < fam.nnames && i < 4; i++) { say_lego(&p->lex, fam.names[i], v, sizeof v); printf(" %s", v); }
                printf("\n");
            }
        }
        fflush(stdout);
    }

    uint32_t held = 0, side_held = 0, side_tried = 0;
    for (uint32_t i = 0; i < p->nhunch; i++) {
        int h = rz_hunch_holds(&p->hunch[i]);
        held += h;
        if (p->hunch[i].from >= 0 && p->hunch[i].tried) { side_tried++; side_held += h; }
    }
    printf("\n%u hunches, %u hold; thought sideways and tried: %u, of which %u hold\n",
           p->nhunch, held, side_tried, side_held);
    if (p->have_keys)
        printf("the key it heard: letter %d from the end, '%c' against '%c', %u twins\n",
               p->keys.property.from_end, p->keys.property.a, p->keys.property.b, p->keys.property.twins);
    printf("on text it had not read yet: bet %llu times, won %llu, chance would win %llu\n",
           (unsigned long long)bets, (unsigned long long)won, (unsigned long long)base);
    free(seen_before);
    free(wondered);
    rz_penzar_free(p);
    free(p);
    free(t);
    return 0;
}

/* --- the map of associations (P47) ---------------------------------------- */

static void show_map(rz_lexicon *x, const rz_assoc *a, int32_t w, const int32_t *rels, uint32_t nr)
{
    char s1[80], s2[80], s3[80];
    int32_t up[64];
    uint32_t nu = rz_assoc_reach(a, w, rels, nr, up, 64);
    say_lego(x, w, s1, sizeof s1);
    printf("%s is, as far as the links go:", s1);
    for (uint32_t i = 0; i < nu && i < 16; i++) { say_lego(x, up[i], s2, sizeof s2); printf(" %s", s2); }
    printf("%s\n", nu ? "" : " nothing it was told");
    static rz_trait tr[512];
    uint32_t nt = rz_assoc_traits(a, w, rels, nr, tr, 512);
    printf("  said of it, or of what it is:\n");
    uint32_t shown = 0;
    for (uint32_t i = 0; i < nt && shown < 24; i++) {
        say_lego(x, tr[i].r, s2, sizeof s2);
        if (tr[i].b >= 0) say_lego(x, tr[i].b, s3, sizeof s3); else s3[0] = 0;
        if (tr[i].via == w) printf("    %s %s\n", s2, s3);
        else { say_lego(x, tr[i].via, s1, sizeof s1); printf("    %s %s          (because it is %s)\n", s2, s3, s1); }
        shown++;
    }
}

static int cmd_map(int argc, char **argv)
{
    if (argc < 1) { fprintf(stderr, "usage: razar map <word> [other] [relation] [path]\n"); return 1; }
    const char *word = argv[0];
    const char *other = (argc > 1) ? argv[1] : NULL;
    const char *rel = (argc > 2) ? argv[2] : "es";
    const char *path = (argc > 3) ? argv[3] : FEED_DIR "/nuevo-testamento.txt";

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);

    rz_mind *m = malloc(sizeof(*m));
    if (!m || !t) { fprintf(stderr, "out of memory\n"); free(t); free(m); return 1; }
    rz_init(m);
    for (int pass = 0; pass < 2; pass++) rz_ingest(m, t, n);
    rz_lexicon x;
    rz_lexicon_init(&x);
    rz_lexicon_read_words(&x, m, t, n);
    rz_keys k;
    rz_assoc a;
    memset(&a, 0, sizeof a);
    if (rz_lexicon_keys(&x, &k) != RZ_OK) { fprintf(stderr, "no key heard: no markers to tell things by\n"); goto done; }
    rz_assoc_build(&x, &k, &a);

    /* the question taught, and its silhouette: es, será, era, sea... */
    int32_t rels[16];
    uint32_t nr = 0;
    int32_t r0 = lego_for_word(&x, rel);
    if (r0 >= 0) { rels[nr++] = r0; nr += rz_lexicon_silhouette(&x, r0, rels + 1, 15); }
    printf("%u links read from what %s says; asked with:", a.n, path);
    for (uint32_t i = 0; i < nr; i++) { char sr[80]; say_lego(&x, rels[i], sr, sizeof sr); printf(" %s", sr); }
    printf("\n\n");

    int32_t w1 = lego_for_word(&x, word);
    if (w1 < 0) { printf("it never met %s\n", word); }
    else show_map(&x, &a, w1, rels, nr);
    if (other && w1 >= 0) {
        int32_t w2 = lego_for_word(&x, other);
        if (w2 < 0) printf("\nit never met %s\n", other);
        else {
            printf("\n");
            show_map(&x, &a, w2, rels, nr);
            int32_t u1[64], u2[64];
            uint32_t n1 = rz_assoc_reach(&a, w1, rels, nr, u1, 64), n2 = rz_assoc_reach(&a, w2, rels, nr, u2, 64);
            printf("\n  both reach (one family):");
            uint32_t shared = 0;
            for (uint32_t i = 0; i < n1; i++)
                for (uint32_t j = 0; j < n2; j++)
                    if (u1[i] == u2[j]) { char sf[80]; say_lego(&x, u1[i], sf, sizeof sf); printf(" %s", sf); shared++; }
            printf("%s\n", shared ? "" : " nothing -- no family the text gives them");
        }
    }
    rz_assoc_free(&a);
    rz_keys_free(&k);
done:
    rz_lexicon_free(&x);
    rz_free(m);
    free(m);
    free(t);
    return 0;
}

/* --- the melody of the path (P48) ----------------------------------------- */

typedef struct { rz_mind *m; rz_lexicon x; rz_keys k; rz_assoc a; int ok; } read_map;

static void map_of(const unsigned char *t, size_t n, read_map *r)
{
    memset(r, 0, sizeof *r);
    r->m = malloc(sizeof *r->m);
    if (!r->m) return;
    rz_init(r->m);
    for (int pass = 0; pass < 2; pass++) rz_ingest(r->m, t, n);
    rz_lexicon_init(&r->x);
    rz_lexicon_read_words(&r->x, r->m, t, n);
    if (rz_lexicon_keys(&r->x, &r->k) != RZ_OK) return;
    rz_assoc_build(&r->x, &r->k, &r->a);
    r->ok = 1;
}

static void map_free(read_map *r)
{
    if (r->ok) { rz_assoc_free(&r->a); rz_keys_free(&r->k); }
    rz_lexicon_free(&r->x);
    if (r->m) { rz_free(r->m); free(r->m); }
}

/* the same word in another reading, by its spelling */
static int32_t same_word(const rz_lexicon *from, int32_t w, const rz_lexicon *to)
{
    if (w < 0) return -1;
    return rz_lexicon_find(to, from->lego[w].s, from->lego[w].len);
}

static int map_says(const rz_assoc *a, int32_t x1, int32_t r, int32_t b)
{
    for (uint32_t i = 0; i < a->n; i++)
        if (a->link[i].a == x1 && a->link[i].r == r && a->link[i].b == b && a->link[i].round == 0) return 1;
    return 0;
}

static int cmd_dominoes(int argc, char **argv)
{
    const char *path = (argc > 0) ? argv[0] : FEED_DIR "/nuevo-testamento.txt";
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);
    size_t half = n / 2;
    while (half < n && t[half] != ' ') half++;

    /* the first half: the map, the tunes heard in it, the dominoes */
    read_map one, two;
    map_of(t, half, &one);
    map_of(t + half, n - half, &two);
    if (!one.ok || !two.ok) { fprintf(stderr, "no key heard in one of the halves\n"); map_free(&one); map_free(&two); free(t); return 1; }
    uint32_t said = one.a.n;
    static rz_tune tu[256], all_tu[4096];
    uint32_t nall = rz_assoc_tunes(&one.a, all_tu, 4096), nt = 0;
    /* the tunes of the question taught (imitation): "es" and its silhouette.
       Every relation let in, the NT's glue -- "Los", "Y", the comma -- made
       the tunes, and the dominoes multiplied it */
    const char *rel = (argc > 1) ? argv[1] : "es";
    int32_t rels[16];
    uint32_t nr = 0;
    int32_t r0 = lego_for_word(&one.x, rel);
    if (r0 >= 0) { rels[nr++] = r0; nr += rz_lexicon_silhouette(&one.x, r0, rels + 1, 15); }
    for (uint32_t i = 0; i < nall && nt < 256; i++) {
        int in1 = 0, in2 = 0, in3 = 0;
        for (uint32_t q = 0; q < nr; q++) { in1 |= rels[q] == all_tu[i].r1; in2 |= rels[q] == all_tu[i].r2; in3 |= rels[q] == all_tu[i].r3; }
        if (in1 && in2 && in3) tu[nt++] = all_tu[i];
    }
    char a1[80], a2[80], a3[80];
    printf("first half: %u links said; %u tunes heard twice or more, %u of them in the question taught\n",
           said, nall, nt);
    for (uint32_t i = 0; i < nt && i < 10; i++) {
        say_lego(&one.x, tu[i].r1, a1, sizeof a1);
        say_lego(&one.x, tu[i].r2, a2, sizeof a2);
        say_lego(&one.x, tu[i].r3, a3, sizeof a3);
        printf("  %s . %s  ->  %s    (%u walks, shortcut said %u times)\n", a1, a2, a3, tu[i].paths, tu[i].heard);
    }
    uint32_t rounds = 0;
    uint32_t fell = rz_assoc_dominoes(&one.a, tu, nt, 50000, &rounds);
    printf("\n%u meanings fell in %u rounds -- nobody wrote them in the first half\n", fell, rounds);

    /* the bar: the second half, read alone. Of what fell, how much does it
       say? Against the same shortcut with its end swapped for another end that
       fell with the same relation (blind) */
    uint32_t tested = 0, found = 0, blind_found = 0, shown = 0;
    uint64_t rng = 0x9E3779B97F4A7C15ull;
    uint32_t *fallen = malloc((one.a.n + 1) * sizeof *fallen);
    uint32_t nfal = 0;
    for (uint32_t i = 0; fallen && i < one.a.n; i++) if (one.a.link[i].round) fallen[nfal++] = i;
    for (uint32_t q = 0; q < nfal; q++) {
        const rz_link *l = &one.a.link[fallen[q]];
        int32_t A = same_word(&one.x, l->a, &two.x), R = same_word(&one.x, l->r, &two.x), B = same_word(&one.x, l->b, &two.x);
        if (A < 0 || R < 0 || B < 0) continue;
        tested++;
        int hit = map_says(&two.a, A, R, B);
        found += (uint32_t)hit;
        /* blind: another fallen link's end, same relation */
        for (int tries = 0; tries < 16; tries++) {
            rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
            const rz_link *o = &one.a.link[fallen[rng % nfal]];
            if (o->r != l->r || o->b == l->b) continue;
            int32_t B2 = same_word(&one.x, o->b, &two.x);
            if (B2 >= 0 && map_says(&two.a, A, R, B2)) blind_found++;
            break;
        }
        if (hit && shown < 12) {
            say_lego(&one.x, l->a, a1, sizeof a1);
            say_lego(&one.x, l->r, a2, sizeof a2);
            say_lego(&one.x, l->b, a3, sizeof a3);
            printf("  fell in round %u, and the second half says it: %s %s %s\n", l->round, a1, a2, a3);
            shown++;
        }
    }
    printf("\nof %u fallen meanings whose words the second half knows: %u said there; blind ends: %u\n",
           tested, found, blind_found);
    free(fallen);
    map_free(&one);
    map_free(&two);
    free(t);
    return 0;
}

/* --- asking every word (P50) ---------------------------------------------- */

static void say_shape(uint8_t sh, char *out, size_t cap)
{
    snprintf(out, cap, "%s%s%s%s",
             (sh & 1) ? "spoken-of " : "", (sh & 2) ? "said-of-others " : "",
             (sh & 4) ? "relates " : "", (sh & 24) ? "keeps-company" : "");
}

static int cmd_asks(int argc, char **argv)
{
    const char *path = (argc > 0) ? argv[0] : FEED_DIR "/nuevo-testamento.txt";
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *t = malloc(size > 0 ? (size_t)size : 1);
    size_t n = t ? fread(t, 1, (size_t)size, f) : 0;
    fclose(f);
    n = continuous(t, n);
    size_t half = n / 2;
    while (half < n && t[half] != ' ') half++;

    read_map one, two;
    map_of(t, half, &one);
    map_of(t + half, n - half, &two);
    if (!one.ok || !two.ok) { fprintf(stderr, "no key heard in one of the halves\n"); map_free(&one); map_free(&two); free(t); return 1; }
    rz_answers *ans = calloc(one.x.n, sizeof *ans);
    if (!ans) { map_free(&one); map_free(&two); free(t); return 1; }
    uint32_t asked = rz_ask_every_word(&one.x, &one.a, ans);
    printf("first half: every word asked -- %u words, the same questions to each\n\n", asked);

    /* the kinds: words grouped by the shape of their answers, most used first */
    char sh[96], w1[80], w2[80];
    for (int s0 = 0; s0 < 32; s0++) {
        uint32_t members = 0;
        for (uint32_t w = 0; w < one.x.n; w++) if (one.x.lego[w].count >= 2 && ans[w].shape == s0) members++;
        if (!members) continue;
        say_shape((uint8_t)s0, sh, sizeof sh);
        printf("  [%2d] %-48s %6u words:", s0, sh, members);
        int32_t top[8];
        uint32_t nt = 0;
        for (uint32_t w = 0; w < one.x.n; w++) {
            if (one.x.lego[w].count < 2 || ans[w].shape != s0) continue;
            uint32_t j = nt < 8 ? nt++ : 8;
            if (j == 8) { if (one.x.lego[w].count <= one.x.lego[top[7]].count) continue; j = 7; }
            while (j > 0 && one.x.lego[top[j - 1]].count < one.x.lego[w].count) { top[j] = top[j - 1]; j--; }
            top[j] = (int32_t)w;
        }
        for (uint32_t i = 0; i < nt; i++) { say_lego(&one.x, top[i], w1, sizeof w1); printf(" %s", w1); }
        printf("\n");
    }

    /* used together, and the pieces that are one */
    static rz_together tg[64];
    uint32_t ng = rz_used_together(&one.x, ans, tg, 64);
    printf("\nused together (pairs of shapes heard across several pairs of words): %u\n", ng);
    for (uint32_t i = 0; i < ng && i < 10; i++)
        printf("  [%2u] . [%2u]   heard %6u times, by %5u pairs of words\n", tg[i].s1, tg[i].s2, tg[i].heard, tg[i].pairs);

    /* the bar: pairs the second half says that the first never did. Of those
       whose words the first half asked, how many fall in a pair of shapes it
       learned to use together? Blind: the same first words, each with the
       second word of another new pair -- the same words, shuffled */
    uint32_t fresh = 0, covered = 0, blind = 0;
    int32_t *b2 = malloc((two.x.npair + 1) * sizeof *b2), *a1 = malloc((two.x.npair + 1) * sizeof *a1);
    for (uint32_t i = 0; b2 && a1 && i < two.x.npair; i++) {
        const rz_lego_pair *p = &two.x.pair[i];
        int32_t A = rz_lexicon_find(&one.x, two.x.lego[p->a].s, two.x.lego[p->a].len);
        int32_t B = rz_lexicon_find(&one.x, two.x.lego[p->b].s, two.x.lego[p->b].len);
        if (A < 0 || B < 0 || one.x.lego[A].count < 2 || one.x.lego[B].count < 2) continue;
        if (rz_lexicon_pair_count(&one.x, A, B)) continue;
        a1[fresh] = A; b2[fresh] = B; fresh++;
    }
    int one_piece_shown = 0;
    for (uint32_t i = 0; i < fresh; i++) {
        int32_t A = a1[i], B = b2[i], Bb = b2[(i + fresh / 2 + 1) % fresh];
        for (uint32_t g = 0; g < ng; g++) {
            if (tg[g].s1 == ans[A].shape && tg[g].s2 == ans[B].shape) covered++;
            if (tg[g].s1 == ans[A].shape && tg[g].s2 == ans[Bb].shape) blind++;
        }
        if (one_piece_shown < 8 && rz_one_piece(&ans[A], &ans[B]) && (ans[B].shape & 1)) {
            say_lego(&one.x, A, w1, sizeof w1);
            say_lego(&one.x, B, w2, sizeof w2);
            printf("%s  never together in the first half, one piece in the second: %s %s\n",
                   one_piece_shown ? "" : "\n", w1, w2);
            one_piece_shown++;
        }
    }
    printf("\nnew pairs in the second half: %u; in a pair of shapes it learned: %u (%.1f%%); blind: %u (%.1f%%)\n",
           fresh, covered, fresh ? 100.0 * covered / fresh : 0.0, blind, fresh ? 100.0 * blind / fresh : 0.0);
    free(a1); free(b2); free(ans);
    map_free(&one);
    map_free(&two);
    free(t);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("razar — spike 1\n\n");
        printf("  razar feed [path...]                  walk material (default: %s/)\n", FEED_DIR);
        printf("  razar autopsy                         examine and report the diff\n");
        printf("  razar recall <cue> [steps] [effort]   ask it what follows\n");
        printf("  razar recall-atoms <cue> ...          the same, addressed by atoms\n");
        printf("  razar predict <path>                  the fixed bar: guess before reading\n");
        printf("  razar interest <path>                 what is opaque, frontier, mastered\n");
        printf("  razar write <cue> [n] [effort] [temp] [seed]   give it the pen\n");
        printf("  razar sleep [seed]                    let it dream on what it lived\n");
        printf("  razar senses [rounds] [path]          watch it grow its own ways of looking\n");
        printf("  razar dash                            its whole state, as JSON\n");
        printf("  razar think [path] [bite]             watch it think out loud\n");
        printf("  razar legos [path] [kinds]            its legos, their kinds, and a game\n");
        printf("  razar asks [path]                     ask every word; the kinds its answers make\n");
        return 0;
    }
    if (!strcmp(argv[1], "feed"))         return cmd_feed(argc - 2, argv + 2);
    if (!strcmp(argv[1], "autopsy"))      return cmd_autopsy();
    if (!strcmp(argv[1], "recall"))       return cmd_recall(argc - 2, argv + 2, 0);
    if (!strcmp(argv[1], "recall-atoms")) return cmd_recall(argc - 2, argv + 2, 1);
    if (!strcmp(argv[1], "predict"))      return cmd_predict(argc - 2, argv + 2);
    if (!strcmp(argv[1], "interest"))     return cmd_interest(argc - 2, argv + 2);
    if (!strcmp(argv[1], "write"))        return cmd_write(argc - 2, argv + 2);
    if (!strcmp(argv[1], "sleep"))        return cmd_sleep(argc - 2, argv + 2);
    if (!strcmp(argv[1], "senses"))       return cmd_senses(argc - 2, argv + 2);
    if (!strcmp(argv[1], "dash"))         return cmd_dash(argc - 2, argv + 2);
    if (!strcmp(argv[1], "think"))        return cmd_think(argc - 2, argv + 2);
    if (!strcmp(argv[1], "legos"))        return cmd_legos(argc - 2, argv + 2);
    if (!strcmp(argv[1], "ask"))          return cmd_ask(argc - 2, argv + 2);
    if (!strcmp(argv[1], "play"))         return cmd_play(argc - 2, argv + 2);
    if (!strcmp(argv[1], "concepts"))     return cmd_concepts(argc - 2, argv + 2);
    if (!strcmp(argv[1], "roles"))        return cmd_roles(argc - 2, argv + 2);
    if (!strcmp(argv[1], "weave"))        return cmd_weave(argc - 2, argv + 2);
    if (!strcmp(argv[1], "melody"))       return cmd_melody(argc - 2, argv + 2);
    if (!strcmp(argv[1], "instruments"))  return cmd_instruments(argc - 2, argv + 2);
    if (!strcmp(argv[1], "penzar"))       return cmd_penzar(argc - 2, argv + 2);
    if (!strcmp(argv[1], "map"))          return cmd_map(argc - 2, argv + 2);
    if (!strcmp(argv[1], "dominoes"))     return cmd_dominoes(argc - 2, argv + 2);
    if (!strcmp(argv[1], "asks"))         return cmd_asks(argc - 2, argv + 2);

    fprintf(stderr, "unknown command: %s\n", argv[1]);
    return 1;
}
