#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "naval_sim.h"

#define MAX_SHOTS_PER_ESCORT 20 /* safety cap on Part 2-B's repeated-fire loop */

/* ================= Setup ================= */

static void exit_on_eof_p2(int scanResult)
{
    if (scanResult == EOF) {
        fprintf(stderr, "\nInput ended unexpectedly. Exiting.\n");
        exit(EXIT_FAILURE);
    }
}

static double read_positive_double_p2(const char *prompt)
{
    double value;
    int result;
    do {
        printf("%s", prompt);
        while ((result = scanf("%lf", &value)) != 1) {
            exit_on_eof_p2(result);
            printf("Please enter a numeric value: ");
            while (getchar() != '\n');
        }
        if (value < 0) printf("Value must be >= 0.\n");
    } while (value < 0);
    return value;
}

/* Gathers T_B, gamma_B, and per-type T_E/gamma_E - all prompted, per your
 * earlier choice to always be asked rather than have these randomised. */
void setup_part2_extra_params(Battlefield *bf)
{
    printf("\n--- Part 2 setup: reload delays and impact-power decay ---\n");
    bf->battleship.reloadDelay = read_positive_double_p2(
        "Battleship reload delay T_B (seconds between shots): ");
    bf->battleship.gamma = read_positive_double_p2(
        "Battleship impact-power decay rate gamma_B (0 = no decay): ");

    for (int i = 0; i < NUM_ESCORT_TYPES; i++) {
        const EscortTypeInfo *info = get_escort_type_info((EscortTypeId)i);
        EscortTypeParams *p = &bf->typeParams[i];
        char buf[128];

        snprintf(buf, sizeof(buf), "E%c reload delay T_E (seconds between shots): ",
                 info->notation);
        p->reloadDelay = read_positive_double_p2(buf);

        snprintf(buf, sizeof(buf), "E%c impact-power decay rate gamma (0 = no decay): ",
                 info->notation);
        p->gamma = read_positive_double_p2(buf);
    }
}

/* ================= Shared: attack-order strategy ================= */

typedef struct {
    int    index;
    double impactPower;
    double distance;
} TargetCandidate;

/* Builds B's list of currently-live, in-range targets and orders them.
 *
 * STRATEGY (documented design decision, not claimed optimal - the spec
 * explicitly says it doesn't need to be): destroy the most dangerous gun
 * types first (descending impactPower), tie-broken by the easiest/closest
 * kill (ascending distance). Rationale: a high-impact-power escort left
 * alive is the biggest cumulative threat if it - or another target - gets
 * another chance at B later in the engagement or a future path point, so
 * eliminating the biggest threats first maximises damage dealt while the
 * distance tie-break converts as many kills as possible in the shots B
 * has time for. This is a greedy heuristic for what the spec calls an
 * NP-hard combinatorial problem; it is fast and easy to justify, at the
 * cost of not being provably optimal.
 */
static void build_attack_order(const Battlefield *bf, TargetCandidate *order, int *count)
{
    *count = 0;
    double maxRange = battleship_max_range(bf);

    for (int i = 0; i < bf->N; i++) {
        const EscortShip *e = &bf->escorts[i];
        if (e->destroyed) continue;
        double dist = distance_between(bf->battleship.x, bf->battleship.y, e->x, e->y);
        if (dist <= maxRange) {
            order[*count].index = e->index;
            order[*count].impactPower = get_escort_type_info(e->type)->impactPower;
            order[*count].distance = dist;
            (*count)++;
        }
    }

    /* Selection sort (roster sizes are small, so O(n^2) is fine and avoids
       qsort's lack of portable extra-context support in plain C11). */
    for (int i = 0; i < *count - 1; i++) {
        int best = i;
        for (int j = i + 1; j < *count; j++) {
            if (order[j].impactPower > order[best].impactPower ||
                (order[j].impactPower == order[best].impactPower &&
                 order[j].distance < order[best].distance)) {
                best = j;
            }
        }
        if (best != i) {
            TargetCandidate tmp = order[i];
            order[i] = order[best];
            order[best] = tmp;
        }
    }
}

