//
//  Copyright (c) 2012 ACME, Inc
//  All rights reserved.
//

#ifndef LOOP_KEXT_DRIVER_H
#define LOOP_KEXT_DRIVER_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/storage/IOStorage.h>

#include "crypto/alg.h"

struct UserIORequest;
class org_acme_LoopDevice;


/**
 * Loop transport driver.
 *
 * Acts as a provider to loop device nubs which it publishes.
 * Implements actual IO request processing.
 *
 * Request processing is accomplished with a user space daemon which does file io.
 * Communication with the user space deamon is done through a mach port.
 */
class org_acme_LoopDriver : public IOService {
OSDeclareDefaultStructors(org_acme_LoopDriver);
public:
    
    /**
     * Init driver instance.
     */
    virtual bool init(UInt64 nblocks, bool readonly, int pid);
    
    /**
     * Registers the driver with the IORegistry.
     * We will not publish our device nub just yet. 
     * Instead we need to wait for the user client to open us first.
     */
    virtual bool start(IOService* provider);
        
    /**
     * Terminate handler called by eject.
     * Will notify attached user client that we are going away.
     */
    virtual bool terminate(IOOptionBits options = 0);

    /**
	 * Create new async IO request.
	 */
	IOReturn createRequest(IOMemoryDescriptor* buffer, UInt64 block, UInt64 nblks, IOStorageCompletion* completion);
		
	/**
	 * Eject disk.
	 * Halt all processing and terminate.
	 */
	IOReturn eject();
	
	/**
	 * Get device size in blocks.
	 */
	UInt64 getSize() {
        return mTotalBlocks;
    }
		
	/**
     * Get device write protection status
     */
    bool isWriteProtected() {
        return mReadOnly;
    }
    
    
protected:
    
    friend class org_acme_LoopDriverClient;
    
    /**
     * Called by user client when user process is prepared to service requests.
     * Will finally publish and register the nub device.
     */
    IOReturn helperProcessAttached(mach_port_t port, task_t task);    
    
    /**
     * Called by user client when our helper process closes or terminates for some reason.
     * We will stop all processing but helper process should try and make sure to complete all requests before detaching (at least with error)
     * Otherwise data may be lost and we have no way to account for it.
     */
    void helperProcessDetached();
        
    /**
     * Called by user daemon through user client instance when previously dispatched reqeust completes.
     */
    void completeRequest(UserIORequest* request);
    
private:
    
    org_acme_LoopDevice*    mDevice;
    UInt64                  mTotalBlocks;
    bool                    mReadOnly;
    mach_port_t             mPort;
    task_t                  mTask;
    BCAlg                   mAlg;
};


/**
 * Driver user client
 * User deamon process notifies io request completions through this.
 */
class org_acme_LoopDriverClient : public IOUserClient {
OSDeclareDefaultStructors(org_acme_LoopDriverClient);
public:
    
    virtual bool start(IOService* provider);
	
	virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type, OSDictionary* properties);
    
    virtual IOReturn clientClose(void);
    
	virtual IOReturn clientDied(void);
    
    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, io_user_reference_t refCon);
    
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments* arguments, IOExternalMethodDispatch* dispatch = 0, OSObject* target = 0, void* reference = 0);
	
	
protected:
    
    // Static CTL dispatcher.
    // We prefer linux-style ioctl dispatch from a single static function.
    // Selector that gues into externalMethod is our magic value while real CTL code is stored in scalar argument
	static IOReturn sIOCTL(OSObject * target, void * reference, IOExternalMethodArguments * arguments);
	
private:
    
    org_acme_LoopDriver*    mDriver;
    task_t                  mTask;
};


#endif
