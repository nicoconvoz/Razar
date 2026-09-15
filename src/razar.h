/* Razar — spike 1: eat and count.
 *
 * The smallest thing that shows behaviour. Atoms are bytes: a fixed, bounded
 * base of 256 types that does not grow with experience. Reading material is
 * traversal, not import — the material is never stored, only the trace of
 * having walked it.
 *
 * No affect, no dreams, no pruning, no projections, no situations. Those need
 * this baseline and its instrument first.
 */
#ifndef RAZAR_H
#define RAZAR_H

#include <stddef.h>
#include <stdint.h>

/* Spike-1 atom choice: the byte. 256 types, fixed forever.
   Codepoints are a later decision — see docs/design. */
#define RZ_ATOMS 256
#define RZ_PATHS (RZ_ATOMS * RZ_ATOMS)

/* --- fragments (P2) ------------------------------------------------------
 *
 * The atom is the medium of DECOMPOSITION, not the unit of meaning. Concepts
 * disintegrate into atoms so they can be understood — that is what "fully
 * mappable" buys. But what ADDRESSES the memory is the fragment: a conjunction
 * of atoms. A lone atom is the degenerate case, and it is rarely enough.
 *
 * Fragments are not enumerated, they are DISCOVERED: a pair of fragments that
 * keeps travelling together becomes a fragment of its own. The path traffic is
 * what gives birth to the unit — the paths are what make the memory work.
 *
 * Every fragment keeps its decomposition (left, right) all the way down to
 * atoms, so no layer is ever opaque. That is the invariant.
 */
#define RZ_FRAG_SPAN     32   /* longest conjunction a fragment may hold */
#define RZ_FRAG_BIRTH     8   /* times a pair must travel together to become one */

/* The abstract layer's alphabet: 4 branch classes x 4 span x 4 concentration.
   See "vertical composition" below. */
#define RZ_SHAPES        64

/* --- modules -------------------------------------------------------------
 *
 * A fragment is born from (left, right) and therefore begins with whatever
 * atom `left` begins with. Every fragment belongs to the lineage of its first
 * atom, and that lineage IS the module — 256 of them, related by descent
 * rather than by a hash, and inherited at birth without anyone assigning it.
 *
 * Modules are the answer to the squeeze. Paths no longer live in a fixed array
 * per fragment; they come out of a POOL shared across the module, so a hub
 * fragment may hold hundreds of paths while the total stays bounded. Only a few
 * modules are awake at once — the rest sleep on disk until something reaches
 * into them.
 */
#define RZ_MODULES      256   /* one per lineage root */
#define RZ_MOD_FRAGS   4096   /* fragments a module may hold */
#define RZ_MOD_EDGES  65536   /* the module's shared path pool: no per-fragment cap */
#define RZ_RESIDENT       8   /* modules awake at once */

/* A fragment is addressed globally as module * RZ_MOD_FRAGS + local slot.
   Slot 0 of module A is always the bare atom A. */
#define RZ_FRAGS       (RZ_MODULES * RZ_MOD_FRAGS)

typedef struct {
    int32_t  left, right;               /* -1, -1 when this is a bare atom */
    uint16_t span;                      /* how many atoms it decomposes into */
    uint32_t seen;                      /* times this fragment was walked */
    /* What a dream is allowed to leave behind. Not a record of anything that
       happened — a mark saying "come back here". It protects from fading and
       pulls the organism's reading toward itself, and it fades on its own. */
    uint16_t attention;
    unsigned char atoms[RZ_FRAG_SPAN];  /* its decomposition, flattened */
} rz_frag;

typedef struct {
    int32_t  to;
    uint32_t weight;
    int32_t  next;   /* next path out of the same fragment, -1 at the end */
} rz_edge;

typedef struct {
    rz_frag  frag[RZ_MOD_FRAGS];
    int32_t  head[RZ_MOD_FRAGS];   /* first path out of each fragment, -1 if none */
    rz_edge  pool[RZ_MOD_EDGES];
    int32_t  nfrags;
    int32_t  npool;
    int32_t  dirty;
    uint64_t decayed_at;           /* the pass this module last caught up to */
} rz_module;

/* --- senses (P30): drawn, not written -------------------------------------
 *
 * The reference organism never invents a sense. It fills a template from a
 * small vocabulary and lets the draw decide:
 *
 *     count pieces of type T of side S within radius R of reference F, when C
 *
 * Out of that comes "enemies near my king", "my pawns near the enemy king",
 * "knights in the centre" — with nobody having written any of those ideas.
 *
 * Everything found by hand in this project so far was a detector an author
 * wrote, one per idea. That is not a faculty. This is the template Razar draws
 * from instead:
 *
 *     count fragments WHERE <what> reachable in <direction> within <radius>
 *
 * `what` is asked of the fragment it lands on, and every question in the
 * vocabulary is one the organism can already answer about itself. Nothing here
 * knows what a word is, or a verb, or a boundary.
 */
