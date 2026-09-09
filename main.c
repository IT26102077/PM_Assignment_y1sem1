#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "naval_sim.h"

#if defined(__unix__) || defined(__APPLE__) || defined(__MINGW32__)
#define HAVE_DIRENT 1
#include <dirent.h>
#endif

/* ---------- RNG seed control (declared extern in naval_sim.h) ---------- */
unsigned int g_rngSeed   = 0;
int          g_seedIsSet = 0;

/* ---------- Program-wide state ---------- */
static Battlefield bf;

/* Setup completeness flags - Show simulation is only allowed once all
   three are true (matches the Setup submenu's three configuration
   items: Battlefield settings, Battleship Properties, Escort Settings). */
static int canvasReady    = 0;
static int battleshipReady = 0;
static int escortReady     = 0;

static int pathK = 0, pathT = 0;
static double jamThetaMin = 0.0;

/* ---------- Small input helpers ---------- */
static void exit_on_eof(int scanResult)
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
            exit_on_eof(result);
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
            exit_on_eof(result);
            printf("Please enter a number: ");
            while (getchar() != '\n');
        }
        if (value <= lo || value >= hi) {
            printf("Value must be strictly between %.2f and %.2f.\n", lo, hi);
        }
    } while (value <= lo || value >= hi);
    return value;
}

static char read_yes_no(const char *prompt)
{
    char c;
    int result;
    do {
        printf("%s (y/n): ", prompt);
        result = scanf(" %c", &c);
        exit_on_eof(result);
        c = (char)((c >= 'a' && c <= 'z') ? c - 32 : c);
    } while (c != 'Y' && c != 'N');
    return c;
}

/* ================= Setup Submenu ================= */

static void do_battlefield_settings(void)
{
    setup_canvas_and_place_escorts(&bf);
    canvasReady = 1;
    /* Escort positions/types are freshly re-randomised - clear any
       stale destroyed/hasFired state from a previous simulation. */
    reset_escort_states(&bf);
}

static void do_battleship_properties(void)
{
    setup_battleship_properties(&bf);
    battleshipReady = 1;
    /* EA's max speed depends on the battleship's VMax (fixed by spec as
       1.2x), so if Escort Settings were already configured against an
       old VMax, they are now stale and must be redone. */
    if (escortReady) {
        printf("\nNote: Battleship VMax changed - please redo 'Escort ship Settings' "
               "so EA's speed (1.2 x VMax) and the VMax-must-be-less-than checks "
               "are recalculated correctly.\n");
        escortReady = 0;
    }
}

static void do_escort_settings(void)
{
    if (!battleshipReady) {
        printf("\nPlease complete 'Battleship Properties' first - Escort ship "
               "Settings depends on the battleship's maximum shell speed.\n");
        return;
    }
    setup_escort_settings(&bf);
    escortReady = 1;
}

static void do_seed_setup(void)
{
    long seed = read_int_in_range("Enter RNG seed value (0-2147483647): ", 0, 2147483647);
    g_rngSeed = (unsigned int)seed;
    g_seedIsSet = 1;
    printf("Seed set to %u. It will be used the next time you run "
           "'Battlefield settings' (which places the escort ships).\n", g_rngSeed);
}

static void setup_submenu(void)
{
    int choice;
    do {
        printf("\n--- Setup Submenu ---\n");
        printf("Status: Battlefield=%s | Battleship Properties=%s | Escort Settings=%s\n",
               canvasReady ? "OK" : "not set", battleshipReady ? "OK" : "not set",
               escortReady ? "OK" : "not set");
        printf("1. Battlefield settings (canvas size D, number of escort ships N)\n");
        printf("2. Battleship Properties (type, position, VMax, T_B, gamma_B)\n");
        printf("3. Escort ship Settings (per-type speed/angle/T_E/gamma)\n");
        printf("4. Seed value (random number generator)\n");
        printf("5. Return to Start Simulation menu\n");
        choice = read_int_in_range("Choose an option (1-5): ", 1, 5);

        switch (choice) {
            case 1: do_battlefield_settings();   break;
            case 2: do_battleship_properties();  break;
            case 3: do_escort_settings();        break;
            case 4: do_seed_setup();             break;
            case 5: /* return */                 break;
        }
    } while (choice != 5);
}

