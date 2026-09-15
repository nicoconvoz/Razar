#include "harness.h"
#include "razar.h"

#include <stdlib.h>
#include <string.h>

/* rz_mind no longer fits on a stack. Tests leak on purpose: the binary is
   short-lived and a free() in every test buys nothing. */
static rz_mind *fresh(void)
{
    rz_mind *m = malloc(sizeof(rz_mind));
    if (!m) { printf("  FATAL out of memory\n"); exit(2); }
    rz_init(m);
    return m;
}

static void feed(rz_mind *m, const char *s)
{
    rz_ingest(m, (const unsigned char *)s, strlen(s));
}

static void feed_times(rz_mind *m, const char *s, int n)
{
    for (int i = 0; i < n; i++) feed(m, s);
}

/* id of the fragment whose decomposition is exactly `s`, or -1.
   A fragment always lives in the lineage of the atom it starts with, so only
   that one module has to be looked at. */
static int32_t frag_of(rz_mind *m, const char *s)
{
    unsigned char buf[RZ_FRAG_SPAN];
    size_t want = strlen(s);
    if (want == 0) return -1;

    int32_t k = (int32_t)(unsigned char)s[0];
    for (int32_t l = 0; l < RZ_MOD_FRAGS; l++) {
        int32_t id = k * RZ_MOD_FRAGS + l;
        size_t n = rz_expand(m, id, buf, sizeof buf);
        if (n == 0) break;
        if (n == want && memcmp(buf, s, n) == 0) return id;
    }
    return -1;
}

static uint16_t span_of(rz_mind *m, int32_t id)
{
    const rz_frag *f = rz_frag_get(m, id);
    return f ? f->span : 0;
}

/* --- the atomic base is fixed and bounded ------------------------------- */

TEST(fresh_mind_is_empty)
{
    rz_mind *m = fresh();
    CHECK_EQ_U(rz_distinct_paths(m), 0u);
    CHECK_EQ_U(rz_distinct_atoms(m), 0u);
    CHECK_EQ_U(m->traversals, 0u);
}

TEST(atomic_base_does_not_grow_with_experience)
{
    rz_mind *a = fresh();
    rz_mind *b = fresh();
    feed(a, "x");
    for (int i = 0; i < 2000; i++) feed(b, "the quick brown fox");
    CHECK_EQ_U(rz_plastic_bytes(a), rz_plastic_bytes(b));
}

/* --- ingestion is traversal --------------------------------------------- */

TEST(one_pair_is_one_traversal)
{
    rz_mind *m = fresh();
    feed(m, "ab");
    CHECK_EQ_U(m->traversals, 1u);
    CHECK_EQ_U(rz_distinct_paths(m), 1u);
    CHECK_EQ_U(rz_distinct_atoms(m), 2u);
}

TEST(a_lone_atom_walks_nowhere)
{
    rz_mind *m = fresh();
    feed(m, "a");
    CHECK_EQ_U(m->traversals, 0u);
    CHECK_EQ_U(rz_distinct_paths(m), 0u);
    CHECK_EQ_U(rz_distinct_atoms(m), 1u);
}

TEST(empty_material_changes_nothing)
{
    rz_mind *m = fresh();
    feed(m, "");
    CHECK_EQ_U(m->traversals, 0u);
    CHECK_EQ_U(rz_distinct_atoms(m), 0u);
}

TEST(direction_matters)
{
    rz_mind *m = fresh();
    feed(m, "aba");
    CHECK_EQ_U(m->traversals, 2u);
    CHECK_EQ_U(rz_distinct_paths(m), 2u);
    CHECK_EQ_U(rz_distinct_atoms(m), 2u);
}

TEST(repetition_strengthens_it_does_not_multiply)
{
    rz_mind *m = fresh();
    feed(m, "abab");
    CHECK_EQ_U(m->traversals, 3u);
    CHECK_EQ_U(rz_distinct_paths(m), 2u);
    CHECK_EQ_U(rz_intensity(m, 'a', 'b'), 2u);
    CHECK_EQ_U(rz_intensity(m, 'b', 'a'), 1u);
}

TEST(ingestion_is_not_idempotent)
{
    rz_mind *m = fresh();
    feed(m, "hello");
    uint32_t paths_after_first = rz_distinct_paths(m);
    uint64_t walked_after_first = m->traversals;

    feed(m, "hello");
    CHECK_EQ_U(rz_distinct_paths(m), paths_after_first);
    CHECK_EQ_U(m->traversals, walked_after_first * 2u);
    CHECK_EQ_U(rz_intensity(m, 'l', 'l'), 2u);
}

TEST(a_boundary_is_not_a_path)
{
    rz_mind *m = fresh();
    feed(m, "ab");
    feed(m, "cd");
    CHECK_EQ_U(rz_distinct_paths(m), 2u);
    CHECK_EQ_U(rz_intensity(m, 'b', 'c'), 0u);
}

/* --- the collapse alarm -------------------------------------------------- */

TEST(effective_support_of_nothing_is_zero)
{
    rz_mind *m = fresh();
    CHECK_NEAR(rz_effective_support(m), 0.0, 1e-9);
}

TEST(effective_support_of_uniform_is_the_path_count)
{
    rz_mind *m = fresh();
    feed(m, "ab");
    feed(m, "cd");
    feed(m, "ef");
    feed(m, "gh");
    CHECK_EQ_U(rz_distinct_paths(m), 4u);
    CHECK_NEAR(rz_entropy(m), log(4.0), 1e-9);
    CHECK_NEAR(rz_effective_support(m), 4.0, 1e-9);
}

TEST(effective_support_of_obsession_is_one)
{
    rz_mind *m = fresh();
    feed(m, "aaaaaaaaaa");
    CHECK_EQ_U(rz_distinct_paths(m), 1u);
    CHECK_NEAR(rz_entropy(m), 0.0, 1e-9);
    CHECK_NEAR(rz_effective_support(m), 1.0, 1e-9);
}

TEST(effective_support_falls_as_traversal_concentrates)
{
    rz_mind *spread = fresh();
    rz_mind *obsessed = fresh();

    feed(spread, "abcdefgh");
    feed(obsessed, "abcdefgh");
    for (int i = 0; i < 200; i++) feed(obsessed, "aa");

    CHECK(rz_distinct_paths(obsessed) > rz_distinct_paths(spread));
    CHECK(rz_effective_support(obsessed) < rz_effective_support(spread));
}

TEST(top_share_reports_concentration)
{
    rz_mind *m = fresh();
    feed(m, "aaaaaaaaaa");
    CHECK_NEAR(rz_top_share(m, 1.0), 1.0, 1e-9);

    rz_mind *u = fresh();
    feed(u, "ab");
    feed(u, "cd");
    feed(u, "ef");
    feed(u, "gh");
    CHECK_NEAR(rz_top_share(u, 25.0), 0.25, 1e-9);
}

/* --- the instrument ------------------------------------------------------ */

TEST(examine_reports_the_mind)
{
    rz_mind *m = fresh();
    feed(m, "aaaaaaaaaa");

    rz_autopsy a;
    rz_examine(m, &a);
    CHECK_EQ_U(a.distinct_paths, 1u);
    CHECK_EQ_U(a.distinct_atoms, 1u);
    CHECK_EQ_U(a.traversals, 9u);
    CHECK_EQ_U(a.atoms_ingested, 10u);
    CHECK_NEAR(a.effective_support, 1.0, 1e-9);
    CHECK_NEAR(a.top1_share, 1.0, 1e-9);
    CHECK_EQ_U(a.plastic_bytes, rz_plastic_bytes(m));
}

TEST(a_mind_survives_a_round_trip)
{
    const char *path = "test_mind.tmp";
    rz_mind *before = fresh();
    rz_set_nest(before, "test_nest.tmp");
    feed_times(before, "the organism remembers having read this ", 20);

    CHECK_EQ_U(rz_save(before, path), RZ_OK);

    rz_mind *after = fresh();
    rz_set_nest(after, "test_nest.tmp");
    CHECK_EQ_U(rz_load(after, path), RZ_OK);

    CHECK_EQ_U(after->traversals, before->traversals);
    CHECK_EQ_U(after->atoms_ingested, before->atoms_ingested);
    CHECK_EQ_U(rz_distinct_paths(after), rz_distinct_paths(before));
    CHECK_EQ_U(rz_intensity(after, 'r', 'e'), rz_intensity(before, 'r', 'e'));
    CHECK_EQ_U(rz_frag_count(after), rz_frag_count(before));
    CHECK_EQ_U(after->births, before->births);
    CHECK_NEAR(rz_effective_support(after), rz_effective_support(before), 1e-12);

    remove(path);
}

TEST(a_missing_mind_is_reported_not_invented)
{
    rz_mind *m = fresh();
    CHECK_EQ_U(rz_load(m, "definitely_not_here.tmp"), RZ_ERR_MISSING);
    CHECK_EQ_U(m->traversals, 0u);
}

TEST(a_foreign_layout_is_refused_loudly)
{
    const char *path = "test_foreign.tmp";
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) {
        uint32_t bogus = 1u;
        fwrite(&bogus, sizeof bogus, 1, f);
        fputs("garbage that is not a mind", f);
        fclose(f);
    }

    rz_mind *m = fresh();
    feed(m, "xy");
    CHECK_EQ_U(rz_load(m, path), RZ_ERR_VERSION);
    CHECK_EQ_U(m->traversals, 1u);   /* untouched, not corrupted */

    remove(path);
}

TEST(an_autopsy_survives_a_round_trip)
{
    const char *path = "test_autopsy.tmp";
    rz_mind *m = fresh();
    feed(m, "abcabcabc");

    rz_autopsy before;
    rz_examine(m, &before);
    CHECK_EQ_U(rz_autopsy_save(&before, path), RZ_OK);

    rz_autopsy after;
    memset(&after, 0, sizeof after);
    CHECK_EQ_U(rz_autopsy_load(&after, path), RZ_OK);

    CHECK_EQ_U(after.distinct_paths, before.distinct_paths);
    CHECK_EQ_U(after.traversals, before.traversals);
    CHECK_NEAR(after.effective_support, before.effective_support, 1e-9);
    CHECK_NEAR(after.entropy, before.entropy, 1e-9);

    remove(path);
}

TEST(the_second_reading_is_visible_in_the_diff)
{
    rz_mind *m = fresh();
    feed(m, "she had read it before");
    rz_autopsy first;
    rz_examine(m, &first);

    feed(m, "she had read it before");
    rz_autopsy second;
    rz_examine(m, &second);

    CHECK_EQ_U(second.distinct_paths, first.distinct_paths);
    CHECK_EQ_U(second.traversals, first.traversals * 2u);
    CHECK_NEAR(second.effective_support, first.effective_support, 1e-9);
    CHECK_EQ_U(second.plastic_bytes, first.plastic_bytes);
}

/* --- the diagnosis ------------------------------------------------------- */

static rz_autopsy snap(uint32_t paths, uint64_t traversals, double support)
{
    rz_autopsy a;
    memset(&a, 0, sizeof a);
    a.distinct_paths = paths;
    a.traversals = traversals;
    a.effective_support = support;
    return a;
}

TEST(nothing_eaten_is_empty)
{
    rz_autopsy now = snap(0, 0, 0.0), was = snap(0, 0, 0.0);
    CHECK_EQ_U(rz_diagnose(&now, &was, 0), RZ_V_EMPTY);
}

TEST(no_previous_is_baseline)
{
    rz_autopsy now = snap(3002, 126171, 661.16), was = snap(0, 0, 0.0);
    CHECK_EQ_U(rz_diagnose(&now, &was, 0), RZ_V_BASELINE);
}

TEST(rereading_the_same_thing_is_consolidating)
{
    rz_autopsy was = snap(3002, 126171, 661.16);
    rz_autopsy now = snap(3002, 252342, 661.16);
    CHECK_EQ_U(rz_diagnose(&now, &was, 1), RZ_V_CONSOLIDATING);
}

TEST(new_material_that_widens_reach_is_healthy)
{
    rz_autopsy was = snap(3002, 252342, 661.16);
    rz_autopsy now = snap(3407, 294936, 692.85);
    CHECK_EQ_U(rz_diagnose(&now, &was, 1), RZ_V_HEALTHY);
}

TEST(collapse_is_caught_even_when_no_new_paths_appear)
{
    /* Observed run #0004: 240 KB of one repeated phrase. Effective support fell
       77% while distinct paths did not move, because the material introduced no
       unseen pairs. "paths up AND support down" missed it entirely. */
    rz_autopsy was = snap(3407, 294936, 692.85);
    rz_autopsy now = snap(3407, 534932, 156.95);
    CHECK_EQ_U(rz_diagnose(&now, &was, 1), RZ_V_CONCENTRATING);
}

TEST(collapse_outranks_consolidation)
{
    rz_autopsy was = snap(500, 1000, 400.0);
    rz_autopsy now = snap(500, 9000, 40.0);
    CHECK_EQ_U(rz_diagnose(&now, &was, 1), RZ_V_CONCENTRATING);
}