typedef enum {
    RZ_WHAT_SHAPE,      /* carries this shape class */
    RZ_WHAT_WIDER,      /* spans more than this many atoms */
    RZ_WHAT_BUSIER,     /* has been walked more than this often */
    RZ_WHAT_FORKED,     /* has more walked ways out than this */
    RZ_WHAT_GUESSED,    /* is reached by a path it was given, never took */
    RZ_WHAT_MARKED,     /* carries attention a dream left behind */
    RZ_WHAT_COUNT
} rz_what;

typedef enum {
    RZ_DIR_FORWARD,     /* what follows from here */
    RZ_DIR_DOWN,        /* what this is made of */
    RZ_DIR_COUNT
} rz_dir;

#define RZ_SENSE_RADIUS 3       /* how far a sense may look. Small on purpose */

typedef struct {
    uint8_t  what;      /* which question, from RZ_WHAT_* */
    uint8_t  value;     /* the answer it is looking for */
    uint8_t  dir;       /* which way it looks */
    uint8_t  radius;    /* 1..RZ_SENSE_RADIUS */
    /* A sense whose input is another sense. This one slot is what turns a list
       of detectors into a faculty: the output of one becomes the material of
       the next, and senses can be built on senses. 0 = reads the world raw. */
    uint8_t  src;
} rz_sense;

#define RZ_SENSES      64      /* how many it may hold at once */
#define RZ_SENSE_HOT    8      /* how many are awake for any one reading */
#define RZ_SENSE_BUCKETS 6     /* how coarsely a reading is remembered */
#define RZ_RULES       256     /* how many lessons it may keep */

/* A rule: the sense, AND the way to use it.
 *
 * From the reference organism, where a rule is a concept with deep=1 -- feat
 * says WHEN, dfrom->dto says WHAT, dscore says HOW MUCH, level says how real the
 * experience that formed it was. Noticing that the queen matters is an
 * observation; the rule is that plus the way to act on it. */
typedef struct {
    uint32_t when;      /* the situation: the shape here, and what the senses read */
    int32_t  from, to;  /* the step it recommends */
    double   score;     /* how much it is worth */
    double   level;     /* 1 lived, 0 dreamt -- and a dream never weighs as much */
    uint32_t uses;
    uint32_t hits, misses;   /* what it cost the organism to listen to it */
} rz_rule;

#define RZ_INNER 64    /* how much of the conversation it keeps */

/* One exchange with itself: what it expected, what came, who said so. */
typedef struct {
    int32_t  from, expected, actual;
    int32_t  rule;      /* which rule spoke, or -1 for its plain experience */
    uint8_t  right;
} rz_inner;


typedef struct {
    /* the atomic base — fixed, bounded, never sleeps */
    uint32_t path[RZ_PATHS];        /* traversal count per ordered atom pair */
    uint32_t atom_seen[RZ_ATOMS];   /* occurrences per atom type */
    uint64_t traversals;            /* total atom transitions walked */
    uint64_t atoms_ingested;        /* total atom occurrences seen */

    /* the plastic layer — paged by module */
    rz_module *mod[RZ_MODULES];     /* NULL while asleep */
    int32_t    mod_nfrags[RZ_MODULES];  /* known even when the module sleeps */
    int32_t    resident[RZ_MODULES];
    uint64_t   used[RZ_MODULES];    /* for the least-recently-used eviction */
    int32_t    nresident;
    uint64_t   clock;

    uint64_t frag_steps;            /* transitions walked between fragments */
    uint64_t births;                /* conjunctions that earned their existence */
    uint64_t pool_full;             /* paths refused: a module's pool was spent */
    uint64_t wakes, sleeps;
    uint64_t held_open;     /* evictions refused: nowhere to write it down */
    uint64_t decay_passes;  /* how much forgetting has been asked for */

    /* the abstract layer: the same traffic, one level up, over roles instead
       of contents */
    uint32_t shape_path[RZ_SHAPES * RZ_SHAPES];
    uint64_t shape_steps;

    /* What actually matters to it. While eating, before recording each step, it
       asks whether the SHAPE of where it stands anticipates what comes next —
       and keeps the score. A shape that keeps being right is knowledge that
       transfers: the pen falls the way the apple did. A shape that is never
       right is decoration.
       This is the value signal the system was missing. Not novelty, not
       progress: successful transfer. */
    uint32_t shape_tried[RZ_SHAPES];
    uint32_t shape_won[RZ_SHAPES];

    /* How it has been since it last slept — the only thing the triage needs */
    uint64_t awake_steps;
    uint64_t awake_births;
    double   awake_strain;       /* how far off it was, summed over the night */
    uint64_t awake_anticipated;  /* steps its own ideas called correctly */
    /* the senses it has grown, and what each is worth */
    rz_sense senses[RZ_SENSES];
    double   sense_worth[RZ_SENSES];
    int32_t  nsenses;
    uint64_t senses_born, senses_pruned, senses_kept_neutral;

    /* What each sense has actually SEEN, accumulated one step at a time.
       Re-reading the whole material once per sense per round cost senses x
       material, so the more it learned the slower it ran -- an organism
       paralysed by growing. Now every step it takes is an observation, and
       reach comes from time rather than from sweeping a radius. */
    uint32_t sense_obs[RZ_SENSES][RZ_SENSE_BUCKETS][RZ_SHAPES];
    uint32_t sense_row[RZ_SENSES][RZ_SENSE_BUCKETS];
    uint64_t sense_n[RZ_SENSES];
    uint32_t sense_col[RZ_SHAPES];      /* what followed, regardless of sense */
    uint64_t sense_col_n;
    uint64_t sense_reads;               /* how much the faculty is costing */
    uint32_t sense_turn;                /* which cold one gets its turn next */

    /* what it has learned to DO, not merely to notice */
    rz_rule  rules[RZ_RULES];
    int32_t  nrules;
    uint64_t rules_born, rules_used;

    /* the conversation it has with itself, and whether it is listening */
    rz_inner inner[RZ_INNER];
    uint64_t inner_n, inner_said, inner_right;
    int      inner_on;

    uint64_t sleeps_had;
    uint64_t dreams[4];          /* how many of each kind it has had */

    char nest[512];                 /* where sleeping modules live */
} rz_mind;

