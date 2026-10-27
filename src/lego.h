/* The legos (P33): what kind of piece this is, and how pieces combine.
 *
 * Memory was never the bottleneck. On text it never read, 59% of the steps go
 * to a piece it has never seen follow the one before -- a place memory cannot
 * reach at all. A child crosses that gap by knowing what KIND of thing comes
 * next before knowing which one, and by recognising a word it never met from
 * where it sits. Three steps, each built on the one before:
 *
 *   1  clean legos   cut where a piece really ends: where what follows becomes
 *                    uncertain. Nobody says what a word is; the spaces are the
 *                    exam, never the rule
 *   2  kinds         two legos are the same kind when they keep the same
 *                    company. A lego never seen gets a kind from its neighbours
 *   3  play          guess the kind that comes next, then the lego inside it
 *
 * Read-only on the mind: the lexicon is a separate thing it builds by looking.
 */
#ifndef RZ_LEGO_H
#define RZ_LEGO_H

#include "razar.h"

#define RZ_LEGO_SPAN     32    /* a lego longer than this is cut anyway */
#define RZ_LEGO_COMPANY 256    /* company is judged against the commonest legos */

/* --- step 1: clean legos --------------------------------------------------
 *
 * A lego ends at byte i when the mind's uncertainty about what follows t[i]
 * peaks there, and the peak belongs to the TALL group: the peak heights of the
 * text fall into two heights (inside a word and between words) and the text
 * itself decides where one becomes the other. ends[i] = 1 marks an end; the
 * last byte always ends one. Returns how many legos. */
size_t rz_lego_cut(const rz_mind *m, const unsigned char *t, size_t n,
                   unsigned char *ends);

/* One level up, the same reading again. The atoms that end legos fall into two
   heights once more: the stem ends inside a word, and the tallest -- on the New
   Testament that side holds exactly the space and the line feed, found, not
   named (precision 1.000, recall 1.000 against the spaces). A word ends only
   where a lego ends on that tallest side. Returns how many words. */
size_t rz_word_cut(const rz_mind *m, const unsigned char *t, size_t n,
                   unsigned char *ends);

/* --- step 2: kinds by company --------------------------------------------- */
typedef struct {
    uint8_t       len;
    unsigned char s[RZ_LEGO_SPAN];
    uint32_t      count;
    int32_t       kind;              /* -1 until sorted */
} rz_lego;

typedef struct {
    uint32_t a, b, count;            /* lego a stood just before lego b */
} rz_lego_pair;

typedef struct {
    rz_lego      *lego;
    uint32_t      n, cap;
    int32_t      *slot;              /* spelling hash -> lego id */
    uint32_t      slots;
    rz_lego_pair *pair;
    uint32_t      npair, pcap;
    int32_t      *pslot;             /* (a,b) hash -> pair index */
    uint32_t      pslots;
    uint32_t      kinds;             /* 0 until sorted */
    uint64_t     *follow;            /* kinds x kinds: kind a before kind b */
    int32_t      *seq;               /* every lego read, in order; -1 between reads */
    uint32_t      nseq, seqcap;
    /* derived, rebuilt by sort and load: never saved */
    uint32_t     *out_start;         /* n + 1 offsets into out_pair */
    uint32_t     *out_pair;          /* pair indices, grouped by the lego before */
    int32_t      *top;               /* commonest lego of each kind */
} rz_lexicon;

void    rz_lexicon_init(rz_lexicon *x);
void    rz_lexicon_free(rz_lexicon *x);
/* Cut the text with the mind and count every lego and every pair. */
void    rz_lexicon_read(rz_lexicon *x, const rz_mind *m,
                        const unsigned char *t, size_t n);
/* The same, cut into words instead of legos: what a question is asked in. */
void    rz_lexicon_read_words(rz_lexicon *x, const rz_mind *m,
                              const unsigned char *t, size_t n);
int32_t rz_lexicon_find(const rz_lexicon *x, const unsigned char *s, size_t len);
/* Group the legos by the company they keep. Deterministic for a seed. */
void    rz_lexicon_sort(rz_lexicon *x, uint32_t kinds, uint64_t seed);
int32_t rz_lexicon_kind(const rz_lexicon *x, int32_t lego);
/* A lego never seen: its kind from the kinds of its neighbours (-1 = none). */
int32_t rz_lexicon_guess_kind(const rz_lexicon *x, int32_t left, int32_t right);

/* --- step 3: play ---------------------------------------------------------- */
int32_t rz_lexicon_next_kind(const rz_lexicon *x, int32_t kind);
/* The kind first, then the lego inside it: the one that followed this lego
   most if any did, else the commonest of that kind. */