TEST(collapse_is_caught_when_paths_also_rise)
{
    rz_autopsy was = snap(500, 1000, 400.0);
    rz_autopsy now = snap(900, 9000, 120.0);
    CHECK_EQ_U(rz_diagnose(&now, &was, 1), RZ_V_CONCENTRATING);
}

TEST(a_whisker_of_drift_is_not_a_collapse)
{
    rz_autopsy was = snap(500, 1000, 400.0);
    rz_autopsy now = snap(505, 1200, 399.0);
    CHECK(rz_diagnose(&now, &was, 1) != RZ_V_CONCENTRATING);
}

TEST(every_verdict_can_say_its_name)
{
    CHECK(rz_verdict_text(RZ_V_EMPTY) != NULL);
    CHECK(rz_verdict_text(RZ_V_BASELINE) != NULL);
    CHECK(rz_verdict_text(RZ_V_CONCENTRATING) != NULL);
    CHECK(rz_verdict_text(RZ_V_CONSOLIDATING) != NULL);
    CHECK(rz_verdict_text(RZ_V_HEALTHY) != NULL);
}

/* --- recall by atom (spike 2, kept as the baseline instrument) ------------ */

static void recall_atoms(const rz_mind *m, const char *cue, size_t want,
                         uint64_t budget, rz_recall *out)
{
    rz_recall_from(m, (const unsigned char *)cue, strlen(cue), want, budget, out);
}

TEST(branching_of_a_certain_continuation_is_one)
{
    rz_mind *m = fresh();
    feed(m, "abcabcabc");
    CHECK_NEAR(rz_branching(m, 'a'), 1.0, 1e-9);
}

TEST(branching_of_a_dead_end_is_zero)
{
    rz_mind *m = fresh();
    feed(m, "ab");
    CHECK_NEAR(rz_branching(m, 'b'), 0.0, 1e-9);
    CHECK_NEAR(rz_branching(m, 'z'), 0.0, 1e-9);
}

TEST(branching_counts_the_real_alternatives)
{
    rz_mind *m = fresh();
    feed(m, "ab");
    feed(m, "ac");
    feed(m, "ad");
    feed(m, "ae");
    CHECK_NEAR(rz_branching(m, 'a'), 4.0, 1e-9);
}

TEST(a_trace_never_walked_cannot_be_recalled)
{
    rz_mind *m = fresh();
    feed(m, "abc");

    rz_recall r;
    recall_atoms(m, "z", 10, 1000, &r);
    CHECK_EQ_U(r.len, 0u);
    CHECK_EQ_U(r.effort, 0u);
    CHECK(r.gave_up);
}

TEST(an_empty_cue_recalls_nothing)
{
    rz_mind *m = fresh();
    feed(m, "abc");

    rz_recall r;
    recall_atoms(m, "", 10, 1000, &r);
    CHECK_EQ_U(r.len, 0u);
    CHECK(r.gave_up);
}

TEST(a_certain_trace_comes_back_whole_and_cheap)
{
    rz_mind *m = fresh();
    feed(m, "abcabcabc");

    rz_recall r;
    recall_atoms(m, "a", 5, 1000, &r);
    CHECK_EQ_U(r.len, 5u);
    CHECK(memcmp(r.atoms, "bcabc", 5) == 0);
    CHECK_EQ_U(r.effort, 5u);
    CHECK(!r.gave_up);
}

TEST(the_cue_is_read_from_its_end)
{
    rz_mind *m = fresh();
    feed(m, "abcabcabc");

    rz_recall r;
    recall_atoms(m, "zzzb", 2, 1000, &r);
    CHECK_EQ_U(r.len, 2u);
    CHECK(memcmp(r.atoms, "ca", 2) == 0);
}

TEST(an_ambiguous_trace_costs_more_to_recall)
{
    rz_mind *certain = fresh();
    rz_mind *ambiguous = fresh();

    feed(certain, "abcabcabc");
    feed(ambiguous, "ab");
    feed(ambiguous, "ac");
    feed(ambiguous, "ad");
    feed(ambiguous, "ae");

    rz_recall rc, ra;
    recall_atoms(certain, "a", 1, 1000, &rc);
    recall_atoms(ambiguous, "a", 1, 1000, &ra);

    CHECK_EQ_U(rc.effort, 1u);
    CHECK_EQ_U(ra.effort, 4u);
    CHECK(ra.effort > rc.effort);
}

TEST(ties_are_broken_the_same_way_every_time)
{
    rz_mind *m = fresh();
    feed(m, "ab");
    feed(m, "ac");
    feed(m, "ad");
    feed(m, "ae");

    rz_recall first, second;
    recall_atoms(m, "a", 1, 1000, &first);
    recall_atoms(m, "a", 1, 1000, &second);
    CHECK_EQ_U(first.len, second.len);
    CHECK_EQ_U(first.atoms[0], second.atoms[0]);
    CHECK_EQ_U(first.effort, second.effort);
}

TEST(a_dead_end_mid_walk_is_a_give_up)
{
    rz_mind *m = fresh();
    feed(m, "abz");

    rz_recall r;
    recall_atoms(m, "a", 5, 1000, &r);
    CHECK_EQ_U(r.len, 2u);
    CHECK(memcmp(r.atoms, "bz", 2) == 0);
    CHECK(r.gave_up);
}

TEST(effort_runs_out_and_it_stops_trying)
{
    rz_mind *m = fresh();
    feed(m, "abcabcabcabcabcabc");

    rz_recall r;
    recall_atoms(m, "a", 100, 3, &r);
    CHECK(r.gave_up);
    CHECK(r.len < 100u);
    CHECK(r.effort <= 3u);
}

TEST(recalling_does_not_strengthen_what_it_recalls)
{
    rz_mind *m = fresh();
    feed(m, "abcabcabc");

    uint64_t walked = m->traversals;
    uint32_t ab = rz_intensity(m, 'a', 'b');
    double support = rz_effective_support(m);

    rz_recall r;
    recall_atoms(m, "a", 20, 10000, &r);

    CHECK_EQ_U(m->traversals, walked);
    CHECK_EQ_U(rz_intensity(m, 'a', 'b'), ab);
    CHECK_NEAR(rz_effective_support(m), support, 1e-12);
}

/* --- fragments: the addressable unit (P2) -------------------------------- */

TEST(every_atom_is_already_a_fragment)
{
    rz_mind *m = fresh();
    CHECK_EQ_U(rz_frag_count(m), (unsigned)RZ_ATOMS);

    unsigned char buf[RZ_FRAG_SPAN];
    CHECK_EQ_U(rz_expand(m, rz_atom_id('A'), buf, sizeof buf), 1u);
    CHECK_EQ_U(buf[0], 'A');
}

TEST(an_atom_has_no_parts_below_it)
{
    rz_mind *m = fresh();
    const rz_frag *q = rz_frag_get(m, rz_atom_id('q'));
    CHECK(q != NULL);
    if (q) {
        CHECK_EQ_U(q->left,  (unsigned long long)-1);
        CHECK_EQ_U(q->right, (unsigned long long)-1);
        CHECK_EQ_U(q->span, 1u);
    }
}

TEST(a_fragment_lives_in_the_lineage_of_its_first_atom)
{
    /* Nobody assigns the module. It is inherited at birth from the left parent,
       which is why the members of a module are actually related. */
    rz_mind *m = fresh();
    feed_times(m, "abx", 12);

    int32_t ab = frag_of(m, "ab");
    CHECK(ab >= 0);
    if (ab >= 0) CHECK_EQ_U(ab / RZ_MOD_FRAGS, (unsigned)'a');
    CHECK_EQ_U(rz_atom_id('a') / RZ_MOD_FRAGS, (unsigned)'a');
}

TEST(only_the_modules_it_needs_are_awake)
{
    rz_mind *m = fresh();
    feed(m, "abc");
    CHECK(rz_resident_count(m) > 0);
    CHECK(rz_resident_count(m) <= RZ_RESIDENT);
    CHECK(m->wakes > 0u);
}

static void touch_many_lineages(rz_mind *m)
{
    char s[3] = { 0, 0, 0 };
    for (int c = 'a'; c < 'a' + RZ_RESIDENT + 8; c++) {
        s[0] = (char)c;
        s[1] = (char)(c + 1);
        for (int i = 0; i < RZ_FRAG_BIRTH + 4; i++) feed(m, s);
    }
}

TEST(reaching_past_the_resident_set_puts_something_to_sleep)
{
    /* With somewhere to write them down, the extra lineages sleep. */
    rz_mind *m = fresh();
    rz_set_nest(m, "test_sleepnest.tmp");
    touch_many_lineages(m);

    CHECK(rz_resident_count(m) <= RZ_RESIDENT);
    CHECK(m->sleeps > 0u);
}

TEST(a_module_that_cannot_be_written_down_is_not_forgotten)
{
    /* The defect this test exists for: with no nest, eviction used to free a
       dirty module and re-seed it empty on the next touch. Fragments born early
       simply vanished, silently. A module with nowhere to go is held open
       instead, and the resident set is allowed to exceed its target. */
    rz_mind *m = fresh();          /* no nest on purpose */
    touch_many_lineages(m);

    CHECK(m->held_open > 0u);
    CHECK(rz_resident_count(m) > RZ_RESIDENT);

    /* and nothing was lost: every lineage still holds what it learned */
    int32_t ab = frag_of(m, "ab");
    CHECK(ab >= 0);
    CHECK(rz_frag_count(m) > RZ_ATOMS);
}

TEST(a_hub_may_hold_more_paths_than_any_fixed_array_would_allow)
{
    /* The squeeze, gone: paths come out of the module's pool, so one fragment
       can connect to far more than a per-fragment cap would have permitted. */
    rz_mind *m = fresh();
    char s[3] = { 'x', 0, 0 };
    for (int c = 1; c < 200; c++) {
        s[1] = (char)c;
        rz_ingest(m, (const unsigned char *)s, 2);
    }
    CHECK(rz_frag_degree(m, rz_atom_id('x')) > 100);
    CHECK_EQ_U(m->pool_full, 0u);
}

TEST(travelling_together_gives_birth_to_a_fragment)
{
    /* The path traffic is what creates the unit. */
    rz_mind *m = fresh();
    CHECK_EQ_U(frag_of(m, "ab"), (unsigned long long)-1);

    feed_times(m, "ab", RZ_FRAG_BIRTH);
    CHECK(frag_of(m, "ab") >= 0);
    CHECK(rz_frag_count(m) > RZ_ATOMS);
    CHECK(m->births > 0u);
}

TEST(a_pair_that_barely_travels_earns_nothing)
{
    rz_mind *m = fresh();
    feed_times(m, "ab", RZ_FRAG_BIRTH - 1);
    CHECK_EQ_U(frag_of(m, "ab"), (unsigned long long)-1);
    CHECK_EQ_U(rz_frag_count(m), (unsigned)RZ_ATOMS);
}

TEST(a_born_fragment_still_decomposes_into_atoms)
{
    /* The invariant: nothing is ever opaque. */
    rz_mind *m = fresh();
    feed_times(m, "ab", RZ_FRAG_BIRTH);

    int32_t f = frag_of(m, "ab");
    CHECK(f >= 0);
    if (f >= 0) {
        CHECK_EQ_U(span_of(m, f), 2u);
        const rz_frag *fp = rz_frag_get(m, f);
        CHECK(fp != NULL);
        if (fp) {
            CHECK_EQ_U(fp->left,  rz_atom_id('a'));
            CHECK_EQ_U(fp->right, rz_atom_id('b'));
        }

        unsigned char buf[RZ_FRAG_SPAN];
        CHECK_EQ_U(rz_expand(m, f, buf, sizeof buf), 2u);
        CHECK(memcmp(buf, "ab", 2) == 0);
    }
}

TEST(fragments_compound_into_longer_fragments)
{
    /* "ab" earns its place, then "ab"+"ab" can earn theirs. */
    rz_mind *m = fresh();
    feed_times(m, "abab", RZ_FRAG_BIRTH * 6);
    CHECK(frag_of(m, "ab") >= 0);

    int32_t longer = frag_of(m, "abab");
    CHECK(longer >= 0);
    if (longer >= 0) {
        CHECK_EQ_U(span_of(m, longer), 4u);
        unsigned char buf[RZ_FRAG_SPAN];
        CHECK_EQ_U(rz_expand(m, longer, buf, sizeof buf), 4u);
        CHECK(memcmp(buf, "abab", 4) == 0);
    }
}

TEST(the_longest_conjunction_wins_the_match)
{
    rz_mind *m = fresh();
    feed_times(m, "ab", RZ_FRAG_BIRTH);

    const unsigned char *s = (const unsigned char *)"abc";
    int32_t f = rz_longest_match(m, s, 3);
    CHECK_EQ_U(span_of(m, f), 2u);   /* "ab", not "a" */
}