/* Where modules page to. Must be set before feeding if they are to survive a
   restart; with an empty nest they live and die in RAM. */
void rz_set_nest(rz_mind *m, const char *dir);

/* Global id of a bare atom. */
int32_t rz_atom_id(unsigned char a);

/* Read a fragment, waking its module if it was asleep. NULL if it does not
   exist. Paging is a side effect of reading — the content is unchanged. */
const rz_frag *rz_frag_get(rz_mind *m, int32_t id);

int32_t rz_resident_count(const rz_mind *m);

void rz_init(rz_mind *m);

/* Release every awake module. The mind struct itself is the caller's. */
void rz_free(rz_mind *m);

/* Walk a buffer. Every adjacent pair is one traversal.
   Separate calls do not link: a boundary is not a path.
   Non-idempotent by design. */
void rz_ingest(rz_mind *m, const unsigned char *buf, size_t n);

uint32_t rz_intensity(const rz_mind *m, unsigned char from, unsigned char to);
uint32_t rz_distinct_paths(const rz_mind *m);
uint32_t rz_distinct_atoms(const rz_mind *m);

/* Size of the plastic layer. Constant by construction — this is the
   type-not-token commitment made measurable. */
size_t rz_plastic_bytes(const rz_mind *m);

/* Shannon entropy (nats) of the traversal distribution over paths. */
double rz_entropy(const rz_mind *m);

/* exp(entropy): how many paths are effectively in use.
   Uniform over N paths -> N. Everything on one path -> 1. Nothing -> 0.
   Falling across sessions while path count rises is the collapse signature. */
double rz_effective_support(const rz_mind *m);

/* Share of all traversals landing on the strongest `pct` percent of paths. */
double rz_top_share(const rz_mind *m, double pct);

/* --- results ------------------------------------------------------------- */

enum {
    RZ_OK          = 0,
    RZ_ERR_MISSING = 1,   /* no such file */
    RZ_ERR_VERSION = 2,   /* present but not this layout — refused, not half-read */
    RZ_ERR_IO      = 3
};

/* --- the instrument ------------------------------------------------------ */

/* One examination. The primary output of the autopsy is the DIFF against the
   previous one, so these are written to disk and never overwritten in place. */
typedef struct {
    uint32_t distinct_paths;
    uint32_t distinct_atoms;
    uint64_t traversals;
    uint64_t atoms_ingested;
    uint64_t plastic_bytes;
    double   entropy;
    double   effective_support;
    double   top1_share;
    /* the plastic layer proper: what it built out of the atoms */
    uint32_t frags;          /* fragments known, atoms included */
    uint64_t births;         /* conjunctions that earned their existence */
    uint64_t frag_steps;     /* transitions walked between fragments */
    uint32_t orphans;        /* known but unwalkable: no traffic, nowhere to go */
    uint64_t pool_full;      /* paths refused because a module's pool was spent */
    uint32_t modules_used;   /* lineages that hold more than their bare atom */
    uint32_t widest_module;  /* fragments in the fullest lineage */
    uint32_t busiest_frag;   /* paths out of the most connected fragment */
    uint64_t wakes, sleeps;
    uint64_t decay_passes;   /* how much forgetting has run */
    uint32_t shapes_transfer;/* ideas tested enough, and right more than chance */
    double   best_shape;     /* the merit of its best idea */
    uint64_t cold_edges;     /* paths at the floor: remembered, not offering themselves */
} rz_autopsy;

/* Fragments that exist but carry no traffic and lead nowhere — born early, then
   displaced by wider conjunctions. Dead weight in the plastic layer, and the
   first thing pruning should be pointed at. */
uint32_t rz_orphans(rz_mind *m);

/* Examining never strengthens what it measures; it may wake a module to read
   it, which is why it is not const. */
void rz_examine(rz_mind *m, rz_autopsy *out);

/* A layout change must fail loudly rather than half-load. On any error the
   destination is left exactly as it was. */
int rz_save(const rz_mind *m, const char *path);
int rz_load(rz_mind *m, const char *path);