int32_t rz_lexicon_next(const rz_lexicon *x, int32_t lego);

/* --- curiosity (P34) -------------------------------------------------------
 *
 * A question brings its own relation: "¿Quién es Cristo?" brings `es` and
 * `Cristo`. Answering is reading the other side of that relation wherever the
 * two stand together -- both ways, because "Jesús es el Cristo" answers it
 * from behind. An answer is only what the subject added beyond what the
 * relation gives by itself (D50): an article follows "es" whatever is asked,
 * so it answers nothing, and nothing has to name it.
 *
 * And the child's "why?" is asking again about the answer. It stops when the
 * answers lead back to what it already asked (a circle closed) or when nothing
 * answers (a gap -- the question stays open). */
typedef struct {
    int32_t  lego;
    uint32_t support;                /* frames that said so */
    int8_t   dir;                    /* +1 "X r answer", -1 "answer r X" */
} rz_answer;

uint32_t rz_lexicon_ask(rz_lexicon *x, int32_t subject, int32_t relation,
                        rz_answer *out, uint32_t cap);

typedef enum {
    RZ_WONDER_CIRCLE = 0,            /* every answer leads back: understood */
    RZ_WONDER_GAP    = 1,            /* nothing answers: the question stays open */
    RZ_WONDER_DEEP   = 2             /* still asking when the room ran out */
} rz_wonder_end;

/* chain[0] is the subject; every next one is the answer to "why?" about the
   one before. Returns the length of the chain. */
uint32_t rz_lexicon_wonder(rz_lexicon *x, int32_t subject, int32_t relation,
                           int32_t *chain, uint32_t cap, rz_wonder_end *end);

/* Cut a text into words the way read_words does, but only look them up: ids in
   x's own numbering, -1 for a word it never met. Nothing is learned. */
uint32_t rz_lexicon_words_of(const rz_lexicon *x, const rz_mind *m,
                             const unsigned char *t, size_t n,
                             int32_t *ids, uint32_t cap);

/* --- behaviours (P35): acting, and what it wants -------------------------
 *
 * What it wants is a mix (author's decision, D73): HARMONY -- an answer is worth
 * something when what accompanies the answer already describes what
 * accompanies the subject -- and TRANSFER -- in text it has not read yet, the
 * subject is followed by something never seen after it, but seen after the
 * answer. Each is read against the same count made with words of similar
 * frequency picked blind; the worth is the sum of the two gains. */
void rz_answer_worth(rz_lexicon *x, const int32_t *later, uint32_t nlater,
                     int32_t subject, int32_t answer,
                     double *harmony, double *transfer);

/* A way of asking: a relation, and what asking with it has paid. */
typedef struct {
    int32_t  relation;
    uint32_t tries;
    double   paid;
    uint8_t  imitated;               /* shown by someone, not found */
} rz_habit;

#define RZ_HABITS 32

typedef struct {
    rz_habit habit[RZ_HABITS];
    uint32_t nhabit;
    uint8_t *asked;                  /* habit x lego: already asked */
    uint32_t asked_n;                /* lexicon size it was sized for */
    uint64_t acts;
    double   sum, sumsq;             /* every pay seen, for the spread */
    int32_t *plan[RZ_HABITS];        /* per way of asking: whom to ask, studied first */
    uint32_t nplan[RZ_HABITS];
    uint64_t blind;                  /* nonzero: the bar -- chooses its way of
                                        asking at random, from this seed */
} rz_player;

typedef struct {
    int32_t  relation, subject, answer;
    double   harmony, transfer, pay;
    uint8_t  new_form;               /* it tried a way of asking for the first time */
} rz_act;

/* Where a question applies (P42). Not chance, and not trying for ever: the
 * question's own structure is studied first. The text already says to whom it
 * can be put -- "is" stands after "rex" and "tom", never after "the" or "a".
 * Those are the subjects it applies to, ranked by how much more they stand
 * there than anywhere (D50). Then the melody is tried on something new: words
 * that never stood before the question but keep the company those subjects
 * keep (the silhouette, P36). Read-only. */
uint32_t rz_form_subjects(rz_lexicon *x, int32_t relation, int32_t *out, uint32_t cap);
uint32_t rz_form_reaches(rz_lexicon *x, const int32_t *studied, uint32_t ns,
                         int32_t *out, uint32_t cap);

