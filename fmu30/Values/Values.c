/* ---------------------------------------------------------------------------*
 * Sample implementation of an FMU.
 * This demonstrates the use of all FMU variable types.
 * Copyright QTronic GmbH. All rights reserved.
 * ---------------------------------------------------------------------------*/

#include <string.h>  // for size_t

// define class name and unique id
#define MODEL_IDENTIFIER values
#define MODEL_GUID "{8c4e810f-3df3-4a00-8276-176fa3c9f004}"

// define model size
#define NUMBER_OF_STATES 1
#define NUMBER_OF_EVENT_INDICATORS 0

#define N_VARIABLES 8
char   s_variableTypes[N_VARIABLES] = "rriibbss";
size_t s_variableSizes[N_VARIABLES] = { 1, 1, 1, 1, 1, 1, 1, 1 };

// include fmu header files, typedefs and macros
#include "fmu3Template.h"

// define all model variables and their value references
// conventions used here:
// - if x is a variable, then macro x_ is its variable reference
// - the vr of a variable is its index in array  r, i, b or s
// - if k is the vr of a real state, then k+1 is the vr of its derivative
#define x_          0
#define der_x_      1
#define int_in_     2
#define int_out_    3
#define bool_in_    4
#define bool_out_   5
#define string_in_  6
#define string_out_ 7

// define state vector as vector of value references
#define STATES { x_ }

const char *month[] = {
    "jan","feb","march","april","may","june","july",
    "august","sept","october","november","december"
};

// called by fmi2Instantiate
// Set values for all variables that define a start value
// Settings used unless changed by fmi2SetX before fmi2EnterInitializationMode
void setStartValues(ModelInstance *comp) {
    r(x_) = 1;
    i(int_in_) = 2;
    i(int_out_) = 0;
    b(bool_in_) = fmi3True;
    b(bool_out_) = fmi3False;
    copy(string_in_, "QTronic", 1);
    copy(string_out_, month[0], 1);
}

// called by fmi2GetReal, fmi2GetInteger, fmi2GetBoolean, fmi2GetString, fmi2ExitInitialization
// if setStartValues or environment set new values through fmi2SetXXX.
// Lazy set values for all variable that are computed from other variables.
void calculateValues(ModelInstance *comp) {
    if (comp->state == modelInitializationMode) {
        // set first time event
        comp->eventInfo.nextEventTimeDefined = fmi3True;
        comp->eventInfo.nextEventTime        = 1 + comp->time;
    }
}

// called by fmi2GetReal, fmi2GetContinuousStates and fmi2GetDerivatives
fmi3Real* getReal(ModelInstance *comp, fmi3ValueReference vr){
    switch (vr) {
        case x_:
            return R(comp, x_);
        case der_x_:
            r(der_x_) = - r(x_);
            return R(comp, der_x_);
        default: return 0;
    }
}

// used to set the next time event, if any.
void eventUpdate(ModelInstance *comp, fmi3EventInfo *eventInfo, int isTimeEvent, int isNewEventIteration) {
    
    if (isTimeEvent) {
        eventInfo->nextEventTimeDefined = fmi3True;
        eventInfo->nextEventTime        = 1 + comp->time;
        i(int_out_) += 1;
        b(bool_out_) = !b(bool_out_);
        if (i(int_out_) < 12) copy(string_out_, month[i(int_out_)], 1);
        else eventInfo->terminateSimulation = fmi3True;
    }
}

// include code that implements the FMI based on the above definitions
#include "fmu3Template.c"
