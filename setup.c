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

static void setup_escort_type_params(Battlefield *bf)
{
    printf("\n--- Escort ship type parameters ---\n");
    printf("(Battleship max shell speed = %.3f. Per spec, EA's max speed is\n"
           " fixed at 1.2 x battleship max speed and is computed automatically.\n"
           " All other types must have max speed below the battleship's.)\n",
           bf->battleship.vMax);

    for (int i = 0; i < NUM_ESCORT_TYPES; i++) {
        const EscortTypeInfo *info = get_escort_type_info((EscortTypeId)i);
        EscortTypeParams *p = &bf->typeParams[i];

        printf("\nType E%c (%s) - fixed angle range: %.1f degrees\n",
               info->notation, info->typeName, info->angleRangeDeg);

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

        p->reloadDelay = 0.0; /* set later by setup_part2_extra_params (Part 2-B+) */
        p->gamma = 0.0;       /* set later by setup_part2_extra_params (Part 2-C)  */
    }
}

void setup_battlefield(Battlefield *bf)
{
    srand((unsigned int)time(NULL));

    printf("=== Naval Battle Simulator: Battlefield Setup ===\n\n");

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

    char bNotation = read_char_from_set("Battleship type (U = USS Iowa, "
                                         "M = King George V, R = Richelieu, "
                                         "S = Sovetsky Soyuz): ", "UMRS");
    bf->battleship.type = battleship_notation_to_id(bNotation);

    bf->battleship.x = read_positive_double("Battleship starting X coordinate: ");
    bf->battleship.y = read_positive_double("Battleship starting Y coordinate: ");
    bf->battleship.vMax = read_positive_double("Battleship maximum shell speed "
                                                "(V_max^B): ");
    bf->battleship.destroyed = 0;
    bf->battleship.healthFraction = 1.0;   /* full health until Part 1-C+ engages it */
    bf->battleship.reloadDelay = 0.0;      /* set later by setup_part2_extra_params  */
    bf->battleship.gamma = 0.0;            /* set later by setup_part2_extra_params  */
    bf->battleship.totalShotsFired = 0;

    setup_escort_type_params(bf);

    randomly_place_escort_ships(bf);

    printf("\nSetup complete: %d escort ships randomly placed on a %.2f x %.2f "
           "canvas.\n", bf->N, bf->D, bf->D);
}