/* ================= Show simulation ================= */

static void do_show_simulation(void)
{
    if (!canvasReady || !battleshipReady || !escortReady) {
        printf("\nSetup is incomplete. Still needed:\n");
        if (!canvasReady)     printf("  - Battlefield settings\n");
        if (!battleshipReady) printf("  - Battleship Properties\n");
        if (!escortReady)     printf("  - Escort ship Settings\n");
        return;
    }

    printf("\n--- Path parameters (used by Part 1-B onward) ---\n");
    pathK = read_int_in_range("Number of path points k (2-1000): ", 2, 1000);
    pathT = read_int_in_range("Iteration t at which the gun jams (1 <= t < k): ", 1, pathK - 1);
    jamThetaMin = read_double_in_range(
        "Jammed minimum vertical angle theta_min (0 < theta_min < 30): ", 0.0, 30.0);

    printf("\n=== Part 1-A: Single static engagement ===\n");
    run_part1a_simulation(&bf, "part1a_run1");

    reset_escort_states(&bf);
    printf("\n=== Part 1-B: Battleship moving through path ===\n");
    run_part1b_both_simulations(&bf, pathK, pathT, jamThetaMin, "part1b_run1");

    reset_escort_states(&bf);
    printf("\n=== Part 1-C: Cumulative impact power ===\n");
    run_part1c_simulations(&bf, pathK, pathT, jamThetaMin, "part1c_run1");

    reset_escort_states(&bf);
    printf("\n=== Part 2-A: Reload delay + attack order ===\n");
    run_part2a_simulations(&bf, pathK, pathT, jamThetaMin, "part2a_run1");

    reset_escort_states(&bf);
    printf("\n=== Part 2-B: Repeated escort fire ===\n");
    run_part2b_simulations(&bf, pathK, pathT, jamThetaMin, "part2b_run1");

    reset_escort_states(&bf);
    printf("\n=== Part 2-C: Impact-power degradation ===\n");
    run_part2c_simulations(&bf, pathK, pathT, jamThetaMin, "part2c_run1");

    printf("\nSimulation complete. All part1a/1b/1c/2a/2b/2c_run1_*.txt files "
           "have been written to the current folder.\n"
           "Use 'Simulation Statistics' from the Main Menu to view them.\n");
}

/* ================= Start Simulation menu ================= */

static void start_simulation_menu(void)
{
    int choice;
    do {
        printf("\n--- Start Simulation ---\n");
        printf("1. Setup\n");
        printf("2. Show simulation\n");
        printf("3. Return to Main Menu\n");
        choice = read_int_in_range("Choose an option (1-3): ", 1, 3);

        switch (choice) {
            case 1: setup_submenu();      break;
            case 2: do_show_simulation(); break;
            case 3: /* return */          break;
        }
    } while (choice != 3);
}

/* ================= View Instructions ================= */

static void view_instructions(void)
{
    printf(
        "\n================= Naval Battle Simulator: Instructions =================\n"
        "This simulator models a single stationary battleship (B) engaging\n"
        "multiple stationary escort ships (E) on a square 2D battlefield,\n"
        "using standard projectile-motion equations for shell trajectories.\n\n"
        "How to run a simulation:\n"
        "  1. From the Main Menu choose 'Start Simulation' -> 'Setup', and\n"
        "     complete all three configuration steps, in this order:\n"
        "     a) Battlefield settings - canvas size D and number of escort\n"
        "        ships N (this randomly places the escort ships).\n"
        "     b) Battleship Properties - type, starting position, max shell\n"
        "        speed, reload delay T_B, and gun-wear rate gamma_B.\n"
        "     c) Escort ship Settings - for each of the 5 escort types: shell\n"
        "        speed range, minimum firing angle, reload delay T_E, and\n"
        "        gun-wear rate gamma (requires Battleship Properties first,\n"
        "        since E_A's max speed is fixed at 1.2x the battleship's).\n"
        "     You may optionally set a fixed RNG seed (Setup > Seed value)\n"
        "     before running Battlefield settings, for a repeatable layout.\n"
        "  2. Choose 'Start Simulation' -> 'Show simulation' and enter the\n"
        "     path parameters (number of path points, when the gun jams,\n"
        "     and the jammed minimum angle).\n"
        "  3. The program runs Part 1-A, 1-B, 1-C, 2-A, 2-B and 2-C in\n"
        "     sequence and saves each stage's results to text files in\n"
        "     the current folder.\n"
        "  4. Use 'Simulation Statistics' from the Main Menu at any time\n"
        "     to browse and view the saved result files.\n\n"
        "Notes:\n"
        "  - Escort ship positions and types are randomly generated per\n"
        "    the assignment spec, not entered manually.\n"
        "  - All three Setup steps must be completed before 'Show\n"
        "    simulation' will run.\n"
        "===========================================================================\n"
    );
}