int rz_autopsy_save(const rz_autopsy *a, const char *path);
int rz_autopsy_load(rz_autopsy *a, const char *path);

/* --- the diagnosis ------------------------------------------------------- */

typedef enum {
    RZ_V_EMPTY,          /* has not eaten */
    RZ_V_BASELINE,       /* first examination, nothing to compare against */
    RZ_V_CONCENTRATING,  /* the alarm: it is using less of what it has */
    RZ_V_CONSOLIDATING,  /* discovered nothing new, lived it again */
    RZ_V_HEALTHY         /* reach and use moving together */
} rz_verdict;

/* Relative fall in effective support that counts as concentration rather than
   noise. A first guess — it wants calibrating against real sessions. */
#define RZ_COLLAPSE_DROP 0.05

/* Effective support falling is the alarm on its own. Requiring path count to
   rise as well misses the case where material concentrates traversal without
   introducing anything unseen — which is exactly what obsessive input does. */
rz_verdict rz_diagnose(const rz_autopsy *now, const rz_autopsy *was, int have_prev);

const char *rz_verdict_text(rz_verdict v);

/* --- recall by cue (P8) --------------------------------------------------- */

#define RZ_RECALL_MAX 4096

/* The effective number of continuations leaving an atom: exp(entropy of its
   outgoing distribution). 1.0 when the next atom is certain, 0 at a dead end.
   This is what recall costs: a trace you know cold is free to follow, an
   ambiguous one has to be worked for. */
double rz_branching(const rz_mind *m, unsigned char from);

#define RZ_TRACE_MAX 512

typedef struct {
    unsigned char atoms[RZ_RECALL_MAX];
    size_t   len;
    uint64_t effort;    /* what it cost to get here */
    uint32_t descents;  /* times it had to go down to find another way on */
    uint32_t wanders;   /* steps where it did not take the strongest path */
    uint32_t damped;    /* steps where the refractory guard changed the choice */
    int      gave_up;   /* stopped short: nowhere left, or the budget ran out */

    /* Where every piece came from, in order. Nothing it produces is opaque:
       each entry expands back to atoms, so any output can be taken apart and
       accounted for. */
    int32_t  trace[RZ_TRACE_MAX];
    uint32_t trace_len;
} rz_recall;

/* Follow the trace onward from the END of the cue, always taking the
   strongest continuation and spending effort equal to how ambiguous each step
   was. Stops at a dead end, when `want` atoms have come back, or when the
   effort budget is spent — a mind that must pay to remember has a reason to
   stop trying.
 *
 * Read-only: recalling never strengthens what it recalls. Whether it SHOULD is
 * still open (P8); until that is decided, recall stays a pure instrument. */
void rz_recall_from(const rz_mind *m,
                    const unsigned char *cue, size_t cue_len,
                    size_t want, uint64_t effort_budget,
                    rz_recall *out);

/* --- fragments: the addressable unit (P2) --------------------------------- */

/* Every atom is already a fragment, so this never counts below RZ_ATOMS. */
int32_t rz_frag_count(const rz_mind *m);

/* Write a fragment's decomposition into atoms. Nothing is ever opaque:
   every fragment can be taken apart all the way down. */
size_t rz_expand(rz_mind *m, int32_t frag, unsigned char *out, size_t cap);

/* The longest known fragment matching at the start of buf. Always succeeds for
   a non-empty buffer, because every atom is a fragment. */
int32_t rz_longest_match(rz_mind *m, const unsigned char *buf, size_t n);

/* The fragment a cue fires.
 *
 * A conjunction of atoms is the trigger; a lone atom rarely is. The rule is
 * overlap: a fragment fires when some SUFFIX of the cue matches a PREFIX of it,
 * longest overlap first. That covers both readings at once —
 *
 *   "…ab"   against fragment "ab"           -> overlap 2, nothing left to say,
 *                                              carry on by its paths
 *   "hello" against fragment "hello world " -> overlap 5, and " world " is what
 *                                              the cue was reaching for
 *
 * Among equal overlaps the strongest fragment wins, then the widest.
 * If a candidate has nothing to say and nowhere to go, the cue backs off to a
 * shorter overlap — a hint that does not fire is tried smaller, not abandoned.
 *
 * `overlap` receives how many atoms of the fragment the cue already covered.
 * It may be NULL. */
int32_t rz_trigger_ex(rz_mind *m, const unsigned char *buf, size_t n,
                      size_t *overlap);
int32_t rz_trigger(rz_mind *m, const unsigned char *buf, size_t n);

uint32_t rz_frag_edge(rz_mind *m, int32_t from, int32_t to);
double   rz_frag_branching(rz_mind *m, int32_t from);
int32_t  rz_frag_degree(rz_mind *m, int32_t from);
/* Only the paths it actually took. This is what shape and branching read. */
int32_t  rz_frag_degree_walked(rz_mind *m, int32_t from);
/* The places this leads to, snapshotted into `out`. Returns how many were
   written. Snapshotting matters: resolving a target can page a module away and
   move the list being walked. */
