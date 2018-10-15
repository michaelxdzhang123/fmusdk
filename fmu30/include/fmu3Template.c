/* ---------------------------------------------------------------------------*
 * fmuTemplate.c
 * Implementation of the FMI interface based on functions and macros to
 * be defined by the includer of this file.
 * The "FMI for Co-Simulation 2.0", implementation assumes that exactly the
 * following capability flags are set to fmi3True:
 *    canHandleVariableCommunicationStepSize, i.e. fmi3DoStep step size can vary
 * and all other capability flags are set to default, i.e. to fmi3False or 0.
 *
 * Revision history
 *  07.03.2014 initial version released in FMU SDK 2.0.0
 *  02.04.2014 allow modules to request termination of simulation, better time
 *             event handling, initialize() moved from fmi3EnterInitialization to
 *             fmi3ExitInitialization, correct logging message format in fmi3DoStep.
 *  10.04.2014 use FMI 2.0 headers that prefix function and types names with 'fmi3'.
 *  13.06.2014 when fmi3setDebugLogging is called with 0 categories, set all
 *             categories to loggingOn value.
 *  09.07.2014 track all states of Model-exchange and Co-simulation and check
 *             the allowed calling sequences, explicit isTimeEvent parameter for
 *             eventUpdate function of the model, lazy computation of computed values.
 *
 * Author: Adrian Tirea
 * Copyright QTronic GmbH. All rights reserved.
 * ---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

// macro to be used to log messages. The macro check if current
// log category is valid and, if true, call the logger provided by simulator.
#define FILTERED_LOG(instance, status, categoryIndex, message, ...) if (status == fmi3Error || status == fmi3Fatal || isCategoryLogged(instance, categoryIndex)) \
        instance->functions->logger(instance->functions->componentEnvironment, instance->instanceName, status, \
        logCategoriesNames[categoryIndex], message, ##__VA_ARGS__);

static fmi3String logCategoriesNames[] = {"logAll", "logError", "logFmiCall", "logEvent"};

// array of value references of states
#if NUMBER_OF_STATES>0
fmi3ValueReference vrStates[NUMBER_OF_STATES] = STATES;
#endif

#ifndef max
#define max(a,b) ((a)>(b) ? (a) : (b))
#endif

#ifndef DT_EVENT_DETECT
#define DT_EVENT_DETECT 1e-10
#endif

// ---------------------------------------------------------------------------
// Private helpers used below to validate function arguments
// ---------------------------------------------------------------------------

fmi3Boolean isCategoryLogged(ModelInstance *comp, int categoryIndex);

static fmi3Boolean invalidNumber(ModelInstance *comp, const char *f, const char *arg, int n, int nExpected) {
    if (n != nExpected) {
        comp->state = modelError;
        FILTERED_LOG(comp, fmi3Error, LOG_ERROR, "%s: Invalid argument %s = %d. Expected %d.", f, arg, n, nExpected)
        return fmi3True;
    }
    return fmi3False;
}

static fmi3Boolean invalidState(ModelInstance *comp, const char *f, int statesExpected) {
    if (!comp)
        return fmi3True;
    if (!(comp->state & statesExpected)) {
        comp->state = modelError;
        FILTERED_LOG(comp, fmi3Error, LOG_ERROR, "%s: Illegal call sequence.", f)
        return fmi3True;
    }
    return fmi3False;
}

static fmi3Boolean nullPointer(ModelInstance* comp, const char *f, const char *arg, const void *p) {
    if (!p) {
        comp->state = modelError;
        FILTERED_LOG(comp, fmi3Error, LOG_ERROR, "%s: Invalid argument %s = NULL.", f, arg)
        return fmi3True;
    }
    return fmi3False;
}

static fmi3Boolean vrOutOfRange(ModelInstance *comp, const char *f, fmi3ValueReference vr, char type) {
    if (vr >= N_VARIABLES) {
        FILTERED_LOG(comp, fmi3Error, LOG_ERROR, "%s: Illegal value reference %u.", f, vr)
        comp->state = modelError;
        return fmi3True;
    }
    if (s_variableTypes[vr] != type) {
        FILTERED_LOG(comp, fmi3Error, LOG_ERROR, "%s: Variable %u is not a %c.", f, vr, type)
        comp->state = modelError;
        return fmi3True;
    }
    return fmi3False;
}

static fmi3Status unsupportedFunction(fmi3Component c, const char *fName, int statesExpected) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, fName, statesExpected))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, fName);
    FILTERED_LOG(comp, fmi3Error, LOG_ERROR, "%s: Function not implemented.", fName)
    return fmi3Error;
}

fmi3Status setString(fmi3Component comp, fmi3ValueReference vr, fmi3String value, size_t size) {
    return fmi3SetString(comp, &vr, 1, &value, size);
}

// ---------------------------------------------------------------------------
// Private helpers logger
// ---------------------------------------------------------------------------

// return fmi3True if logging category is on. Else return fmi3False.
fmi3Boolean isCategoryLogged(ModelInstance *comp, int categoryIndex) {
    if (categoryIndex < NUMBER_OF_CATEGORIES
        && (comp->logCategories[categoryIndex] || comp->logCategories[LOG_ALL])) {
        return fmi3True;
    }
    return fmi3False;
}

// ---------------------------------------------------------------------------
// FMI functions
// ---------------------------------------------------------------------------
fmi3Component fmi3Instantiate(fmi3String instanceName, fmi3Type fmuType, fmi3String fmuGUID,
                            fmi3String fmuResourceLocation, const fmi3CallbackFunctions *functions,
                            fmi3Boolean visible, fmi3Boolean loggingOn) {
    // ignoring arguments: fmuResourceLocation, visible
    ModelInstance *comp;
    if (!functions->logger) {
        return NULL;
    }

    if (!functions->allocateMemory || !functions->freeMemory) {
        functions->logger(functions->componentEnvironment, instanceName, fmi3Error, "error",
                "fmi3Instantiate: Missing callback function.");
        return NULL;
    }
    if (!instanceName || strlen(instanceName) == 0) {
        functions->logger(functions->componentEnvironment, "?", fmi3Error, "error",
                "fmi3Instantiate: Missing instance name.");
        return NULL;
    }
    if (!fmuGUID || strlen(fmuGUID) == 0) {
        functions->logger(functions->componentEnvironment, instanceName, fmi3Error, "error",
                "fmi3Instantiate: Missing GUID.");
        return NULL;
    }
    if (strcmp(fmuGUID, MODEL_GUID)) {
        functions->logger(functions->componentEnvironment, instanceName, fmi3Error, "error",
                "fmi3Instantiate: Wrong GUID %s. Expected %s.", fmuGUID, MODEL_GUID);
        return NULL;
    }
    comp = (ModelInstance *)functions->allocateMemory(NULL, 1, sizeof(ModelInstance));
    if (comp) {
        int i;
        
        for (i=0; i < N_VARIABLES; i++) {
            size_t typeSize;
            
            switch (s_variableTypes[i]) {
                case 'r': typeSize = sizeof(fmi3Real); break;
                case 'i': typeSize = sizeof(fmi3Integer); break;
                case 'b': typeSize = sizeof(fmi3Boolean); break;
                case 's': typeSize = sizeof(fmi3String); break;
                default: return NULL; // error
            }
            
            comp->variables[i] = functions->allocateMemory(comp, s_variableSizes[i], typeSize);
        }
        
        comp->isPositive = (fmi3Boolean *)functions->allocateMemory(comp, NUMBER_OF_EVENT_INDICATORS,
            sizeof(fmi3Boolean));
        
        comp->instanceName = (char *)functions->allocateMemory(comp, 1 + strlen(instanceName), sizeof(char));
        comp->GUID = (char *)functions->allocateMemory(comp, 1 + strlen(fmuGUID), sizeof(char));

        // set all categories to on or off. fmi3SetDebugLogging should be called to choose specific categories.
        for (i = 0; i < NUMBER_OF_CATEGORIES; i++) {
            comp->logCategories[i] = loggingOn;
        }
    }
    if (!comp
        || !comp->variables
        || !comp->isPositive
        || !comp->instanceName || !comp->GUID) {

        functions->logger(functions->componentEnvironment, instanceName, fmi3Error, "error",
            "fmi3Instantiate: Out of memory.");
        return NULL;
    }
    comp->time = 0; // overwrite in fmi3SetupExperiment, fmi3SetTime
    strcpy((char *)comp->instanceName, (char *)instanceName);
    comp->type = fmuType;
    strcpy((char *)comp->GUID, (char *)fmuGUID);
    comp->functions = functions;
    comp->componentEnvironment = functions->componentEnvironment;
    comp->loggingOn = loggingOn;
    comp->state = modelInstantiated;
    setStartValues(comp); // to be implemented by the includer of this file
    comp->isDirtyValues = fmi3True; // because we just called setStartValues
    comp->isNewEventIteration = fmi3False;

    comp->eventInfo.newDiscreteStatesNeeded = fmi3False;
    comp->eventInfo.terminateSimulation = fmi3False;
    comp->eventInfo.nominalsOfContinuousStatesChanged = fmi3False;
    comp->eventInfo.valuesOfContinuousStatesChanged = fmi3False;
    comp->eventInfo.nextEventTimeDefined = fmi3False;
    comp->eventInfo.nextEventTime = 0;

    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3Instantiate: GUID=%s", fmuGUID)

    return comp;
}

fmi3Status fmi3SetupExperiment(fmi3Component c, fmi3Boolean toleranceDefined, fmi3Real tolerance,
                            fmi3Real startTime, fmi3Boolean stopTimeDefined, fmi3Real stopTime) {

    // ignore arguments: stopTimeDefined, stopTime
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3SetupExperiment", MASK_fmi3SetupExperiment))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetupExperiment: toleranceDefined=%d tolerance=%g",
        toleranceDefined, tolerance)

    comp->time = startTime;
    return fmi3OK;
}

fmi3Status fmi3EnterInitializationMode(fmi3Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3EnterInitializationMode", MASK_fmi3EnterInitializationMode))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3EnterInitializationMode")

    comp->state = modelInitializationMode;
    return fmi3OK;
}

fmi3Status fmi3ExitInitializationMode(fmi3Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3ExitInitializationMode", MASK_fmi3ExitInitializationMode))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3ExitInitializationMode")

    // if values were set and no fmi3GetXXX triggered update before,
    // ensure calculated values are updated now
    if (comp->isDirtyValues) {
        calculateValues(comp);
        comp->isDirtyValues = fmi3False;
    }

    if (comp->type == fmi3ModelExchange) {
        comp->state = modelEventMode;
        comp->isNewEventIteration = fmi3True;
    }
    else comp->state = modelStepComplete;
    return fmi3OK;
}

fmi3Status fmi3Terminate(fmi3Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3Terminate", MASK_fmi3Terminate))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3Terminate")

    comp->state = modelTerminated;
    return fmi3OK;
}

fmi3Status fmi3Reset(fmi3Component c) {
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3Reset", MASK_fmi3Reset))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3Reset")

    comp->state = modelInstantiated;
    setStartValues(comp); // to be implemented by the includer of this file
    comp->isDirtyValues = fmi3True; // because we just called setStartValues
    return fmi3OK;
}

void fmi3FreeInstance(fmi3Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (!comp) return;
    if (invalidState(comp, "fmi3FreeInstance", MASK_fmi3FreeInstance))
        return;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3FreeInstance")
    
    // TODO: free variables

//    if (comp->r) comp->functions->freeMemory(c, comp->r);
//    if (comp->i) comp->functions->freeMemory(c, comp->i);
//    if (comp->b) comp->functions->freeMemory(c, comp->b);
//    if (comp->s) {
//        int i;
//        for (i = 0; i < NUMBER_OF_STRINGS; i++){
//            if (comp->s[i]) comp->functions->freeMemory(c, (void *)comp->s[i]);
//        }
//        comp->functions->freeMemory(c, (void *)comp->s);
//    }
    if (comp->isPositive) comp->functions->freeMemory(c, comp->isPositive);
    if (comp->instanceName) comp->functions->freeMemory(c, (void *)comp->instanceName);
    if (comp->GUID) comp->functions->freeMemory(c, (void *)comp->GUID);
    comp->functions->freeMemory(c, comp);
}

// ---------------------------------------------------------------------------
// FMI functions: class methods not depending of a specific model instance
// ---------------------------------------------------------------------------

const char* fmi3GetVersion() {
    return fmi3Version;
}

const char* fmi3GetTypesPlatform() {
    return fmi3TypesPlatform;
}

// ---------------------------------------------------------------------------
// FMI functions: logging control, setters and getters for Real, Integer,
// Boolean, String
// ---------------------------------------------------------------------------

fmi3Status fmi3SetDebugLogging(fmi3Component c, fmi3Boolean loggingOn, size_t nCategories, const fmi3String categories[]) {
    int i, j;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3SetDebugLogging", MASK_fmi3SetDebugLogging))
        return fmi3Error;
    comp->loggingOn = loggingOn;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetDebugLogging")

    // reset all categories
    for (j = 0; j < NUMBER_OF_CATEGORIES; j++) {
        comp->logCategories[j] = fmi3False;
    }

    if (nCategories == 0) {
        // no category specified, set all categories to have loggingOn value
        for (j = 0; j < NUMBER_OF_CATEGORIES; j++) {
            comp->logCategories[j] = loggingOn;
        }
    } else {
        // set specific categories on
        for (i = 0; i < nCategories; i++) {
            fmi3Boolean categoryFound = fmi3False;
            for (j = 0; j < NUMBER_OF_CATEGORIES; j++) {
                if (strcmp(logCategoriesNames[j], categories[i]) == 0) {
                    comp->logCategories[j] = loggingOn;
                    categoryFound = fmi3True;
                    break;
                }
            }
            if (!categoryFound) {
                comp->functions->logger(comp->componentEnvironment, comp->instanceName, fmi3Warning,
                    logCategoriesNames[LOG_ERROR],
                    "logging category '%s' is not supported by model", categories[i]);
            }
        }
    }
    return fmi3OK;
}

fmi3Status fmi3GetReal(fmi3Component c, const fmi3ValueReference vr[], size_t nvr, fmi3Real value[], size_t nValues) {
    
    int i, j, k = 0;
    
    ModelInstance *comp = (ModelInstance *)c;
    
    if (invalidState(comp, "fmi3GetReal", MASK_fmi3GetReal)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3GetReal", "vr[]", vr)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3GetReal", "value[]", value)) return fmi3Error;
    
    if (nvr > 0 && comp->isDirtyValues) {
        calculateValues(comp);
        comp->isDirtyValues = fmi3False;
    }
    
    for (i = 0; i < nvr; i++) {
        
        if (vrOutOfRange(comp, "fmi3GetReal", vr[i], 'r')) return fmi3Error;
        
        for (j = 0; j < s_variableSizes[i]; j++) {
            value[k++] = getReal(comp, vr[i])[j]; // to be implemented by the includer of this file
        }

        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3GetReal: #r%u# = %.16g", vr[i], value[i])
    }
    
    return fmi3OK;
}

fmi3Status fmi3GetInteger(fmi3Component c, const fmi3ValueReference vr[], size_t nvr, fmi3Integer value[], size_t nValues) {
    
    int i, j, k = 0;
    
    ModelInstance *comp = (ModelInstance *)c;
    
    if (invalidState(comp, "fmi3GetInteger", MASK_fmi3GetInteger)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3GetInteger", "vr[]", vr)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3GetInteger", "value[]", value)) return fmi3Error;
    
    if (nvr > 0 && comp->isDirtyValues) {
        calculateValues(comp);
        comp->isDirtyValues = fmi3False;
    }
    
    for (i = 0; i < nvr; i++) {
        
        if (vrOutOfRange(comp, "fmi3GetInteger", vr[i], 'i')) return fmi3Error;
        
        for (j = 0; j < s_variableSizes[i]; j++) {
            value[k++] = I(comp, vr[i])[j];
        }
        
        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3GetInteger: #i%u# = %d", vr[i], value[i])
    }

    return fmi3OK;
}

fmi3Status fmi3GetBoolean(fmi3Component c, const fmi3ValueReference vr[], size_t nvr, fmi3Boolean value[], size_t nValues) {
    
    int i, j, k = 0;
    
    ModelInstance *comp = (ModelInstance *)c;
    
    if (invalidState(comp, "fmi3GetBoolean", MASK_fmi3GetBoolean)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3GetBoolean", "vr[]", vr)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3GetBoolean", "value[]", value)) return fmi3Error;
    
    if (nvr > 0 && comp->isDirtyValues) {
        calculateValues(comp);
        comp->isDirtyValues = fmi3False;
    }

    for (i = 0; i < nvr; i++) {
        
        if (vrOutOfRange(comp, "fmi3GetBoolean", vr[i], 'b')) return fmi3Error;
        
        for (j = 0; j < s_variableSizes[i]; j++) {
            value[k++] = B(comp, vr[i])[j];
        }
        
        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3GetBoolean: #i%u# = %d", vr[i], value[i])
    }

    return fmi3OK;
}

fmi3Status fmi3GetString(fmi3Component c, const fmi3ValueReference vr[], size_t nvr, fmi3String value[], size_t nValues) {
    
    int i, j, k = 0;

    ModelInstance *comp = (ModelInstance *)c;
    
    if (invalidState(comp, "fmi3GetString", MASK_fmi3GetString)) return fmi3Error;
    
    if (nvr>0 && nullPointer(comp, "fmi3GetString", "vr[]", vr)) return fmi3Error;
    
    if (nvr>0 && nullPointer(comp, "fmi3GetString", "value[]", value)) return fmi3Error;
    
    if (nvr > 0 && comp->isDirtyValues) {
        calculateValues(comp);
        comp->isDirtyValues = fmi3False;
    }
    
    for (i = 0; i < nvr; i++) {
        
        if (vrOutOfRange(comp, "fmi3GetString", vr[i], 'b')) return fmi3Error;
        
        for (j = 0; j < s_variableSizes[i]; j++) {
            value[k++] = S(comp, vr[i])[j];
        }
        
        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3GetString: #s%u# = \"%s\"", vr[i], value[i])
    }
    
    return fmi3OK;
}

fmi3Status fmi3SetReal(fmi3Component c, const fmi3ValueReference vr[], size_t nvr, const fmi3Real value[], size_t nValues) {
    
    int i, j, k = 0;
    
    ModelInstance *comp = (ModelInstance *)c;
    
    if (invalidState(comp, "fmi3SetReal", MASK_fmi3SetReal)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3SetReal", "vr[]", vr)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3SetReal", "value[]", value)) return fmi3Error;
    
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetReal: nvr = %d", nvr)
    
    // no check whether setting the value is allowed in the current state
    for (i = 0; i < nvr; i++) {
        
        if (vrOutOfRange(comp, "fmi3SetRoolean", vr[i], 'r')) return fmi3Error;
        
        for (j = 0; j < s_variableSizes[i]; j++) {
            R(comp, vr[i])[j] = value[k++];
        }
        
        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetReal: #r%u# = %.16g", vr[i], value[i])
    }

    if (nvr > 0) comp->isDirtyValues = fmi3True;
    
    return fmi3OK;
}

fmi3Status fmi3SetInteger(fmi3Component c, const fmi3ValueReference vr[], size_t nvr, const fmi3Integer value[], size_t nValues) {
    int i, j, k = 0;
    
    ModelInstance *comp = (ModelInstance *)c;
    
    if (invalidState(comp, "fmi3SetInteger", MASK_fmi3SetInteger)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3SetInteger", "vr[]", vr)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3SetInteger", "value[]", value)) return fmi3Error;
    
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetInteger: nvr = %d", nvr)

    for (i = 0; i < nvr; i++) {
        
        if (vrOutOfRange(comp, "fmi3SetInteger", vr[i], 'i')) return fmi3Error;
        
        for (j = 0; j < s_variableSizes[i]; j++) {
            I(comp, vr[i])[j] = value[k++];
        }
        
        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetInteger: #i%u# = %d", vr[i], value[i])
    }
    
    if (nvr > 0) comp->isDirtyValues = fmi3True;
    
    return fmi3OK;
}

fmi3Status fmi3SetBoolean(fmi3Component c, const fmi3ValueReference vr[], size_t nvr, const fmi3Boolean value[], size_t nValues) {
    
    int i, j, k = 0;

    ModelInstance *comp = (ModelInstance *)c;
    
    if (invalidState(comp, "fmi3SetBoolean", MASK_fmi3SetBoolean)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3SetBoolean", "vr[]", vr)) return fmi3Error;
    
    if (nvr > 0 && nullPointer(comp, "fmi3SetBoolean", "value[]", value)) return fmi3Error;
    
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetBoolean: nvr = %d", nvr)

    for (i = 0; i < nvr; i++) {
        
        if (vrOutOfRange(comp, "fmi3SetBoolean", vr[i], 'i')) return fmi3Error;
        
        for (j = 0; j < s_variableSizes[i]; j++) {
            B(comp, vr[i])[j] = value[k++];
        }
        
        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetBoolean: #b%u# = %s", vr[i], value[i] ? "true" : "false")
    }

    if (nvr > 0) comp->isDirtyValues = fmi3True;
    
    return fmi3OK;
}

fmi3Status fmi3SetString (fmi3Component c, const fmi3ValueReference vr[], size_t nvr, const fmi3String value[], size_t nValues) {

    int i, j, k = 0;
    
    ModelInstance *comp = (ModelInstance *)c;
    
    if (invalidState(comp, "fmi3SetString", MASK_fmi3SetString)) return fmi3Error;
    
    if (nvr>0 && nullPointer(comp, "fmi3SetString", "vr[]", vr)) return fmi3Error;
    
    if (nvr>0 && nullPointer(comp, "fmi3SetString", "value[]", value)) return fmi3Error;
    
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetString: nvr = %d", nvr)

    for (i = 0; i < nvr; i++) {
        
        if (vrOutOfRange(comp, "fmi3SetString", vr[i], 's')) return fmi3Error;
        
        for (j = 0; j < s_variableSizes[i]; j++) {
            
//            char *string = (char *)comp->s[vr[i]];
//
//            if (value[k] == NULL) {
//                if (string) comp->functions->freeMemory(c, string);
//                comp->s[vr[i]] = NULL;
//                FILTERED_LOG(comp, fmi3Warning, LOG_ERROR, "fmi3SetString: string argument value[%d] = NULL.", i);
//            } else {
//                if (string == NULL || strlen(string) < strlen(value[i])) {
//                    if (string) comp->functions->freeMemory(c, string);
//                    comp->s[vr[i]] = (char *)comp->functions->allocateMemory(c, 1 + strlen(value[i]), sizeof(char));
//                    if (!comp->s[vr[i]]) {
//                        comp->state = modelError;
//                        FILTERED_LOG(comp, fmi3Error, LOG_ERROR, "fmi3SetString: Out of memory.")
//                        return fmi3Error;
//                    }
//                }
//                strcpy((char *)comp->s[vr[i]], (char *)value[i]);
//            }
            
            //S(comp, vr[i])[j] = value[k++];
        }
        
        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetBoolean: #b%u# = %s", vr[i], value[i] ? "true" : "false")
    }
    
//    for (i = 0; i < nvr; i++) {
//        char *string = (char *)comp->s[vr[i]];
//        if (vrOutOfRange(comp, "fmi3SetString", vr[i], NUMBER_OF_STRINGS))
//            return fmi3Error;
//        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetString: #s%d# = '%s'", vr[i], value[i])
//
//        if (value[i] == NULL) {
//            if (string) comp->functions->freeMemory(c, string);
//            comp->s[vr[i]] = NULL;
//            FILTERED_LOG(comp, fmi3Warning, LOG_ERROR, "fmi3SetString: string argument value[%d] = NULL.", i);
//        } else {
//            if (string == NULL || strlen(string) < strlen(value[i])) {
//                if (string) comp->functions->freeMemory(c, string);
//                comp->s[vr[i]] = (char *)comp->functions->allocateMemory(c, 1 + strlen(value[i]), sizeof(char));
//                if (!comp->s[vr[i]]) {
//                    comp->state = modelError;
//                    FILTERED_LOG(comp, fmi3Error, LOG_ERROR, "fmi3SetString: Out of memory.")
//                    return fmi3Error;
//                }
//            }
//            strcpy((char *)comp->s[vr[i]], (char *)value[i]);
//        }
//    }
    
    if (nvr > 0) comp->isDirtyValues = fmi3True;
    
    return fmi3OK;
}

fmi3Status fmi3GetFMUstate (fmi3Component c, fmi3FMUstate* FMUstate) {
    return unsupportedFunction(c, "fmi3GetFMUstate", MASK_fmi3GetFMUstate);
}
fmi3Status fmi3SetFMUstate (fmi3Component c, fmi3FMUstate FMUstate) {
    return unsupportedFunction(c, "fmi3SetFMUstate", MASK_fmi3SetFMUstate);
}
fmi3Status fmi3FreeFMUstate(fmi3Component c, fmi3FMUstate* FMUstate) {
    return unsupportedFunction(c, "fmi3FreeFMUstate", MASK_fmi3FreeFMUstate);
}
fmi3Status fmi3SerializedFMUstateSize(fmi3Component c, fmi3FMUstate FMUstate, size_t *size) {
    return unsupportedFunction(c, "fmi3SerializedFMUstateSize", MASK_fmi3SerializedFMUstateSize);
}
fmi3Status fmi3SerializeFMUstate (fmi3Component c, fmi3FMUstate FMUstate, fmi3Byte serializedState[], size_t size) {
    return unsupportedFunction(c, "fmi3SerializeFMUstate", MASK_fmi3SerializeFMUstate);
}
fmi3Status fmi3DeSerializeFMUstate (fmi3Component c, const fmi3Byte serializedState[], size_t size,
                                    fmi3FMUstate* FMUstate) {
    return unsupportedFunction(c, "fmi3DeSerializeFMUstate", MASK_fmi3DeSerializeFMUstate);
}

fmi3Status fmi3GetDirectionalDerivative(fmi3Component c,
                                        const fmi3ValueReference vUnknown_ref[], size_t nUnknown,
                                        const fmi3ValueReference vKnown_ref[] , size_t nKnown,
                                        const fmi3Real dvKnown[], size_t nDvKnown,
                                        fmi3Real dvUnknown[], size_t nDvUnknown) {
    return unsupportedFunction(c, "fmi3GetDirectionalDerivative", MASK_fmi3GetDirectionalDerivative);
}

// ---------------------------------------------------------------------------
// Functions for FMI for Co-Simulation
// ---------------------------------------------------------------------------
/* Simulating the slave */
fmi3Status fmi3SetRealInputDerivatives(fmi3Component c, const fmi3ValueReference vr[], size_t nvr,
                                     const fmi3Integer order[], const fmi3Real value[], size_t size) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3SetRealInputDerivatives", MASK_fmi3SetRealInputDerivatives)) {
        return fmi3Error;
    }
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetRealInputDerivatives: nvr= %d", nvr)
    FILTERED_LOG(comp, fmi3Error, LOG_ERROR, "fmi3SetRealInputDerivatives: ignoring function call."
        " This model cannot interpolate inputs: canInterpolateInputs=\"fmi3False\"")
    return fmi3Error;
}

