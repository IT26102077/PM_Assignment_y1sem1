#include <stdio.h>

#include <stdlib.h>

#include <time.h>

#include <string.h>

#include <ctype.h>

#include "naval_sim.h"



/* Every blocking read below treats EOF/unreadable stdin as fatal instead of

 * looping forever, so the program can never hang on bad or exhausted input

 * (e.g. piped input running out, or the user sending Ctrl+D). */

static void exit_on_eof(int scanResult)

{

    if (scanResult == EOF) {

        fprintf(stderr, "\nInput ended unexpectedly. Exiting.\n");

        exit(EXIT_FAILURE);

    }

}



static double read_positive_double(const char *prompt)

{

    double value;

    int result;

    do {

        printf("%s", prompt);

        while ((result = scanf("%lf", &value)) != 1) {

            exit_on_eof(result);

            printf("Please enter a numeric value: ");

            while (getchar() != '\n'); /* clear bad input */

        }

        if (value < 0) printf("Value must be >= 0.\n");

    } while (value < 0);

    return value;

}



/* Inclusive-range double reader, used for coordinates that must fall

   within the canvas [0, D] (unlike other inputs, both endpoints are

   valid here: a ship can legitimately sit exactly on the edge). */

static double read_double_range_incl(const char *prompt, double lo, double hi)

{

    double value;

    int result;

    do {

        printf("%s", prompt);

        while ((result = scanf("%lf", &value)) != 1) {

            exit_on_eof(result);

            printf("Please enter a numeric value: ");

            while (getchar() != '\n');

        }

        if (value < lo || value > hi) {

            printf("Value must be between %.3f and %.3f.\n", lo, hi);

        }

    } while (value < lo || value > hi);

    return value;

}



static char read_char_from_set(const char *prompt, const char *validChars)

{

    char c;

    int valid, result;

    do {

        printf("%s", prompt);

        result = scanf(" %c", &c);

        exit_on_eof(result);

        valid = (strchr(validChars, (int)toupper((unsigned char)c)) != NULL) ||

                (strchr(validChars, (int)tolower((unsigned char)c)) != NULL);

        if (!valid) printf("Invalid option. Choose one of: %s\n", validChars);

    } while (!valid);

    return (char)toupper((unsigned char)c);

}



/* ================= Setup Submenu item: Battlefield settings ================= */



/* Escort ship (x,y) position and TYPE assignment are mandated by the spec

   to be randomly generated, not user-supplied (see "Main features" list). */

static void randomly_place_escort_ships(Battlefield *bf)

{

    for (int i = 0; i < bf->N; i++) {

        EscortShip *e = &bf->escorts[i];

        e->index = i;

        e->x = ((double)rand() / RAND_MAX) * bf->D;

        e->y = ((double)rand() / RAND_MAX) * bf->D;

        e->type = (EscortTypeId)(rand() % NUM_ESCORT_TYPES);

        e->destroyed = 0;

        e->hasFired = 0;

        e->impactFactorLeft = 1.0;

    }

}



/* Asks for the canvas size D and the number of escort ships N, then

   randomly places N escort ships on the D x D canvas. This step only

   needs D and N, so it is independent of Battleship Properties / Escort

   Settings and can be (re)run on its own from the Setup submenu. */

void setup_canvas_and_place_escorts(Battlefield *bf)

{

    /* Honour a user-supplied seed (set via Setup > Seed value) so

       simulations can be reproduced; otherwise use a time-based seed. */

    if (g_seedIsSet) {

        srand(g_rngSeed);

    } else {

        srand((unsigned int)time(NULL));

    }



    printf("\n--- Battlefield Settings ---\n");

    bf->D = read_positive_double("Canvas size D (battlefield is D x D, "

                                  "corners (0,0) to (D,D)): ");



    do {

        int result;

        printf("Number of escort ships N (1-%d): ", MAX_ESCORT_SHIPS);

        while ((result = scanf("%d", &bf->N)) != 1) {

            exit_on_eof(result);

            printf("Please enter an integer: ");

            while (getchar() != '\n');

        }

        if (bf->N < 1 || bf->N > MAX_ESCORT_SHIPS) {

            printf("N must be between 1 and %d.\n", MAX_ESCORT_SHIPS);

        }

    } while (bf->N < 1 || bf->N > MAX_ESCORT_SHIPS);



    randomly_place_escort_ships(bf);



    printf("Battlefield ready: %d escort ships randomly placed on a "

           "%.2f x %.2f canvas.\n", bf->N, bf->D, bf->D);

}



/* ================= Setup Submenu item: Battleship Properties ================= */



/* Type/notation, starting position (validated against the canvas),

   maximum shell speed, reload delay T_B, and gun-wear gamma_B - matches

   the spec's "Battleship Properties: Notation used (Type), Gamma values"

   Setup submenu item (T_B and starting position are also gathered here

   since they are properties of the battleship itself). */

void setup_battleship_properties(Battlefield *bf)

