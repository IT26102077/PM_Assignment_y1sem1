#ifndef NAVAL_SIM_H
#define NAVAL_SIM_H

#include <stdio.h>

/* ---------- Physical / simulation constants ---------- */
#define GRAVITY 9.81
#define PI 3.14
#define MAX_ESCORT_SHIPS 200   /* upper bound for array sizing */
#define NUM_ESCORT_TYPES 5

//Escort ship types
typedef enum {
    ESCORT_A = 0,  /* 1936A-class Destroyer   */
    ESCORT_B = 1,  /* Gabbiano-class Corvette */
    ESCORT_C = 2,  /* Matsu-class Destroyer   */
    ESCORT_D = 3,  /* F-class Escort Ship     */
    ESCORT_E = 4   /* Japanese Kaibokan       */
} EscortTypeId;

// Battleship types
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
} Battleship;

/* Result of checking whether a shooter can hit a target */
typedef struct {
    int    canHit;
    double distance;
    double speedUsed;
    double angleUsedDeg;
    double flightTimeSec;
} HitResult;

/* The whole battlefield / scenario state */
typedef struct {
    double D;                 /* canvas is a D x D square, origin (0,0) */
    int    N;                 /* number of escort ships                 */
    Battleship battleship;
    EscortTypeParams typeParams[NUM_ESCORT_TYPES];
    EscortShip escorts[MAX_ESCORT_SHIPS];
} Battlefield;

// function prototypes for ship_data.c
const EscortTypeInfo    *get_escort_type_info(EscortTypeId t);
const BattleshipTypeInfo *get_battleship_type_info(BattleshipTypeId t);
EscortTypeId escort_notation_to_id(char notation);
BattleshipTypeId battleship_notation_to_id(char notation);

// function prototypes for setup.c
void setup_battlefield(Battlefield *bf);

// function prototypes for physics.c
double deg2rad(double deg);
double distance_between(double x1, double y1, double x2, double y2);
double projectile_range(double speed, double angleDeg);
double projectile_flight_time(double speed, double angleDeg);
double battleship_max_range(const Battlefield *bf);
void   escort_attack_range(const Battlefield *bf, const EscortShip *e,
                            double *rMin, double *rMax);
                            
HitResult resolve_battleship_shot(const Battlefield *bf, const EscortShip *e);
HitResult resolve_escort_shot(const Battlefield *bf, const EscortShip *e);

// function prototypes for simulate.c
void run_part1a_simulation(Battlefield *bf, const char *outFilePrefix);

// function prototypes for fileio.c
void save_initial_conditions(const Battlefield *bf, const char *filename);
void save_final_conditions(const Battlefield *bf, const char *filename,
                            int sunkByEscortIndex);

#endif