fmi3Status fmi3GetRealOutputDerivatives(fmi3Component c, const fmi3ValueReference vr[], size_t nvr,
                                      const fmi3Integer order[], fmi3Real value[], size_t size) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3GetRealOutputDerivatives", MASK_fmi3GetRealOutputDerivatives))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3GetRealOutputDerivatives: nvr= %d", nvr)
    FILTERED_LOG(comp, fmi3Error, LOG_ERROR,"fmi3GetRealOutputDerivatives: ignoring function call."
        " This model cannot compute derivatives of outputs: MaxOutputDerivativeOrder=\"0\"")
    for (i = 0; i < nvr; i++) value[i] = 0;
    return fmi3Error;
}

fmi3Status fmi3CancelStep(fmi3Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3CancelStep", MASK_fmi3CancelStep)) {
        // always fmi3CancelStep is invalid, because model is never in modelStepInProgress state.
        return fmi3Error;
    }
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3CancelStep")
    FILTERED_LOG(comp, fmi3Error, LOG_ERROR,"fmi3CancelStep: Can be called when fmi3DoStep returned fmi3Pending."
        " This is not the case.");
    // comp->state = modelStepCanceled;
    return fmi3Error;
}

fmi3Status fmi3DoStep(fmi3Component c, fmi3Real currentCommunicationPoint,
                    fmi3Real communicationStepSize, fmi3Boolean noSetFMUStatePriorToCurrentPoint) {
    ModelInstance *comp = (ModelInstance *)c;
    double h = communicationStepSize / 10;
    int k,i;
    const int n = 10; // how many Euler steps to perform for one do step
    double prevState[max(NUMBER_OF_STATES, 1)];
    double prevEventIndicators[max(NUMBER_OF_EVENT_INDICATORS, 1)];
    int stateEvent = 0;
    int timeEvent = 0;

    if (invalidState(comp, "fmi3DoStep", MASK_fmi3DoStep))
        return fmi3Error;

    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3DoStep: "
        "currentCommunicationPoint = %g, "
        "communicationStepSize = %g, "
        "noSetFMUStatePriorToCurrentPoint = fmi3%s",
        currentCommunicationPoint, communicationStepSize, noSetFMUStatePriorToCurrentPoint ? "True" : "False")

    if (communicationStepSize <= 0) {
        FILTERED_LOG(comp, fmi3Error, LOG_ERROR,
            "fmi3DoStep: communication step size must be > 0. Fount %g.", communicationStepSize)
        comp->state = modelError;
        return fmi3Error;
    }

