#include <stdio.h>
#include <string.h>
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
        printf("Battleship SUNK \n");
        printf("Sunk by escort ship index %d (shell flight time %.4f s)\n",
               killerIndex, killerTime);
    } else {
        printf("Battleship survives this scenario.\n");
        printf("Escort ships destroyed by Battleship: %d\n", escortsSunkByB);
        printf("Battle duration (longest shell flight time among B's hits): %.4f s\n",
               lastHitTime);
        printf("Per-hit detail saved to: %s\n", hitLogName);
    }

    save_final_conditions(bf, finalName, bIsSunk ? killerIndex : -1);
}
