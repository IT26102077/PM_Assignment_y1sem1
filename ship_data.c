#include <string.h>
#include <ctype.h>
#include "naval_sim.h"

/* Table 1: Escort ship types. Impact power and angle range are fixed
   by the assignment spec and never change at runtime. .*/
static const EscortTypeInfo ESCORT_TABLE[NUM_ESCORT_TYPES] = {
    { 'A', "1936A-class Destroyer",   "SK C/34 naval gun",        0.08, 20.0 },
    { 'B', "Gabbiano-class Corvette", "L/47 dual-purpose gun",    0.06, 30.0 },
    { 'C', "Matsu-class Destroyer",   "Type 89 dual-purpose gun", 0.07, 25.0 },
    { 'D', "F-class Escort Ships",    "SK C/32 naval gun",        0.05, 50.0 },
    { 'E', "Japanese Kaibokan",       "(4.7 inch) naval guns",    0.04, 70.0 }
};

/* Table 2: Battleship types */
static const BattleshipTypeInfo BATTLESHIP_TABLE[4] = {
    { 'U', "USS Iowa (BB-61)",     "50-caliber Mark 7 gun" },
    { 'M', "MS King George V",     "(356 mm) Mark VII gun" },
    { 'R', "Richelieu",            "(15 inch) Mle 1935 gun" },
    { 'S', "Sovetsky Soyuz-class", "(16 inch) B-37 gun" }
};

const EscortTypeInfo *get_escort_type_info(EscortTypeId t)
{
    return &ESCORT_TABLE[t];
}

const BattleshipTypeInfo *get_battleship_type_info(BattleshipTypeId t)
{
    return &BATTLESHIP_TABLE[t];
}

EscortTypeId escort_notation_to_id(char notation)
{
    notation = (char)toupper((unsigned char)notation);
    for (int i = 0; i < NUM_ESCORT_TYPES; i++) {
        if (ESCORT_TABLE[i].notation == notation) return (EscortTypeId)i;
    }
    return ESCORT_A; /* fallback; setup.c validates input before this is hit */
}

BattleshipTypeId battleship_notation_to_id(char notation)
{
    notation = (char)toupper((unsigned char)notation);
    for (int i = 0; i < 4; i++) {
        if (BATTLESHIP_TABLE[i].notation == notation) return (BattleshipTypeId)i;
    }
    return BATTLESHIP_U;
}