static void log_attack_order(FILE *logFile, const TargetCandidate *order, int count)
{
    if (!logFile || count == 0) return;
    fprintf(logFile, "  Attack order chosen: ");
    for (int i = 0; i < count; i++) {
        fprintf(logFile, "E%d%s", order[i].index, (i < count - 1) ? ", " : "");
    }
    fprintf(logFile, "\n");
}

/* ================= Part 2-A =================
 * Redo of 1-A/1-B/1-C, adding: B needs T_B seconds between shots and fires
 * its targets in the strategy order above; escorts still fire their single
 * shot at t=0 exactly as in every earlier part. Because escort shots are
 * committed the instant the engagement begins regardless of what B does,
 * whether B survives is unaffected by reload delay - only HOW MANY targets
 * B manages to destroy before its own possible destruction changes.
 */
IterationResult run_battle_iteration_2a(Battlefield *bf, FILE *logFile, int iterationNum,
                                         double bThetaMinDeg, double bThetaMaxDeg)
{
    IterationResult result = {0, 0, -1, -1.0};
    if (logFile) {
        fprintf(logFile, "--- Iteration %d: B at (%.3f,%.3f) [health=%.3f, T_B=%.3f] ---\n",
                iterationNum, bf->battleship.x, bf->battleship.y,
                bf->battleship.healthFraction, bf->battleship.reloadDelay);
    }

    DamageEvent events[MAX_ESCORT_SHIPS];
    int ec = gather_first_volley_damage(bf, events, logFile);

    /* Find out (without yet applying it) when B would be sunk by this
       volley, so we know how many of B's own sequential shots get fired. */
    double tKill = compute_kill_time(bf, events, ec, NULL);

    TargetCandidate order[MAX_ESCORT_SHIPS];
    int oc;
    build_attack_order(bf, order, &oc);
    log_attack_order(logFile, order, oc);

    for (int i = 0; i < oc; i++) {
        double fireTime = i * bf->battleship.reloadDelay;
        if (tKill >= 0.0 && fireTime >= tKill) {
            if (logFile) {
                fprintf(logFile, "  [t=%.4f] B was sunk at t=%.4f - remaining planned "
                        "shots never fire\n", fireTime, tKill);
            }
            break;
        }
        EscortShip *target = &bf->escorts[order[i].index];
        if (target->destroyed) continue;
        HitResult r = resolve_battleship_shot_ex(bf, target, bThetaMinDeg, bThetaMaxDeg);
        if (r.canHit) {
            target->destroyed = 1;
            result.escortsHitByB++;
            if (logFile) {
                fprintf(logFile, "  [t=%.4f] B fires shot #%d at E%d -> lands t=%.4f, "
                        "destroyed\n", fireTime, i + 1, target->index,
                        fireTime + r.flightTimeSec);
            }
        }
    }

    /* Now actually apply the incoming damage for real. */
    apply_cumulative_damage(bf, events, ec, &result, logFile);
    if (result.battleshipSunk) bf->battleship.destroyed = 1;

    if (logFile) {
        if (result.battleshipSunk) {
            fprintf(logFile, "  >>> Battleship SUNK by E%d (t=%.4f) <<<\n\n",
                    result.killerIndex, result.killerFlightTime);
        } else {
            fprintf(logFile, "  Battleship survives this iteration. Escorts destroyed: "
                    "%d. Remaining health: %.3f\n\n", result.escortsHitByB,
                    bf->battleship.healthFraction);
        }
    }
    return result;
}

void run_part2a_simulations(Battlefield *bf, int k, int t, double jamThetaMinDeg,
                             const char *outFilePrefix)
{
    char staticPrefix[256], pathPrefix[256];
    snprintf(staticPrefix, sizeof(staticPrefix), "%s_static", outFilePrefix);
    snprintf(pathPrefix, sizeof(pathPrefix), "%s_path", outFilePrefix);

    printf("\n=== Part 2-A: redo of Part 1-A (B reload delay + attack order) ===\n");
    run_generic_static(bf, run_battle_iteration_2a, staticPrefix);
    reset_escort_states(bf);

    printf("\n=== Part 2-A: redo of Part 1-B (B reload delay + attack order) ===\n");
    run_generic_path(bf, k, t, jamThetaMinDeg, pathPrefix, run_battle_iteration_2a);
}