int32_t  rz_frag_edges(rz_mind *m, int32_t from, int32_t *out, int32_t cap);
/* The target of one path it was handed and never took, or -1. */
int32_t  rz_frag_first_guess(rz_mind *m, int32_t from);

/* Recall addressed by fragments rather than atoms. Same effort accounting.
   Non-const only because reading may wake a sleeping module; no intensity,
   path or fragment is changed. */
void rz_recall_frag(rz_mind *m,
                    const unsigned char *cue, size_t cue_len,
                    size_t want_atoms, uint64_t effort_budget,
                    rz_recall *out);

/* --- vertical composition (P18) -------------------------------------------
 *
 * Everything up to here composes HORIZONTALLY: A followed by B becomes AB.
 * Concatenation. You can stack bricks end to end forever and never notice that
 * two different towers have the same silhouette.
 *
 * "more mass -> more attraction" and "more practice -> more skill" share not a
 * single atom. They share a SHAPE. Seeing that is a different operation, and it
 * is what turns a very good index into something that can carry what it learned
 * into a place it has never been.
 *
 * A fragment's SHAPE is a small signature of the role it plays, with its
 * identity thrown away: how decided it is, how wide it is, how concentrated its
 * traffic is. Two fragments with the same shape behave alike without having
 * anything in common.
 *
 * And then the graph exists a second time, one level up: shape -> shape. That
 * is the abstract layer, and it is built from the same traffic that builds the
 * concrete one.
 *
 * Why this is also the answer to what should MATTER: knowledge is worth having
 * when it can be APPLIED — when it lets you handle something you have never
 * met. A shape that only ever fits one fragment applies to nothing. A shape
 * that fits hundreds, and still predicts, is instrumentable. That is measurable
 * here, and it is the only kind of value this system can honestly claim yet.
 */
/* The role a fragment plays, with its content discarded. */
uint8_t rz_shape_of(rz_mind *m, int32_t frag);

/* Distinct fragments carrying this shape: how broadly it applies. */
uint32_t rz_shape_members(rz_mind *m, uint8_t shape);

/* How often this shape anticipated correctly, 0..1. Its merit — and the only
   thing in the system that measures the worth of a piece of KNOWLEDGE rather
   than the worth of a piece of material. */
double rz_shape_merit(const rz_mind *m, uint8_t shape);

/* Shapes that have been tested enough to be believed, and that transfer better
   than chance. */
uint32_t rz_shapes_that_transfer(const rz_mind *m);

/* A shape has to have been tried this often before its merit means anything. */
#define RZ_SHAPE_TESTED 24

/* What a fragment of this shape tends to be followed by. -1 if it has no idea.
   This is the fallback that makes the general useful when the specific is
   unknown — applied knowledge, in the only sense the system can measure. */
int32_t rz_predict_shape(const rz_mind *m, uint8_t from);

/* --- forgetting (P5 + P7) -------------------------------------------------
 *
 * A mind does not delete. The book stays on the shelf; the path to it becomes
 * hard to walk. So nothing here is ever removed — a path that stops being used
 * fades until it is cold, and a cold path is one that no longer comes up on its
 * own but can still be reached if something points at it.
 *
 * Two forces, and they pull against each other:
 *
 *   use    +1 per traversal, up to a ceiling — no path grows forever
 *   decay  ×RZ_DECAY every RZ_DECAY_EVERY steps of experience
 *
 * Repetition outruns decay; a single accident does not. Nobody has to decide
 * what is noise — the two forces sort it out.
 *
 * Decay runs against EXPERIENCE, not against the clock. Razar has no clock, and
 * a mind that spent the interval thinking should not forget as much as one that
 * spent it switched off.
 *
 * It is applied lazily: a sleeping module catches up on everything it missed
 * the moment something reaches into it, in one step. Forgetting therefore costs
 * nothing for the parts nobody is touching.
 */
#define RZ_DECAY        0.98    /* what an unused path keeps per pass */
#define RZ_DECAY_EVERY  20000   /* steps of experience between passes */
/* A path it was GIVEN and a path it TOOK must not look the same.
 *
 * A newborn inherits its right parent's exits so it is never a dead end, and
 * those arrive meaning "this is possible" — a guess, not experience. Until they
 * were told apart, a guess and a fully-decayed memory both read as weight 1, and
 * with mean out-degree at 93 the guesses drowned the experience: branching
 * saturated, concentration flattened, and the shape alphabet collapsed to four
 * classes that differed only in span.
 *
 * So the floor of experience sits ABOVE the weight of a guess. Nothing is spent
 * to store it: the distinction is the number itself. */
#define RZ_EDGE_INHERITED 1     /* possible. Never walked. Has nothing to forget */
#define RZ_EDGE_FLOOR   2       /* a path never vanishes — it goes cold */
#define RZ_CEILING      60000   /* no path grows without limit */

/* One pass of forgetting. Resident modules feel it now; the sleeping ones feel
   it when they wake. */
void rz_decay(rz_mind *m);

/* Paths sitting at the floor: remembered, but no longer offering themselves.
   Counts both the faded and the never-warmed — an inherited path arrives at the
   floor by construction. Either way it will not come up on its own. */