void rz_player_init(rz_player *p);
void rz_player_free(rz_player *p);
/* Imitation: a way of asking someone used in front of it. */
void rz_player_imitate(rz_player *p, int32_t relation);
/* One act. It chooses a way of asking -- one it never tried first, otherwise
   the one whose pay, plus what it still does not know about it, is highest --
   and a subject it never asked that way. Then it asks, judges the answer
   against `later`, and credits the way of asking. 0 when it has nothing left
   to ask. */
int  rz_player_act(rz_player *p, rz_lexicon *x, const int32_t *later,
                   uint32_t nlater, rz_act *out);

/* --- concepts (P36) --------------------------------------------------------
 *
 * Rex, Fido and the neighbour's dog are not alike, yet all of them bark and all
 * of them "are dogs". A concept is what repeats across many different cases: a
 * group of subjects that give the same answer to the same question ("X es ?"),
 * together with that answer.
 *
 * It is the floor rising once more -- bytes into legos, legos into kinds, facts
 * into concepts -- and it is adopted on the same terms as everything else: only
 * if it pays for itself, in what Razar wants (D73). HARMONY: what follows each
 * member is already described by what follows the rest of the concept. TRANSFER:
 * in text it has not read, a member does something it was never seen doing,
 * but the rest of the concept was. Both against a blind group of words of the
 * same frequencies. A group of one is an anecdote and never forms. */
#define RZ_CONCEPT_MEMBERS 64

typedef struct {
    int32_t  head;                   /* the shared answer: "perro" */
    int32_t  relation;               /* the question it answers: "es" */
    uint32_t nmember;
    int32_t  member[RZ_CONCEPT_MEMBERS];
    double   harmony, transfer;      /* gains over the blind group */
    uint8_t  adopted;                /* it paid for itself */
} rz_concept;

/* Every concept the relation gives, most members first. Read-only. */
uint32_t rz_concepts_find(rz_lexicon *x, int32_t relation,
                          const int32_t *later, uint32_t nlater,
                          rz_concept *out, uint32_t cap);
/* The same, asked with every relation in the set at once. */
uint32_t rz_concepts_find_any(rz_lexicon *x, const int32_t *relations, uint32_t nrel,
                              const int32_t *later, uint32_t nlater,
                              rz_concept *out, uint32_t cap);

/* The silhouette of a question: the words that keep the company a word keeps
   ("es" -> será, era, sea, está), most alike first. Where it ends is read off
   the similarities themselves, the way words were found: the split of all of
   them into two, then the widest gap inside the tall side. Read-only. */
uint32_t rz_lexicon_silhouette(rz_lexicon *x, int32_t word, int32_t *out, uint32_t cap);

/* --- types and identities (P37) --------------------------------------------
 *
 * "hijo" is a type and "David" is an identity (the author, 2026-10-11). The
 * difference is an asymmetry, and nothing has to name either side:
 *
 *   the binding  what follows the seed type far more than it follows anything
 *                ("hijo" -> "de"), found from one type the question brings
 *   a type       a word followed by the binding word on the tall side of the
 *                split: it asks for an owner ("rey de", "padre de", "amigo de")
 *   an identity  a word that fills the slot of two or more different types and
 *                spends the tall side of its life in those slots: "la" fills
 *                them too, but lives everywhere
 *
 * Capitals are only the bar. Returns the number of identities; is_type and
 * is_identity are x->n flags the caller owns. */
uint32_t rz_lexicon_roles(rz_lexicon *x, int32_t seed, int32_t *binding,
                          uint8_t *is_type, uint8_t *is_identity);

/* Weaving (P38). A concept is music: a structure that forms, deforms, and turns
 * up again somewhere else (the author). "su madre" is "la madre de X" deformed:
 * the owner moved in front, and so "de" is gone. A new binding is a word, on
 * the other side of the type, that takes the owner's place -- with it there,
 * the known bindings vanish ("su": 3% "de" after; "la": 59%, it replaces
 * nothing). Bindings bring types, types bring bindings, round after round,
 * until nothing new comes. Nothing names a possessive. */
typedef struct {
    int8_t  side;                    /* -1 before the type, +1 after it */
    int32_t word;
    double  replaces;                /* the other side's binding, relative to base: < 1 */
    uint8_t round;                   /* when it was woven in */
} rz_binding;

#define RZ_BINDINGS 64

/* Returns the number of bindings; is_type is x->n flags the caller owns. */
uint32_t rz_lexicon_weave(rz_lexicon *x, int32_t seed, rz_binding *out, uint32_t cap,
                          uint8_t *is_type);

/* The melody (P39): the bindings, kept as notes -- a side and a spelling --
 * so that they belong to no text and travel from book to book. Every text it
 * reads, it carries the melody in, weaves on from it whatever that text has
 * that the melody did not, and keeps the larger melody. It always weaves the
 * new; it never forgets a note. */
