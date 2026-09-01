#ifndef NAVAL_SIM_H
#define NAVAL_SIM_H

#include <stdio.h>

/* ---------- Physical / simulation constants ---------- */
#define GRAVITY 9.81
#define PI 3.14159265358979323846
#define MAX_ESCORT_SHIPS 200   /* upper bound for array sizing */
#define NUM_ESCORT_TYPES 5

/* ---------- Escort ship types ---------- */
typedef enum {
    ESCORT_A = 0,  /* 1936A-class Destroyer   */
    ESCORT_B = 1,  /* Gabbiano-class Corvette */
    ESCORT_C = 2,  /* Matsu-class Destroyer   */
    ESCORT_D = 3,  /* F-class Escort Ship     */
    ESCORT_E = 4   /* Japanese Kaibokan       */
} EscortTypeId;

/* ---------- Battleship types ---------- */
typedef enum {
    BATTLESHIP_U = 0, /* USS Iowa (BB-61)   */
    BATTLESHIP_M = 1, /* MS King George V   */
    BATTLESHIP_R = 2, /* Richelieu          */
    BATTLESHIP_S = 3  /* Sovetsky Soyuz-class */
} BattleshipTypeId;

/* Fixed per-type data (Table 1). Angle range and impact power are FIXED
   by the spec; velocity range and theta_L are configured at setup time. */
typedef struct {
    char        notation;
    const char *typeName;
    const char *gunName;
    double      impactPower;   /* fraction of B destroyed per hit (Part 1-C) */
    double      angleRangeDeg; /* theta_H - theta_L, FIXED by spec table     */
} EscortTypeInfo;

typedef struct {
    char        notation;
    const char *typeName;
    const char *gunName;
} BattleshipTypeInfo;

/* Per-type tunable parameters gathered during setup (one entry per type) */
typedef struct {
    double vMin;       /* minimum shell speed for this escort type   */
    double vMax;        /* maximum shell speed for this escort type   */
    double thetaLDeg;   /* minimum vertical firing angle (degrees)    */
    double thetaHDeg;   /* = thetaLDeg + fixed angle range (degrees)  */
} EscortTypeParams;

/* A single escort ship instance on the battlefield */
typedef struct {
    int    index;          /* unique identifier                       */
    EscortTypeId type;
    double x, y;            /* position on the canvas                  */
    int    destroyed;       /* 1 if sunk                                */
    int    hasFired;        /* Part 1-A/B/C: can only fire once         */
    double impactFactorLeft;/* remaining "health" fraction (1.0 = full) */
} EscortShip;

/* The single battleship on the battlefield */
typedef struct {
    BattleshipTypeId type;
    double x, y;
    double vMax;             /* max shell speed (min is always 0)       */
    int    destroyed;
    double healthFraction;   /* remaining "health" fraction, Part 1-C+  */
} Battleship;

/* Result of checking whether a shooter can hit a target */
typedef struct {
    int    canHit;
    double distance;
    double speedUsed;
    double angleUsedDeg;
    double flightTimeSec;
} HitResult;

/* A single (x,y) waypoint on the battleship's movement path (Part 1-B) */
typedef struct {
    double x, y;
} Point;

/* Summary of one battle iteration (Part 1-B: one point along B's path) */
typedef struct {
    int    escortsHitByB;      /* how many E's B destroyed this iteration */
    int    battleshipSunk;     /* 1 if B was sunk this iteration          */
    int    killerIndex;        /* index of the E that sank B, or -1       */
    double killerFlightTime;   /* flight time of the killing shot         */
} IterationResult;

/* One incoming shell that will deal cumulative damage to B (Part 1-C+).
   Used so a batch of simultaneous escort hits can be resolved in the
   correct chronological (arrival-time) order rather than all at once. */
typedef struct {
    int    escortIndex;
    double impactPower;   /* damage this specific shell deals (post any decay) */
    double arrivalTime;   /* absolute time this shell lands                    */
} DamageEvent;

/* The whole battlefield / scenario state */
typedef struct {
    double D;                 /* canvas is a D x D square, origin (0,0) */
    int    N;                 /* number of escort ships                 */
    Battleship battleship;
    EscortTypeParams typeParams[NUM_ESCORT_TYPES];
    EscortShip escorts[MAX_ESCORT_SHIPS];
} Battlefield;

/* ---------- ship_data.c ---------- */
const EscortTypeInfo    *get_escort_type_info(EscortTypeId t);
const BattleshipTypeInfo *get_battleship_type_info(BattleshipTypeId t);
EscortTypeId escort_notation_to_id(char notation);
BattleshipTypeId battleship_notation_to_id(char notation);

/* ---------- setup.c ---------- */
void setup_battlefield(Battlefield *bf);

/* ---------- physics.c ---------- */
double deg2rad(double deg);
double distance_between(double x1, double y1, double x2, double y2);
double projectile_range(double speed, double angleDeg);
double projectile_flight_time(double speed, double angleDeg);
double battleship_max_range(const Battlefield *bf);
void   escort_attack_range(const Battlefield *bf, const EscortShip *e,
                            double *rMin, double *rMax);
HitResult resolve_battleship_shot(const Battlefield *bf, const EscortShip *e);
HitResult resolve_battleship_shot_ex(const Battlefield *bf, const EscortShip *e,
                                      double thetaMinDeg, double thetaMaxDeg);
HitResult resolve_escort_shot(const Battlefield *bf, const EscortShip *e);

/* ---------- path.c ---------- */
void generate_random_path(const Battlefield *bf, Point *path, int k);

/* A "battle iteration" function resolves everything that happens at ONE
   static battlefield snapshot (a fixed B position). Part 1-B/C and Part 2
   each define their own such function (single-hit vs cumulative damage,
   with/without reload delays); this typedef lets the path-walking and
   static-engagement drivers stay generic and reusable across all of them
   instead of duplicating the driving loop for every part. */
typedef IterationResult (*BattleIterationFunc)(Battlefield *bf, FILE *logFile,
                                                int iterationNum,
                                                double bThetaMinDeg,
                                                double bThetaMaxDeg);

/* ---------- simulate.c ---------- */
void reset_escort_states(Battlefield *bf);
IterationResult run_battle_iteration(Battlefield *bf, FILE *logFile, int iterationNum,
                                      double bThetaMinDeg, double bThetaMaxDeg);
IterationResult run_battle_iteration_1c(Battlefield *bf, FILE *logFile, int iterationNum,
                                         double bThetaMinDeg, double bThetaMaxDeg);
int  gather_first_volley_damage(Battlefield *bf, DamageEvent *events, FILE *logFile);
void apply_cumulative_damage(Battlefield *bf, DamageEvent *events, int count,
                              IterationResult *result, FILE *logFile);
void run_generic_static(Battlefield *bf, BattleIterationFunc iterFunc,
                         const char *outFilePrefix);
void run_generic_path(Battlefield *bf, int k, int t, double jamThetaMinDeg,
                       const char *outFilePrefix, BattleIterationFunc iterFunc);
void run_part1a_simulation(Battlefield *bf, const char *outFilePrefix);
void run_part1b_both_simulations(Battlefield *bf, int k, int t, double jamThetaMinDeg,
                                  const char *outFilePrefix);
void run_part1c_simulations(Battlefield *bf, int k, int t, double jamThetaMinDeg,
                             const char *outFilePrefix);

/* ---------- fileio.c ---------- */
void save_initial_conditions(const Battlefield *bf, const char *filename);
void save_final_conditions(const Battlefield *bf, const char *filename,
                            int sunkByEscortIndex);

#endif