uint64_t rz_cold_edges(rz_mind *m);

/* --- the fixed external bar (D23) -----------------------------------------
 *
 * Every panel in the autopsy is an INTERNAL measure: the organism grading its
 * own homework. All of them can look excellent while it is useless, because a
 * bar that rises with its subject measures nothing absolute.
 *
 * This is the bar that does not move. Walk material the organism has never
 * eaten, and at every step ask it to name the next fragment BEFORE the material
 * is read. It cannot cheat: the answer is committed before the question is
 * revealed, and the material does not bend to please it.
 *
 * It needs no opponent and no game — only material held back from the feed.
 *
 * `rz_measure` LEARNS NOTHING. No intensity, no fragment, no path is changed.
 * The bar measures and never teaches.
 */
/* --- interest (P16) -------------------------------------------------------
 *
 * Interest is not a feeling and it is not programmed. It falls out of how the
 * organism already learns, and it needs no affect layer underneath it.
 *
 * Every step of a walk lands in one of three places relative to what it knows:
 *
 *   OPAQUE     it had no expectation at all — nothing to hook onto. Noise.
 *   FRONTIER   it had expectations and they were imperfect. The edge.
 *   MASTERED   it called the step with near-certainty. Nothing left to gain.
 *
 * What a mind wants is the middle. Too easy bores, too hard does not stick, and
 * the band between them moves on its own as it learns — what interests it today
 * will bore it later without anyone changing a threshold.
 *
 * The same walk that scores the bar produces this: one observation read two
 * ways. Nothing here writes.
 */
#define RZ_MASTERY 0.75   /* probability at or above which a step counts as known */

typedef struct {
    /* the bar: could it call the next piece before seeing it */
    uint64_t predictions;   /* steps where a guess was possible */
    uint64_t hits;          /* the guess was right */
    double   surprise;      /* total, 0 = fully expected, 1 = never seen */
    uint64_t atoms;         /* material measured */

    /* where this material sits relative to what it already knows */
    /* where the general saved what the specific could not: steps the organism
       had never taken, answered by the shape of the place it was standing in */
    uint64_t shape_tried;
    uint64_t shape_hits;

    uint64_t opaque;        /* no expectation whatsoever */
    uint64_t frontier;      /* it had expectations, and they were imperfect */
    uint64_t novel;         /* frontier steps it had never taken at all */
    uint64_t mastered;      /* called with near-certainty */
} rz_measure_result;

/* Share of the unknown steps that the shape got right anyway: how often the
   general rescued what the specific could not answer. */
double rz_shape_rescue(const rz_measure_result *r);

double rz_opaque_share(const rz_measure_result *r);
double rz_frontier_share(const rz_measure_result *r);
double rz_mastered_share(const rz_measure_result *r);
double rz_novel_share(const rz_measure_result *r);

/* --- the appetite ledger --------------------------------------------------
 *
 * Interest is better read as a DERIVATIVE than as a level: not "the surprise is
 * middling" but "the surprise is falling" — I am getting better at this. That
 * needs memory of previous appraisals, which is what this holds.
 *
 * Keyed by content, not by name: a file that changed is not the same material
 * and must not be compared against a stale reading.
 */
#define RZ_APPRAISALS 256

typedef struct {
    char     path[192];
    uint64_t fingerprint;
    double   surprise;      /* mean, when last appraised */
    double   frontier;      /* share, when last appraised */
    uint64_t run;
} rz_appraisal;

typedef struct {
    rz_appraisal item[RZ_APPRAISALS];
    int32_t  n;
    uint64_t run;
} rz_ledger;

uint64_t rz_fingerprint(const unsigned char *buf, size_t n);

void rz_ledger_init(rz_ledger *L);
int  rz_ledger_load(rz_ledger *L, const char *path);
int  rz_ledger_save(rz_ledger *L, const char *path);

/* The previous appraisal of this exact material, or NULL if it is new or has
   changed since. */
const rz_appraisal *rz_ledger_find(const rz_ledger *L, const char *path,
                                   uint64_t fingerprint);

void rz_ledger_record(rz_ledger *L, const char *path, uint64_t fingerprint,
                      double surprise, double frontier);

/* The fragment the organism expects to follow `from`, or -1 if it has no idea. */
int32_t rz_predict_next(rz_mind *m, int32_t from);

void rz_measure(rz_mind *m, const unsigned char *buf, size_t n,
                rz_measure_result *out);

/* Share of predictions that were right, 0..1. */
double rz_hit_rate(const rz_measure_result *r);
/* Mean surprise per prediction, 0..1. */
double rz_mean_surprise(const rz_measure_result *r);



/* What a sense is worth: how much it tells the organism about what comes next.
   Not an opinion — the information its reading carries about the next step,
   in nats, measured on material and never taught. */
double rz_sense_worth(rz_mind *m, const rz_sense *s,
                      const unsigned char *buf, size_t n);