{

    printf("\n--- Battleship Properties ---\n");



    char bNotation = read_char_from_set("Battleship type (U = USS Iowa, "

                                         "M = King George V, R = Richelieu, "

                                         "S = Sovetsky Soyuz): ", "UMRS");

    bf->battleship.type = battleship_notation_to_id(bNotation);



    if (bf->D > 0.0) {

        bf->battleship.x = read_double_range_incl("Battleship starting X coordinate "

                                                    "(0 to D): ", 0.0, bf->D);

        bf->battleship.y = read_double_range_incl("Battleship starting Y coordinate "

                                                    "(0 to D): ", 0.0, bf->D);

    } else {

        printf("(Canvas size D not set yet - enter Battlefield Settings first for "

               "coordinate bounds checking. Accepting unbounded values for now.)\n");

        bf->battleship.x = read_positive_double("Battleship starting X coordinate: ");

        bf->battleship.y = read_positive_double("Battleship starting Y coordinate: ");

    }



    bf->battleship.vMax = read_positive_double("Battleship maximum shell speed "

                                                "(V_max^B): ");

    bf->battleship.reloadDelay = read_positive_double(

        "Battleship reload delay T_B (seconds between shots): ");

    bf->battleship.gamma = read_positive_double(

        "Battleship gun-wear decay rate gamma_B (0 = no decay): ");



    bf->battleship.destroyed = 0;

    bf->battleship.healthFraction = 1.0;

    bf->battleship.totalShotsFired = 0;



    printf("Battleship properties set: Type=%c, Position=(%.2f, %.2f), "

           "VMax=%.2f, T_B=%.2f, gamma_B=%.4f\n",

           bNotation, bf->battleship.x, bf->battleship.y, bf->battleship.vMax,

           bf->battleship.reloadDelay, bf->battleship.gamma);

}



/* ================= Setup Submenu item: Escort ship Settings ================= */



/* Per escort type: min/max shell speed, minimum vertical angle theta_L

   (theta_H is derived from the fixed angle range in Table 1), reload

   delay T_E, and gun-wear gamma. Impact power and angle RANGE are fixed

   by the spec's Table 1 and are only displayed here, not asked for.

   E_A's max speed is fixed by spec as 1.2 x battleship's V_max^B, so

   Battleship Properties must be set first. */

void setup_escort_settings(Battlefield *bf)

{

    printf("\n--- Escort Ship Settings ---\n");

    printf("(Battleship max shell speed = %.3f. Per spec, EA's max speed is\n"

           " fixed at 1.2 x battleship max speed and is computed automatically.\n"

           " All other types must have max speed below the battleship's.)\n",

           bf->battleship.vMax);



    for (int i = 0; i < NUM_ESCORT_TYPES; i++) {

        const EscortTypeInfo *info = get_escort_type_info((EscortTypeId)i);

        EscortTypeParams *p = &bf->typeParams[i];



        printf("\nType E%c (%s) - fixed angle range: %.1f degrees, "

               "fixed impact power: %.2f\n",

               info->notation, info->typeName, info->angleRangeDeg, info->impactPower);



        char buf[128];



        snprintf(buf, sizeof(buf), "  Minimum shell speed for E%c: ", info->notation);

        p->vMin = read_positive_double(buf);



        if (i == ESCORT_A) {

            p->vMax = 1.2 * bf->battleship.vMax;

            printf("  Maximum shell speed for E%c (fixed by spec) = %.3f\n",

                   info->notation, p->vMax);

        } else {

            do {

                snprintf(buf, sizeof(buf),

                         "  Maximum shell speed for E%c (must be < %.3f): ",

                         info->notation, bf->battleship.vMax);

                p->vMax = read_positive_double(buf);

                if (p->vMax >= bf->battleship.vMax) {

                    printf("  Must be strictly less than the battleship's max speed.\n");

                }

            } while (p->vMax >= bf->battleship.vMax);

        }



        if (p->vMin > p->vMax) {

            printf("  Min speed exceeded max; swapping them.\n");

            double tmp = p->vMin; p->vMin = p->vMax; p->vMax = tmp;

        }



        do {

            snprintf(buf, sizeof(buf),

                     "  Minimum vertical angle theta_L for E%c (0-%.1f): ",

                     info->notation, 90.0 - info->angleRangeDeg);

            p->thetaLDeg = read_positive_double(buf);

            if (p->thetaLDeg + info->angleRangeDeg > 90.0) {

                printf("  theta_L + angle range must not exceed 90 degrees.\n");

            }

        } while (p->thetaLDeg + info->angleRangeDeg > 90.0);



        p->thetaHDeg = p->thetaLDeg + info->angleRangeDeg;

        printf("  -> theta_H for E%c = %.2f degrees\n", info->notation, p->thetaHDeg);



        snprintf(buf, sizeof(buf), "  E%c reload delay T_E (seconds between shots): ",

                 info->notation);

        p->reloadDelay = read_positive_double(buf);



        snprintf(buf, sizeof(buf), "  E%c gun-wear decay rate gamma (0 = no decay): ",

                 info->notation);

        p->gamma = read_positive_double(buf);

    }



    printf("\nEscort ship settings complete for all %d types.\n", NUM_ESCORT_TYPES);

}
