#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "naval_sim.h"

/* Part 1-A assumptions (per spec):
 *  - Gun reload/fire time is 0 seconds (no delay between firings).
 *  - Each escort ship fires at most once.
 *  - A single shell impact destroys B.
 *  - A single shell impact destroys any E.
 *  - B can hit every E ship within its attack range.
 *  - Any E can hit B if B is within that E's attack range.
 *
 * Because everything fires "simultaneously" (0 reload time), we resolve it
 * as: B destroys every E in its range; independently, every E that has B in
 * its range gets a shot at B. If B is not sunk, we still assume the escort
 * ships that could hit B do sink B (that is the definition of Part 1-A) -
 * so if ANY E can hit B, B sinks. The E credited with the kill is the one
 * whose shell arrives first (minimum flight time).
 */
void run_part1a_simulation(Battlefield *bf, const char *outFilePrefix)
{
    char hitLogName[256], finalName[256], initialName[256];
    snprintf(initialName, sizeof(initialName), "%s_initial_conditions.txt", outFilePrefix);
    snprintf(hitLogName, sizeof(hitLogName), "%s_hits_by_B.txt", outFilePrefix);
    snprintf(finalName, sizeof(finalName), "%s_final_conditions.txt", outFilePrefix);

    save_initial_conditions(bf, initialName);

    int   escortsSunkByB = 0;
    double lastHitTime = 0.0;

    int    bIsSunk = 0;
    int    killerIndex = -1;
    double killerTime = -1.0;

    FILE *hitLog = fopen(hitLogName, "w");
    if (!hitLog) {
        fprintf(stderr, "Error: could not open %s for writing.\n", hitLogName);
        return;
    }
    fprintf(hitLog, "Escort ships hit by Battleship (Part 1-A)\n");
    fprintf(hitLog, "Index\tSpeedUsed\tAngleDeg\tDistance\tTimeToHit\n");

    /* 1. Battleship engages every escort ship within its range */
    for (int i = 0; i < bf->N; i++) {
        EscortShip *e = &bf->escorts[i];
        HitResult r = resolve_battleship_shot(bf, e);
        if (r.canHit) {
            e->destroyed = 1;
            escortsSunkByB++;
            if (r.flightTimeSec > lastHitTime) lastHitTime = r.flightTimeSec;
            fprintf(hitLog, "%d\t%.4f\t\t%.2f\t\t%.4f\t\t%.4f\n",
                    e->index, r.speedUsed, r.angleUsedDeg, r.distance, r.flightTimeSec);
        }
    }
    fclose(hitLog);

    /* 2. Every escort ship that has B within its own attack range fires back
          (an escort ship destroyed by B above still gets to fire in Part
          1-A, since all firing is simultaneous with 0 reload time). */
    for (int i = 0; i < bf->N; i++) {
        EscortShip *e = &bf->escorts[i];
        if (e->hasFired) continue;
        HitResult r = resolve_escort_shot(bf, e);
        if (r.canHit) {
            e->hasFired = 1;
            if (!bIsSunk || r.flightTimeSec < killerTime) {
                bIsSunk = 1;
                killerIndex = e->index;
                killerTime = r.flightTimeSec;
            }
        }
    }

    if (bIsSunk) {
        bf->battleship.destroyed = 1;
        printf("\n*** Battleship SUNK ***\n");
        printf("Sunk by escort ship index %d (shell flight time %.4f s)\n",
               killerIndex, killerTime);
    } else {
        printf("\nBattleship survives this scenario.\n");
        printf("Escort ships destroyed by Battleship: %d\n", escortsSunkByB);
        printf("Battle duration (longest shell flight time among B's hits): %.4f s\n",
               lastHitTime);
        printf("Per-hit detail saved to: %s\n", hitLogName);
    }

    save_final_conditions(bf, finalName, bIsSunk ? killerIndex : -1);
}

/* ================= Part 1-B ================= */

