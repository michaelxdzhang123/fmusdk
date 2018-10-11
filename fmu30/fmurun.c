#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "fmi3Functions.h"

//typedef void      (*fmi3CallbackLogger)          (fmi3ComponentEnvironment, fmi3String, fmi3Status, fmi3String, fmi3String, ...);
//typedef void*     (*fmi3CallbackAllocateMemory)  (fmi3ComponentEnvironment, size_t, size_t);
//typedef void      (*fmi3CallbackFreeMemory)      (fmi3ComponentEnvironment, void*);
//typedef void      (*fmi3StepFinished)            (fmi3ComponentEnvironment, fmi3Status);

void logger(fmi3ComponentEnvironment component, fmi3String a, fmi3Status b, fmi3String c, fmi3String d, ...) {
    printf(a);
}

void* allocateMemory(fmi3ComponentEnvironment component, size_t nobj, size_t size) {
    return calloc(nobj, size);
}

void freeMemory(fmi3ComponentEnvironment component, void *obj) {
    free(obj);
}

void stepFinished(fmi3ComponentEnvironment componentEnvironment, fmi3Status status) {
    
}

int main(int argc, char *argv[]) {
    
    void *handle = NULL;
    
    fmi3InstantiateTYPE *instantiate = NULL;
    fmi3SetupExperimentTYPE *setupExperiment = NULL;
    fmi3EnterInitializationModeTYPE *enterInitializationMode = NULL;
    fmi3ExitInitializationModeTYPE *exitInitializationMode = NULL;
//    fmi3GetVariablesTYPE *getVariables = NULL;
//    fmi3SetVariablesTYPE *setVariables = NULL;
    fmi3GetRealTYPE *getReal = NULL;
    fmi3SetRealTYPE *setReal = NULL;
    fmi3DoStepTYPE *doStep = NULL;

//    fmi3Integer v10 = -10;
//    fmi3Real v11[] = { 0.1, 0.2, 0.3 };
    fmi3Status status = fmi3Fatal;
    
    fmi3Component c = NULL;
    fmi3ValueReference valueReferences[] = { 0, 2 };
//    fmi3Variable variables[] = { &v10, v11 };
    fmi3Real realVariables[] = { 1.1, 0.1 };
    
    fmi3CallbackFunctions callbacks = { logger, allocateMemory, freeMemory, stepFinished, NULL };
    
//    size_t variableSizes[] = { sizeof(fmi3Integer), 3 * sizeof(fmi3Real) };
    
    handle = dlopen(argv[1], RTLD_LAZY);
    
    instantiate = dlsym(handle, "fmi3Instantiate");
    setupExperiment = dlsym(handle, "fmi3SetupExperiment");
    enterInitializationMode = dlsym(handle, "fmi3EnterInitializationMode");
    exitInitializationMode = dlsym(handle, "fmi3ExitInitializationMode");
//    getVariables = dlsym(handle, "fmi3GetVariables");
//    setVariables = dlsym(handle, "fmi3SetVariables");
    getReal = dlsym(handle, "fmi3GetReal");
    setReal = dlsym(handle, "fmi3SetReal");
    doStep = dlsym(handle, "fmi3DoStep");

//    typedef fmi3Component fmi3InstantiateTYPE (fmi3String, fmi3Type, fmi3String, fmi3String, const fmi3CallbackFunctions*, fmi3Boolean, fmi3Boolean);
//    fmi2String  instanceName,
//    fmi2Type    fmuType,
//    fmi2String fmuGUID,
//    fmi2String fmuResourceLocation,
//    const fmi2CallbackFunctions* functions,
//    fmi2Boolean visible,
//    fmi2Boolean loggingOn
    
    c = instantiate("instanceName", fmi3CoSimulation, "{8c4e810f-3df3-4a00-8276-176fa3c9f003}", "file:///path", &callbacks, fmi3False, fmi3False);
    
    status = setupExperiment(c, fmi3False, 0.0, 0.0, fmi3False, 0.0);
    
    status = enterInitializationMode(c);

    status = exitInitializationMode(c);

//    status = setReal(c, valueReferences, 2, realVariables, 2);
//
//    realVariables[0] = 0;
//    realVariables[1] = 0;
//
//    status = getReal(c, valueReferences, 2, realVariables, 2);

    for (int i = 0; i < 1; i++) {
        doStep(c, 0.0, 0.1, fmi3False);
    }
    
//    status = setVariables(component, valueReferences, 2, variables, variableSizes);
//
//    v10 = 0;
//    v11[0] = 0;
//    v11[1] = 0;
//    v11[2] = 0;
//
//    status = getVariables(component, valueReferences, 2, variables, variableSizes);
    
    dlclose(handle);
    
    return 0;
}
