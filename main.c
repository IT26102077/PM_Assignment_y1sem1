#include <stdio.h>
#include "naval_sim.h"

/* NOTE: This is the Part 1-A driver. The full menu system (Start
 * Simulation / View Instructions / Simulation Statistics / Exit) will be
 * added once Part 2 is complete, so all parts can be reached from one menu.
 */
int main(void)
{
    Battlefield bf;

    setup_battlefield(&bf);
    run_part1a_simulation(&bf, "part1a_run1");

    printf("\nDone. Check part1a_run1_initial_conditions.txt, "
           "part1a_run1_hits_by_B.txt and part1a_run1_final_conditions.txt\n");
    return 0;
}