typedef struct {
    int8_t        side;
    uint8_t       len;
    unsigned char s[RZ_LEGO_SPAN];
    uint16_t      heard;             /* texts it sounded in */
    uint16_t      held;              /* texts where it held against the blind */
    float         role;              /* where it held: share of its uses next to a type */
    char          where[24];         /* empty: general; else the text it is special to */
} rz_note;

#define RZ_MELODY 512

/* Weave on from bindings already known instead of from a seed. `out` starts
   with the known ones that occur in x (round 0) and grows. */
uint32_t rz_lexicon_weave_from(rz_lexicon *x, const rz_binding *known, uint32_t nknown,
                               rz_binding *out, uint32_t cap, uint8_t *is_type);
/* notes -> bindings of x (absent words are skipped); note_of[k], when given,
   is the note binding k came from */
uint32_t rz_melody_carry(const rz_lexicon *x, const rz_note *notes, uint32_t n,
                         rz_binding *out, uint32_t cap, uint32_t *note_of);

/* Why it worked in one text and not in another (P40). Every note is judged in
   the text being read: does it, on its own, tell which words take an owner in
   the half not read, better than a blind word of its frequency on its side?
   And what work does it do here: the share of its uses that stand next to a
   type. A note that held elsewhere and fails here is explained by the
   difference: it did not sound, it does other work here, or it does the same
   work and still tells nothing. */
typedef struct {
    uint8_t sounds, holds;
    double  info, blind;             /* what it alone tells, and a blind word */
    double  role;                    /* share of its uses next to a type, here */
} rz_note_verdict;

typedef enum {
    RZ_WHY_HOLDS = 0,
    RZ_WHY_SILENT,                   /* it did not sound here */
    RZ_WHY_OTHER_WORK,               /* it sounds, but next to other things */
    RZ_WHY_SAYS_NOTHING              /* same work, and still tells nothing here */
} rz_why;

void   rz_melody_judge(rz_lexicon *x, const rz_binding *b, uint32_t nb,
                       const int32_t *later, uint32_t nlater, rz_note_verdict *v);
/* the conclusion for one note, from its history and today's verdict */
rz_why rz_melody_why(const rz_note *note, const rz_note_verdict *v);
/* fold today's verdict into the note's history: the melody rewriting itself */
void   rz_melody_remember(rz_note *note, const rz_note_verdict *v);
/* whether the note is trusted to weave with: it has held at least as often as
   it failed (a note with no history is trusted) */
int    rz_melody_trusted(const rz_note *note);
/* bindings of x -> notes, appended when new; returns the new count. `where`
   marks them as special to one text (NULL: general). */
uint32_t rz_melody_learn(const rz_lexicon *x, const rz_binding *b, uint32_t nb,
                         rz_note *notes, uint32_t n, uint32_t cap, const char *where);

/* A specialised key (P41). The text is first read with fresh eyes -- woven
   from a seed, alone, with no melody -- and then compared with what the melody
   carried in says of it. Where they coincide, the melody already knew; what the
   fresh reading found and the melody did not is the difference, and it becomes
   a key special to this text. Returns the number of fresh bindings that are new
   (written to diff); *agree gets how many coincide. */
uint32_t rz_melody_compare(const rz_binding *fresh, uint32_t nf,
                           const rz_binding *carried, uint32_t nc,
                           rz_binding *diff, uint32_t cap, uint32_t *agree);
int      rz_melody_save(const rz_note *notes, uint32_t n, const char *path);
int      rz_melody_load(rz_note *notes, uint32_t cap, uint32_t *n, const char *path);

/* --- the instruments of a word (P43) --------------------------------------
 *
 * "es" is not one instrument: it defines ("Jesús es el…"), points ("Este es
 * mi…"), restates (", es decir"), compares ("que es más…"). The song is one --
 * the whole text -- and the instruments are heard in the mix: every time the
 * word sounds, what sounds just before and just after it is its timbre (the
 * company of those neighbours). The mix is split in two where the timbres part,
 * and split again, only while it pays: on the half of the text it did not split
 * on, knowing the instrument must tell more about the neighbours than a split
 * of the same sizes drawn at random -- every one of eight. A new occurrence
 * goes to the instrument it resonates with most. Read-only. */
#define RZ_INSTRUMENTS 16

typedef struct {
    uint32_t uses;                   /* occurrences that resonate with it */
    int32_t  before[6], after[6];    /* what sounds around it more than around the word */
    uint32_t example;                /* a position in x->seq where it plays */
} rz_instrument;

