/* ---------------------------------------------------------------------------*
 * Sample implementation of an FMU - a bouncing ball.
 * This demonstrates the use of state events and reinit of states.
 * Equations:
 *  der(h) = v;
 *  der(v) = -g;
 *  when h<0 then v := -e * v;
 *  where
 *    h      height [m], used as state, start = 1
 *    v      velocity of ball [m/s], used as state
 *    der(h) velocity of ball [m/s]
 *    der(v) acceleration of ball [m/s2]
 *    g      acceleration of gravity [m/s2], a parameter, start = 9.81
 *    e      a dimensionless parameter, start = 0.7
 *
 * Copyright QTronic GmbH. All rights reserved.
 * ---------------------------------------------------------------------------*/

#include <string.h>  // for size_t

// define class name and unique id
#define MODEL_IDENTIFIER bouncingBall
#define MODEL_GUID "{8c4e810f-3df3-4a00-8276-176fa3c9f003}"

// define model size
#define NUMBER_OF_STATES 2
#define NUMBER_OF_EVENT_INDICATORS 1

#define N_VARIABLES 6
char   s_variableTypes[N_VARIABLES] = "rrrrrr";
size_t s_variableSizes[N_VARIABLES] = {1, 1, 1, 1, 1, 1};

// include fmu header files, typedefs and macros
#include "fmu3Template.h"

// define all model variables and their value references
// conventions used here:
// - if x is a variable, then macro x_ is its variable reference
// - the vr of a variable is its index in array  r, i, b or s
// - if k is the vr of a real state, then k+1 is the vr of its derivative
#define h_      0
#define der_h_  1
#define v_      2
#define der_v_  3
#define g_      4
#define e_      5

// define initial state vector as vector of value references
#define STATES { h_, v_ }

fmi3Variable variables[6] = { NULL };

// called by fmi3Instantiate
// Set values for all variables that define a start value
// Settings used unless changed by fmi3SetX before fmi3EnterInitializationMode
void setStartValues(ModelInstance *comp) {
    r(h_) = 1;
    r(v_) = 0;
    r(g_) = 9.81;
    r(e_) = 0.7;
}

// called by fmi3GetReal, fmi3GetInteger, fmi3GetBoolean, fmi3GetString, fmi3ExitInitialization
// if setStartValues or environment set new values through fmi3SetXXX.
// Lazy set values for all variable that are computed from other variables.
void calculateValues(ModelInstance *comp) {
    if (comp->state == modelInitializationMode) {
        r(der_v_) = -r(g_);
        pos(0) = r(h_) > 0;

        // set first time event, if any, using comp->eventInfo.nextEventTime
    }
}

fmi3Integer var_int = 0; // vr: 10
fmi3Real var_real[3] = { 0, 0, 0 }; // vr: 11

fmi3Status fmi3GetVariables (fmi3Component component,
                             const fmi3ValueReference valueReferences[], size_t nValueReferences,
                             fmi3Variable variables[], const size_t variableSizes[]) {
    
    size_t i, vs;
    fmi3ValueReference vr;
    
    for (i = 0; i < nValueReferences; i++) {
        
        vr = valueReferences[i];
        vs = variableSizes[i];
        
        switch (vr) {
            case 10:
                if (vs != sizeof(fmi3Integer)) return fmi3Error;
                *((fmi3Integer *)variables[i]) = var_int;
                break;
            case 11:
                if (vs != 3 * sizeof(fmi3Real)) return fmi3Error;
                ((fmi3Real *)variables[i])[0] = var_real[0];
                ((fmi3Real *)variables[i])[1] = var_real[1];
                ((fmi3Real *)variables[i])[2] = var_real[2];
                break;
            default:
                break;
        }
    }
    
    return fmi3OK;
}

fmi3Status fmi3SetVariables (fmi3Component component,
                             const fmi3ValueReference valueReferences[], size_t nValueReferences,
                             const fmi3Variable variables[], const size_t variableSizes[]) {
    
    size_t i, vs;
    fmi3ValueReference vr;
    
    for (i = 0; i < nValueReferences; i++) {
        
        vr = valueReferences[i];
        vs = variableSizes[i];

        switch (vr) {
            case 10:
                if (vs != sizeof(fmi3Integer)) return fmi3Error;
                var_int = *((fmi3Integer *)variables[i]);
                break;
            case 11:
                if (vs != 3 * sizeof(fmi3Real)) return fmi3Error;
                var_real[0] = ((fmi3Real *)variables[i])[0];
                var_real[1] = ((fmi3Real *)variables[i])[1];
                var_real[2] = ((fmi3Real *)variables[i])[2];
                break;
            default:
                break;
        }
    }
    
    return fmi3OK;
}

// called by fmi3GetReal, fmi3GetContinuousStates and fmi3GetDerivatives
fmi3Real* getReal(ModelInstance* comp, fmi3ValueReference vr) {
    switch (vr) {
        case h_     : return R(comp, h_);
        case der_h_ : return R(comp, v_);
        case v_     : return R(comp, v_);
        case der_v_ : return R(comp, der_v_);
        case g_     : return R(comp, g_);
        case e_     : return R(comp, e_);
        default: return (fmi3Real*)NULL;
    }
}

// offset for event indicator, adds hysteresis and prevents z=0 at restart
#define EPS_INDICATORS 1e-14

fmi3Real getEventIndicator(ModelInstance* comp, int z) {
    switch (z) {
        case 0 : return r(h_) + (pos(0) ? EPS_INDICATORS : -EPS_INDICATORS);
        default: return 0;
    }
}

// previous value of r(v_).
fmi3Real prevV;

// used to set the next time event, if any.
void eventUpdate(ModelInstance *comp, fmi3EventInfo *eventInfo, int isTimeEvent, int isNewEventIteration) {
    if (isNewEventIteration) {
        prevV = r(v_);
    }
    pos(0) = r(h_) > 0;
    if (!pos(0)) {
        fmi3Real tempV = - r(e_) * prevV;
        if (r(v_) != tempV) {
            r(v_) = tempV;
            eventInfo->valuesOfContinuousStatesChanged = fmi3True;
        }
        // avoid fall-through effect. The ball will not jump high enough, so v and der_v is set to 0 at this surface impact.
        if (r(v_) < 1e-3) {
            r(v_) = 0;
            r(der_v_) = 0;  // turn off gravity.
        }
    }
    eventInfo->nominalsOfContinuousStatesChanged = fmi3False;
    eventInfo->terminateSimulation   = fmi3False;
    eventInfo->nextEventTimeDefined  = fmi3False;
}

// include code that implements the FMI based on the above definitions
#include "fmu3Template.c"
