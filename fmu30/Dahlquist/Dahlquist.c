/* ---------------------------------------------------------------------------*
 * Sample implementation of an FMU - the Dahlquist test equation.
 *
 *   der(x) = - k * x and x(0) = 1. 
 *   Analytical solution: x(t) = exp(-k*t).
 * Copyright QTronic GmbH. All rights reserved.
 * ---------------------------------------------------------------------------*/

#include <string.h>  // for size_t

// define class name and unique id
#define MODEL_IDENTIFIER dq
#define MODEL_GUID "{8c4e810f-3df3-4a00-8276-176fa3c9f000}"

// define model size
#define NUMBER_OF_STATES 1
#define NUMBER_OF_EVENT_INDICATORS 0

#define N_VARIABLES 3
char   s_variableTypes[N_VARIABLES] = "rrr";
size_t s_variableSizes[N_VARIABLES] = { 1, 1, 1 };

// include fmu header files, typedefs and macros
#include "fmu3Template.h"

// define all model variables and their value references
// conventions used here:
// - if x is a variable, then macro x_ is its variable reference
// - the vr of a variable is its index in array  r, i, b or s
// - if k is the vr of a real state, then k+1 is the vr of its derivative
#define x_     0
#define der_x_ 1
#define k_     2

// define state vector as vector of value references
#define STATES { x_ }

// called by fmi2Instantiate
// Set values for all variables that define a start value
// Settings used unless changed by fmi2SetX before fmi2EnterInitializationMode
void setStartValues(ModelInstance *comp) {
    r(x_) = 1;
    r(k_) = 1;
}

// called by fmi2GetReal, fmi2GetInteger, fmi2GetBoolean, fmi2GetString, fmi2ExitInitialization
// if setStartValues or environment set new values through fmi2SetXXX.
// Lazy set values for all variable that are computed from other variables.
void calculateValues(ModelInstance *comp) {
    //if (comp->state == modelInitializationMode) {
    //  initialization code here
    //  set first time event, if any, using comp->eventInfo.nextEventTime
    //}
}

// called by fmi2GetReal, fmi2GetContinuousStates and fmi2GetDerivatives
fmi3Real* getReal(ModelInstance* comp, fmi3ValueReference vr){
    switch (vr) {
        case x_:
            return R(comp, x_);
        case der_x_ :
            r(der_x_) = -(r(x_)) * (r(x_));
            return R(comp, der_x_);
        case k_:
            return R(comp, k_);
        default:
            return NULL;
    }
}

// used to set the next time event, if any.
void eventUpdate(ModelInstance *comp, fmi3EventInfo *eventInfo, int isTimeEvent, int isNewEventIteration) {
} 

// include code that implements the FMI based on the above definitions
#include "fmu3Template.c"


