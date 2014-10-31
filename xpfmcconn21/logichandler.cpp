#include "logichandler.h"
#include "XPLMProcessing.h"
#include <iostream>

float HandlerCallbackInit(float, float, int, void* inRefCon)
{
    LogicHandler* handler = static_cast<LogicHandler*>(inRefCon);
    handler->initState();
    return 0;
}

float HandlerCallbackProcess(float, float, int, void* inRefCon)
{
    LogicHandler* handler = static_cast<LogicHandler*>(inRefCon);
    if (!handler->isSuspended())
        handler->processState();
    else
        printf("Handler %s suspended\n",handler->name().c_str());
    return handler->processingFrequency();
}

void LogicHandler::hookMeIn()
{
    if (!this->publishData()) {
        std::cerr << "Publishing Data failed for " << this->name() << std::endl;
    }
    this->suspend(false);
    XPLMRegisterFlightLoopCallback(HandlerCallbackInit,-1,this);
    XPLMRegisterFlightLoopCallback(HandlerCallbackProcess,-3,this);
}

void LogicHandler::hookMeOff()
{
    XPLMUnregisterFlightLoopCallback(HandlerCallbackInit, this);
    XPLMUnregisterFlightLoopCallback(HandlerCallbackProcess, this);
}
