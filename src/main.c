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

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("razar — spike 1\n\n");
        printf("  razar feed [path...]                  walk material (default: %s/)\n", FEED_DIR);
        printf("  razar autopsy                         examine and report the diff\n");
        printf("  razar recall <cue> [steps] [effort]   ask it what follows\n");
        printf("  razar recall-atoms <cue> ...          the same, addressed by atoms\n");
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

    fprintf(stderr, "unknown command: %s\n", argv[1]);
    return 1;
}