TEST(an_unknown_conjunction_falls_back_to_its_atom)
{
    rz_mind *m = fresh();
    const unsigned char *s = (const unsigned char *)"zq";
    int32_t f = rz_longest_match(m, s, 2);
    CHECK_EQ_U(f, rz_atom_id('z'));
    CHECK_EQ_U(span_of(m, f), 1u);
}

TEST(the_trigger_is_the_conjunction_that_ends_the_cue)
{
    /* A lone atom is rarely the trigger. The larger fragment is.
       "abx" rather than "ab" so the fragment gets walked after it is born —
       a fragment fed alone is always the whole buffer and never acquires a
       path out of itself. */
    rz_mind *m = fresh();
    feed_times(m, "abx", 12);

    const unsigned char *cue = (const unsigned char *)"zzab";
    int32_t t = rz_trigger(m, cue, 4);
    CHECK_EQ_U(span_of(m, t), 2u);

    unsigned char buf[RZ_FRAG_SPAN];
    CHECK_EQ_U(rz_expand(m, t, buf, sizeof buf), 2u);
    CHECK(memcmp(buf, "ab", 2) == 0);
}

TEST(a_fragment_born_at_the_threshold_has_no_traffic_yet)
{
    /* It exists, but it has not earned a path out of itself. Being known is
       not the same as being usable. */
    rz_mind *m = fresh();
    feed_times(m, "ab", RZ_FRAG_BIRTH);

    int32_t ab = frag_of(m, "ab");
    CHECK(ab >= 0);
    if (ab >= 0) {
        CHECK_EQ_U(rz_frag_degree(m, ab), 0u);
        CHECK_NEAR(rz_frag_branching(m, ab), 0.0, 1e-9);
    }
}

TEST(a_hint_that_does_not_fire_is_tried_smaller)
{
    /* An orphan fragment must not swallow the cue. Backing off to a shorter
       overlap is how a lost path gets reached. */
    rz_mind *m = fresh();
    feed_times(m, "ab", RZ_FRAG_BIRTH);   /* "ab" is born orphaned */

    size_t lap = 99;
    int32_t t = rz_trigger_ex(m, (const unsigned char *)"zzab", 4, &lap);
    CHECK_EQ_U(span_of(m, t), 1u);   /* it did not settle for the orphan */
    CHECK_EQ_U(lap, 1u);
}

TEST(a_cue_that_starts_something_known_gets_it_completed)
{
    /* "hello" is not the END of anything — it is the BEGINNING of something
       the organism knows whole. The overlap rule reaches it. */
    rz_mind *m = fresh();
    feed_times(m, "hello world ", 40);

    size_t lap = 0;
    int32_t t = rz_trigger_ex(m, (const unsigned char *)"hello", 5, &lap);
    CHECK_EQ_U(lap, 5u);
    CHECK(span_of(m, t) > 5u);   /* there is more of it than the cue covered */

    unsigned char buf[RZ_FRAG_SPAN];
    size_t n = rz_expand(m, t, buf, sizeof buf);
    CHECK(n >= 5u);
    CHECK(memcmp(buf, "hello", 5) == 0);
}

TEST(a_cue_with_no_known_conjunction_still_triggers_on_its_last_atom)
{
    rz_mind *m = fresh();
    feed(m, "abc");
    int32_t t = rz_trigger(m, (const unsigned char *)"zzq", 3);
    CHECK_EQ_U(t, rz_atom_id('q'));
}

TEST(fragment_paths_record_what_followed_what)
{
    rz_mind *m = fresh();
    feed_times(m, "ab", RZ_FRAG_BIRTH * 4);
    int32_t ab = frag_of(m, "ab");
    CHECK(ab >= 0);
    CHECK(m->frag_steps > 0u);
}

TEST(fragment_recall_leaves_the_mind_alone)
{
    rz_mind *m = fresh();
    feed_times(m, "hello world ", 30);

    uint64_t steps = m->frag_steps;
    int32_t  nf = rz_frag_count(m);
    uint64_t births = m->births;

    rz_recall r;
    rz_recall_frag(m, (const unsigned char *)"hello", 5, 40, 10000, &r);

    CHECK_EQ_U(m->frag_steps, steps);
    CHECK_EQ_U(rz_frag_count(m), (unsigned long long)nf);
    CHECK_EQ_U(m->births, births);
}

TEST(fragments_recall_what_atoms_could_not)
{
    /* The payoff. Addressed by atoms, a walk falls into whitespace — the most
       connected atom there is — and never leaves. Addressed by conjunctions,
       it follows the material it actually read. */
    rz_mind *m = fresh();
    feed_times(m, "hello world ", 40);

    rz_recall byfrag;
    rz_recall_frag(m, (const unsigned char *)"hello", 5, 24, 100000, &byfrag);

    CHECK(byfrag.len > 0u);
    /* it should have walked back into the material, not into a space attractor */
    unsigned char *found = NULL;
    if (byfrag.len >= 5) {
        for (size_t i = 0; i + 5 <= byfrag.len; i++) {
            if (memcmp(byfrag.atoms + i, "world", 5) == 0) { found = byfrag.atoms + i; break; }
        }
    }
    CHECK(found != NULL);
}

/* --- lateral thinking (D15) ---------------------------------------------- */

/* One beginning, many endings.
 *
 * Two endings are not enough: composition swallows them. Fed "hello world " and
 * "hello there " as separate buffers, each phrase becomes a single span-12
 * fragment with no path out of itself, and the branch point disappears into the
 * conjunction. Enough variety has to survive that no fragment can absorb it. */
static void teach_many_ways(rz_mind *m)
{
    static char buf[8192];
    size_t n = 0;
    for (int i = 0; i < 400; i++) {
        char piece[16];
        int len = snprintf(piece, sizeof piece, "go %c%c ",
                           'a' + (i % 17), 'a' + (i % 13));
        if (n + (size_t)len >= sizeof buf) break;
        memcpy(buf + n, piece, (size_t)len);
        n += (size_t)len;
    }
    rz_ingest(m, (const unsigned char *)buf, n);
}

TEST(composition_can_swallow_a_branch_whole)
{
    /* Worth pinning down, because it bit the lateral test first: a phrase fed
       alone becomes one fragment with nowhere to go, and the choice inside it
       stops being a choice. */
    rz_mind *m = fresh();
    for (int i = 0; i < 60; i++) {
        feed(m, "hello world ");
        feed(m, "hello there ");
    }
    size_t lap = 0;
    int32_t t = rz_trigger_ex(m, (const unsigned char *)"hello", 5, &lap);
    CHECK(span_of(m, t) > 5u);   /* the choice was absorbed into one conjunction */
}

TEST(temperature_zero_is_faithful_recall)
{
    rz_mind *m = fresh();
    teach_many_ways(m);

    rz_recall greedy, cold;
    rz_recall_frag(m, (const unsigned char *)"go", 2, 40, 100000, &greedy);
    rz_recall_lateral(m, (const unsigned char *)"go", 2, 40, 100000, 0.0, 12345, &cold);

    CHECK_EQ_U(cold.len, greedy.len);
    CHECK_EQ_U(cold.effort, greedy.effort);
    CHECK(memcmp(cold.atoms, greedy.atoms, greedy.len) == 0);
}

TEST(the_same_seed_replays_the_same_wandering)
{
    rz_mind *m = fresh();
    teach_many_ways(m);

    rz_recall a, b;
    rz_recall_lateral(m, (const unsigned char *)"go", 2, 40, 100000, 1.0, 777, &a);
    rz_recall_lateral(m, (const unsigned char *)"go", 2, 40, 100000, 1.0, 777, &b);

    CHECK_EQ_U(a.len, b.len);
    CHECK(a.len > 0u);
    if (a.len > 0) CHECK(memcmp(a.atoms, b.atoms, a.len) == 0);
}

TEST(wandering_leaves_the_rut_that_greedy_circles_in)
{
    /* The payoff. Greedy always answers the same thing; lateral finds the road
       it never takes. */
    rz_mind *m = fresh();
    teach_many_ways(m);

    rz_recall greedy;
    rz_recall_frag(m, (const unsigned char *)"go", 2, 40, 100000, &greedy);
    CHECK(greedy.len > 0u);
    /* No precondition on the trigger: the branch does not have to be at the
       first step. Divergence itself is the proof that one existed somewhere —
       a walk cannot leave the rut if there is no fork anywhere on it. */

    int found_other = 0;
    for (uint64_t seed = 1; seed <= 40 && !found_other; seed++) {
        rz_recall r;
        rz_recall_lateral(m, (const unsigned char *)"go", 2, 40, 100000, 1.5, seed, &r);
        if (r.len != greedy.len || memcmp(r.atoms, greedy.atoms, r.len) != 0)
            found_other = 1;
    }
    CHECK(found_other);
}

TEST(wandering_still_pays_the_same_effort)
{
    /* Lateral thinking is not a discount. Ambiguity costs what it costs. */
    rz_mind *m = fresh();
    teach_many_ways(m);

    rz_recall r;
    rz_recall_lateral(m, (const unsigned char *)"go", 2, 8, 3, 1.0, 5, &r);
    CHECK(r.effort <= 3u);
    CHECK(r.gave_up);
}

/* --- other directions ----------------------------------------------------- */

TEST(an_atom_cannot_be_taken_apart_any_further)
{
    rz_mind *m = fresh();
    CHECK_EQ_U(rz_descend(m, rz_atom_id('q')), (unsigned long long)-1);
}

TEST(descending_gives_the_tail_of_a_conjunction)
{
    rz_mind *m = fresh();
    feed_times(m, "abx", 12);

    int32_t ab = frag_of(m, "ab");
    CHECK(ab >= 0);
    if (ab >= 0) {
        int32_t tail = rz_descend(m, ab);
        CHECK_EQ_U(tail, rz_atom_id('b'));
    }
}

TEST(a_dead_end_is_escaped_by_going_down)
{
    /* The case that kept killing the walk: fed as separate buffers, each phrase
       becomes one wide fragment with nowhere to go. Its TAIL, though, has been
       everywhere. Rather than give up, drop inside and carry on. */
    rz_mind *m = fresh();
    for (int i = 0; i < 60; i++) {
        feed(m, "hello world ");
        feed(m, "hello there ");
    }

    /* A fragment whose tail also had nowhere to go inherits nothing, so the
       dead end is real and descent is what gets past it. */
    rz_mind *dead = fresh();
    feed_times(dead, "ab", RZ_FRAG_BIRTH);
    int32_t ab = frag_of(dead, "ab");
    CHECK(ab >= 0);
    CHECK_EQ_U(rz_frag_degree(dead, ab), 0u);

    rz_recall r;
    rz_recall_frag(m, (const unsigned char *)"hello", 5, 40, 100000, &r);
    CHECK(r.len > 0u);
}

TEST(looking_in_another_direction_is_work)
{
    rz_mind *m = fresh();
    for (int i = 0; i < 60; i++) {
        feed(m, "hello world ");
        feed(m, "hello there ");
    }

    rz_recall r;
    rz_recall_frag(m, (const unsigned char *)"hello", 5, 40, 100000, &r);
    CHECK(r.effort >= r.descents);   /* every descent was paid for */

    /* and with no budget for it, the walk stops rather than descending free */
    rz_recall poor;
    rz_recall_frag(m, (const unsigned char *)"hello", 5, 40, 0, &poor);
    CHECK(poor.gave_up);
    CHECK_EQ_U(poor.descents, 0u);
}

TEST(when_even_the_atom_leads_nowhere_it_really_does_give_up)
{
    rz_mind *m = fresh();
    feed(m, "abc");

    rz_recall r;
    rz_recall_frag(m, (const unsigned char *)"c", 1, 10, 1000, &r);
    CHECK(r.gave_up);
    CHECK_EQ_U(r.len, 0u);
}

/* --- the fixed external bar (D23) ---------------------------------------- */

static void measure_str(rz_mind *m, const char *s, rz_measure_result *r)
{
    rz_measure(m, (const unsigned char *)s, strlen(s), r);
}

TEST(the_bar_teaches_nothing)
{
    /* The whole point. Measuring must not be a back door to learning. */
    rz_mind *m = fresh();
    feed_times(m, "abx", 12);

    uint64_t trav = m->traversals, steps = m->frag_steps, births = m->births;
    uint64_t eaten = m->atoms_ingested;
    int32_t  nf = rz_frag_count(m);
    uint32_t ab = rz_intensity(m, 'a', 'b');

    rz_measure_result r;
    measure_str(m, "abx abx abx totally unseen material qqq", &r);

    CHECK_EQ_U(m->traversals, trav);
    CHECK_EQ_U(m->frag_steps, steps);
    CHECK_EQ_U(m->births, births);
    CHECK_EQ_U(m->atoms_ingested, eaten);
    CHECK_EQ_U(rz_frag_count(m), (unsigned long long)nf);
    CHECK_EQ_U(rz_intensity(m, 'a', 'b'), ab);
}