/* ================= Simulation Statistics ================= */

#ifdef HAVE_DIRENT
static int list_txt_files(char names[][256], int maxNames)
{
    DIR *d = opendir(".");
    struct dirent *entry;
    int count = 0;

    if (!d) {
        printf("Could not open the current directory.\n");
        return 0;
    }

    while ((entry = readdir(d)) != NULL && count < maxNames) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".txt") == 0) {
            strncpy(names[count], entry->d_name, 255);
            names[count][255] = '\0';
            count++;
        }
    }
    closedir(d);
    return count;
}
#endif

static void display_file(const char *filename)
{
    FILE *f = fopen(filename, "r");
    char line[512];

    if (!f) {
        printf("Could not open '%s'.\n", filename);
        return;
    }

    printf("\n----- %s -----\n", filename);
    while (fgets(line, sizeof(line), f)) {
        fputs(line, stdout);
    }
    printf("----- End of %s -----\n", filename);
    fclose(f);
}

static void simulation_statistics(void)
{
#ifdef HAVE_DIRENT
    char names[256][256];
    int count = list_txt_files(names, 256);

    if (count == 0) {
        printf("\nNo saved simulation result files (*.txt) found in the "
               "current folder yet. Run a simulation first.\n");
        return;
    }

    printf("\n--- Saved Simulation Result Files ---\n");
    for (int i = 0; i < count; i++) {
        printf("%2d. %s\n", i + 1, names[i]);
    }
    printf("%2d. Return to Main Menu\n", count + 1);

    int choice = read_int_in_range("Choose a file to view: ", 1, count + 1);
    if (choice <= count) {
        display_file(names[choice - 1]);
    }
#else
    char filename[256];
    printf("\nEnter the exact filename to view (e.g. part1a_run1_final_conditions.txt),\n"
           "or press Enter to cancel: ");
    while (getchar() != '\n');
    if (fgets(filename, sizeof(filename), stdin)) {
        filename[strcspn(filename, "\n")] = '\0';
        if (strlen(filename) > 0) {
            display_file(filename);
        }
    }
#endif
}

/* ================= Exit ================= */

static int confirm_exit(void)
{
    char c = read_yes_no("Are you sure you want to exit the simulator?");
    return (c == 'Y');
}

/* ================= Main Menu ================= */

int main(void)
{
    int choice;
    int running = 1;

    printf("=====================================================\n");
    printf("   SLIIT SE1012 - Advanced Naval Battle Simulator\n");
    printf("=====================================================\n");

    while (running) {
        printf("\n===== Main Menu =====\n");
        printf("1. Start Simulation\n");
        printf("2. View Instructions\n");
        printf("3. Simulation Statistics\n");
        printf("4. Exit\n");
        choice = read_int_in_range("Choose an option (1-4): ", 1, 4);

        switch (choice) {
            case 1: start_simulation_menu(); break;
            case 2: view_instructions();     break;
            case 3: simulation_statistics(); break;
            case 4:
                if (confirm_exit()) {
                    running = 0;
                }
                break;
        }
    }

    printf("\nExiting Naval Battle Simulator. Goodbye.\n");
    return 0;
}
