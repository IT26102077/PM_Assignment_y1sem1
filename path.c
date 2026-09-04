#include <stdlib.h>
#include "naval_sim.h"

/* Part 1-B, Simulation 1: "generate k number of points within the canvas...
 * student can use a random number generator to generate these points."
 * The order they're generated in IS the path/order the battleship visits
 * them in (no separate ordering step is needed).
 */
void generate_random_path(const Battlefield *bf, Point *path, int k)
{
    for (int i = 0; i < k; i++) {
        path[i].x = ((double)rand() / RAND_MAX) * bf->D;
        path[i].y = ((double)rand() / RAND_MAX) * bf->D;
    }
}