/* *which, when given, gets the instrument of every occurrence, in order */
uint32_t rz_word_instruments(rz_lexicon *x, int32_t word, rz_instrument *out, uint32_t cap,
                             uint8_t *which, uint32_t which_cap);

/* --- the same song in another key (P44) -----------------------------------
 *
 * "hijo" and "hija", "este" and "esta", "padre" and "madre" share almost every
 * letter and keep a similar structure (the author). Twins are words of the same
 * length that differ in one letter at one place and keep similar company (the
 * tall side of the split of their company similarity: "toma/tomo" share letters
 * and not structure). The difference that repeats across the most twins is a
 * PROPERTY -- found, not named; on the NT it is the final a/o. Each twin falls
 * on one side of it. Then the words before them: a word that stands before one
 * side only marks that side ("el", "la"); one that stands before both is for
 * both ("su", "mi") -- three groups, cut by the exact optimum. And two markers
 * of opposite sides that are followed by the same words once each twin is read
 * as its partner are one note in two keys: "el" and "la". */
typedef struct {
    int8_t   from_end;               /* where the letter differs, counted from the end */
    uint8_t  a, b;                   /* the two letters: side 0 has a, side 1 has b */
    uint32_t twins;                  /* how many twins carry it */
} rz_property;

typedef struct {
    rz_property property;
    int8_t  *side;                   /* x->n: -1, or the side of a twin */
    int32_t *twin;                   /* x->n: the partner across the property, or -1 */
    int8_t  *marks;                  /* x->n: -1, 0 or 1 (marks a side), 2 (both) */
    int32_t *key;                    /* x->n: a marker's partner in the other key, or -1 */
} rz_keys;

int  rz_lexicon_keys(rz_lexicon *x, rz_keys *k);
void rz_keys_free(rz_keys *k);
/* the same note: equal, twins, or markers that are each other's key */
int  rz_keys_same(const rz_keys *k, int32_t a, int32_t b);

/* --- PENZAR: thinking in real time (P45) ----------------------------------
 *
 * The author's word: not a study run once over a finished book, but thinking
 * while reading. Bite after bite:
 *
 *   1. before the bite is read, every hunch bets on it -- "after `la` comes a
 *      word on the a side" -- and is judged against the base rate of that same
 *      bite: the bar is always text it had not yet seen
 *   2. the bite is read
 *   3. it thinks: the markers of the key it now hears become hunches
 *   4. lateral thinking: a hunch that holds lends itself to the words of its
 *      silhouette, which become hunches of their own, to be judged on the
 *      next bite like any other
 *
 * Nothing is deleted; a hunch that fails stays, and simply is not trusted. */
typedef struct {
    int32_t  word;                   /* the marker it bets on */
    int8_t   side;                   /* the side of the key it bets follows */
    int32_t  from;                   /* the hunch it was borrowed from, or -1 */
    uint32_t born;                   /* the bite it was born in */
    uint32_t tried, right;           /* its bets, and the ones it won */
    uint32_t base_tried, base_right; /* the same bites: how often that side follows anything */
    uint8_t  lent;                   /* it has already lent itself sideways */
} rz_hunch;

#define RZ_HUNCHES 1024

typedef struct {
    rz_mind   *mind;
    rz_lexicon lex;
    rz_keys    keys;
    int        have_keys;
    rz_hunch   hunch[RZ_HUNCHES];
    uint32_t   nhunch;
    uint32_t   bites;
} rz_penzar;

typedef struct {
    uint32_t born, lateral;          /* hunches born in this bite, and how many sideways */
    uint32_t bets, won, base_won;    /* this bite's bets */
} rz_thought;

void rz_penzar_init(rz_penzar *p);
void rz_penzar_free(rz_penzar *p);
/* one bite: bet, read, think */
void rz_penzar_bite(rz_penzar *p, const unsigned char *t, size_t n, rz_thought *out);
/* a hunch holds when it has bet at least twice and won more often than its
   side follows anything */
int  rz_hunch_holds(const rz_hunch *h);

/* --- the family of two in the same chair (P46) ----------------------------
 *
 * Sitting in the same chair says nothing of what two words are (riddle 3: "yet"
 * and "is" share their followers). The author: what family are dog and cat?
 * They ARE animals, they ARE four-legged; their shapes, sizes and breeds
 * differ; and their names may be the same. So two in the same chair are asked
 * what they are, with the relation's silhouette:
 *
 *   family      what both are -- the answers they share ("animal") and the
 *               rest of the chair does not ("here" is said of the dog, the
 *               cat and the table: that is the chair)
 *   apart       what only one is ("ladra" / "maúlla" -- told apart)
 *   names       who is said to be each (answers from behind: "rex es un
 *               perro"); a name shared by both makes no family, it is an
 *               identity, not a kind (P37)
 *
 * Siblings share a family; the same note in another key is P44; and two that
 * share nothing but the chair are only neighbours. */