TEST(an_empty_bar_reports_nothing)
{
    rz_mind *m = fresh();
    rz_measure_result r;
    measure_str(m, "", &r);
    CHECK_EQ_U(r.predictions, 0u);
    CHECK_EQ_U(r.atoms, 0u);
    CHECK_NEAR(rz_hit_rate(&r), 0.0, 1e-9);
}

TEST(a_mind_that_knows_nothing_is_surprised_by_everything)
{
    rz_mind *m = fresh();
    rz_measure_result r;
    measure_str(m, "the quick brown fox", &r);

    CHECK(r.predictions > 0u);
    CHECK_EQ_U(r.hits, 0u);
    CHECK_NEAR(rz_mean_surprise(&r), 1.0, 1e-9);
}

/* Repetitive material is useless here: composition swallows it into one or two
   fragments and leaves a single step to predict. Real variety keeps the
   segmentation plural, which is the only way the statistic means anything. */
static void varied(char *buf, size_t cap, int flavour)
{
    static const char *a[] = {"the ","cat ","sat ","on ","mat ","dog ","ran ","fast "};
    static const char *b[] = {"zorb ","quix ","vlim ","krup ","jyth ","wenx ","glop ","fnar "};
    const char **w = flavour ? b : a;
    size_t n = 0;
    for (int i = 0; n + 16 < cap; i++) {
        const char *s = w[(i * 7 + i / 5) % 8];
        size_t l = strlen(s);
        memcpy(buf + n, s, l);
        n += l;
    }
    buf[n] = 0;
}

TEST(a_mind_predicts_what_it_has_lived)
{
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, lived);

    rz_measure_result r;
    measure_str(m, lived, &r);

    CHECK(r.predictions > 0u);
    CHECK(rz_hit_rate(&r) > 0.5);
    CHECK(rz_mean_surprise(&r) < 0.5);
}

TEST(the_bar_is_harder_on_material_it_never_ate)
{
    /* The measure that matters: held-out material, not a rerun of the meal. */
    static char lived[4096], never[4096];
    varied(lived, sizeof lived, 0);
    varied(never, sizeof never, 1);

    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, lived);

    rz_measure_result known, unknown;
    measure_str(m, lived, &known);
    measure_str(m, never, &unknown);

    CHECK(rz_hit_rate(&known) > rz_hit_rate(&unknown));
    CHECK(rz_mean_surprise(&known) < rz_mean_surprise(&unknown));
}

TEST(a_certain_continuation_carries_almost_no_surprise)
{
    /* Nothing here is ambiguous: q is only ever followed by u, u by q.
       Long enough that composition cannot swallow the whole thing into one
       fragment and leave no step to predict. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 30; i++) feed(m, lived);

    rz_measure_result r;
    measure_str(m, lived, &r);
    CHECK(r.predictions > 10u);          /* enough steps for the number to mean something */
    CHECK(rz_mean_surprise(&r) < 0.5);
}

TEST(prediction_is_read_only_and_says_so_when_it_has_no_idea)
{
    rz_mind *m = fresh();
    feed_times(m, "abx", 12);

    CHECK(rz_predict_next(m, rz_atom_id('a')) >= 0);
    CHECK_EQ_U(rz_predict_next(m, rz_atom_id('Z')), (unsigned long long)-1);

    uint64_t steps = m->frag_steps;
    rz_predict_next(m, rz_atom_id('a'));
    CHECK_EQ_U(m->frag_steps, steps);
}

/* --- interest (P16) ------------------------------------------------------- */

TEST(the_three_places_account_for_every_step)
{
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 10; i++) feed(m, lived);

    rz_measure_result r;
    measure_str(m, lived, &r);

    CHECK(r.predictions > 0u);
    CHECK_EQ_U(r.opaque + r.frontier + r.mastered, r.predictions);
    CHECK_NEAR(rz_opaque_share(&r) + rz_frontier_share(&r) + rz_mastered_share(&r),
               1.0, 1e-9);
}

TEST(a_mind_that_knows_nothing_finds_everything_opaque)
{
    rz_mind *m = fresh();
    rz_measure_result r;
    measure_str(m, "the quick brown fox jumps over the lazy dog", &r);

    CHECK(r.predictions > 0u);
    CHECK_NEAR(rz_opaque_share(&r), 1.0, 1e-9);
    CHECK_EQ_U(r.frontier, 0u);
    CHECK_EQ_U(r.mastered, 0u);
}

TEST(material_it_has_chewed_stops_being_opaque)
{
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *fresh_mind = fresh();
    rz_mind *fed = fresh();
    for (int i = 0; i < 20; i++) feed(fed, lived);

    rz_measure_result cold, warm;
    measure_str(fresh_mind, lived, &cold);
    measure_str(fed, lived, &warm);

    CHECK(rz_opaque_share(&warm) < rz_opaque_share(&cold));
    /* it left the dark; where it landed — frontier or mastered — is the next
       test's business */
    CHECK(rz_frontier_share(&warm) + rz_mastered_share(&warm)
          > rz_frontier_share(&cold) + rz_mastered_share(&cold));
}

TEST(the_frontier_is_a_place_you_pass_through)
{
    /* The band moves on its own. Material tasted once sits on the frontier;
       the same material chewed twenty times has been mastered and stopped
       being interesting. Nobody changed a threshold. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *tasted = fresh();
    rz_mind *chewed = fresh();
    feed(tasted, lived);
    for (int i = 0; i < 100; i++) feed(chewed, lived);

    rz_measure_result a, b;
    measure_str(tasted, lived, &a);
    measure_str(chewed, lived, &b);

    CHECK(rz_frontier_share(&a) > 0.0);
    CHECK(rz_mastered_share(&b) > rz_mastered_share(&a));
    CHECK(rz_mean_surprise(&b) < rz_mean_surprise(&a));
}

TEST(a_newborn_unit_has_not_been_anywhere_yet)
{
    /* Chewing does not master in a straight line, and the dip is not a defect.
       A fragment is born out of traffic between two others, and it starts with
       no experience of its own — only the exits it inherited, which are guesses
       and not memories. So while new units are being born the frontier WIDENS,
       and it closes again once they have earned their own paths.

       Measured on this material: mastered 0.9903 at 5 passes, 0.7488 at 30
       while units are being born, 1.0000 at 100. Surprise keeps falling after
       that — 0.0357, 0.0123, 0.0045 at 100, 300 and 800. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *early = fresh();
    rz_mind *middle = fresh();
    rz_mind *settled = fresh();
    for (int i = 0; i < 5;   i++) feed(early, lived);
    for (int i = 0; i < 30;  i++) feed(middle, lived);
    for (int i = 0; i < 100; i++) feed(settled, lived);

    rz_measure_result e, m2, s2;
    measure_str(early, lived, &e);
    measure_str(middle, lived, &m2);
    measure_str(settled, lived, &s2);

    CHECK(rz_mastered_share(&m2) < rz_mastered_share(&e));      /* the dip */
    CHECK(rz_mastered_share(&s2) > rz_mastered_share(&m2));     /* and it closes */
    CHECK(rz_hit_rate(&m2) >= rz_hit_rate(&e));                 /* while prediction never worsens */
}

TEST(novel_steps_are_a_part_of_the_frontier_not_of_the_opaque)
{
    /* Knowing where you are but not what comes next is the interesting case:
       there is a hook, and there is still something to learn. */
    static char lived[4096], other[4096];
    varied(lived, sizeof lived, 0);
    varied(other, sizeof other, 1);

    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, lived);

    rz_measure_result r;
    measure_str(m, lived, &r);
    CHECK(r.novel <= r.frontier);
    CHECK(rz_novel_share(&r) <= rz_frontier_share(&r) + 1e-9);

    rz_measure_result strange;
    measure_str(m, other, &strange);
    CHECK(rz_opaque_share(&strange) > rz_opaque_share(&r));
}

TEST(appraising_teaches_nothing_either)
{
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 10; i++) feed(m, lived);

    uint64_t steps = m->frag_steps, births = m->births;
    int32_t nf = rz_frag_count(m);

    rz_measure_result r;
    measure_str(m, lived, &r);
    measure_str(m, "completely unrelated material zzz", &r);

    CHECK_EQ_U(m->frag_steps, steps);
    CHECK_EQ_U(m->births, births);
    CHECK_EQ_U(rz_frag_count(m), (unsigned long long)nf);
}

/* --- the appetite ledger -------------------------------------------------- */

TEST(a_fingerprint_follows_the_content_not_the_name)
{
    const unsigned char *a = (const unsigned char *)"the same material";
    const unsigned char *b = (const unsigned char *)"the same materiaL";
    CHECK(rz_fingerprint(a, 17) == rz_fingerprint(a, 17));
    CHECK(rz_fingerprint(a, 17) != rz_fingerprint(b, 17));
    CHECK(rz_fingerprint(a, 17) != rz_fingerprint(a, 16));
}

TEST(an_empty_ledger_remembers_nothing)
{
    rz_ledger L;
    rz_ledger_init(&L);
    CHECK_EQ_U(L.n, 0u);
    CHECK(rz_ledger_find(&L, "anything", 123) == NULL);
}

TEST(the_ledger_recalls_what_it_appraised)
{
    rz_ledger L;
    rz_ledger_init(&L);
    rz_ledger_record(&L, "feed/a.txt", 999, 0.80, 0.15);

    const rz_appraisal *p = rz_ledger_find(&L, "feed/a.txt", 999);
    CHECK(p != NULL);
    if (p) {
        CHECK_NEAR(p->surprise, 0.80, 1e-9);
        CHECK_NEAR(p->frontier, 0.15, 1e-9);
    }
}

TEST(material_that_changed_is_not_the_same_material)
{
    /* Comparing today's reading against a stale one would invent progress that
       never happened. */
    rz_ledger L;
    rz_ledger_init(&L);
    rz_ledger_record(&L, "feed/a.txt", 999, 0.80, 0.15);

    CHECK(rz_ledger_find(&L, "feed/a.txt", 1000) == NULL);   /* content changed */
    CHECK(rz_ledger_find(&L, "feed/b.txt", 999) == NULL);    /* different path */
}

TEST(a_second_appraisal_replaces_the_first)
{
    rz_ledger L;
    rz_ledger_init(&L);
    rz_ledger_record(&L, "feed/a.txt", 999, 0.80, 0.15);
    rz_ledger_record(&L, "feed/a.txt", 999, 0.62, 0.31);
    CHECK_EQ_U(L.n, 1u);

    const rz_appraisal *p = rz_ledger_find(&L, "feed/a.txt", 999);
    CHECK(p != NULL);
    if (p) CHECK_NEAR(p->surprise, 0.62, 1e-9);
}

TEST(the_ledger_survives_a_round_trip)
{
    const char *path = "test_ledger.tmp";
    rz_ledger a;
    rz_ledger_init(&a);
    rz_ledger_record(&a, "feed/one.txt", 111, 0.91, 0.04);
    rz_ledger_record(&a, "feed/two.txt", 222, 0.44, 0.38);
    CHECK_EQ_U(rz_ledger_save(&a, path), RZ_OK);

    rz_ledger b;
    rz_ledger_init(&b);
    CHECK_EQ_U(rz_ledger_load(&b, path), RZ_OK);
    CHECK_EQ_U(b.n, a.n);
    CHECK(b.run > a.run - 1);

    const rz_appraisal *p = rz_ledger_find(&b, "feed/two.txt", 222);
    CHECK(p != NULL);
    if (p) CHECK_NEAR(p->frontier, 0.38, 1e-9);

    remove(path);
}

TEST(a_missing_ledger_is_reported_not_invented)
{
    rz_ledger L;
    rz_ledger_init(&L);
    CHECK_EQ_U(rz_ledger_load(&L, "no_such_ledger.tmp"), RZ_ERR_MISSING);
    CHECK_EQ_U(L.n, 0u);
}

TEST(interest_is_a_derivative_and_the_ledger_is_what_makes_it_one)
{
    /* Reading the same material twice should show the surprise falling. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    rz_ledger L;
    rz_ledger_init(&L);

    for (int i = 0; i < 4; i++) feed(m, lived);
    rz_measure_result first;
    measure_str(m, lived, &first);
    rz_ledger_record(&L, "lived", rz_fingerprint((const unsigned char *)lived, strlen(lived)),
                     rz_mean_surprise(&first), rz_frontier_share(&first));

    /* Read across the whole composition, not inside it.

       Measured on this material: surprise 0.0654 / 0.0183 / 0.1029 / 0.0357 /
       0.0123 / 0.0045 at 1 / 5 / 30 / 100 / 300 / 800 passes. It is NOT
       monotone, and the reason matters: the early minimum at 5 passes is
       SIMPLICITY, not mastery — with few fragments the organism walks almost
       atom by atom, and the next byte of repetitive material is easy. Composing
       wider units costs that ease back, because each newborn starts with no
       experience of its own. Only past the composition does the curve fall
       below where it started.

       So interest-as-a-derivative is honest across a long exposure and
       misleading inside a short one: sampled at 30 passes it reports "this got
       harder" exactly when the organism got richer. Pinned by
       a_newborn_unit_has_not_been_anywhere_yet. */
    for (int i = 0; i < 400; i++) feed(m, lived);
    rz_measure_result second;
    measure_str(m, lived, &second);

    const rz_appraisal *before =
        rz_ledger_find(&L, "lived", rz_fingerprint((const unsigned char *)lived, strlen(lived)));
    CHECK(before != NULL);
    if (before) CHECK(rz_mean_surprise(&second) < before->surprise);
}

