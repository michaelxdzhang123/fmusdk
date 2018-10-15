/* ---------------------------------------------------------------------------*
 * Sample implementation of an FMU - the Dahlquist test equation.
 *
 *   der(x) = - k * x and x(0) = 1. 
 *   Analytical solution: x(t) = exp(-k*t).
 * Copyright QTronic GmbH. All rights reserved.
 * ---------------------------------------------------------------------------*/

#include <string.h>  // for size_t

// define class name and unique id
#define MODEL_IDENTIFIER LinearTransform
#define MODEL_GUID "{8c4e810f-3df3-4a00-8276-176fa3c9f001}"

// define model size
#define NUMBER_OF_STATES 0
#define NUMBER_OF_EVENT_INDICATORS 0

#define N_VARIABLES 7
char   s_variableTypes[N_VARIABLES] = "rrriibb";
size_t s_variableSizes[N_VARIABLES] = { 3, 9, 3, 2, 2, 2, 2 };

// include fmu header files, typedefs and macros
#include "fmu3Template.h"

// define all model variables and their value references
// conventions used here:
// - if x is a variable, then macro x_ is its variable reference
// - the vr of a variable is its index in array  r, i, b or s
// - if k is the vr of a real state, then k+1 is the vr of its derivative
#define u_     0
#define T_     1
#define y_     2

#define i_in_  3
#define i_out_ 4

#define b_in_  5
#define b_out_ 6


// called by fmi2Instantiate
// Set values for all variables that define a start value
// Settings used unless changed by fmi2SetX before fmi2EnterInitializationMode
void setStartValues(ModelInstance *comp) {
    
    R(comp, u_)[0] = -0.1;
    R(comp, u_)[1] = -0.2;
    R(comp, u_)[2] = -0.3;
    
    R(comp, T_)[0] =  0; R(comp, T_)[1] =  0; R(comp, T_)[2] = -1;
    R(comp, T_)[3] =  0; R(comp, T_)[4] = -1; R(comp, T_)[5] =  0;
    R(comp, T_)[6] = -1; R(comp, T_)[7] =  0; R(comp, T_)[8] =  0;

    R(comp, y_)[0] = 0.1;
    R(comp, y_)[1] = 0.2;
    R(comp, y_)[2] = 0.3;
    
    I(comp, i_in_)[0] = -1;
    I(comp, i_in_)[1] =  1;

    I(comp, i_out_)[0] = -1;
    I(comp, i_out_)[1] =  1;
    
    B(comp, b_in_)[0] = fmi3False;
    B(comp, b_in_)[1] = fmi3True;
    
    B(comp, b_out_)[0] = fmi3False;
    B(comp, b_out_)[1] = fmi3True;
}

// called by fmi2GetReal, fmi2GetInteger, fmi2GetBoolean, fmi2GetString, fmi2ExitInitialization
// if setStartValues or environment set new values through fmi2SetXXX.
// Lazy set values for all variable that are computed from other variables.
void calculateValues(ModelInstance *comp) {
    //if (comp->state == modelInitializationMode) {
    //  initialization code here
    //  set first time event, if any, using comp->eventInfo.nextEventTime
    //}
    
    I(comp, i_out_)[0] = I(comp, i_in_)[0];
    I(comp, i_out_)[1] = I(comp, i_in_)[1];
    
    printf("calc\n");
}

// called by fmi2GetReal, fmi2GetContinuousStates and fmi2GetDerivatives
fmi3Real* getReal(ModelInstance* comp, fmi3ValueReference vr) {
        
    switch (vr) {
        case u_:
            return R(comp, u_);
        case T_:
            return R(comp, T_);
        case y_:
            return R(comp, y_);
        default:
            return NULL;
    }
}

// used to set the next time event, if any.
void eventUpdate(ModelInstance *comp, fmi3EventInfo *eventInfo, int isTimeEvent, int isNewEventIteration) {
} 

// include code that implements the FMI based on the above definitions
#include "fmu3Template.c"


