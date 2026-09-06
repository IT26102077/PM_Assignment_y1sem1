#include <stdio.h>
#include <stdlib.h>
#include "naval_sim.h"

/* NOTE: This is the Part 1-A/1-B driver. The full menu system (Start
 * Simulation / View Instructions / Simulation Statistics / Exit) will be
 * added once Part 2 is complete, so all parts can be reached from one menu.
 */

/* Same EOF-safety rule as setup.c's readers: never spin forever if stdin
   runs out (piped input exhausted, or the user sends Ctrl+D). */
static void exit_on_eof_main(int scanResult)
{
    if (scanResult == EOF) {
        fprintf(stderr, "\nInput ended unexpectedly. Exiting.\n");
        exit(EXIT_FAILURE);
    }
}

static int read_int_in_range(const char *prompt, int lo, int hi)
{
    int value, result;
    do {
        printf("%s", prompt);
        while ((result = scanf("%d", &value)) != 1) {
            exit_on_eof_main(result);
            printf("Please enter an integer: ");
            while (getchar() != '\n');
        }
        if (value < lo || value > hi) {
            printf("Value must be between %d and %d.\n", lo, hi);
        }
    } while (value < lo || value > hi);
    return value;
}

static double read_double_in_range(const char *prompt, double lo, double hi)
{
    double value;
    int result;
    do {
        printf("%s", prompt);
        while ((result = scanf("%lf", &value)) != 1) {
            exit_on_eof_main(result);
            printf("Please enter a number: ");
            while (getchar() != '\n');
        }
        if (value <= lo || value >= hi) {
            printf("Value must be strictly between %.2f and %.2f.\n", lo, hi);
        }
    } while (value <= lo || value >= hi);
    return value;
}

int main(void)
{
    Battlefield bf;

    setup_battlefield(&bf);

    /* ---- Part 1-A: single static engagement at the setup position ---- */
    printf("\n=== Part 1-A: Single static engagement ===\n");
    run_part1a_simulation(&bf, "part1a_run1");

    /* Part 1-B reuses the same escort placements/types, so their
       destroyed/hasFired flags from Part 1-A must be cleared first. */
    reset_escort_states(&bf);

    /* ---- Part 1-B: battleship moves through k points ---- */
    printf("\n=== Part 1-B setup ===\n");
    int k = read_int_in_range("Number of path points k (2-1000): ", 2, 1000);
    int t = read_int_in_range("Iteration t at which the gun jams "
                               "(1 <= t < k): ", 1, k - 1);
    double jamThetaMin = read_double_in_range(
        "Jammed minimum vertical angle theta_min (0 < theta_min < 30): ", 0.0, 30.0);

    run_part1b_both_simulations(&bf, k, t, jamThetaMin, "part1b_run1");

    /* ---- Part 1-C: redo 1-A and 1-B with cumulative impact on B ---- */
    reset_escort_states(&bf);
    run_part1c_simulations(&bf, k, t, jamThetaMin, "part1c_run1");

    /* ---- Part 2-A: reload delay + attack order ---- */
    reset_escort_states(&bf);
    setup_part2_extra_params(&bf);
    run_part2a_simulations(&bf, k, t, jamThetaMin, "part2a_run1");
    reset_escort_states(&bf);

    /* ---- Part 2-B: repeated escort fire ---- */
    run_part2b_simulations(&bf, k, t, jamThetaMin, "part2b_run1");

    printf("\nDone. All Part 1 (A/B/C) and Part 2 (A/B) output files have "
           "been written to the current folder.\n");
    return 0;
}