/* ================= Part 2-B =================
 * Builds on 2-A: escort ships can now fire repeatedly, every T_E^p seconds,
 * for as long as the engagement lasts and they remain alive and in range.
 * This makes B's actions and the escorts' actions genuinely interdependent
 * (an escort destroyed by B stops firing further repeats), so this uses a
 * proper chronological (discrete-event) resolution: every shot from both
 * sides is scheduled up front, then processed strictly in the order shells
 * actually land, checking that each shot's FIRER was still alive at the
 * moment it fired (not just when it lands) before applying its effect.
 */
typedef struct {
    double arrivalTime;
    double fireTime;
    int    isFromB;     /* 1 = B firing at an escort, 0 = escort firing at B */
    int    otherIndex;  /* target escort index (isFromB) or firer index (!isFromB) */
    double impactPower; /* only used for escort -> B events here (fixed value) */
} CombinedEvent;

static int compare_combined_events(const void *a, const void *b)
{
    const CombinedEvent *ea = (const CombinedEvent *)a;
    const CombinedEvent *eb = (const CombinedEvent *)b;
    if (ea->arrivalTime < eb->arrivalTime) return -1;
    if (ea->arrivalTime > eb->arrivalTime) return 1;
    return 0;
}

IterationResult run_battle_iteration_2b(Battlefield *bf, FILE *logFile, int iterationNum,
                                         double bThetaMinDeg, double bThetaMaxDeg)
{
    IterationResult result = {0, 0, -1, -1.0};
    if (logFile) {
        fprintf(logFile, "--- Iteration %d: B at (%.3f,%.3f) [health=%.3f, T_B=%.3f] ---\n",
                iterationNum, bf->battleship.x, bf->battleship.y,
                bf->battleship.healthFraction, bf->battleship.reloadDelay);
    }

    TargetCandidate order[MAX_ESCORT_SHIPS];
    int oc;
    build_attack_order(bf, order, &oc);
    log_attack_order(logFile, order, oc);

    /* Design decision: the engagement at this point/iteration lasts exactly
       as long as B's own planned campaign would take (one reload cycle per
       target); if B has no targets, allow one reload cycle anyway so any
       lurking threat still gets its first shot in. */
    double engagementDuration = (oc > 0 ? oc : 1) * bf->battleship.reloadDelay;
    if (engagementDuration <= 0.0) engagementDuration = 1.0;

    int maxEvents = oc + bf->N * MAX_SHOTS_PER_ESCORT;
    if (maxEvents < 1) maxEvents = 1;
    CombinedEvent *events = malloc(sizeof(CombinedEvent) * (size_t)maxEvents);
    if (!events) {
        fprintf(stderr, "Error: out of memory building Part 2-B event list.\n");
        return result;
    }
    int ec = 0;

    /* B's planned shots: exactly one per target, at fireTime = i * T_B */
    for (int i = 0; i < oc; i++) {
        EscortShip *target = &bf->escorts[order[i].index];
        HitResult r = resolve_battleship_shot_ex(bf, target, bThetaMinDeg, bThetaMaxDeg);
        double fireTime = i * bf->battleship.reloadDelay;
        events[ec].fireTime    = fireTime;
        events[ec].arrivalTime = fireTime + r.flightTimeSec;
        events[ec].isFromB     = 1;
        events[ec].otherIndex  = target->index;
        events[ec].impactPower = 0.0;
        ec++;
    }

    /* Escort ships currently threatening B fire repeatedly every T_E,
       within the engagement window, up to a safety cap on repeat count. */
    for (int i = 0; i < bf->N; i++) {
        EscortShip *e = &bf->escorts[i];
        if (e->destroyed) continue;
        HitResult r = resolve_escort_shot(bf, e);
        if (!r.canHit) continue;

        double reload = bf->typeParams[e->type].reloadDelay;
        if (reload <= 0.0) reload = engagementDuration + 1.0;
        const EscortTypeInfo *info = get_escort_type_info(e->type);

        int shots = 0;
        for (double fireTime = 0.0; fireTime < engagementDuration && shots < MAX_SHOTS_PER_ESCORT;
             fireTime += reload) {
            events[ec].fireTime    = fireTime;
            events[ec].arrivalTime = fireTime + r.flightTimeSec;
            events[ec].isFromB     = 0;
            events[ec].otherIndex  = e->index;
            events[ec].impactPower = info->impactPower;
            ec++;
            shots++;
        }
    }

    qsort(events, (size_t)ec, sizeof(CombinedEvent), compare_combined_events);

    /* Discovered as we walk events in arrival-time order. Since flight time
       is never negative, any death that happens strictly before a later
       event's fire time has necessarily already been processed by the time
       we reach that event - one forward pass is enough, no re-scanning. */
    double escortDestroyedAt[MAX_ESCORT_SHIPS];
    for (int i = 0; i < bf->N; i++) {
        escortDestroyedAt[i] = bf->escorts[i].destroyed ? -1.0 : 1e18;
    }
    double bDestroyedAt = 1e18;

    for (int idx = 0; idx < ec; idx++) {
        CombinedEvent *ev = &events[idx];
        if (ev->isFromB) {
            if (bDestroyedAt <= ev->fireTime) continue;
            int tIdx = ev->otherIndex;
            if (escortDestroyedAt[tIdx] <= ev->fireTime) continue;
            escortDestroyedAt[tIdx] = ev->arrivalTime;
            result.escortsHitByB++;
            if (logFile) {
                fprintf(logFile, "  [t=%.4f -> %.4f] B destroys E%d\n",
                        ev->fireTime, ev->arrivalTime, tIdx);
            }
        } else {
            int fIdx = ev->otherIndex;
            if (escortDestroyedAt[fIdx] <= ev->fireTime) continue;
            if (bDestroyedAt <= ev->fireTime) continue;
            bf->battleship.healthFraction -= ev->impactPower;
            if (logFile) {
                fprintf(logFile, "  [t=%.4f -> %.4f] E%d hits B for %.3f, health now %.3f\n",
                        ev->fireTime, ev->arrivalTime, fIdx, ev->impactPower,
                        bf->battleship.healthFraction);
            }
            if (bf->battleship.healthFraction <= 0.0 && bDestroyedAt >= 1e18) {
                bDestroyedAt = ev->arrivalTime;
                result.battleshipSunk = 1;
                result.killerIndex = fIdx;
                result.killerFlightTime = ev->arrivalTime;
            }
        }
    }
    free(events);

    for (int i = 0; i < bf->N; i++) {
        if (escortDestroyedAt[i] >= 0.0 && escortDestroyedAt[i] < 1e18) {
            bf->escorts[i].destroyed = 1;
        }
    }
    if (result.battleshipSunk) bf->battleship.destroyed = 1;

    if (logFile) {
        if (result.battleshipSunk) {
            fprintf(logFile, "  >>> Battleship SUNK by E%d (t=%.4f) <<<\n\n",
                    result.killerIndex, result.killerFlightTime);
        } else {
            fprintf(logFile, "  Battleship survives this iteration. Escorts destroyed: "
                    "%d. Remaining health: %.3f\n\n", result.escortsHitByB,
                    bf->battleship.healthFraction);
        }
    }
    return result;
}

void run_part2b_simulations(Battlefield *bf, int k, int t, double jamThetaMinDeg,
                             const char *outFilePrefix)
{
    char staticPrefix[256], pathPrefix[256];
    snprintf(staticPrefix, sizeof(staticPrefix), "%s_static", outFilePrefix);
    snprintf(pathPrefix, sizeof(pathPrefix), "%s_path", outFilePrefix);

    printf("\n=== Part 2-B: redo of Part 1-A (+ repeated escort fire) ===\n");
    run_generic_static(bf, run_battle_iteration_2b, staticPrefix);
    reset_escort_states(bf);

    printf("\n=== Part 2-B: redo of Part 1-B (+ repeated escort fire) ===\n");
    run_generic_path(bf, k, t, jamThetaMinDeg, pathPrefix, run_battle_iteration_2b);
}
