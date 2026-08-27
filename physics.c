#include <math.h>
#include "naval_sim.h"
// converting degrees to radians
double deg2rad(double deg)
{
    return deg * PI / 180.0;
}

// computing the distance between two points
double distance_between(double x1, double y1, double x2, double y2)
{
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

/* R = u^2 * sin(2*theta) / g   (derived from standard projectile equations) */
double projectile_range(double speed, double angleDeg)
{
    // calling the above function to convert degrees to radians
    double angleRad = deg2rad(angleDeg);
    // R = u² sin(2*theta) / g
    return (speed * speed * sin(2.0 * angleRad)) / GRAVITY;
}

// t_flight = 2 * u * sin(theta) / g
double projectile_flight_time(double speed, double angleDeg)
{
    double angleRad = deg2rad(angleDeg);
    return (2.0 * speed * sin(angleRad)) / GRAVITY;
}

// Battleship can use ANY angle 0-90 and any speed 0..vMax, so its attack
// max range is when tetha =45 degrees, where
// sin(2*theta) = 1, so R_max = vMax^2 / g. Min range is 0 (vMin = 0).
double battleship_max_range(const Battlefield *bf)
{
    return (bf->battleship.vMax * bf->battleship.vMax) / GRAVITY;
}

/* An escort ship's attack range is an ANNULUS: angle is restricted to
   [thetaL, thetaH] and speed to [vMin, vMax]. Because R(u,theta) increases
   monotonically with u, and sin(2*theta) is maximised at theta = 45 (clamped
   into [thetaL,thetaH]) and minimised at whichever endpoint is furthest
   from 45, we get:
     R_max = vMax^2 * sin(2*theta_best)  / g
     R_min = vMin^2 * sin(2*theta_worst) / g
*/
void escort_attack_range(const Battlefield *bf, const EscortShip *e,
                         double *rMin, double *rMax)
{
    const EscortTypeParams *p = &bf->typeParams[e->type];
    double thetaL = p->thetaLDeg;
    double thetaH = p->thetaHDeg;

    /* angle that maximises sin(2*theta) within [thetaL, thetaH] */
    double thetaBest = 45.0;
    if (thetaBest < thetaL)
    {
        thetaBest = thetaL;
    }
    if (thetaBest > thetaH)
    {
        thetaBest = thetaH;
    }

    /* angle that minimises sin(2*theta): whichever bound is further from 45 */
    double thetaWorst = (fabs(thetaL - 45.0) >= fabs(thetaH - 45.0)) ? thetaL : thetaH;

    *rMax = projectile_range(p->vMax, thetaBest);
    *rMin = projectile_range(p->vMin, thetaWorst);

    if (*rMin > *rMax)
    {
        double tmp = *rMin;
        *rMin = *rMax;
        *rMax = tmp;
    }
}

/* Battleship firing at an escort ship: convention is to fire at theta = 45
   degrees (minimises the speed needed to reach any given distance, and
   45 is always inside B's allowed [0,90] range), then solve for the
   speed required to reach the target distance exactly. */
HitResult resolve_battleship_shot(const Battlefield *bf, const EscortShip *e)
{
    HitResult res = {0, 0, 0, 0, 0}; // keeep avoiding reading garbage values

    res.distance = distance_between(bf->battleship.x, bf->battleship.y, e->x, e->y);

    double theta = 45.0;
    double sin2t = sin(deg2rad(2.0 * theta)); /* = 1.0 */
    double requiredSpeedSq = (res.distance * GRAVITY) / sin2t;
    if (requiredSpeedSq < 0)
        return res;
    double requiredSpeed = sqrt(requiredSpeedSq);

    // main logics

    if (requiredSpeed <= bf->battleship.vMax)
    {
        res.canHit = 1;
        res.speedUsed = requiredSpeed;
        res.angleUsedDeg = theta;
        res.flightTimeSec = projectile_flight_time(requiredSpeed, theta);
    }
    return res;
}

// Escort ship firing at the battleship: fire at whichever angle inside
//[thetaL,thetaH] is closest to 45
HitResult resolve_escort_shot(const Battlefield *bf, const EscortShip *e)
{
    HitResult res = {0, 0, 0, 0, 0};
    const EscortTypeParams *p = &bf->typeParams[e->type];

    res.distance = distance_between(e->x, e->y, bf->battleship.x, bf->battleship.y);

    double theta = 45.0;
    if (theta < p->thetaLDeg)
    {
        theta = p->thetaLDeg;
    }
    if (theta > p->thetaHDeg)
    {
        theta = p->thetaHDeg;
    }

    double sin2t = sin(deg2rad(2.0 * theta));
    if (sin2t <= 0.0)
    {
        return res; // cannot generate any range at this angle
    }

    double requiredSpeedSq = (res.distance * GRAVITY) / sin2t;
    if (requiredSpeedSq < 0)
    {
        return res;
    }
    double requiredSpeed = sqrt(requiredSpeedSq);

    if (requiredSpeed >= p->vMin && requiredSpeed <= p->vMax)
    {
        res.canHit = 1;
        res.speedUsed = requiredSpeed;
        res.angleUsedDeg = theta;
        res.flightTimeSec = projectile_flight_time(requiredSpeed, theta);
    }
    return res;
}
