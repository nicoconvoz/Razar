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
        printf("  razar think [path] [bite]           watch it think out loud\n");
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

    fprintf(stderr, "unknown command: %s\n", argv[1]);
    return 1;
}