/* One round of living: score what it holds, let a newborn in, and audit an old
   one. The gates are deliberately asymmetric — a sense is born easily, kept
   when it is merely neutral, and dropped only when it adds nothing at all that
   another sense was not already giving.

   Keeping the neutral ones is the whole point. A sense that says little today
   is the material another sense is built from tomorrow, and an organism that
   prunes for usefulness starves its own composition. */
void   rz_senses_live(rz_mind *m, uint64_t seed);
int32_t rz_senses_count(const rz_mind *m);
/* Add a sense to the population, clearing what it has seen. Returns its index,
   or -1 when there is no room. */
int32_t  rz_sense_add(rz_mind *m, rz_sense s);
/* How many steps this sense has actually watched. Worth is read off these. */
uint64_t rz_sense_observations(const rz_mind *m, int32_t i);
/* One step, watched. Only the awake senses read it; a sleeping one still gets
   its turn in rotation, because asleep must not mean buried. */
void     rz_sense_observe(rz_mind *m, int32_t from, int32_t to);

/* --- rules (P31) ----------------------------------------------------------
 *
 * WHEN this moment looks like that: the shape of where it stands together with
 * what the awake senses read there. This is where the senses stop observing and
 * start being the thing a decision is keyed on. */
uint32_t rz_situation(rz_mind *m, int32_t here);
uint32_t rz_rules_count(const rz_mind *m);
/* Born from having thought, reinforced by meeting the same lesson again. */
void     rz_rule_harvest(rz_mind *m, int32_t from, int32_t to,
                         double score, double level);
/* Recognising it is what saves the thinking. Returns the rule, or -1. */
int32_t  rz_rule_consult(rz_mind *m, int32_t here, int32_t *to, double *weight);
/* Walk material and keep only what was expensive to work out. */
void     rz_rules_learn(rz_mind *m, const unsigned char *buf, size_t n);

/* --- the inner conversation (P32) -----------------------------------------
 *
 * It had senses that watched and rules that voted, and nothing closed the
 * circle: it never found out whether it had been right, so nobody paid for
 * being wrong. Now every step is an exchange -- it says what it expects before
 * the next piece is looked at, reality answers, and whoever spoke is credited
 * or blamed. A rule that keeps being wrong is never deleted; it fades until its
 * vote cannot move anything, which is what forgetting has always meant here. */
void     rz_inner_listen(rz_mind *m, int on);   /* on by default */
uint32_t rz_inner_count(const rz_mind *m);
/* 0 is the most recent thing it said to itself. */
int      rz_inner_at(const rz_mind *m, uint32_t back, rz_inner *out);
uint64_t rz_inner_right(const rz_mind *m);
uint64_t rz_inner_said(const rz_mind *m);
const rz_sense *rz_sense_at(const rz_mind *m, int32_t i);
double  rz_sense_score(const rz_mind *m, int32_t i);
void    rz_sense_describe(const rz_mind *m, int32_t i, char *out, size_t cap);

/* Drawn from a seed. The same seed is the same sense — a draw, not a roll. */
/* Drawn to fit the organism it belongs to: a sense that asks about a shape
   nobody carries is born dead, and half the draws were being wasted that way. */
rz_sense rz_sense_draw(rz_mind *m, uint64_t seed);
/* A newborn is a divergent copy: it differs in exactly one slot. Drawing wholly
   at random every time would accumulate nothing. */
rz_sense rz_sense_mutate(rz_mind *m, const rz_sense *parent, uint64_t seed);
int      rz_sense_same(const rz_sense *a, const rz_sense *b);
uint32_t rz_sense_distance(const rz_sense *a, const rz_sense *b);
/* What it sees from where it stands. Learns nothing. */
uint32_t rz_sense_read(rz_mind *m, const rz_sense *s, int32_t here);

/* --- harmony (P25) --------------------------------------------------------
 *
 * The referee. Not "is this harmonious" — always "did this become MORE
 * harmonious", because harmony is a direction and the process never finishes.
 *
 * D39: describing the SAME material with LESS, while still accounting for ALL
 * of it. Two numbers, and the second is what makes the first safe.
 *
 *   effort       the cost of walking the material — the order it has found
 *   unaccounted  the share of steps it could not hook onto anything
 *
 * Effort alone would crown a collapse: one path, no branching, no doubt, and no
 * contact with the material either. That shortcut is what left D24 open since
 * P15 was designed. It closes here, because a mind that never met the material
 * cannot account for it, and the second number refuses it.
 *
 * Like the external bar, this LEARNS NOTHING. A referee that trains what it
 * judges is not a referee.
 */
typedef struct {
    double   cost;         /* TOTAL nats to describe the material, not a mean */
    double   unaccounted;  /* 0..1 — steps with no experience to hook onto */
    uint64_t steps;
} rz_harmony;

void rz_measure_harmony(rz_mind *m, const unsigned char *buf, size_t n,
                        rz_harmony *out);

/* Better means cheaper WITHOUT having dropped more of the material. Both, or it
   is not an order found. */
int rz_more_harmonious(const rz_harmony *a, const rz_harmony *b);