#if NUMBER_OF_EVENT_INDICATORS>0
    // initialize previous event indicators with current values
    for (i = 0; i < NUMBER_OF_EVENT_INDICATORS; i++) {
        prevEventIndicators[i] = getEventIndicator(comp, i);
    }
#endif

    // break the step into n steps and do forward Euler.
    comp->time = currentCommunicationPoint;
    for (k = 0; k < n; k++) {
        comp->time += h;

#if NUMBER_OF_STATES>0
        for (i = 0; i < NUMBER_OF_STATES; i++) {
            prevState[i] = R(comp, vrStates[i])[0];
        }
        for (i = 0; i < NUMBER_OF_STATES; i++) {
            fmi3ValueReference vr = vrStates[i];
            R(comp, vr)[0] += h * getReal(comp, vr + 1)[0]; // forward Euler step
        }
#endif

#if NUMBER_OF_EVENT_INDICATORS>0
        // check for state event
        for (i = 0; i < NUMBER_OF_EVENT_INDICATORS; i++) {
            double ei = getEventIndicator(comp, i);
            if (ei * prevEventIndicators[i] < 0) {
                FILTERED_LOG(comp, fmi3OK, LOG_EVENT,
                    "fmi3DoStep: state event at %g, z%d crosses zero -%c-", comp->time, i, ei < 0 ? '\\' : '/')
                stateEvent++;
            }
            prevEventIndicators[i] = ei;
        }
#endif
        // check for time event
        if (comp->eventInfo.nextEventTimeDefined && (comp->time - comp->eventInfo.nextEventTime > -DT_EVENT_DETECT)) {
            FILTERED_LOG(comp, fmi3OK, LOG_EVENT, "fmi3DoStep: time event detected at %g", comp->time)
            timeEvent = 1;
        }

        if (stateEvent || timeEvent) {
            eventUpdate(comp, &comp->eventInfo, timeEvent, fmi3True);
            timeEvent = 0;
            stateEvent = 0;
        }

        // terminate simulation, if requested by the model in the previous step
        if (comp->eventInfo.terminateSimulation) {
            FILTERED_LOG(comp, fmi3Discard, LOG_ALL, "fmi3DoStep: model requested termination at t=%g", comp->time)
            comp->state = modelStepFailed;
            return fmi3Discard; // enforce termination of the simulation loop
        }
    }
    return fmi3OK;
}

