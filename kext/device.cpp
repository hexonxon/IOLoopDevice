//
//  Copyright (c) 2012 ACME, Inc
//  All rights reserved.
//


#include "device.h"
#include "driver.h"
#include "build.h"
#include "loopctl.h"


#define kLoopDeviceVendorString     "ACME, Inc."
#define kLoopDeviceVersionString	"1.0"
#define kLoopDeviceModelString      "Loop Device"
#define kLoopDeviceInfoString       "loop"


bool org_acme_LoopDevice::init(OSDictionary* properties)
{
    if (!IOBlockStorageDevice::init(properties)) {
        return false;
    }
	
    mDriver = NULL;
    mLocked = false;
    
    return true;
}

IOReturn org_acme_LoopDevice::doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                    UInt64 block, UInt64 nblks,
                                    IOStorageAttributes *attributes,
                                    IOStorageCompletion *completion) 
{
    return this->mDriver->createRequest(buffer, block, nblks, completion);
}


bool org_acme_LoopDevice::attach(IOService* provider)
{
    mDriver = OSDynamicCast(org_acme_LoopDriver, provider);
    LOOP_ASSERT(mDriver);
    
    if (!IOService::attach(provider)) {
        LOOP_IOLOG("Attach failed\n");
        return false;
    }
	
    return true;
}


IOReturn org_acme_LoopDevice::doEjectMedia(void) 
{
    if (mLocked) {
        return kIOReturnNotPermitted;
    } else {
        return mDriver->eject();
    }
}


IOReturn org_acme_LoopDevice::doFormatMedia(UInt64 byteCapacity) 
{
    if ((mDriver->getSize() * kLoopBlockSize) != byteCapacity) {
        LOOP_IOLOG("Invalid capacity %llu\n", byteCapacity);
        return kIOReturnBadArgument;
    }
	
    return kIOReturnSuccess;
}


UInt32 org_acme_LoopDevice::doGetFormatCapacities(UInt64* capacities, UInt32 capacitiesMaxCount) const
{
    if ((capacities != NULL) && (capacitiesMaxCount != 0)) {
        capacities[0] = mDriver->getSize() * kLoopBlockSize;
        return 1;
    } else {
        return 0;
    }
}


IOReturn org_acme_LoopDevice::doLockUnlockMedia(bool doLock) 
{
    mLocked = doLock;
    return kIOReturnSuccess;
}


IOReturn org_acme_LoopDevice::doSynchronizeCache(void) 
{
    return kIOReturnSuccess;
}


char* org_acme_LoopDevice::getProductString(void) 
{
    static char s_model_string[] = kLoopDeviceModelString;
    return s_model_string;
}


char* org_acme_LoopDevice::getRevisionString(void) 
{
    static char s_version_string[] = kLoopDeviceVersionString;
    return s_version_string;
}


char* org_acme_LoopDevice::getVendorString(void) 
{
    static char s_vendor_string[] = kLoopDeviceVendorString;
    return s_vendor_string;
}


char* org_acme_LoopDevice::getAdditionalDeviceInfoString(void) 
{
    static char s_info_string[] = kLoopDeviceInfoString;
    return s_info_string;
}


IOReturn org_acme_LoopDevice::reportBlockSize(UInt64 *blockSize) 
{
    *blockSize = kLoopBlockSize;
    return kIOReturnSuccess;
}


IOReturn org_acme_LoopDevice::reportMaxValidBlock(UInt64 *maxBlock) 
{
    *maxBlock = mDriver->getSize() - 1;
    return kIOReturnSuccess;
}


IOReturn org_acme_LoopDevice::reportMaxReadTransfer(UInt64 blockSize, UInt64 *max) 
{
    *max = kLoopMaxBufferSize;
    return kIOReturnSuccess;
}


IOReturn org_acme_LoopDevice::reportMaxWriteTransfer(UInt64 blockSize, UInt64 *max) 
{
    *max = kLoopMaxBufferSize;
    return kIOReturnSuccess;
}


IOReturn org_acme_LoopDevice::reportRemovability(bool *isRemovable) 
{
    *isRemovable = true;
    return kIOReturnSuccess;
}


IOReturn org_acme_LoopDevice::reportEjectability(bool *isEjectable) 
{
    *isEjectable = !mLocked;
    return kIOReturnSuccess;
}

IOReturn org_acme_LoopDevice::reportLockability(bool *isLockable) 
{
    *isLockable = true;
    return kIOReturnSuccess;
}


IOReturn org_acme_LoopDevice::reportMediaState(bool* mediaPresent, bool* changedState) 
{
    *mediaPresent = true;
    *changedState = false;
    return kIOReturnSuccess;
}


IOReturn org_acme_LoopDevice::reportPollRequirements(bool* pollRequired, bool* pollIsExpensive) 
{
    *pollRequired = false;
    *pollIsExpensive = false;
    return kIOReturnSuccess;
}


IOReturn org_acme_LoopDevice::getWriteCacheState(bool* enabled) 
{
    return kIOReturnUnsupported;
}


IOReturn org_acme_LoopDevice::setWriteCacheState(bool enabled) 
{
    return kIOReturnUnsupported;
}


IOReturn org_acme_LoopDevice::reportWriteProtection(bool* isWriteProtected) 
{
    *isWriteProtected = mDriver->isWriteProtected();
    return kIOReturnSuccess;
}


OSDefineMetaClassAndStructors(org_acme_LoopDevice, IOBlockStorageDevice);