/* --- other directions -----------------------------------------------------
 *
 * A mind does not only look backwards and forwards. The memory holds more than
 * one direction, and until now recall used exactly one of them:
 *
 *   forward   what followed        the only one walked so far
 *   down      its parts            recorded as left/right, never used
 *   backward  what preceded        not recorded yet — needs a reverse index
 *   up        what contains it     not recorded yet
 *
 * Going DOWN costs nothing extra, because the decomposition is already there.
 * It is also the way out of the dead end that keeps appearing: a wide
 * conjunction may lead nowhere while its tail leads everywhere. Rather than
 * give up, the walk drops to the tail and carries on from inside.
 */

/* The tail of a fragment — one step down. -1 for a bare atom, which cannot be
   taken apart any further. */
int32_t rz_descend(rz_mind *m, int32_t id);

/* --- lateral thinking (D15) -----------------------------------------------
 *
 * Always taking the strongest path is periodic by construction: a deterministic
 * walk on a finite graph must eventually revisit a fragment, and from there it
 * circles forever. That is not a fault of the memory — it is a fault of the
 * walk.
 *
 * `temperature` samples among the paths instead of taking the best:
 *   0        strict best — faithful recall, what a question deserves
 *   ~0.3     mostly faithful, occasionally elsewhere
 *   1        proportional to how travelled each path is
 *   large    indifferent
 *
 * Deterministic given `seed`, so a lateral walk can be replayed exactly. What
 * is measured should stay at 0; wandering belongs to the self-directed walks.
 */
void rz_recall_lateral(rz_mind *m,
                       const unsigned char *cue, size_t cue_len,
                       size_t want_atoms, uint64_t effort_budget,
                       double temperature, uint64_t seed,
                       rz_recall *out);

/* --- dreaming (P14) -------------------------------------------------------
 *
 * Dreaming is not one job. It is a job the organism CHOOSES according to how it
 * currently is, which is why none of the three has to be thrown away:
 *
 *   stuck            -> LOOSEN   deform what it knows too well, so it stops
 *                                circling. Costs fidelity, buys reach.
 *   something new    -> SETTLE   exercise what just formed, so it becomes
 *                                solid instead of evaporating.
 *   overwhelmed      -> CALM     go back over what it could not digest, so it
 *                                stops being a wall.
 *
 * All three states are already measured. Since the last sleep the organism
 * knows how much it walked, how much new structure it built, how often it was
 * completely blind, and how often its own ideas called the shot. That is enough
 * to triage.
 *
 * And whichever dream it has, it may write ONE thing: attention. The episode is
 * false and is discarded — a dream never touches an intensity, never records a
 * traversal, never creates a fragment. What survives waking is only a set of
 * marks saying where to look again.
 */
typedef enum {
    RZ_DREAM_NONE,     /* nothing has happened worth sleeping on */
    RZ_DREAM_LOOSEN,   /* it is circling: shake it out of the rut */
    RZ_DREAM_SETTLE,   /* it built something new: make it solid */
    RZ_DREAM_CALM      /* it was overwhelmed: go back over the wall */
} rz_dream_kind;

#define RZ_DREAM_MIN_STEPS  2000   /* below this there is nothing to sleep on */
#define RZ_OVERWHELMED      0.55   /* share of blind steps that counts as too much */
#define RZ_FERTILE         0.004   /* births per step that counts as a lot of new structure */
#define RZ_ATTENTION_MAX   1000
#define RZ_ATTENTION_KEEP  0.90    /* attention fades too, per decay pass */

/* What this organism needs to dream about right now, and why. */
rz_dream_kind rz_dream_needed(const rz_mind *m);
const char   *rz_dream_reason(rz_dream_kind k);

/* Sleep once: pick the dream the state calls for, walk it, and leave nothing
   behind but attention. Returns which dream was had. */
rz_dream_kind rz_sleep(rz_mind *m, uint64_t seed);

uint16_t rz_attention_of(rz_mind *m, int32_t frag);
uint64_t rz_attention_total(rz_mind *m);

/* --- the pen (P17) --------------------------------------------------------
 *
 * Writing is not recalling. A recall answers "what follows this?"; a text has
 * to keep going without turning into a chant, and greedy walking is periodic by
 * construction — it always ends up circling.
 *
 * So writing takes what the rest of the system already knows:
 *
 *   wandering     it does not always take the strongest path (D15)
 *   refractory    a fragment just used is briefly harder to reach again, the
 *                 way a neuron that just fired is. Not forbidden — dampened
 *   descent       a dead end is walked around rather than surrendered to
 *   effort        ambiguity costs, and the budget is what ends the text
 *   trace         every piece is recorded, so the output can be taken apart
 *
 * Read-only, like everything else that looks: it writes a text, not a memory.
 */
#define RZ_REFRACTORY   12   /* how many recent fragments stay dampened */
#define RZ_REFRACTORY_D  8   /* how much their weight is divided by */

void rz_write(rz_mind *m,
              const unsigned char *cue, size_t cue_len,
              size_t want_atoms, uint64_t effort_budget,
              double temperature, uint64_t seed,
              rz_recall *out);

#endif