/* Inquire slave status */
static fmi3Status getStatus(char* fname, fmi3Component c, const fmi3StatusKind s) {
    const char *statusKind[3] = {"fmi3DoStepStatus","fmi3PendingStatus","fmi3LastSuccessfulTime"};
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, fname, MASK_fmi3GetStatus)) // all get status have the same MASK_fmi3GetStatus
            return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "$s: fmi3StatusKind = %s", fname, statusKind[s])

    switch(s) {
        case fmi3DoStepStatus: FILTERED_LOG(comp, fmi3Error, LOG_ERROR,
            "%s: Can be called with fmi3DoStepStatus when fmi3DoStep returned fmi3Pending."
            " This is not the case.", fname)
            break;
        case fmi3PendingStatus: FILTERED_LOG(comp, fmi3Error, LOG_ERROR,
            "%s: Can be called with fmi3PendingStatus when fmi3DoStep returned fmi3Pending."
            " This is not the case.", fname)
            break;
        case fmi3LastSuccessfulTime: FILTERED_LOG(comp, fmi3Error, LOG_ERROR,
            "%s: Can be called with fmi3LastSuccessfulTime when fmi3DoStep returned fmi3Discard."
            " This is not the case.", fname)
            break;
        case fmi3Terminated: FILTERED_LOG(comp, fmi3Error, LOG_ERROR,
            "%s: Can be called with fmi3Terminated when fmi3DoStep returned fmi3Discard."
            " This is not the case.", fname)
            break;
    }
    return fmi3Discard;
}

