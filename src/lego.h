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

int     rz_lexicon_save(const rz_lexicon *x, const char *path);
int     rz_lexicon_load(rz_lexicon *x, const char *path);

#endif
