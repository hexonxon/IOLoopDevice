//
//  Copyright (c) 2012 Evgeny Yakovlev. 
//  All rights reserved.
//

#include "controller.h"
#include "driver.h"
#include "build.h"
#include "loopctl.h"


#pragma mark -
#pragma mark Controller


bool org_acme_LoopController::init(OSDictionary* props)
{
    if (!IOService::init(props)) {
        return false;
    }
    
    // init code
    return true;
}


bool org_acme_LoopController::start(IOService* provider)
{
    LOOP_TRACE;
    this->registerService();
    return IOService::start(provider);
}


void org_acme_LoopController::stop(IOService* provider)
{
    LOOP_TRACE;
    IOService::stop(provider);
}


void org_acme_LoopController::free()
{
    LOOP_TRACE;
    IOService::free();
}


IOReturn org_acme_LoopController::loopAttach(struct LoopAttachCtl* arg)
{
    LOOP_TRACE;

    org_acme_LoopDriver* driver = new org_acme_LoopDriver;
    IOReturn error = kIOReturnSuccess;
    
    if (!driver) {
        LOOP_IOLOG("Could not create loop driver instance\n");
        return kIOReturnNoMemory;
    }
    
    if (!driver->init(arg->size, arg->readonly, arg->pid)) {
        LOOP_IOLOG("Could not initialize loop driver instance\n");
        error = kIOReturnInternalError;
        goto ERROR_OUT;
    }
    
    if (!driver->attach(this)) {
        LOOP_IOLOG("Could not attach new driver instance\n");
        error = kIOReturnNotAttached;
        goto ERROR_OUT;
    }
         
    driver->registerService();
    
ERROR_OUT:
    
    driver->release(); // in case of success we will have an extra reference
    return error;
}


#pragma mark -
#pragma mark User Client


bool org_acme_LoopControllerClient::initWithTask(task_t owningTask, void* securityToken, UInt32 type, OSDictionary* properties)
{
    return IOUserClient::initWithTask(owningTask, securityToken, type, properties);
}

bool org_acme_LoopControllerClient::start(IOService* provider)
{
    LOOP_TRACE;

    mController = OSDynamicCast(org_acme_LoopController, provider);
    LOOP_ASSERT(mController);

    return IOUserClient::start(provider);
}

IOReturn org_acme_LoopControllerClient::externalMethod(uint32_t selector, IOExternalMethodArguments* arguments, IOExternalMethodDispatch* dispatch, OSObject* target, void* reference)
{
    LOOP_TRACE;

    if (selector != kLoopCTL_Magic) {
        return kIOReturnUnsupported;
    }
    
    struct IOExternalMethodDispatch d = { 
        sIOCTL, 
        arguments->scalarInputCount,
        arguments->structureInputSize,
        arguments->scalarOutputCount,
        arguments->structureOutputSize 
    };
        
    target = mController;
    return IOUserClient::externalMethod(selector, arguments, &d, target, reference);
}


IOReturn org_acme_LoopControllerClient::sIOCTL(OSObject * target, void * reference, IOExternalMethodArguments * arguments)
{
    uint64_t ctlcode = *arguments->scalarInput;
	org_acme_LoopController* controller = (org_acme_LoopController*) target;
    
    switch (ctlcode) {
    case kLoopCTL_Attach: {
        struct LoopAttachCtl* arg = (struct LoopAttachCtl*) arguments->structureInput;
        return controller->loopAttach(arg);
    }
            
    default: {
        LOOP_ASSERT(0 && "Unknown ioctl");
        return kIOReturnUnsupported;    // Shut up GCC
    }
            
    };
}


OSDefineMetaClassAndStructors(org_acme_LoopController, IOService);
OSDefineMetaClassAndStructors(org_acme_LoopControllerClient, IOUserClient);