fmi3Status fmi3GetStatus(fmi3Component c, const fmi3StatusKind s, fmi3Status *value) {
    return getStatus("fmi3GetStatus", c, s);
}

fmi3Status fmi3GetRealStatus(fmi3Component c, const fmi3StatusKind s, fmi3Real *value) {
    if (s == fmi3LastSuccessfulTime) {
        ModelInstance *comp = (ModelInstance *)c;
        if (invalidState(comp, "fmi3GetRealStatus", MASK_fmi3GetRealStatus))
            return fmi3Error;
        *value = comp->time;
        return fmi3OK;
    }
    return getStatus("fmi3GetRealStatus", c, s);
}

fmi3Status fmi3GetIntegerStatus(fmi3Component c, const fmi3StatusKind s, fmi3Integer *value) {
    return getStatus("fmi3GetIntegerStatus", c, s);
}

fmi3Status fmi3GetBooleanStatus(fmi3Component c, const fmi3StatusKind s, fmi3Boolean *value) {
    if (s == fmi3Terminated) {
        ModelInstance *comp = (ModelInstance *)c;
        if (invalidState(comp, "fmi3GetBooleanStatus", MASK_fmi3GetBooleanStatus))
            return fmi3Error;
        *value = comp->eventInfo.terminateSimulation;
        return fmi3OK;
    }
    return getStatus("fmi3GetBooleanStatus", c, s);
}

