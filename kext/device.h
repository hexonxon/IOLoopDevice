//
//  Copyright (c) 2012 ACME, Inc
//  All rights reserved.
//

#ifndef LOOP_KEXT_DEVICE_H
#define LOOP_KEXT_DEVICE_H

#include <IOKit/storage/IOBlockStorageDevice.h>


class org_acme_LoopDriver;


/**
 * Storage device nub.
 * Interfaces with the rest of the storage family to send requests to our transport driver.
 */
class org_acme_LoopDevice : public IOBlockStorageDevice {
OSDeclareDefaultStructors(org_acme_LoopDevice);
    
public:
	
	virtual bool init(OSDictionary * properties = 0);
	
    virtual bool attach(IOService* provider);

		
    IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer,
                              UInt64 block, UInt64 nblks,
                              IOStorageCompletion completion);
	
	IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer,
                              UInt64 block, UInt64 nblks,
                              IOStorageAttributes *attributes,
                              IOStorageCompletion *completion);
	
	IOReturn doSyncReadWrite(IOMemoryDescriptor *buffer, UInt32 block, UInt32 nblks);

    
	char* getVendorString(void);
	char* getProductString(void);
	char* getRevisionString(void);
	char* getAdditionalDeviceInfoString(void);
    
	IOReturn doLockUnlockMedia(bool doLock);
    
	IOReturn doSynchronizeCache(void);

	IOReturn doEjectMedia(void);
	
	IOReturn doFormatMedia(UInt64 byteCapacity);
	
	UInt32 doGetFormatCapacities(UInt64* capacities, UInt32 capacitiesMaxCount) const;
    
	IOReturn reportBlockSize(UInt64 *blockSize);
	IOReturn reportMaxValidBlock(UInt64 *maxBlock);
	
	
	IOReturn reportRemovability(bool *isRemovable);
	IOReturn reportEjectability(bool *isEjectable);
	IOReturn reportLockability(bool *isLockable);
	
	
	IOReturn reportMaxReadTransfer(UInt64 blockSize, UInt64 *max);
	IOReturn reportMaxWriteTransfer(UInt64 blockSize,UInt64 *max);
	
	
	IOReturn reportMediaState(bool *mediaPresent,bool *changedState);
	IOReturn reportPollRequirements(bool *pollRequired, bool *pollIsExpensive);
	
    
	IOReturn getWriteCacheState(bool *enabled);
	IOReturn setWriteCacheState(bool enabled);
    
	IOReturn reportWriteProtection(bool *isWriteProtected);
	
	
private:
	
	org_acme_LoopDriver*	mDriver;
	bool                    mLocked;
};

#endif
