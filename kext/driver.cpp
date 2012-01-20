//
//  Copyright (c) 2012 ACME, Inc
//  All rights reserved.
//

#include "build.h"
#include "driver.h"
#include "device.h"
#include "loopctl.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>


// IO request context structure
typedef struct {
    IOMemoryDescriptor*         buffer;
    IOBufferMemoryDescriptor*   data;
    IOMemoryMap*                mapping;
    IOStorageCompletion         completion;
} LoopIO;


static void complete(IOStorageCompletion* completion, IOReturn result, UInt64 nbytes)
{
    if (completion && completion->action) {
        completion->action(completion->target, completion->parameter, result, nbytes);
    }
}


static void releaseRequest(LoopIO* io)
{
    io->mapping->release();
    io->data->release();
    IOFree(io, sizeof(*io));
}


#pragma mark -
#pragma mark Driver

bool org_acme_LoopDriver::init(UInt64 nblocks, bool readonly, int pid)
{
    if (!IOService::init()) {
        return false;
    }
    
    
    OSNumber* pid_prop = OSNumber::withNumber(pid, sizeof(pid) * 8);
    if(!pid_prop) {
        LOOP_IOLOG("Could not create pid property\n");
        return false;
    }

    if (!this->setProperty(kLoopDriverPIDKey, pid_prop)) {
        LOOP_IOLOG("Could not set pid property\n");
        pid_prop->release();
        return false;
    }

    pid_prop->release();

    
    OSString* client_class = OSString::withCString("org_acme_LoopDriverClient");
    if (!client_class) {
        LOOP_IOLOG("Could not create user client class property\n");
        return false;
    }

    if (!this->setProperty("IOUserClientClass", client_class)) {
        LOOP_IOLOG("Could not set user client class property\n");
        client_class->release();
        return false;
    }
    
    client_class->release();
    
    mTotalBlocks = nblocks;
    mReadOnly = readonly;
    mTask = NULL;
    mPort = NULL;
    
    return true;
}


bool org_acme_LoopDriver::start(IOService* provider)
{
    if (!IOService::start(provider)) {
        return false;
    }
    
    // At this point we will not publish our device nub just yet.
    // First we need to wait for the user process to contact us
    this->registerService();
    
    return true;
}


IOReturn org_acme_LoopDriver::helperProcessAttached(mach_port_t port, task_t task) 
{
    LOOP_TRACE;
    
    if (mPort) {
        LOOP_IOLOG("Driver port already registered\n");
        return kIOReturnError;
    }
    
    mPort = port;
    mTask = task;
    
    org_acme_LoopDevice* device = new org_acme_LoopDevice;
    if (!device) {
        LOOP_IOLOG("Could not create new device nub\n");
        return kIOReturnNoMemory;
    }
    
    if (!device->init()) {
        LOOP_IOLOG("Could not init new device nub\n");
        device->release();
        return kIOReturnInternalError;
    }
    
    if (!device->attach(this)) {
        LOOP_IOLOG("Could not attach new device nub\n");
        device->release();
        return kIOReturnInternalError;
    }
    
    mDevice = device;
    
    // attach got +1 refcount.
    // if we err somewhere after that detach will occur for us, no need to release anything.
    device->release();
    device->registerService();	
    
    return kIOReturnSuccess;
}


void org_acme_LoopDriver::helperProcessDetached()
{
    if (!mPort) {
        LOOP_IOLOG("Helper process already detached\n");
        return;
    }
    
    mTask = NULL;
    mPort = NULL;
    
    mDevice->stop(this);
}

void org_acme_LoopDriver::completeRequest(UserIORequest* request)
{
    LoopIO* io = (LoopIO*) request->priv;
    LOOP_ASSERT(io);
    
    if (request->result != kIOReturnSuccess) {
        complete(&io->completion, request->result, 0);
    } else {
        
        if (io->buffer->getDirection() == kIODirectionIn) {
            // read completion
            // decrypt and copy data to original caller buffer
            io->buffer->writeBytes(0, io->data->getBytesNoCopy(), io->buffer->getLength());
        } else {
            // write completion
        }
        complete(&io->completion, kIOReturnSuccess, request->nblocks * kLoopBlockSize);
    }
    
    releaseRequest(io);
}