fmi3Status fmi3GetStringStatus(fmi3Component c, const fmi3StatusKind s, fmi3String *value) {
    return getStatus("fmi3GetStringStatus", c, s);
}

// ---------------------------------------------------------------------------
// Functions for fmi3 for Model Exchange
// ---------------------------------------------------------------------------
/* Enter and exit the different modes */
fmi3Status fmi3EnterEventMode(fmi3Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3EnterEventMode", MASK_fmi3EnterEventMode))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3EnterEventMode")

    comp->state = modelEventMode;
    comp->isNewEventIteration = fmi3True;
    return fmi3OK;
}

fmi3Status fmi3NewDiscreteStates(fmi3Component c, fmi3EventInfo *eventInfo) {
    ModelInstance *comp = (ModelInstance *)c;
    int timeEvent = 0;
    if (invalidState(comp, "fmi3NewDiscreteStates", MASK_fmi3NewDiscreteStates))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3NewDiscreteStates")

    comp->eventInfo.newDiscreteStatesNeeded = fmi3False;
    comp->eventInfo.terminateSimulation = fmi3False;
    comp->eventInfo.nominalsOfContinuousStatesChanged = fmi3False;
    comp->eventInfo.valuesOfContinuousStatesChanged = fmi3False;

    if (comp->eventInfo.nextEventTimeDefined && comp->eventInfo.nextEventTime <= comp->time) {
        timeEvent = 1;
    }
    eventUpdate(comp, &comp->eventInfo, timeEvent, comp->isNewEventIteration);
    comp->isNewEventIteration = fmi3False;

    // copy internal eventInfo of component to output eventInfo
    eventInfo->newDiscreteStatesNeeded = comp->eventInfo.newDiscreteStatesNeeded;
    eventInfo->terminateSimulation = comp->eventInfo.terminateSimulation;
    eventInfo->nominalsOfContinuousStatesChanged = comp->eventInfo.nominalsOfContinuousStatesChanged;
    eventInfo->valuesOfContinuousStatesChanged = comp->eventInfo.valuesOfContinuousStatesChanged;
    eventInfo->nextEventTimeDefined = comp->eventInfo.nextEventTimeDefined;
    eventInfo->nextEventTime = comp->eventInfo.nextEventTime;

    return fmi3OK;
}

