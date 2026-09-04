#include <stdio.h>
#include "naval_sim.h"

static void write_battleship_block(FILE *f, const Battlefield *bf)
{
    const BattleshipTypeInfo *info = get_battleship_type_info(bf->battleship.type);
    fprintf(f, "[Battleship]\n");
    fprintf(f, "Type=%c (%s, %s)\n", info->notation, info->typeName, info->gunName);
    fprintf(f, "Position=(%.3f, %.3f)\n", bf->battleship.x, bf->battleship.y);
    fprintf(f, "VMin=0.000\n");
    fprintf(f, "VMax=%.3f\n", bf->battleship.vMax);
    fprintf(f, "MaxAttackRange=%.3f\n", battleship_max_range(bf));
    fprintf(f, "CumulativeImpactFactorRemaining=%.4f\n", bf->battleship.healthFraction);
    fprintf(f, "Destroyed=%s\n\n", bf->battleship.destroyed ? "YES" : "NO");
}

static void write_escort_block(FILE *f, const Battlefield *bf, const EscortShip *e)
{
    const EscortTypeInfo *info = get_escort_type_info(e->type);
    const EscortTypeParams *p = &bf->typeParams[e->type];
    double rMin, rMax;
    escort_attack_range(bf, e, &rMin, &rMax);

    fprintf(f, "Index=%d\n", e->index);
    fprintf(f, "Type=E%c (%s)\n", info->notation, info->typeName);
    fprintf(f, "Position=(%.3f, %.3f)\n", e->x, e->y);
    fprintf(f, "ImpactPower=%.3f\n", info->impactPower);
    fprintf(f, "AngleRangeDeg=%.2f (thetaL=%.2f, thetaH=%.2f)\n",
            info->angleRangeDeg, p->thetaLDeg, p->thetaHDeg);
    fprintf(f, "VMin=%.3f\n", p->vMin);
    fprintf(f, "VMax=%.3f\n", p->vMax);
    fprintf(f, "AttackRange=[%.3f, %.3f]\n", rMin, rMax);
    fprintf(f, "Destroyed=%s\n", e->destroyed ? "YES" : "NO");
    fprintf(f, "ImpactFactorLeft=%.3f\n\n", e->impactFactorLeft);
}

void save_initial_conditions(const Battlefield *bf, const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f) { fprintf(stderr, "Error: could not open %s\n", filename); return; }

    fprintf(f, "===== INITIAL BATTLEFIELD CONDITIONS =====\n");
    fprintf(f, "CanvasD=%.3f\n", bf->D);
    fprintf(f, "NumEscortShips=%d\n\n", bf->N);

    write_battleship_block(f, bf);

    fprintf(f, "[Escort Ships]\n");
    for (int i = 0; i < bf->N; i++) {
        write_escort_block(f, bf, &bf->escorts[i]);
    }

    fclose(f);
    printf("Initial conditions saved to: %s\n", filename);
}

void save_final_conditions(const Battlefield *bf, const char *filename,
                            int sunkByEscortIndex)
{
    FILE *f = fopen(filename, "w");
    if (!f) { fprintf(stderr, "Error: could not open %s\n", filename); return; }

    fprintf(f, "===== FINAL BATTLEFIELD CONDITIONS =====\n");
    fprintf(f, "CanvasD=%.3f\n", bf->D);
    fprintf(f, "NumEscortShips=%d\n", bf->N);
    if (sunkByEscortIndex >= 0) {
        fprintf(f, "BattleshipSunkBy=Index %d\n\n", sunkByEscortIndex);
    } else {
        fprintf(f, "BattleshipSunkBy=NONE (battleship survived)\n\n");
    }

    write_battleship_block(f, bf);

    int destroyedCount = 0;
    fprintf(f, "[Escort Ships]\n");
    for (int i = 0; i < bf->N; i++) {
        write_escort_block(f, bf, &bf->escorts[i]);
        if (bf->escorts[i].destroyed) destroyedCount++;
    }
    fprintf(f, "TotalEscortShipsDestroyed=%d\n", destroyedCount);

    fclose(f);
    printf("Final conditions saved to: %s\n", filename);
}