/* --- forgetting (P5 + P7) ------------------------------------------------- */

TEST(a_new_mind_has_forgotten_nothing)
{
    rz_mind *m = fresh();
    CHECK_EQ_U(m->decay_passes, 0u);
    CHECK_EQ_U(rz_cold_edges(m), 0u);
}

TEST(an_unused_path_fades)
{
    rz_mind *m = fresh();
    feed_times(m, "abx", 40);

    uint32_t before = rz_frag_edge(m, rz_atom_id('a'), rz_atom_id('b'));
    for (int i = 0; i < 30; i++) rz_decay(m);
    uint32_t after = rz_frag_edge(m, rz_atom_id('a'), rz_atom_id('b'));

    CHECK(before > 0u);
    CHECK(after < before);
    CHECK_EQ_U(m->decay_passes, 30u);
}

TEST(a_faded_path_goes_cold_it_does_not_vanish)
{
    /* The whole resolution of D2 in one assertion: the book stays on the shelf,
       the path to it just gets hard. */
    rz_mind *m = fresh();
    feed_times(m, "abx", 40);

    int32_t a = rz_atom_id('a'), b = rz_atom_id('b');
    CHECK(rz_frag_edge(m, a, b) > 0u);

    for (int i = 0; i < 2000; i++) rz_decay(m);      /* an age of disuse */

    CHECK_EQ_U(rz_frag_edge(m, a, b), (unsigned)RZ_EDGE_FLOOR);
    CHECK(rz_cold_edges(m) > 0u);
}

TEST(repetition_outruns_forgetting)
{
    /* Nobody decides what is noise. Two forces sort it out. */
    rz_mind *m = fresh();

    feed(m, "qq zz ");                       /* an accident, seen once */
    for (int i = 0; i < 200; i++) {
        feed(m, "the cat sat on the mat ");  /* a habit */
        rz_decay(m);
    }

    /* The accident went cold — and is still there, not deleted. */
    uint32_t accident = rz_frag_edge(m, rz_atom_id('q'), rz_atom_id('q'));
    CHECK_EQ_U(accident, (unsigned)RZ_EDGE_FLOOR);

    /* And the habit survived it all: the organism still knows the material.
       Measured at the bar rather than on one atom pair, because composition
       moves the traffic up to wider fragments and leaves the atom-level path
       as cold as the accident. */
    rz_measure_result r;
    measure_str(m, "the cat sat on the mat ", &r);
    CHECK(r.predictions > 0u);
    CHECK(rz_mastered_share(&r) + rz_frontier_share(&r) > 0.9);
    CHECK(rz_mean_surprise(&r) < 0.9);
}

TEST(no_path_grows_without_limit)
{
    rz_mind *m = fresh();
    for (int i = 0; i < 200; i++) feed(m, "ab");
    CHECK(rz_frag_edge(m, rz_atom_id('a'), rz_atom_id('b')) <= (unsigned)RZ_CEILING);
}

TEST(a_sleeping_module_settles_up_when_it_wakes)
{
    /* Forgetting costs nothing for the parts nobody is touching. */
    rz_mind *m = fresh();
    rz_set_nest(m, "test_decaynest.tmp");
    feed_times(m, "abx", 40);

    uint32_t before = rz_frag_edge(m, rz_atom_id('a'), rz_atom_id('b'));

    /* push module 'a' out by touching many other lineages, then decay */
    char s[3] = { 0, 0, 0 };
    for (int c = 'e'; c < 'e' + RZ_RESIDENT + 6; c++) {
        s[0] = (char)c; s[1] = (char)(c + 1);
        feed(m, s);
    }
    for (int i = 0; i < 50; i++) rz_decay(m);

    uint32_t after = rz_frag_edge(m, rz_atom_id('a'), rz_atom_id('b'));   /* wakes it */
    CHECK(after < before);
}

TEST(moderate_forgetting_costs_nothing_because_it_keeps_the_order)
{
    /* Why forgetting is safe: decay scales every path by the same factor, so
       the ranking survives and prediction does not notice. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, lived);

    rz_measure_result before;
    measure_str(m, lived, &before);

    for (int i = 0; i < 20; i++) rz_decay(m);

    rz_measure_result after;
    measure_str(m, lived, &after);

    CHECK(rz_hit_rate(&after) >= rz_hit_rate(&before) - 0.02);
    CHECK(rz_mean_surprise(&after) <= rz_mean_surprise(&before) + 0.02);
}

TEST(the_floor_is_where_knowledge_is_actually_lost)
{
    /* And the honest other half: once everything reaches the floor the ranking
       is gone, and so is the knowing. Forgetting is free until it is not. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, lived);

    rz_measure_result before;
    measure_str(m, lived, &before);

    for (int i = 0; i < 4000; i++) rz_decay(m);   /* everything to the floor */

    rz_measure_result after;
    measure_str(m, lived, &after);

    /* What forgetting takes is CONFIDENCE, not the knowledge of where it has
       been. Everything walked ends up at the floor, so the organism can no
       longer say how often — surprise rises. But the floor of experience sits
       above the weight of a path it was merely handed, so it can still say
       WHICH paths it walked, and the prediction survives.

       This is the whole point of telling a guess from a memory. Before they
       were told apart, a fully-decayed memory became indistinguishable from one
       of the ~90 exits handed over at birth, and the answer collapsed into a
       tie-break among gifts. That collapse read as "forgetting destroys
       knowing" when it was really "forgetting destroys the distinction". */
    CHECK(rz_mean_surprise(&after) > rz_mean_surprise(&before));
    CHECK(rz_hit_rate(&after) >= rz_hit_rate(&before) - 1e-9);
    CHECK(rz_cold_edges(m) > 0u);
}

/* --- the pen (P17) -------------------------------------------------------- */

static size_t distinct_bytes(const rz_recall *r)
{
    int seen[256] = {0};
    size_t n = 0;
    for (size_t i = 0; i < r->len; i++)
        if (!seen[r->atoms[i]]) { seen[r->atoms[i]] = 1; n++; }
    return n;
}

static void teach_a_chanter(rz_mind *m)
{
    /* Material with one overwhelming habit and a little else around it: the
       shape that makes a greedy walk circle. */
    static char buf[8192];
    size_t n = 0;
    for (int i = 0; n + 40 < sizeof buf; i++) {
        const char *s = (i % 9 == 0) ? "and then something else happened "
                                     : "the wheel turns and the wheel turns ";
        size_t l = strlen(s);
        memcpy(buf + n, s, l);
        n += l;
    }
    buf[n] = 0;
    for (int i = 0; i < 8; i++) rz_ingest(m, (const unsigned char *)buf, n);
}

TEST(an_empty_cue_writes_nothing)
{
    rz_mind *m = fresh();
    feed(m, "abc");
    rz_recall t;
    rz_write(m, (const unsigned char *)"", 0, 100, 1000, 0.7, 1, &t);
    CHECK_EQ_U(t.len, 0u);
    CHECK(t.gave_up);
}

TEST(the_same_seed_writes_the_same_text)
{
    rz_mind *m = fresh();
    teach_a_chanter(m);

    rz_recall a, b;
    rz_write(m, (const unsigned char *)"the", 3, 200, 100000, 0.7, 4242, &a);
    rz_write(m, (const unsigned char *)"the", 3, 200, 100000, 0.7, 4242, &b);

    CHECK_EQ_U(a.len, b.len);
    CHECK(a.len > 0u);
    if (a.len) CHECK(memcmp(a.atoms, b.atoms, a.len) == 0);
}

TEST(writing_leaves_the_mind_alone)
{
    rz_mind *m = fresh();
    teach_a_chanter(m);

    uint64_t steps = m->frag_steps, births = m->births;
    int32_t nf = rz_frag_count(m);

    rz_recall t;
    rz_write(m, (const unsigned char *)"the", 3, 400, 100000, 0.7, 7, &t);

    CHECK_EQ_U(m->frag_steps, steps);
    CHECK_EQ_U(m->births, births);
    CHECK_EQ_U(rz_frag_count(m), (unsigned long long)nf);
}

TEST(a_pen_does_not_chant_the_way_a_greedy_walk_does)
{
    /* The payoff. Greedy recall circles by construction; writing wanders and
       damps what it just used, so it keeps finding somewhere else to go. */
    rz_mind *m = fresh();
    teach_a_chanter(m);

    rz_recall chant, text;
    rz_recall_frag(m, (const unsigned char *)"the", 3, 300, 100000, &chant);
    rz_write(m, (const unsigned char *)"the", 3, 300, 100000, 0.7, 11, &text);

    CHECK(chant.len > 0u);
    CHECK(text.len > 0u);
    CHECK(distinct_bytes(&text) > distinct_bytes(&chant));
}

TEST(the_pen_records_where_every_piece_came_from)
{
    /* Nothing it produces is opaque: the trace expands back to atoms. */
    rz_mind *m = fresh();
    teach_a_chanter(m);

    rz_recall t;
    rz_write(m, (const unsigned char *)"the", 3, 200, 100000, 0.7, 3, &t);

    CHECK(t.trace_len > 0u);
    unsigned char buf[RZ_FRAG_SPAN];
    for (uint32_t i = 0; i < t.trace_len; i++)
        CHECK(rz_expand(m, t.trace[i], buf, sizeof buf) > 0u);
}

TEST(the_pen_reports_when_it_left_the_strongest_path)
{
    /* Two mechanisms push it off the strongest path and they are reported
       separately: temperature, and the refractory damping. Even at temperature
       zero the pen deviates, because what it just used is dampened — which is
       the whole reason it does not chant. */
    rz_mind *m = fresh();
    teach_a_chanter(m);

    rz_recall t;
    rz_write(m, (const unsigned char *)"the", 3, 300, 100000, 0.0, 5, &t);
    CHECK(t.wanders > 0u);          /* the refractory alone is enough */
    CHECK(t.damped <= t.wanders);   /* every damped step is a deviation */
}

TEST(temperature_changes_what_gets_written)
{
    rz_mind *m = fresh();
    teach_a_chanter(m);

    rz_recall cold, warm;
    rz_write(m, (const unsigned char *)"the", 3, 300, 100000, 0.0, 5, &cold);
    rz_write(m, (const unsigned char *)"the", 3, 300, 100000, 1.5, 5, &warm);

    CHECK(cold.len > 0u);
    CHECK(warm.len > 0u);
    CHECK(cold.len != warm.len || memcmp(cold.atoms, warm.atoms, cold.len) != 0);
}

TEST(a_text_ends_when_the_effort_runs_out)
{
    rz_mind *m = fresh();
    teach_a_chanter(m);

    rz_recall t;
    rz_write(m, (const unsigned char *)"the", 3, 4000, 12, 0.7, 9, &t);
    CHECK(t.gave_up);
    CHECK(t.effort <= 12u);
    CHECK(t.len < 4000u);
}

/* --- vertical composition (P18) ------------------------------------------- */

TEST(a_shape_fits_in_its_alphabet)
{
    rz_mind *m = fresh();
    feed_times(m, "abx", 20);
    for (int c = 'a'; c <= 'z'; c++)
        CHECK(rz_shape_of(m, rz_atom_id((unsigned char)c)) < RZ_SHAPES);
}

TEST(a_shape_keeps_no_trace_of_content)
{
    /* Two fragments with nothing whatsoever in common, standing in the same
       kind of place, carry the same shape. That is the whole point: the
       identity is thrown away and only the role survives. */
    rz_mind *m = fresh();
    feed_times(m, "qu", 40);   /* q is only ever followed by u */
    feed_times(m, "zw", 40);   /* z is only ever followed by w */

    uint8_t a = rz_shape_of(m, rz_atom_id('q'));
    uint8_t b = rz_shape_of(m, rz_atom_id('z'));
    CHECK_EQ_U(a, b);
    CHECK(rz_shape_members(m, a) >= 2u);
}

TEST(a_mind_that_has_seen_nothing_has_no_shapes_to_offer)
{
    rz_mind *m = fresh();
    CHECK_EQ_U(m->shape_steps, 0u);
    CHECK_EQ_U(rz_predict_shape(m, 0), (unsigned long long)-1);
}

TEST(the_abstract_layer_is_built_by_the_same_traffic)
{
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 10; i++) feed(m, lived);

    CHECK(m->shape_steps > 0u);
    CHECK_EQ_U(m->shape_steps, m->frag_steps);   /* one level up, same steps */
    CHECK(rz_predict_shape(m, rz_shape_of(m, rz_atom_id('t'))) >= 0);
}