fmi3Status fmi3EnterContinuousTimeMode(fmi3Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3EnterContinuousTimeMode", MASK_fmi3EnterContinuousTimeMode))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL,"fmi3EnterContinuousTimeMode")

    comp->state = modelContinuousTimeMode;
    return fmi3OK;
}

fmi3Status fmi3CompletedIntegratorStep(fmi3Component c, fmi3Boolean noSetFMUStatePriorToCurrentPoint,
                                     fmi3Boolean *enterEventMode, fmi3Boolean *terminateSimulation) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3CompletedIntegratorStep", MASK_fmi3CompletedIntegratorStep))
        return fmi3Error;
    if (nullPointer(comp, "fmi3CompletedIntegratorStep", "enterEventMode", enterEventMode))
        return fmi3Error;
    if (nullPointer(comp, "fmi3CompletedIntegratorStep", "terminateSimulation", terminateSimulation))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL,"fmi3CompletedIntegratorStep")
    *enterEventMode = fmi3False;
    *terminateSimulation = fmi3False;
    return fmi3OK;
}

/* Providing independent variables and re-initialization of caching */
fmi3Status fmi3SetTime(fmi3Component c, fmi3Real time) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3SetTime", MASK_fmi3SetTime))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetTime: time=%.16g", time)
    comp->time = time;
    return fmi3OK;
}