typedef enum {
    RZ_KIN_CHAIR = 0,                /* only the chair: nothing they are is shared */
    RZ_KIN_SIBLINGS,                 /* a family in common, and differences */
    RZ_KIN_KEY                       /* one note in two keys (P44) */
} rz_kin;

#define RZ_FAMILY 16

typedef struct {
    rz_kin   kin;
    int32_t  family[RZ_FAMILY];      uint32_t nfamily;
    int32_t  only_a[RZ_FAMILY];      uint32_t nonly_a;
    int32_t  only_b[RZ_FAMILY];      uint32_t nonly_b;
    int32_t  names[RZ_FAMILY];       uint32_t nnames;    /* said to be both */
} rz_family;

void rz_lexicon_family(rz_lexicon *x, int32_t a, int32_t b,
                       const int32_t *relations, uint32_t nrel, rz_family *out);

/* --- the map of associations (P47) ----------------------------------------
 *
 * Associative, not statistical (the author): understanding is chaining what
 * the text says, not grouping what looks alike on average. Every statement
 * becomes a named link:
 *
 *   "el perro es un animal"    perro --es--> animal
 *   "la mesa es de madera"     mesa  --es--> madera   (de is a marker too)
 *   "el animal vive"           animal --vive          (an act: no object)
 *
 * A thing is told by the markers Razar found itself (P44): a word that a
 * marker stands before. Nothing is counted into a distribution and nothing is
 * clustered: a link exists because the text said it.
 *
 * Thinking is walking. From "perro", follow the links of the question taught
 * ("es", imitation) as far as they go: perro -> animal. Everything said of
 * what it reaches is inherited -- the dog lives, because the animal lives. The
 * family of two is what both reach. */
typedef struct {
    int32_t  a;                      /* the thing the statement is about */
    int32_t  r;                      /* the relation: the word between */
    int32_t  b;                      /* what it says it is, or -1 for an act */
    uint32_t said;                   /* how many times the text said it */
    uint16_t round;                  /* 0: said; else the domino round it fell in (P48) */
} rz_link;

typedef struct {
    rz_link *link;
    uint32_t n, cap;
} rz_assoc;

typedef struct {
    int32_t r, b;                    /* what is said: relation and object (-1: an act) */
    int32_t via;                     /* whom it was said of: the word itself or an ancestor */
} rz_trait;

uint32_t rz_assoc_build(rz_lexicon *x, const rz_keys *k, rz_assoc *m);
void     rz_assoc_free(rz_assoc *m);
/* what it is, as far as the links go: every ancestor reached through the
   relations given, at any depth (cycles are walked once) */
uint32_t rz_assoc_reach(const rz_assoc *m, int32_t start, const int32_t *rel, uint32_t nrel,
                        int32_t *out, uint32_t cap);
/* everything said of it and of what it reaches -- inherited */
uint32_t rz_assoc_traits(const rz_assoc *m, int32_t start, const int32_t *rel, uint32_t nrel,
                         rz_trait *out, uint32_t cap);

/* --- the melody of the path (P48) -----------------------------------------
 *
 * Not every construction stored, but the shape of the walk (the author). Two
 * steps, a --r1--> m --r2--> c, are a tune (r1, r2). When the text also says
 * the shortcut, a --r3--> c, the tune is heard to lead to r3. A tune heard so
 * twice is learned -- a tool for thinking: "Cristo es Dios", "Dios es amor",
 * and somewhere "X es Z" said straight out.
 *
 * Learned tunes then write what nobody wrote: every path with that shape and
 * no shortcut gets one, marked with the round it fell in. The new links make
 * new paths, and the next round falls from them -- dominoes, until nothing new
 * falls. Meanings of meanings. */
typedef struct {
    int32_t  r1, r2, r3;
    uint32_t paths, heard;           /* walks of this shape, and those the text shortcut */
} rz_tune;

/* learned tunes, most heard first */
uint32_t rz_assoc_tunes(const rz_assoc *m, rz_tune *out, uint32_t cap);
/* let the dominoes fall; returns how many links fell, *rounds how many rounds */
uint32_t rz_assoc_dominoes(rz_assoc *m, const rz_tune *t, uint32_t nt, uint32_t max_new,
                           uint32_t *rounds);