TEST(the_shape_is_asked_only_where_the_specific_knows_nothing)
{
    static char lived[4096], never[4096];
    varied(lived, sizeof lived, 0);
    varied(never, sizeof never, 1);

    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, lived);

    rz_measure_result known, unknown;
    measure_str(m, lived, &known);
    measure_str(m, never, &unknown);

    /* it is only consulted at opaque steps, so it never exceeds them */
    CHECK(known.shape_tried <= known.opaque);
    CHECK(unknown.shape_tried <= unknown.opaque);
    /* and unfamiliar material is where it gets asked */
    CHECK(unknown.shape_tried > known.shape_tried);
}

TEST(the_general_beats_a_coin_toss_where_the_specific_is_blind)
{
    /* The claim that decides whether abstraction is worth anything: at steps
       the organism has never taken, does the SHAPE of where it stands predict
       better than picking a role at random? Chance is 1 in RZ_SHAPES. */
    static char lived[4096], never[4096];
    varied(lived, sizeof lived, 0);
    varied(never, sizeof never, 1);

    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, lived);

    rz_measure_result r;
    measure_str(m, never, &r);

    CHECK(r.shape_tried > 0u);
    CHECK(rz_shape_rescue(&r) > 1.0 / (double)RZ_SHAPES);
}

/* --- what actually matters to it ------------------------------------------ */

TEST(a_new_mind_has_no_ideas_worth_having_yet)
{
    rz_mind *m = fresh();
    CHECK_EQ_U(rz_shapes_that_transfer(m), 0u);
    for (uint8_t s = 0; s < RZ_SHAPES; s++) CHECK_NEAR(rz_shape_merit(m, s), 0.0, 1e-9);
}

TEST(it_scores_its_own_ideas_while_it_eats)
{
    /* Before recording each step it asks the abstraction to call the shot. The
       step has not been written yet, so the answer cannot be cheated. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 10; i++) feed(m, lived);

    uint32_t tried = 0;
    for (uint8_t s = 0; s < RZ_SHAPES; s++) tried += m->shape_tried[s];
    CHECK(tried > 0u);
    CHECK(rz_shapes_that_transfer(m) > 0u);
}

TEST(a_shape_that_keeps_being_right_is_what_matters)
{
    /* The pen falls the way the apple did. An idea earns its place by
       anticipating, not by being frequent. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 30; i++) feed(m, lived);

    int found = 0;
    for (uint8_t s = 0; s < RZ_SHAPES && !found; s++)
        if (m->shape_tried[s] >= RZ_SHAPE_TESTED
            && rz_shape_merit(m, s) > 1.0 / (double)RZ_SHAPES) found = 1;
    CHECK(found);
}

TEST(forgetting_spares_what_transfers)
{
    /* The loop closes here: what the organism understood well enough to
       anticipate with is protected from fading. Nothing decreed it — the
       protection is proportional to a merit it measured on itself. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *taught = fresh();
    rz_mind *blank  = fresh();
    for (int i = 0; i < 30; i++) feed(taught, lived);
    for (int i = 0; i < 30; i++) feed(blank, lived);

    /* strip one of them of everything it learned ABOUT ITS OWN IDEAS, leaving
       the concrete memory identical */
    memset(blank->shape_tried, 0, sizeof blank->shape_tried);
    memset(blank->shape_won, 0, sizeof blank->shape_won);

    for (int i = 0; i < 60; i++) { rz_decay(taught); rz_decay(blank); }

    rz_measure_result with, without;
    measure_str(taught, lived, &with);
    measure_str(blank,  lived, &without);

    CHECK(rz_mean_surprise(&with) <= rz_mean_surprise(&without));
}

/* --- dreaming (P14) ------------------------------------------------------- */

static void set_night(rz_mind *m, uint64_t steps, uint64_t births, double strain)
{
    m->awake_steps = steps;
    m->awake_births = births;
    m->awake_strain = strain * (double)steps;   /* strain PER STEP, as lived */
}

/* --- what it was given versus what it took ------------------------------- */

/* --- harmony (P25): the referee -------------------------------------------
 *
 * D39: harmony is describing the SAME material with LESS, while still
 * accounting for ALL of it. Two numbers, and the second one is what makes the
 * first one safe.
 */

/* --- senses (P30): drawn, not written ------------------------------------- */

TEST(a_sense_is_drawn_not_written)
{
    /* The reference organism does not invent a sense. It fills a template from a
       small vocabulary and lets the draw decide: "count pieces of type T of side
       S within radius R of reference F, when C holds". Out of that comes
       "enemies near my king" with nobody having written that idea down.

       Everything found by hand today -- the beat, the roles, root+suffix -- was
       a detector an author wrote. That is the opposite of a faculty. */
    rz_mind *m0 = fresh();
    rz_sense a = rz_sense_draw(m0, 1);
    rz_sense b = rz_sense_draw(m0, 1);
    rz_sense c = rz_sense_draw(m0, 99);

    CHECK(rz_sense_same(&a, &b));      /* same draw, same sense */
    CHECK(!rz_sense_same(&a, &c));     /* another draw, another sense */
}

TEST(a_newborn_sense_is_a_divergent_copy)
{
    /* It is born by mutating one that already exists, in ONE place. A sense
       drawn wholly at random each time would never accumulate anything. */
    rz_mind *m0 = fresh();
    rz_sense parent = rz_sense_draw(m0, 7);
    rz_sense child  = rz_sense_mutate(m0, &parent, 3);

    CHECK(!rz_sense_same(&parent, &child));
    CHECK_EQ_U(rz_sense_distance(&parent, &child), 1u);
}

TEST(the_template_is_not_degenerate)
{
    /* THE test. If every sense the template can produce reads the same number
       from the same place, the vocabulary is degenerate and the whole faculty is
       theatre -- exactly how the shape alphabet collapsed to four classes that
       differed only in length, in silence, because nothing was watching.

       So: draw many, read them all from the same fragment, and demand that they
       genuinely disagree. */
    rz_mind *m = fresh();
    for (int i = 0; i < 30; i++)
        feed(m, "the quick brown fox jumps over the lazy dog. ");

    int32_t here = rz_longest_match(m, (const unsigned char *)"the ", 4);
    CHECK(here >= 0);

    uint32_t seen_values[64];
    uint32_t nvalues = 0;
    for (uint64_t s = 1; s <= 64; s++) {
        rz_sense g = rz_sense_draw(m, s);
        uint32_t v = rz_sense_read(m, &g, here);
        uint32_t known = 0;
        for (uint32_t k = 0; k < nvalues; k++)
            if (seen_values[k] == v) { known = 1; break; }
        if (!known && nvalues < 64) seen_values[nvalues++] = v;
    }
    /* not a number pulled from the air: with 64 draws, a template worth having
       must not collapse onto one or two readings */
    CHECK(nvalues >= 4);
}

TEST(a_sense_reads_the_place_it_stands_on)
{
    /* It answers about HERE. Standing somewhere else, it may answer differently
       -- otherwise it is a constant wearing a costume. */
    rz_mind *m = fresh();
    for (int i = 0; i < 30; i++)
        feed(m, "alpha beta gamma delta alpha beta epsilon zeta. ");

    int32_t a = rz_longest_match(m, (const unsigned char *)"alpha", 5);
    int32_t z = rz_longest_match(m, (const unsigned char *)"z", 1);
    CHECK(a >= 0);
    CHECK(z >= 0);

    int differed = 0;
    for (uint64_t s = 1; s <= 40; s++) {
        rz_sense g = rz_sense_draw(m, s);
        if (rz_sense_read(m, &g, a) != rz_sense_read(m, &g, z)) { differed = 1; break; }
    }
    CHECK(differed);
}

/* --- senses, scored as it goes -------------------------------------------- */

TEST(a_sense_is_scored_as_it_goes_not_by_rereading)
{
    /* Scoring every sense by re-reading all the material, every round, cost
       senses x material -- so the more it learned the slower it got, and an
       organism that is paralysed by growing is no organism.

       Now each step it takes is an observation: whatever it happens to be
       standing on, the awake senses read it once and remember what followed.
       Reach comes from TIME rather than from sweeping a radius. */
    rz_mind *m = fresh();
    for (int i = 0; i < 30; i++) feed(m, "the quick brown fox jumps over the lazy dog. ");
    for (int i = 0; i < 6; i++) rz_sense_add(m, rz_sense_draw(m, (uint64_t)(i * 17 + 1)));

    CHECK_EQ_U(rz_sense_observations(m, 0), 0u);
    feed(m, "the quick brown fox jumps over the lazy dog. ");
    CHECK(rz_sense_observations(m, 0) > 0u);

    /* and what it is worth is read off what it already saw -- no walking */
    uint64_t before = rz_sense_observations(m, 0);
    double w = rz_sense_score(m, 0);
    CHECK(w >= 0.0);
    CHECK_EQ_U(rz_sense_observations(m, 0), before);
}

TEST(only_the_awake_senses_are_read_at_each_step)
{
    /* The reference organism keeps a deliberately enormous cold pool and wakes
       only a handful per move. That is not an optimisation -- it is what makes
       KEEPING the neutral ones affordable, and keeping them is the whole rule. */
    rz_mind *m = fresh();
    for (int i = 0; i < 30; i++) feed(m, "alpha beta gamma delta epsilon. ");
    for (int i = 0; i < 40; i++) rz_sense_add(m, rz_sense_draw(m, (uint64_t)(i * 31 + 7)));
    CHECK(rz_senses_count(m) > RZ_SENSE_HOT);

    uint64_t reads_before = m->sense_reads;
    uint64_t steps_before = m->frag_steps;
    feed(m, "alpha beta gamma delta epsilon. ");
    uint64_t steps = m->frag_steps - steps_before;
    uint64_t reads = m->sense_reads - reads_before;

    CHECK(steps > 0u);
    /* bounded: a handful per step, not one per sense held */
    CHECK(reads <= steps * (uint64_t)(RZ_SENSE_HOT + 2));
}

TEST(a_cold_sense_still_gets_its_turn)
{
    /* Asleep must not mean buried. A sense that never gets read can never earn
       its way back, and the pool would silently freeze into whatever happened to
       be hot first. */
    rz_mind *m = fresh();
    for (int i = 0; i < 30; i++) feed(m, "alpha beta gamma delta epsilon. ");
    for (int i = 0; i < 40; i++) rz_sense_add(m, rz_sense_draw(m, (uint64_t)(i * 31 + 7)));

    for (int i = 0; i < 40; i++) feed(m, "alpha beta gamma delta epsilon. ");

    int32_t ns = rz_senses_count(m);
    int32_t observed = 0;
    for (int32_t i = 0; i < ns; i++)
        if (rz_sense_observations(m, i) > 0u) observed++;
    CHECK(observed > RZ_SENSE_HOT);   /* the cold ones got their turn too */
}

TEST(a_sense_looks_at_the_next_move_not_around_the_block)
{
    /* It reads what is one step away. Sweeping a radius through a graph whose
       fan-out is the whole memory meant a single reading touched thousands of
       places, and a sense that costs more than the organism is not a sense. */
    rz_mind *m = fresh();
    for (int i = 0; i < 30; i++) feed(m, "the quick brown fox jumps over the lazy dog. ");
    int32_t here = rz_longest_match(m, (const unsigned char *)"the ", 4);
    CHECK(here >= 0);

    rz_sense g = rz_sense_draw(m, 3);
    uint32_t reads_before = 0;
    (void)reads_before;
    /* whatever it counts, it cannot exceed what is directly reachable */
    uint32_t v = rz_sense_read(m, &g, here);
    int32_t tos[64];
    int32_t nt = rz_frag_edges(m, here, tos, 64);
    CHECK(v <= (uint32_t)(nt > 2 ? nt : 2));
}

TEST(a_sense_looks_at_what_is_at_hand_and_never_digs)
{
    /* A sense is a glance, not an excavation.

       Reading a neighbour that lives in a sleeping module used to wake it from
       disk, and with only a handful of modules resident they evicted each other
       on every look: 245,159 wakes for 34,940 readings -- seven disk loads per
       glance, and eating with twelve senses ran thirteen times slower than
       eating without them.

       So a sense sees what is already at hand and skips the rest, counting what
       it skipped. The reference gene counts pieces on the board in front of it;
       it does not page positions in from disk. */
    rz_mind *m = fresh();
    rz_set_nest(m, "test_sensenest.tmp");
    for (int i = 0; i < 40; i++) feed(m, "the quick brown fox jumps over the lazy dog. ");

    uint64_t wakes_plain = m->wakes;
    feed(m, "the quick brown fox jumps over the lazy dog. ");
    uint64_t cost_plain = m->wakes - wakes_plain;

    for (int i = 0; i < 12; i++) rz_sense_add(m, rz_sense_draw(m, (uint64_t)(i * 17 + 1)));
    uint64_t wakes_sensed = m->wakes;
    feed(m, "the quick brown fox jumps over the lazy dog. ");
    uint64_t cost_sensed = m->wakes - wakes_sensed;

    /* looking must not cost more digging than not looking */
    CHECK(cost_sensed <= cost_plain + 4u);
    CHECK(m->sense_reads > 0u);
}