fmi3Status fmi3SetContinuousStates(fmi3Component c, const fmi3Real x[], size_t nx){
    ModelInstance *comp = (ModelInstance *)c;
    int i;
    if (invalidState(comp, "fmi3SetContinuousStates", MASK_fmi3SetContinuousStates))
        return fmi3Error;
    if (invalidNumber(comp, "fmi3SetContinuousStates", "nx", nx, NUMBER_OF_STATES))
        return fmi3Error;
    if (nullPointer(comp, "fmi3SetContinuousStates", "x[]", x))
        return fmi3Error;
//#if NUMBER_OF_STATES>0
//    for (i = 0; i < nx; i++) {
//        fmi3ValueReference vr = vrStates[i];
//        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3SetContinuousStates: #r%d#=%.16g", vr, x[i])
//        assert(vr < NUMBER_OF_REALS);
//        comp->r[vr] = x[i];
//    }
//#endif
    return fmi3OK;
}

/* Evaluation of the model equations */
fmi3Status fmi3GetDerivatives(fmi3Component c, fmi3Real derivatives[], size_t nx) {
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3GetDerivatives", MASK_fmi3GetDerivatives))
        return fmi3Error;
    if (invalidNumber(comp, "fmi3GetDerivatives", "nx", nx, NUMBER_OF_STATES))
        return fmi3Error;
    if (nullPointer(comp, "fmi3GetDerivatives", "derivatives[]", derivatives))
        return fmi3Error;
#if NUMBER_OF_STATES>0
    for (i = 0; i < nx; i++) {
        fmi3ValueReference vr = vrStates[i] + 1;
        derivatives[i] = getReal(comp, vr)[0]; // to be implemented by the includer of this file
        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3GetDerivatives: #r%d# = %.16g", vr, derivatives[i])
    }
#endif
    return fmi3OK;
}

fmi3Status fmi3GetEventIndicators(fmi3Component c, fmi3Real eventIndicators[], size_t ni) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3GetEventIndicators", MASK_fmi3GetEventIndicators))
        return fmi3Error;
    if (invalidNumber(comp, "fmi3GetEventIndicators", "ni", ni, NUMBER_OF_EVENT_INDICATORS))
        return fmi3Error;
#if NUMBER_OF_EVENT_INDICATORS>0
    for (i = 0; i < ni; i++) {
        eventIndicators[i] = getEventIndicator(comp, i); // to be implemented by the includer of this file
        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3GetEventIndicators: z%d = %.16g", i, eventIndicators[i])
    }
#endif
    return fmi3OK;
}

fmi3Status fmi3GetContinuousStates(fmi3Component c, fmi3Real states[], size_t nx) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3GetContinuousStates", MASK_fmi3GetContinuousStates))
        return fmi3Error;
    if (invalidNumber(comp, "fmi3GetContinuousStates", "nx", nx, NUMBER_OF_STATES))
        return fmi3Error;
    if (nullPointer(comp, "fmi3GetContinuousStates", "states[]", states))
        return fmi3Error;
#if NUMBER_OF_STATES>0
    for (i = 0; i < nx; i++) {
        fmi3ValueReference vr = vrStates[i];
        states[i] = getReal(comp, vr)[0]; // to be implemented by the includer of this file
        FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3GetContinuousStates: #r%u# = %.16g", vr, states[i])
    }
#endif
    return fmi3OK;
}

fmi3Status fmi3GetNominalsOfContinuousStates(fmi3Component c, fmi3Real x_nominal[], size_t nx) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi3GetNominalsOfContinuousStates", MASK_fmi3GetNominalsOfContinuousStates))
        return fmi3Error;
    if (invalidNumber(comp, "fmi3GetNominalContinuousStates", "nx", nx, NUMBER_OF_STATES))
        return fmi3Error;
    if (nullPointer(comp, "fmi3GetNominalContinuousStates", "x_nominal[]", x_nominal))
        return fmi3Error;
    FILTERED_LOG(comp, fmi3OK, LOG_FMI_CALL, "fmi3GetNominalContinuousStates: x_nominal[0..%d] = 1.0", nx-1)
    for (i = 0; i < nx; i++)
        x_nominal[i] = 1;
    return fmi3OK;
}

#ifdef __cplusplus
} // closing brace for extern "C"
#endif