/* --- asking every word (P50) ----------------------------------------------
 *
 * The child does not choose what to ask about (the author): it asks about
 * every word, "el" as much as "perro", and learns the differences, how they
 * join, and how they are used together. Riddle 1 had searched for the word
 * worth asking; there is none to choose.
 *
 * Every word gets the same questions, answered by what the map says and by
 * the company it keeps -- each answer said twice, never once (an anecdote):
 *
 *   of        what is said of it        (it is the thing spoken of)
 *   about     what it is said to be     (it is what is said of another)
 *   relates   what it joins             (it is the relation)
 *   before    words it stands before
 *   after     words it stands after
 *
 * The shape -- which questions got an answer -- is the kind of the word. "el"
 * gets only company; "perro" is spoken of; "es" relates. Nothing names the
 * kinds: the shapes are what the answers happened to be.
 *
 * Used together: a pair of shapes that stand side by side, heard across
 * several different pairs of words, so that it is the shapes and not one
 * pair that is heard. One piece: two words where one brings nothing to the
 * map that the other lacks -- "el perro" says what "perro" says. */
typedef struct {
    uint32_t of, about, relates;     /* map answers, each link said twice */
    uint32_t before, after;          /* company: neighbours met twice */
    uint8_t  shape;                  /* bit per question that got an answer */
} rz_answers;

typedef struct {
    uint8_t  s1, s2;                 /* the shape before, the shape after */
    uint32_t heard, pairs;           /* times side by side; distinct word pairs */
} rz_together;

/* every word, asked; out holds x->n answers. Returns the words asked. */
uint32_t rz_ask_every_word(const rz_lexicon *x, const rz_assoc *m, rz_answers *out);
/* pairs of shapes used together, heard across two or more word pairs, most first */
uint32_t rz_used_together(const rz_lexicon *x, const rz_answers *ans, rz_together *out, uint32_t cap);
/* one brings nothing to the map that the other lacks */
int      rz_one_piece(const rz_answers *a, const rz_answers *b);
/* --- the silhouette of a path (P51) ---------------------------------------
 *
 * The author: in the concept map "el", "perro", "es", "un", "animal" are one
 * connected path. The silhouette of what it forms gives weight to every
 * construction like it; it is taken apart and put together again in other
 * examples -- where there is no "animal" but there is "mamífero" -- and so it
 * understands, because it is a connected path whose pieces are not all worth
 * the same: some are glue used in many forms, the others belong to the map.
 *
 * A silhouette is a run of 3 to 5 pieces read in a row with two of them left
 * as holes: el _ es un _. Heard with two or more different pairs in its holes
 * it is a construction, and the pairs it was put together with are its
 * weight. Nothing tells a relation from glue: a piece standing fixed in many
 * silhouettes is glue; a piece filling their holes belongs to the map.
 *
 * Mates: two pieces that filled the same hole of the same silhouette beside
 * the same piece in the other hole -- "animal" and "mamífero", each with
 * "perro" in el _ es un _. Put together again: a mate stands where its mate
 * stood, so a path never said is built from one that was.
 *
 * Run length and two holes are the bias of the proposal; the text decides
 * what is heard. */
typedef struct {
    uint8_t  len;                    /* pieces in the run, 3..5 */
    int32_t  word[5];                /* the fixed pieces; -1 at a hole */
    uint32_t fills, said;            /* distinct pairs in its holes; times heard */
    uint32_t first, count;           /* its pairs, in rz_paths.fill */
    uint32_t sticker;                /* the sticker it is a variant of */
} rz_silhouette;

typedef struct { int32_t f1, f2; uint32_t said; } rz_fill;
typedef struct { int32_t a, b; uint32_t frames; } rz_mates;
typedef struct { uint32_t sil; int32_t f1, f2; int32_t from; uint32_t forms; uint8_t hole; } rz_recomposed;

typedef struct {
    rz_silhouette *s;  uint32_t ns;
    rz_fill       *fill; uint32_t nfill;
    rz_mates      *m;  uint32_t nm;  /* frames >= 2 */
    uint32_t      *fixed, *filled;   /* per lego: silhouettes it stands fixed in, fills */
    uint32_t       nwords;
    uint32_t       nsticker;         /* stickers: silhouettes joined by their variants */
} rz_paths;

uint32_t rz_paths_build(const rz_lexicon *x, rz_paths *p);
void     rz_paths_free(rz_paths *p);
/* paths never said, put together from said ones by a mate: from = the piece replaced,
   hole = which hole the mate went into, forms = the forms the two mates collected */