/* "Same initial conditions as simulation 1" (spec's wording for Simulation
 * 2) means: same escort ship positions/types, same path — only the
 * destroyed/hasFired flags (which change AS a simulation runs) need to be
 * cleared before re-running from scratch. */
void reset_escort_states(Battlefield *bf)
{
    for (int i = 0; i < bf->N; i++) {
        bf->escorts[i].destroyed = 0;
        bf->escorts[i].hasFired = 0;
        bf->escorts[i].impactFactorLeft = 1.0;
    }
    bf->battleship.destroyed = 0;
}

/* One "Part 1-A style" resolution step at the battleship's CURRENT
 * position, but respecting state carried over from earlier iterations:
 *  - an escort already destroyed is skipped entirely ("should not be
 *    present when simulating the rest of the iterations")
 *  - an escort that already fired in an earlier iteration does not fire
 *    again ("one E ship can only fire once" - this holds across the
 *    WHOLE simulation, not per iteration)
 * bThetaMinDeg/bThetaMaxDeg let the caller restrict B's firing angle
 * window, which is how the Part 1-B "Simulation 2" gun jam is applied.
 */
IterationResult run_battle_iteration(Battlefield *bf, FILE *logFile, int iterationNum,
                                      double bThetaMinDeg, double bThetaMaxDeg)
{
    IterationResult result = {0, 0, -1, -1.0};

    if (logFile) {
        fprintf(logFile, "--- Iteration %d: Battleship at (%.3f, %.3f) ---\n",
                iterationNum, bf->battleship.x, bf->battleship.y);
    }

    /* Battleship engages every surviving escort ship within range */
    for (int i = 0; i < bf->N; i++) {
        EscortShip *e = &bf->escorts[i];
        if (e->destroyed) continue;
        HitResult r = resolve_battleship_shot_ex(bf, e, bThetaMinDeg, bThetaMaxDeg);
        if (r.canHit) {
            e->destroyed = 1;
            result.escortsHitByB++;
            if (logFile) {
                fprintf(logFile, "  B destroys E%d (speed=%.3f angle=%.2f dist=%.3f t=%.4f)\n",
                        e->index, r.speedUsed, r.angleUsedDeg, r.distance, r.flightTimeSec);
            }
        }
    }

    /* Every escort ship that hasn't fired yet AND currently has B in its
       range fires back (still simultaneous with B's shots above, so a
       ship B just destroyed this same iteration still gets its shot in) */
    for (int i = 0; i < bf->N; i++) {
        EscortShip *e = &bf->escorts[i];
        if (e->hasFired) continue;
        HitResult r = resolve_escort_shot(bf, e);
        if (r.canHit) {
            e->hasFired = 1;
            if (logFile) {
                fprintf(logFile, "  E%d fires at B (speed=%.3f angle=%.2f dist=%.3f t=%.4f)\n",
                        e->index, r.speedUsed, r.angleUsedDeg, r.distance, r.flightTimeSec);
            }
            if (!result.battleshipSunk || r.flightTimeSec < result.killerFlightTime) {
                result.battleshipSunk = 1;
                result.killerIndex = e->index;
                result.killerFlightTime = r.flightTimeSec;
            }
        }
    }

    if (result.battleshipSunk) {
        bf->battleship.destroyed = 1;
        if (logFile) {
            fprintf(logFile, "  >>> Battleship SUNK by E%d (flight time %.4f s) <<<\n\n",
                    result.killerIndex, result.killerFlightTime);
        }
    } else if (logFile) {
        fprintf(logFile, "  Battleship survives this iteration. Escorts destroyed "
                "this iteration: %d\n\n", result.escortsHitByB);
    }

    return result;
}

/* Runs one full pass (either Simulation 1 or Simulation 2) along a given
 * path, iteration by iteration, until either the path is exhausted or B
 * is sunk. jamStartIteration <= 0 means "never jam" (Simulation 1);
 * otherwise B's firing angle window narrows to [jamThetaMinDeg, 90] from
 * that iteration number onward (Simulation 2). */