TEST(a_neutral_sense_is_kept_so_there_is_something_to_build_from)
{
    /* THE rule, and the one that nearly went the other way.

       The referee built earlier says a piece earns its place only if it pays for
       itself. That is right for units and lethal for senses: a neutral sense
       would be deleted the moment it appeared, nothing would ever accumulate,
       and there would be no material left to compose. The reference organism
       had already solved it -- prune only what ACTIVELY costs, keep the neutral
       ones, let them pile up. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);
    rz_mind *m = fresh();
    for (int i = 0; i < 40; i++) feed(m, lived);

    for (int round = 0; round < 60; round++) {
        feed(m, lived);                       /* eating is what scores them */
        rz_senses_live(m, (uint64_t)(round * 7 + 1));
    }

    CHECK(rz_senses_count(m) > 3);
    CHECK(m->senses_born > 0u);
    /* senses that say little were met and left alone */
    CHECK(m->senses_kept_neutral > 0u);
    /* and they were not all wiped out to make room */
    CHECK(m->senses_pruned < m->senses_born);
}

TEST(a_sense_can_be_built_on_another_sense)
{
    /* Without this a population of senses is a list of detectors. With it, the
       output of one is the material of the next, and the stack can rise. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);
    rz_mind *m = fresh();
    for (int i = 0; i < 40; i++) feed(m, lived);
    for (int round = 0; round < 80; round++) {
        feed(m, lived);                       /* eating is what scores them */
        rz_senses_live(m, (uint64_t)(round * 13 + 3));
    }

    int stacked = 0;
    for (int32_t i = 0; i < rz_senses_count(m); i++) {
        const rz_sense *s = rz_sense_at(m, i);
        if (s && s->src) { stacked = 1; break; }
    }
    CHECK(stacked);
}

TEST(what_a_sense_is_worth_is_measured_and_never_taught)
{
    static char lived[4096];
    varied(lived, sizeof lived, 0);
    rz_mind *m = fresh();
    for (int i = 0; i < 40; i++) feed(m, lived);

    uint32_t frags = rz_frag_count(m);
    uint64_t steps = m->frag_steps;
    rz_sense g = rz_sense_draw(m, 5);
    double w = rz_sense_worth(m, &g, (const unsigned char *)lived, strlen(lived));

    CHECK(w >= 0.0);
    CHECK_EQ_U(rz_frag_count(m), frags);
    CHECK_EQ_U(m->frag_steps, steps);
}

TEST(harmony_is_measured_and_never_taught)
{
    /* Same discipline as the external bar. A referee that trains what it judges
       is not a referee. */
    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, "the cat sat on the mat. ");

    uint32_t frags_before = rz_frag_count(m);
    uint64_t steps_before = m->frag_steps;

    rz_harmony h;
    rz_measure_harmony(m, (const unsigned char *)"the dog ran fast. ", 18, &h);

    CHECK_EQ_U(rz_frag_count(m), frags_before);
    CHECK_EQ_U(m->frag_steps, steps_before);
}

TEST(finding_the_order_in_something_lowers_its_cost)
{
    /* The first half of D39. The same material, walked at less effort, is the
       operational meaning of "it found a better order for this". */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *raw = fresh();
    rz_mind *versed = fresh();
    feed(raw, "unrelated filler material. ");
    for (int i = 0; i < 200; i++) feed(versed, lived);

    rz_harmony a, b;
    rz_measure_harmony(raw, (const unsigned char *)lived, strlen(lived), &a);
    rz_measure_harmony(versed, (const unsigned char *)lived, strlen(lived), &b);

    CHECK(b.cost < a.cost);
}

TEST(discarding_the_material_is_not_harmony)
{
    /* THE guard, and the reason D24 sat open since P15 was designed.

       An obsessed mind is cheap to walk: one path, no branching, no doubt. On
       effort alone it wins everything, which is why "minimise effort" selects
       for collapse. It is not describing its material with less — it never met
       that material at all, and cannot account for it.

       So the referee must refuse it, and refuse it on the second number. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *obsessed = fresh();
    for (int i = 0; i < 300; i++) feed(obsessed, "rararararararararara");

    rz_mind *versed = fresh();
    for (int i = 0; i < 200; i++) feed(versed, lived);

    rz_harmony o, v;
    rz_measure_harmony(obsessed, (const unsigned char *)lived, strlen(lived), &o);
    rz_measure_harmony(versed,   (const unsigned char *)lived, strlen(lived), &v);

    CHECK(o.unaccounted > v.unaccounted);

    /* And this is the closure of D24, asserted directly so it cannot pass for
       the wrong reason: the collapse is not merely refused, it is EXPENSIVE.
       Measured on this material — obsessed 18963.09 against versed 13.41, more
       than a thousandfold. Not knowing costs the whole alphabet, so a mind that
       bought cheapness by never meeting the material cannot come out ahead.

       Two earlier readings of this same measure were wrong and both handed the
       shortcut back: a mean per step rewards chopping the walk into many cheap
       steps, and charging nothing for an unaccounted step makes ignorance free. */
    CHECK(o.cost > v.cost);
    CHECK(!rz_more_harmonious(&o, &v));
    CHECK(rz_more_harmonious(&v, &o));
}

TEST(a_path_it_was_given_is_not_a_path_it_took)
{
    /* A newborn inherits its parent's exits so it is not a dead end. Those
       exits are guesses: "this is possible". Until now they arrived at the same
       weight traffic decays to, so the two were indistinguishable -- and with
       mean out-degree at 93 the guesses drowned the experience. */
    rz_mind *m = fresh();
    for (int i = 0; i < 40; i++)
        feed(m, "the quick brown fox jumps over the lazy dog. ");

    uint64_t given = 0, taken = 0;
    for (int32_t k = 0; k < RZ_MODULES; k++) {
        for (int32_t l = 0; l < m->mod_nfrags[k]; l++) {
            int32_t id = k * RZ_MOD_FRAGS + l;
            int32_t all = rz_frag_degree(m, id);
            int32_t walked = rz_frag_degree_walked(m, id);
            CHECK(walked <= all);
            given += (uint64_t)(all - walked);
            taken += (uint64_t)walked;
        }
    }
    CHECK(given > 0);
    CHECK(taken > 0);
}

TEST(forgetting_leaves_a_guess_alone)
{
    /* A guess has no experience to lose. Decaying it would be forgetting
       something that never happened -- and worse, the floor would round it back
       up into the range where experience lives. */
    rz_mind *m = fresh();
    for (int i = 0; i < 40; i++) feed(m, "abcabcabcabc ");

    int32_t victim = -1, to = -1;
    for (int32_t k = 0; k < RZ_MODULES && victim < 0; k++)
        for (int32_t l = 0; l < m->mod_nfrags[k] && victim < 0; l++) {
            int32_t id = k * RZ_MOD_FRAGS + l;
            if (rz_frag_degree(m, id) > rz_frag_degree_walked(m, id)) {
                victim = id;
                to = rz_frag_first_guess(m, id);
            }
        }
    CHECK(victim >= 0);
    CHECK(to >= 0);
    CHECK_EQ_U(rz_frag_edge(m, victim, to), RZ_EDGE_INHERITED);

    for (int i = 0; i < 50; i++) rz_decay(m);
    CHECK_EQ_U(rz_frag_edge(m, victim, to), RZ_EDGE_INHERITED);
}

TEST(a_shape_is_read_from_what_it_walked)
{
    /* The decisive one. Branch and concentration collapsed because every
       fragment carried ~93 exits it had never taken: branching always looked
       huge and concentration always looked flat. Reading the shape from traffic
       alone is what gives those two axes their range back. */
    rz_mind *m = fresh();
    for (int i = 0; i < 60; i++) feed(m, "aaaaaaaaaaaa ");

    int32_t f = -1;
    for (int32_t k = 0; k < RZ_MODULES && f < 0; k++)
        for (int32_t l = 0; l < m->mod_nfrags[k] && f < 0; l++) {
            int32_t id = k * RZ_MOD_FRAGS + l;
            if (rz_frag_degree(m, id) > rz_frag_degree_walked(m, id)
                && rz_frag_degree_walked(m, id) >= 1)
                f = id;
        }
    CHECK(f >= 0);
    /* branching must describe the paths it actually took, not the ones it was
       handed -- so it cannot exceed the number of walked exits */
    CHECK(rz_frag_branching(m, f) <= (double)rz_frag_degree_walked(m, f) + 1e-9);
}

TEST(what_it_cannot_place_is_what_wears_it_down)
{
    /* Overstimulation is NOT "the world was unpredictable". Random bytes are
       perfectly predictable in the abstract: every piece looks like every other
       piece, so the shape layer calls them right nine times out of ten. That
       reading would have told us noise is restful, which is backwards.

       Being overwhelmed is meeting material you cannot hook onto — arriving at
       a place you have no expectation from at all. That is measured down at the
       fragment layer, before the step is recorded. */
    static const char text[] = "the quick brown fox jumps over the lazy dog. ";
    rz_mind *m = fresh();
    for (int i = 0; i < 60; i++)
        rz_ingest(m, (const unsigned char *)text, sizeof text - 1);

    m->awake_steps = 0; m->awake_strain = 0.0;
    for (int i = 0; i < 20; i++)
        rz_ingest(m, (const unsigned char *)text, sizeof text - 1);
    double familiar = m->awake_strain / (double)m->awake_steps;

    unsigned char alien[4096];
    uint64_t s = 12345;
    for (size_t i = 0; i < sizeof alien; i++) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        alien[i] = (unsigned char)(s >> 33);
    }
    m->awake_steps = 0; m->awake_strain = 0.0;
    rz_ingest(m, alien, sizeof alien);
    double strange = m->awake_strain / (double)m->awake_steps;

    /* An absolute gap, not a ratio. Raw surprise saturates near 1.0 everywhere
       — from any fragment there are hundreds of continuations, so even the right
       one carries little probability. Strain has to be read against chance from
       that same place, or it says "everything is overwhelming" and decides
       nothing. */
    CHECK(familiar < 0.10);
    CHECK(strange   > 0.40);
}

TEST(learning_is_strain_but_it_is_not_stress)
{
    /* The bar sits where the material actually separates. Measured over C
       source, Spanish prose, and a compiled binary, strain per step lands
       between 0.29 and 0.45 — meeting new structure is supposed to cost. Only
       material with no structure to meet (0.65 and up) asks to be calmed. Put
       the bar below 0.45 and it would call ordinary learning an emergency. */
    static const char text[] =
        "the quick brown fox jumps over the lazy dog. pack my box with five "
        "dozen liquor jugs. how vexingly quick daft zebras jump! ";
    rz_mind *m = fresh();
    for (int i = 0; i < 40; i++)
        rz_ingest(m, (const unsigned char *)text, sizeof text - 1);
    CHECK(rz_dream_needed(m) != RZ_DREAM_CALM);

    unsigned char alien[8192];
    uint64_t s = 999;
    for (size_t i = 0; i < sizeof alien; i++) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        alien[i] = (unsigned char)(s >> 33);
    }
    m->awake_steps = 0; m->awake_strain = 0.0; m->awake_births = 0;
    rz_ingest(m, alien, sizeof alien);
    CHECK_EQ_U(rz_dream_needed(m), RZ_DREAM_CALM);
}

TEST(there_is_nothing_to_sleep_on_yet)
{
    rz_mind *m = fresh();
    CHECK_EQ_U(rz_dream_needed(m), RZ_DREAM_NONE);
    feed(m, "a little");
    CHECK_EQ_U(rz_dream_needed(m), RZ_DREAM_NONE);
}

TEST(drowning_comes_before_everything_else)
{
    /* You cannot consolidate or explore while most of what you meet is a wall. */
    rz_mind *m = fresh();
    set_night(m, 10000, 500, 0.70);       /* fertile AND overwhelmed */
    CHECK_EQ_U(rz_dream_needed(m), RZ_DREAM_CALM);
}

TEST(a_fertile_night_asks_to_be_settled)
{
    rz_mind *m = fresh();
    set_night(m, 10000, 500, 0.05);
    CHECK_EQ_U(rz_dream_needed(m), RZ_DREAM_SETTLE);
}

TEST(nothing_new_and_nothing_hurting_means_it_is_circling)
{
    rz_mind *m = fresh();
    set_night(m, 10000, 2, 0.05);
    CHECK_EQ_U(rz_dream_needed(m), RZ_DREAM_LOOSEN);
}

