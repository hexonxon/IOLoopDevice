//
//  Copyright (c) 2012 Evgeny Yakovlev. 
//  All rights reserved.
//

#ifndef LOOP_CONTROLLER_H
#define LOOP_CONTROLLER_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>


/**
 * Loop controller driver.
 * IOResources matched class that accepts requests to publish new loop devices.
 */
class org_acme_LoopController : public IOService {
OSDeclareDefaultStructors(org_acme_LoopController)
    
public:
    
    /**
     * IOService constructor.
     */
	bool init(OSDictionary* props);
	
	/**
	 * Activates and publishes controller to start accepting user requests.
	 */
	bool start(IOService* privider);
	
	/**
	 * Deactivate controller and stop accepting requests.
	 */
	void stop(IOService* provider);

    /**
     * IOService destructor.
     */
	void free();
	

protected:
    
    friend class org_acme_LoopControllerClient;
    
    /**
     * Attach new loop device implementation.
     * @param arg       New loop device info.
     */
    IOReturn loopAttach(struct LoopAttachCtl* arg);
    
};


/**
 * Loop controller user client
 */
class org_acme_LoopControllerClient : public IOUserClient {
OSDeclareDefaultStructors(org_acme_LoopControllerClient);

public:
    
    virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type, OSDictionary* properties);

    virtual bool start(IOService* provider);
    
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments* arguments, IOExternalMethodDispatch* dispatch = 0, OSObject* target = 0, void* reference = 0);
	
	
protected:
    
    // Static CTL dispatcher.
    // We prefer linux-style ioctl dispatch from a single static function.
    // Selector that gues into externalMethod is our magic value while real CTL code is stored in scalar argument
	static IOReturn sIOCTL(OSObject * target, void * reference, IOExternalMethodArguments * arguments);
	
private:
    
    org_acme_LoopController*    mController;
};

#endif