static void run_part1b_pass(Battlefield *bf, const Point *path, int k,
                             int jamStartIteration, double jamThetaMinDeg,
                             const char *outFilePrefix)
{
    char initialName[256], logName[256], finalName[256];
    snprintf(initialName, sizeof(initialName), "%s_initial_conditions.txt", outFilePrefix);
    snprintf(logName, sizeof(logName), "%s_iterations.txt", outFilePrefix);
    snprintf(finalName, sizeof(finalName), "%s_final_conditions.txt", outFilePrefix);

    /* Position B at the first waypoint BEFORE saving "initial conditions",
       so the saved file reflects where this pass actually starts rather
       than wherever B was left after a previous pass (e.g. Simulation 1's
       last point) - this is also what makes Simulation 1 and Simulation 2's
       initial-conditions files identical, as the spec requires. */
    if (k > 0) {
        bf->battleship.x = path[0].x;
        bf->battleship.y = path[0].y;
    }
    save_initial_conditions(bf, initialName);

    FILE *logFile = fopen(logName, "w");
    if (!logFile) { fprintf(stderr, "Error: could not open %s\n", logName); return; }

    if (jamStartIteration > 0) {
        fprintf(logFile, "Part 1-B Simulation 2: %d path points; gun jams to "
                "[%.2f, 90] deg vertical angle from iteration %d onward\n\n",
                k, jamThetaMinDeg, jamStartIteration);
    } else {
        fprintf(logFile, "Part 1-B Simulation 1: battleship moves along %d "
                "generated points\n\n", k);
    }

    int totalDestroyedByB = 0;
    int sunkAtIteration = -1;
    int killerIndex = -1;

    for (int i = 0; i < k; i++) {
        bf->battleship.x = path[i].x;
        bf->battleship.y = path[i].y;

        double thetaMin = 0.0, thetaMax = 90.0;
        if (jamStartIteration > 0 && (i + 1) >= jamStartIteration) {
            thetaMin = jamThetaMinDeg; /* gun has jammed by this iteration */
        }

        IterationResult r = run_battle_iteration(bf, logFile, i + 1, thetaMin, thetaMax);
        totalDestroyedByB += r.escortsHitByB;

        if (r.battleshipSunk) {
            sunkAtIteration = i + 1;
            killerIndex = r.killerIndex;
            break; /* "repeat ... until the B sinks due to a shell attack" */
        }
    }
    fclose(logFile);

    if (sunkAtIteration > 0) {
        printf("\n[%s] Battleship sunk at iteration %d of %d, by escort ship "
               "index %d.\n", outFilePrefix, sunkAtIteration, k, killerIndex);
    } else {
        printf("\n[%s] Battleship survived all %d iterations. Total escort "
               "ships destroyed by B: %d\n", outFilePrefix, k, totalDestroyedByB);
    }
    printf("[%s] Per-iteration detail saved to: %s\n", outFilePrefix, logName);

    save_final_conditions(bf, finalName, killerIndex);
}

void run_part1b_both_simulations(Battlefield *bf, int k, int t, double jamThetaMinDeg,
                                  const char *outFilePrefix)
{
    Point *path = malloc(sizeof(Point) * (size_t)k);
    if (!path) { fprintf(stderr, "Error: out of memory generating path.\n"); return; }
    generate_random_path(bf, path, k);

    char prefix1[256], prefix2[256];
    snprintf(prefix1, sizeof(prefix1), "%s_sim1", outFilePrefix);
    snprintf(prefix2, sizeof(prefix2), "%s_sim2", outFilePrefix);

    printf("\n=== Part 1-B: Simulation 1 (gun never jams) ===\n");
    run_part1b_pass(bf, path, k, -1, 0.0, prefix1);

    /* Re-run from scratch: same escort placements/types, same path,
       but the destroyed/hasFired state from Simulation 1 must be cleared. */
    reset_escort_states(bf);

    printf("\n=== Part 1-B: Simulation 2 (gun jams after iteration %d) ===\n", t);
    run_part1b_pass(bf, path, k, t, jamThetaMinDeg, prefix2);

    free(path);
}