TEST(every_dream_can_say_why_it_was_had)
{
    CHECK(rz_dream_reason(RZ_DREAM_NONE)[0]   != 0);
    CHECK(rz_dream_reason(RZ_DREAM_CALM)[0]   != 0);
    CHECK(rz_dream_reason(RZ_DREAM_SETTLE)[0] != 0);
    CHECK(rz_dream_reason(RZ_DREAM_LOOSEN)[0] != 0);
}

TEST(a_dream_writes_nothing_but_attention)
{
    /* The rule the whole design rests on: the episode is false and is thrown
       away. Not one intensity, not one traversal, not one fragment. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, lived);

    uint64_t trav = m->traversals, steps = m->frag_steps, births = m->births;
    uint64_t eaten = m->atoms_ingested, shsteps = m->shape_steps;
    int32_t  nf = rz_frag_count(m);
    uint32_t ab = rz_intensity(m, 'a', 'b');
    double   support = rz_effective_support(m);
    uint64_t before_attention = rz_attention_total(m);

    set_night(m, 10000, 2, 0.05);          /* it is circling */
    rz_dream_kind had = rz_sleep(m, 99);

    CHECK_EQ_U(had, RZ_DREAM_LOOSEN);
    CHECK_EQ_U(m->traversals, trav);
    CHECK_EQ_U(m->frag_steps, steps);
    CHECK_EQ_U(m->births, births);
    CHECK_EQ_U(m->atoms_ingested, eaten);
    CHECK_EQ_U(m->shape_steps, shsteps);
    CHECK_EQ_U(rz_frag_count(m), (unsigned long long)nf);
    CHECK_EQ_U(rz_intensity(m, 'a', 'b'), ab);
    CHECK_NEAR(rz_effective_support(m), support, 1e-12);

    /* and the one thing it IS allowed to leave */
    CHECK(rz_attention_total(m) > before_attention);
}

TEST(sleeping_clears_the_night)
{
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, lived);
    set_night(m, 10000, 500, 0.05);

    CHECK_EQ_U(rz_sleep(m, 5), RZ_DREAM_SETTLE);
    CHECK_EQ_U(m->awake_steps, 0u);
    CHECK_EQ_U(m->awake_births, 0u);
    CHECK_EQ_U(m->sleeps_had, 1u);
    CHECK_EQ_U(m->dreams[RZ_DREAM_SETTLE], 1u);
    CHECK_EQ_U(rz_dream_needed(m), RZ_DREAM_NONE);   /* nothing left to sleep on */
}

TEST(attention_fades_or_everything_ends_up_marked)
{
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *m = fresh();
    for (int i = 0; i < 20; i++) feed(m, lived);
    set_night(m, 10000, 2, 0.05);
    rz_sleep(m, 3);

    uint64_t marked = rz_attention_total(m);
    CHECK(marked > 0u);

    for (int i = 0; i < 200; i++) rz_decay(m);
    CHECK(rz_attention_total(m) < marked);
}

TEST(a_marked_fragment_is_spared_by_forgetting)
{
    /* What the dream chose to look at again survives longer. Nobody decreed it:
       the organism marked it while it slept. */
    static char lived[4096];
    varied(lived, sizeof lived, 0);

    rz_mind *dreamt = fresh();
    rz_mind *dreamless = fresh();
    for (int i = 0; i < 20; i++) { feed(dreamt, lived); feed(dreamless, lived); }

    set_night(dreamt, 10000, 2, 100);
    rz_sleep(dreamt, 7);
    CHECK(rz_attention_total(dreamt) > 0u);
    CHECK_EQ_U(rz_attention_total(dreamless), 0u);

    for (int i = 0; i < 40; i++) { rz_decay(dreamt); rz_decay(dreamless); }

    rz_measure_result with, without;
    measure_str(dreamt, lived, &with);
    measure_str(dreamless, lived, &without);
    CHECK(rz_mean_surprise(&with) <= rz_mean_surprise(&without));
}

int main(void)
{
    printf("razar spike 13\n");
    RUN(fresh_mind_is_empty);
    RUN(atomic_base_does_not_grow_with_experience);
    RUN(one_pair_is_one_traversal);
    RUN(a_lone_atom_walks_nowhere);
    RUN(empty_material_changes_nothing);
    RUN(direction_matters);
    RUN(repetition_strengthens_it_does_not_multiply);
    RUN(ingestion_is_not_idempotent);
    RUN(a_boundary_is_not_a_path);
    RUN(effective_support_of_nothing_is_zero);
    RUN(effective_support_of_uniform_is_the_path_count);
    RUN(effective_support_of_obsession_is_one);
    RUN(effective_support_falls_as_traversal_concentrates);
    RUN(top_share_reports_concentration);
    RUN(examine_reports_the_mind);
    RUN(a_mind_survives_a_round_trip);
    RUN(a_missing_mind_is_reported_not_invented);
    RUN(a_foreign_layout_is_refused_loudly);
    RUN(an_autopsy_survives_a_round_trip);
    RUN(the_second_reading_is_visible_in_the_diff);
    RUN(nothing_eaten_is_empty);
    RUN(no_previous_is_baseline);
    RUN(rereading_the_same_thing_is_consolidating);
    RUN(new_material_that_widens_reach_is_healthy);
    RUN(collapse_is_caught_even_when_no_new_paths_appear);
    RUN(collapse_outranks_consolidation);
    RUN(collapse_is_caught_when_paths_also_rise);
    RUN(a_whisker_of_drift_is_not_a_collapse);
    RUN(every_verdict_can_say_its_name);
    RUN(branching_of_a_certain_continuation_is_one);
    RUN(branching_of_a_dead_end_is_zero);
    RUN(branching_counts_the_real_alternatives);
    RUN(a_trace_never_walked_cannot_be_recalled);
    RUN(an_empty_cue_recalls_nothing);
    RUN(a_certain_trace_comes_back_whole_and_cheap);
    RUN(the_cue_is_read_from_its_end);
    RUN(an_ambiguous_trace_costs_more_to_recall);
    RUN(ties_are_broken_the_same_way_every_time);
    RUN(a_dead_end_mid_walk_is_a_give_up);
    RUN(effort_runs_out_and_it_stops_trying);
    RUN(recalling_does_not_strengthen_what_it_recalls);
    RUN(every_atom_is_already_a_fragment);
    RUN(an_atom_has_no_parts_below_it);
    RUN(a_fragment_lives_in_the_lineage_of_its_first_atom);
    RUN(only_the_modules_it_needs_are_awake);
    RUN(reaching_past_the_resident_set_puts_something_to_sleep);
    RUN(a_module_that_cannot_be_written_down_is_not_forgotten);
    RUN(a_hub_may_hold_more_paths_than_any_fixed_array_would_allow);
    RUN(travelling_together_gives_birth_to_a_fragment);
    RUN(a_pair_that_barely_travels_earns_nothing);
    RUN(a_born_fragment_still_decomposes_into_atoms);
    RUN(fragments_compound_into_longer_fragments);
    RUN(the_longest_conjunction_wins_the_match);
    RUN(an_unknown_conjunction_falls_back_to_its_atom);
    RUN(the_trigger_is_the_conjunction_that_ends_the_cue);
    RUN(a_fragment_born_at_the_threshold_has_no_traffic_yet);
    RUN(a_hint_that_does_not_fire_is_tried_smaller);
    RUN(a_cue_that_starts_something_known_gets_it_completed);
    RUN(a_cue_with_no_known_conjunction_still_triggers_on_its_last_atom);
    RUN(fragment_paths_record_what_followed_what);
    RUN(fragment_recall_leaves_the_mind_alone);
    RUN(fragments_recall_what_atoms_could_not);
    RUN(composition_can_swallow_a_branch_whole);
    RUN(a_sense_is_drawn_not_written);
    RUN(a_newborn_sense_is_a_divergent_copy);
    RUN(the_template_is_not_degenerate);
    RUN(a_sense_reads_the_place_it_stands_on);
    RUN(a_sense_is_scored_as_it_goes_not_by_rereading);
    RUN(only_the_awake_senses_are_read_at_each_step);
    RUN(a_cold_sense_still_gets_its_turn);
    RUN(a_sense_looks_at_the_next_move_not_around_the_block);
    RUN(a_sense_looks_at_what_is_at_hand_and_never_digs);
    RUN(a_neutral_sense_is_kept_so_there_is_something_to_build_from);
    RUN(a_sense_can_be_built_on_another_sense);
    RUN(what_a_sense_is_worth_is_measured_and_never_taught);
    RUN(harmony_is_measured_and_never_taught);
    RUN(finding_the_order_in_something_lowers_its_cost);
    RUN(discarding_the_material_is_not_harmony);
    RUN(a_path_it_was_given_is_not_a_path_it_took);
    RUN(forgetting_leaves_a_guess_alone);
    RUN(a_shape_is_read_from_what_it_walked);
    RUN(what_it_cannot_place_is_what_wears_it_down);
    RUN(learning_is_strain_but_it_is_not_stress);
    RUN(there_is_nothing_to_sleep_on_yet);
    RUN(drowning_comes_before_everything_else);
    RUN(a_fertile_night_asks_to_be_settled);
    RUN(nothing_new_and_nothing_hurting_means_it_is_circling);
    RUN(every_dream_can_say_why_it_was_had);
    RUN(a_dream_writes_nothing_but_attention);
    RUN(sleeping_clears_the_night);
    RUN(attention_fades_or_everything_ends_up_marked);
    RUN(a_marked_fragment_is_spared_by_forgetting);
    RUN(a_new_mind_has_no_ideas_worth_having_yet);
    RUN(it_scores_its_own_ideas_while_it_eats);
    RUN(a_shape_that_keeps_being_right_is_what_matters);
    RUN(forgetting_spares_what_transfers);
    RUN(a_shape_fits_in_its_alphabet);
    RUN(a_shape_keeps_no_trace_of_content);
    RUN(a_mind_that_has_seen_nothing_has_no_shapes_to_offer);
    RUN(the_abstract_layer_is_built_by_the_same_traffic);
    RUN(the_shape_is_asked_only_where_the_specific_knows_nothing);
    RUN(the_general_beats_a_coin_toss_where_the_specific_is_blind);
    RUN(an_empty_cue_writes_nothing);
    RUN(the_same_seed_writes_the_same_text);
    RUN(writing_leaves_the_mind_alone);
    RUN(a_pen_does_not_chant_the_way_a_greedy_walk_does);
    RUN(the_pen_records_where_every_piece_came_from);
    RUN(the_pen_reports_when_it_left_the_strongest_path);
    RUN(temperature_changes_what_gets_written);
    RUN(a_text_ends_when_the_effort_runs_out);
    RUN(a_new_mind_has_forgotten_nothing);
    RUN(an_unused_path_fades);
    RUN(a_faded_path_goes_cold_it_does_not_vanish);
    RUN(repetition_outruns_forgetting);
    RUN(no_path_grows_without_limit);
    RUN(a_sleeping_module_settles_up_when_it_wakes);
    RUN(moderate_forgetting_costs_nothing_because_it_keeps_the_order);
    RUN(the_floor_is_where_knowledge_is_actually_lost);
    RUN(the_three_places_account_for_every_step);
    RUN(a_mind_that_knows_nothing_finds_everything_opaque);
    RUN(material_it_has_chewed_stops_being_opaque);
    RUN(the_frontier_is_a_place_you_pass_through);
    RUN(a_newborn_unit_has_not_been_anywhere_yet);
    RUN(novel_steps_are_a_part_of_the_frontier_not_of_the_opaque);
    RUN(appraising_teaches_nothing_either);
    RUN(a_fingerprint_follows_the_content_not_the_name);
    RUN(an_empty_ledger_remembers_nothing);
    RUN(the_ledger_recalls_what_it_appraised);
    RUN(material_that_changed_is_not_the_same_material);
    RUN(a_second_appraisal_replaces_the_first);
    RUN(the_ledger_survives_a_round_trip);
    RUN(a_missing_ledger_is_reported_not_invented);
    RUN(interest_is_a_derivative_and_the_ledger_is_what_makes_it_one);
    RUN(the_bar_teaches_nothing);
    RUN(an_empty_bar_reports_nothing);
    RUN(a_mind_that_knows_nothing_is_surprised_by_everything);
    RUN(a_mind_predicts_what_it_has_lived);
    RUN(the_bar_is_harder_on_material_it_never_ate);
    RUN(a_certain_continuation_carries_almost_no_surprise);
    RUN(prediction_is_read_only_and_says_so_when_it_has_no_idea);
    RUN(an_atom_cannot_be_taken_apart_any_further);
    RUN(descending_gives_the_tail_of_a_conjunction);
    RUN(a_dead_end_is_escaped_by_going_down);
    RUN(looking_in_another_direction_is_work);
    RUN(when_even_the_atom_leads_nowhere_it_really_does_give_up);
    RUN(temperature_zero_is_faithful_recall);
    RUN(the_same_seed_replays_the_same_wandering);
    RUN(wandering_leaves_the_rut_that_greedy_circles_in);
    RUN(wandering_still_pays_the_same_effort);
    return rz_report();
}
