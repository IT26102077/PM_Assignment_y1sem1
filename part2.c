#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "naval_sim.h"

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
