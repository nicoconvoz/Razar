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
    uint64_t blind;                  /* nonzero: the bar -- chooses its way of
                                        asking at random, from this seed */
} rz_player;

typedef struct {
    int32_t  relation, subject, answer;
    double   harmony, transfer, pay;
    uint8_t  new_form;               /* it tried a way of asking for the first time */
} rz_act;

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

int     rz_lexicon_save(const rz_lexicon *x, const char *path);
int     rz_lexicon_load(rz_lexicon *x, const char *path);

#endif