IOReturn org_acme_LoopDriver::createRequest(IOMemoryDescriptor *buffer, UInt64 block, UInt64 nblks, IOStorageCompletion* completion)
{
    IOReturn                    error = kIOReturnSuccess;
    IOBufferMemoryDescriptor*   sharedBuffer = NULL;
    IOMemoryMap*                userMapping = NULL;
    LoopIODirection             direction = (buffer->getDirection() == kIODirectionOut) ? kLoopIODirection_Write : kLoopIODirection_Read;
    LoopIO*                     io = NULL;
    
    if (!mPort) {
        LOOP_IOLOG("Helper process not attached\n");
        return kIOReturnNotReady;
    }
    
    // Check request
    if ((block + nblks) > this->getSize()) {
        LOOP_IOLOG("Request too large\n");
        return kIOReturnBadArgument;
    }
    
    if ((buffer->getDirection() == kIODirectionOut) && (this->isWriteProtected())) {
        LOOP_IOLOG("Write request for read only device\n");
        return kIOReturnBadArgument;
    }


    // Allocate a buffer and create user mapping
    sharedBuffer = IOBufferMemoryDescriptor::withOptions(kIOMemoryKernelUserShared | kIODirectionOutIn, buffer->getLength(), page_size);
    if (!sharedBuffer) {
        LOOP_IOLOG("Could not allocate memory buffer\n");
        return kIOReturnNoMemory;
    }
    
    if (buffer->getDirection() == kIODirectionOut) {
        // write request
        // copy caller data to shared buffer and encrypt
        if (buffer->getLength() != buffer->readBytes(0, sharedBuffer->getBytesNoCopy(), buffer->getLength())) {
            error = kIOReturnIOError;
            goto ERROR_OUT;
        }
    }
    
    userMapping = sharedBuffer->createMappingInTask(mTask, NULL, kIOMapAnywhere);
    if (!userMapping) {
        LOOP_IOLOG("Could not map request buffer to user space\n");
        error = kIOReturnIPCError;
        goto ERROR_OUT;
    } else {
        LOOP_IOLOG_DEBUG("Mapped request buffer to user space address %p\n", (void*)userMapping->getVirtualAddress());
    }
    
    
    // Store request context and notify user process
    io = (LoopIO*) IOMalloc(sizeof(LoopIO));
    if (!io) {
        LOOP_IOLOG("Could not allocate io request structure\n");
        error = kIOReturnNoMemory;
        goto ERROR_OUT;
    }
    
    memset(io, 0, sizeof(*io));
    
    io->buffer      = buffer;
    io->completion  = *completion;
    io->mapping     = userMapping;
    io->data        = sharedBuffer;

    UserRequestNotification request;
    memset(&request, 0, sizeof(request));
    
    request.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0); 
    request.header.msgh_size        = sizeof(UserRequestNotification); 
    request.header.msgh_remote_port = mPort; 
    request.header.msgh_local_port  = MACH_PORT_NULL; 
    request.header.msgh_id          = kLoopUserIONotification; 
    
    request.data.offset             = block; 
    request.data.nblocks            = nblks;
    request.data.direction          = direction;
    request.data.buffer             = userMapping->getVirtualAddress();
    request.data.priv               = (uint64_t) io;
    
    error = mach_msg_send_from_kernel(&request.header, sizeof(UserRequestNotification)); 
    if (kIOReturnSuccess != error) {
        LOOP_IOLOG("Could not enqueue new request\n");
        goto ERROR_OUT;
    }

    return kIOReturnSuccess;
    
    
ERROR_OUT:
    
    if (io)             IOFree(io, sizeof(*io));
    if (userMapping)    userMapping->release();
    if (sharedBuffer)   sharedBuffer->release();
    return error;
}

bool org_acme_LoopDriver::terminate(IOOptionBits options)
{
    if (mPort) {
        
        // Notify user that we are going away
        // User can also be notified with standard service interest notifications 
        // but it is generally easier to handle this on the same port that receives io notifications
        
        UserRequestNotification request;
        memset(&request, 0, sizeof(request));
    
        request.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0); 
        request.header.msgh_size        = sizeof(UserRequestNotification); 
        request.header.msgh_remote_port = mPort; 
        request.header.msgh_local_port  = MACH_PORT_NULL; 
        request.header.msgh_id          = kLoopUserTerminateNotification; 
    
        // Ignore the error because client might be already dead
        (void) mach_msg_send_from_kernel(&request.header, sizeof(UserRequestNotification)); 
    }
    
    return IOService::terminate(options);
}


IOReturn org_acme_LoopDriver::eject()
{
    this->terminate(kIOServiceRequired);
    return kIOReturnSuccess;
}


#pragma mark -
#pragma mark Driver client

bool org_acme_LoopDriverClient::initWithTask(task_t owningTask, void* securityToken, UInt32 type, OSDictionary* properties)
{
    if (!IOUserClient::initWithTask(owningTask, securityToken, type, properties)) {
        return false;
    }
    
    mTask = owningTask;
    return true;
}

bool org_acme_LoopDriverClient::start(IOService* provider)
{
    mDriver = OSDynamicCast(org_acme_LoopDriver, provider);
    LOOP_ASSERT(mDriver);

    return IOUserClient::start(provider);
}

IOReturn org_acme_LoopDriverClient::clientClose(void)
{
    mDriver->helperProcessDetached();
    return IOUserClient::clientClose();
}

IOReturn org_acme_LoopDriverClient::clientDied(void)
{
    mDriver->helperProcessDetached();
    return IOUserClient::clientDied(); // TODO
}

IOReturn org_acme_LoopDriverClient::registerNotificationPort(mach_port_t port, UInt32 type, io_user_reference_t refCon) 
{ 
    return mDriver->helperProcessAttached(port, mTask);
}

IOReturn org_acme_LoopDriverClient::externalMethod(uint32_t selector, IOExternalMethodArguments* arguments, IOExternalMethodDispatch* dispatch, OSObject* target, void* reference)
{
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
    
    target = mDriver;
    return IOUserClient::externalMethod(selector, arguments, &d, target, reference);
}


IOReturn org_acme_LoopDriverClient::sIOCTL(OSObject * target, void * reference, IOExternalMethodArguments * arguments)
{
    uint64_t ctlcode = *arguments->scalarInput;
    org_acme_LoopDriver* driver = (org_acme_LoopDriver*) target;
    
    switch (ctlcode) {
    case kLoopDriverCTL_Complete: {
        struct UserIORequest* arg = (struct UserIORequest*) arguments->structureInput;
        driver->completeRequest(arg);
        return kIOReturnSuccess;
    }
            
    default: {
        LOOP_ASSERT(0 && "Unknown ioctl");
        return kIOReturnUnsupported;    // Shut up GCC
    }
            
    };
}

OSDefineMetaClassAndStructors(org_acme_LoopDriver, IOService);
OSDefineMetaClassAndStructors(org_acme_LoopDriverClient, IOUserClient);
