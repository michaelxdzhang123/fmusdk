/* ---------------------------------------------------------------------------*
 * fmuTemplate.h
 * Definitions by the includer of this file
 * Copyright QTronic GmbH. All rights reserved.
 * ---------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <assert.h>

// C-code FMUs have functions names prefixed with MODEL_IDENTIFIER_.
// Define DISABLE_PREFIX to build a binary FMU.
#ifndef DISABLE_PREFIX
#define pasteA(a,b)     a ## b
#define pasteB(a,b)    pasteA(a,b)
#define fmi3_FUNCTION_PREFIX pasteB(MODEL_IDENTIFIER, _)
#endif
#include "fmi3Functions.h"

#ifdef __cplusplus
extern "C" {
#endif

// macros used to define variables
#define  r(vr) (*R(comp, vr))
#define  i(vr) (*I(comp, vr))
#define  b(vr) (*B(comp, vr))
//#define  s(vr) comp->s[vr]
#define pos(z) comp->isPositive[z]
#define copy(vr, value, size) setString(comp, vr, value, size)

fmi3Status setString(fmi3Component comp, fmi3ValueReference vr, fmi3String value, size_t size);

// categories of logging supported by model.
// Value is the index in logCategories of a ModelInstance.
#define LOG_ALL       0
#define LOG_ERROR     1
#define LOG_FMI_CALL  2
#define LOG_EVENT     3

#define NUMBER_OF_CATEGORIES 4

typedef enum {
    modelStartAndEnd        = 1<<0,
    modelInstantiated       = 1<<1,
    modelInitializationMode = 1<<2,

    // ME states
    modelEventMode          = 1<<3,
    modelContinuousTimeMode = 1<<4,
    // CS states
    modelStepComplete       = 1<<5,
    modelStepInProgress     = 1<<6,
    modelStepFailed         = 1<<7,
    modelStepCanceled       = 1<<8,

    modelTerminated         = 1<<9,
    modelError              = 1<<10,
    modelFatal              = 1<<11,
} ModelState;

// ---------------------------------------------------------------------------
// Function calls allowed state masks for both Model-exchange and Co-simulation
// ---------------------------------------------------------------------------
#define MASK_fmi3GetTypesPlatform        (modelStartAndEnd | modelInstantiated | modelInitializationMode \
                                        | modelEventMode | modelContinuousTimeMode \
                                        | modelStepComplete | modelStepInProgress | modelStepFailed | modelStepCanceled \
                                        | modelTerminated | modelError)
#define MASK_fmi3GetVersion              MASK_fmi3GetTypesPlatform
#define MASK_fmi3SetDebugLogging         (modelInstantiated | modelInitializationMode \
                                        | modelEventMode | modelContinuousTimeMode \
                                        | modelStepComplete | modelStepInProgress | modelStepFailed | modelStepCanceled \
                                        | modelTerminated | modelError)
#define MASK_fmi3Instantiate             (modelStartAndEnd)
#define MASK_fmi3FreeInstance            (modelInstantiated | modelInitializationMode \
                                        | modelEventMode | modelContinuousTimeMode \
                                        | modelStepComplete | modelStepFailed | modelStepCanceled \
                                        | modelTerminated | modelError)
#define MASK_fmi3SetupExperiment         modelInstantiated
#define MASK_fmi3EnterInitializationMode modelInstantiated
#define MASK_fmi3ExitInitializationMode  modelInitializationMode
#define MASK_fmi3Terminate               (modelEventMode | modelContinuousTimeMode \
                                        | modelStepComplete | modelStepFailed)
#define MASK_fmi3Reset                   MASK_fmi3FreeInstance
#define MASK_fmi3GetReal                 (modelInitializationMode \
                                        | modelEventMode | modelContinuousTimeMode \
                                        | modelStepComplete | modelStepFailed | modelStepCanceled \
                                        | modelTerminated | modelError)
#define MASK_fmi3GetInteger              MASK_fmi3GetReal
#define MASK_fmi3GetBoolean              MASK_fmi3GetReal
#define MASK_fmi3GetString               MASK_fmi3GetReal
#define MASK_fmi3SetReal                 (modelInstantiated | modelInitializationMode \
                                        | modelEventMode | modelContinuousTimeMode \
                                        | modelStepComplete)
#define MASK_fmi3SetInteger              (modelInstantiated | modelInitializationMode \
                                        | modelEventMode \
                                        | modelStepComplete)
#define MASK_fmi3SetBoolean              MASK_fmi3SetInteger
#define MASK_fmi3SetString               MASK_fmi3SetInteger
#define MASK_fmi3GetFMUstate             MASK_fmi3FreeInstance
#define MASK_fmi3SetFMUstate             MASK_fmi3FreeInstance
#define MASK_fmi3FreeFMUstate            MASK_fmi3FreeInstance
#define MASK_fmi3SerializedFMUstateSize  MASK_fmi3FreeInstance
#define MASK_fmi3SerializeFMUstate       MASK_fmi3FreeInstance
#define MASK_fmi3DeSerializeFMUstate     MASK_fmi3FreeInstance
#define MASK_fmi3GetDirectionalDerivative (modelInitializationMode \
                                        | modelEventMode | modelContinuousTimeMode \
                                        | modelStepComplete | modelStepFailed | modelStepCanceled \
                                        | modelTerminated | modelError)

// ---------------------------------------------------------------------------
// Function calls allowed state masks for Model-exchange
// ---------------------------------------------------------------------------
#define MASK_fmi3EnterEventMode          (modelEventMode | modelContinuousTimeMode)
#define MASK_fmi3NewDiscreteStates       modelEventMode
#define MASK_fmi3EnterContinuousTimeMode modelEventMode
#define MASK_fmi3CompletedIntegratorStep modelContinuousTimeMode
#define MASK_fmi3SetTime                 (modelEventMode | modelContinuousTimeMode)
#define MASK_fmi3SetContinuousStates     modelContinuousTimeMode
#define MASK_fmi3GetEventIndicators      (modelInitializationMode \
                                        | modelEventMode | modelContinuousTimeMode \
                                        | modelTerminated | modelError)
#define MASK_fmi3GetContinuousStates     MASK_fmi3GetEventIndicators
#define MASK_fmi3GetDerivatives          (modelEventMode | modelContinuousTimeMode \
                                        | modelTerminated | modelError)
#define MASK_fmi3GetNominalsOfContinuousStates ( modelInstantiated \
                                        | modelEventMode | modelContinuousTimeMode \
                                        | modelTerminated | modelError)

// ---------------------------------------------------------------------------
// Function calls allowed state masks for Co-simulation
// ---------------------------------------------------------------------------
#define MASK_fmi3SetRealInputDerivatives (modelInstantiated | modelInitializationMode \
                                        | modelStepComplete)
#define MASK_fmi3GetRealOutputDerivatives (modelStepComplete | modelStepFailed | modelStepCanceled \
                                        | modelTerminated | modelError)
#define MASK_fmi3DoStep                  modelStepComplete
#define MASK_fmi3CancelStep              modelStepInProgress
#define MASK_fmi3GetStatus               (modelStepComplete | modelStepInProgress | modelStepFailed \
                                        | modelTerminated)
#define MASK_fmi3GetRealStatus           MASK_fmi3GetStatus
#define MASK_fmi3GetIntegerStatus        MASK_fmi3GetStatus
#define MASK_fmi3GetBooleanStatus        MASK_fmi3GetStatus
#define MASK_fmi3GetStringStatus         MASK_fmi3GetStatus

typedef struct {
    
    void   *variables[N_VARIABLES];
    
    fmi3Boolean *isPositive;

    fmi3Real time;
    fmi3String instanceName;
    fmi3Type type;
    fmi3String GUID;
    const fmi3CallbackFunctions *functions;
    fmi3Boolean loggingOn;
    fmi3Boolean logCategories[NUMBER_OF_CATEGORIES];

    fmi3ComponentEnvironment componentEnvironment;
    ModelState state;
    fmi3EventInfo eventInfo;
    fmi3Boolean isDirtyValues;
    fmi3Boolean isNewEventIteration;
} ModelInstance;

fmi3Real* R(ModelInstance *comp, int vr) {
    return comp->variables[vr];
}
    
fmi3Integer* I(ModelInstance *comp, int vr) {
    return comp->variables[vr];
}
 
fmi3Boolean* B(ModelInstance *comp, int vr) {
    return comp->variables[vr];
}
    
char** S(ModelInstance *comp, int vr) {
    return comp->variables[vr];
}
    
#ifdef __cplusplus
} // closing brace for extern "C"
#endif