uint32_t rz_paths_recompose(const rz_paths *p, rz_recomposed *out, uint32_t cap);

/* --- weaving meaning while reading (P52) ----------------------------------
 *
 * The author: the meaning of what is not structure is found by association
 * across the parts of the document, WHILE reading it -- not one by one
 * afterwards. As it meets a word it meets its context and its relations, and
 * weaves its meaning; the search, and the heuristic a search needs, are saved.
 * Searched afterwards, the order of the search crowned glue.
 *
 * One pass, left to right. A silhouette is born when it is put together with a
 * second different pair (the anecdote rule). A word that fills a hole of a
 * born one adds that place to its meaning. Two words that filled the same hole
 * beside the same partner, in two different silhouettes, are associates.
 *
 * The bar is live: each time the word just read stands in a place it never
 * stood in, were its associates there before it? Against a word picked blind
 * from what was already read. Nothing in the bar is learned from it. */
typedef struct {
    int32_t  word;                   /* the word just read */
    char     associate[RZ_LEGO_SPAN + 1];  /* the associate already there, spelled:
                                        it may come from another book */
    uint8_t  len;
    int32_t  path[5];                /* the path it stands in, holes filled */
} rz_foreseen;

typedef struct {
    uint32_t    structures, associates;  /* silhouettes born in this read; pairs of associates in memory */
    uint64_t    events, foreseen;        /* new places for the word just read; foreseen */
    uint64_t    blind_events, blind_foreseen;
    rz_foreseen seen[64];                /* the last foreseen, to show (a ring) */
    uint32_t    nseen;
    uint64_t    thoughts_confirmed;      /* thoughts this book said */
    uint64_t    blind_confirmed;         /* blind thoughts this book said */
} rz_loom_report;

/* The loom lives in the AMMC (the author: the conceptual design of meaning is
   built as it is seen, and the connections stay in the memory -- melody and
   meaning, the associations). Words are known by their spelling, so what one
   book wove is there when the next is read; a new book is woven on top. */
typedef struct rz_loom rz_loom;

rz_loom *rz_loom_new(void);
void     rz_loom_free(rz_loom *l);
int      rz_loom_read(rz_loom *l, const rz_lexicon *x, rz_loom_report *out);
int      rz_loom_save(const rz_loom *l, const char *path);
int      rz_loom_load(rz_loom *l, const char *path);

/* --- the neuron that thinks one step at a time (P53) ----------------------
 *
 * The author: multidimensional branches making paths in many directions,
 * like neurons, with the woven structures and the meanings of every word, its
 * use and its variants -- learning and generating on its own; one depth at a
 * time, as if it were thinking; and that depth carries a cache of the path
 * walked: nothing it builds is lost.
 *
 * Every word is a neuron. Its branches: the places it stands in (a structure,
 * beside a partner) and its associates (meaning). One step of thought, from
 * the words the last step reached: their associates stand in places they
 * never stood in, and they could stand there too -- an idea. The words the
 * step reaches are the next step's. Nothing is searched in depth; each step
 * only looks beside it.
 *
 * The cache belongs to each path (the author: a path can vary and find other
 * ways to circulate): a word reached by two paths is two entries, each with
 * its own trail, and thinks again from each; a path only never walks back
 * onto itself. The cache and every idea stay in the memory with the rest of
 * the loom, apart from what was read. Each idea
 * has a blind twin (a word picked at random, in the same place). A book read
 * later that says an idea confirms it: the bar of thinking. */
typedef struct {
    uint8_t  level;                  /* the step of thought it came from */
    char     word[RZ_LEGO_SPAN + 1]; /* who could stand there */
    char     via[RZ_LEGO_SPAN + 1];  /* the associate that stood there */
    char     text[6 * (RZ_LEGO_SPAN + 1)];   /* the path thought */
    char     trail[4 * (RZ_LEGO_SPAN + 1)];  /* the path walked to it, from the start */
} rz_idea;

/* begin a train of thought at a word (the cache starts over) */
int      rz_loom_think_from(rz_loom *l, const char *word);
/* one depth further, from the cache. Returns the ideas reported (up to cap);
   *made, every idea new to the memory. */
uint32_t rz_loom_step(rz_loom *l, rz_idea *out, uint32_t cap, uint32_t *made);
/* how many times lego a stood just before lego b (0: never) */
uint32_t rz_lexicon_pair_count(const rz_lexicon *x, int32_t a, int32_t b);

int     rz_lexicon_save(const rz_lexicon *x, const char *path);
int     rz_lexicon_load(rz_lexicon *x, const char *path);

#endif
