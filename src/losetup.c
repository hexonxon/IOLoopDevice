//
//  Copyright (c) 2012 ACME, Inc. All rights reserved.
//
//  Utility to setup new loop devices
//  losetup [-r] file
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

#include "kext/loopctl.h"


#define DIE(msg, args...) { fprintf(stderr, msg, ## args); exit(EXIT_FAILURE); }


static io_connect_t open_controller(void)
{
    CFMutableDictionaryRef dict = IOServiceMatching(kLoopControllerMatchKey);
    if(!dict) {
        return IO_OBJECT_NULL;
    }
	
    io_iterator_t iter;
    kern_return_t rc = IOServiceGetMatchingServices(kIOMasterPortDefault, dict, &iter);
    if(KERN_SUCCESS != rc) {
        return IO_OBJECT_NULL;
    }
	
    io_service_t serv = IOIteratorNext(iter);
    IOObjectRelease(iter);
    
    if(serv == IO_OBJECT_NULL) {
        return IO_OBJECT_NULL;
    }
    
    io_connect_t port;
    rc = IOServiceOpen(serv, mach_task_self(), 0, &port);
    IOObjectRelease(serv);
    
    if(KERN_SUCCESS != rc) {
        return IO_OBJECT_NULL;
    }
	
    return port;
}


static void close_controller(io_connect_t port)
{
    IOServiceClose(port);
}


static IOReturn controller_ctl(int ctlcode, void* data_in, size_t insize, void* data_out, size_t outsize)
{
    io_connect_t port = open_controller();
    if(IO_OBJECT_NULL == port) {
        return kIOReturnNotAttached;
    }
	
    uint64_t ctl_u64 = ctlcode;

    int rc = IOConnectCallMethod(port, 
                                 kLoopCTL_Magic, 
                                 &ctl_u64, 1, 
                                 data_in, insize, 
                                 NULL, NULL, 
                                 data_out, &outsize);
	
	close_controller(port);
    return rc;
}


static int loop_attach(uint64_t nblocks, int ro)
{
    struct LoopAttachCtl ctl;
    ctl.readonly = ro;
    ctl.size = nblocks;
    ctl.pid = getpid();
    
    return controller_ctl(kLoopCTL_Attach, &ctl, sizeof(ctl), NULL, 0);
}


static void onDeviceAdded(void* refCon, io_iterator_t iter) 
{ 
    io_service_t* service = (io_service_t*) refCon;
    while ((*service = IOIteratorNext(iter)) != 0) { 
        CFNumberRef pid_prop = (CFNumberRef) IORegistryEntryCreateCFProperty(*service, CFSTR(kLoopDriverPIDKey), kCFAllocatorDefault, 0);
        if (!pid_prop) {
            DIE("Could not get PID property\n");
        }
        
        int pid;
        CFNumberGetValue(pid_prop, kCFNumberIntType, &pid);
        if (pid == getpid()) {
            printf("Found device object for pid %d\n", pid);
            CFRunLoopStop(CFRunLoopGetCurrent());
            return;
        }
        
        IOObjectRelease(*service); 
    } 

} 


static void onDeviceRemoved(void* refCon, io_iterator_t iter)
{
    io_service_t service;
    while ((service = IOIteratorNext(iter)) != 0) { 
        char name[0xff] = {0};
        IORegistryEntryGetName(service, name);
        
        printf("Device %s removed\n", name);
        
        IOObjectRelease(service); 
    } 
}


static io_service_t waitForLoopDevice(void)
{
    CFDictionaryRef matchingDict = IOServiceMatching(kLoopDriverMatchKey); 
    IONotificationPortRef notificationPort = IONotificationPortCreate(kIOMasterPortDefault); 
    CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(notificationPort); 
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode); 
    
    io_iterator_t iter;
    io_service_t service;

    kern_return_t kr = IOServiceAddMatchingNotification(notificationPort, kIOFirstMatchNotification, matchingDict, onDeviceAdded, &service, &iter); 
    if (KERN_SUCCESS != kr) {
        DIE("Could not add loop device notification\n");
    }

    onDeviceAdded(&service, iter);
    if (!service) {
        CFRunLoopRun();
    }
    
    IONotificationPortDestroy(notificationPort); 
    IOObjectRelease(iter); 
    
    return service; 
}


static int gTerminate = 0;


struct LoopContext {
    const char*     file;
    int             fd;
    int             readonly;
    io_connect_t    deviceConn;
    io_object_t     notification;
};


static void requestPortCallback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    struct UserRequestNotification* request = (struct UserRequestNotification*) msg;
    struct LoopContext* context = (struct LoopContext*) info;
    
    if (request->header.msgh_id == kLoopUserTerminateNotification) {
        // Driver terminates?
        printf("Request loop terminated\n");
        CFRunLoopStop(CFRunLoopGetCurrent());
        return;
    } else if (gTerminate) {
        // We are terminating?
        printf("Request loop terminated 2\n");
        CFRunLoopStop(CFRunLoopGetCurrent());
        return;
    }
        
    
    void* buffer        = (void*) request->data.buffer;
    size_t nbytes       = (size_t) request->data.nblocks * kLoopBlockSize;
    off_t offset        = request->data.offset * kLoopBlockSize;

    printf("New %s request arrived: file %s, offset %llu, size %lu, buffer %p\n", 
           (request->data.direction == kLoopIODirection_Read ? "read" : "write"),
           context->file, offset, nbytes, buffer);

    if (request->data.direction == kLoopIODirection_Read) {
        
        if (nbytes != pread(context->fd, buffer, nbytes, offset)) {
            DIE("Could not read %lu bytes from file %s at offset %llu\n", nbytes, context->file, offset);
        }
        
    } else {
        
        assert(!context->readonly);
        
        if (nbytes != pwrite(context->fd, buffer, nbytes, offset)) {
            DIE("Could not write %lu bytes from file %s at offset %llu\n", nbytes, context->file, offset);
        }
        
    }
    
    // Send result to driver
    uint64_t ctl = kLoopDriverCTL_Complete;
    request->data.result = kIOReturnSuccess;
    
    int rc = IOConnectCallMethod(context->deviceConn, 
                                 kLoopCTL_Magic, 
                                 &ctl, 1, 
                                 &request->data, sizeof(request->data), 
                                 NULL, NULL, 
                                 NULL, NULL);
    
    if (KERN_SUCCESS != rc) {
        DIE("Complete request failed with 0x%x\n", rc);
    }
}


static void beginRequestQueue(io_service_t driver, const char* file, int readonly)
{
    // Open driver
    io_connect_t driverConn;
    kern_return_t error = IOServiceOpen(driver, mach_task_self(), 0, &driverConn);
    if (KERN_SUCCESS != error) {
        DIE("Failed opening loop driver: 0x%x\n", error);
    }

    
    // Create port context
    struct LoopContext* ctx = (struct LoopContext*) malloc(sizeof(struct LoopContext));
    if (!ctx) {
        DIE("Could not allocate context structure\n");
    }
    
    int fd = open(file, (readonly ? O_RDONLY : O_RDWR));
    if (fd < 0) {
        DIE("Could not open file\n");
    }
    
    ctx->file       = file;
    ctx->fd         = fd;
    ctx->readonly   = readonly;
    ctx->deviceConn = driverConn;
    
    
    // Setup notification port
    CFMachPortContext      portContext; 
    portContext.version         = 0; 
    portContext.info            = ctx; 
    portContext.retain          = NULL; 
    portContext.release         = NULL; 
    portContext.copyDescription = NULL; 

    CFMachPortRef notificationPort = CFMachPortCreate(kCFAllocatorDefault, requestPortCallback, &portContext, NULL); 
    if (!notificationPort) {
        DIE("Could not create mach notification port\n");
    }
    
    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, notificationPort, 0); 
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode); 
    
    
    // Set driver notification port
    error = IOConnectSetNotificationPort(driverConn, 0, CFMachPortGetPort(notificationPort), 0); 
    if (KERN_SUCCESS != error) {
        DIE("Failed setting driver notification port: 0x%x\n", error);
    }
    
    
    // Begin request loop
    CFRunLoopRun();
    
    
    // Clean up resources after request loop terminated
    IOServiceClose(driverConn);
    IOObjectRelease(driver);
    
    close(ctx->fd);
    free(ctx);
}


static void sighandler(int signo)
{
    gTerminate = 1;
}

static void usage(void) 
{
    printf("Usage: losetup [-r] file\n");
}


int main(int argc, char** argv)
{
    int ro = 0;
    char opt;
    
    while (-1 != (opt = getopt(argc, argv, "r:"))) {
        switch (opt) {
        case 'r': 
            ro = 1; 
            break;
                
        default: 
            usage(); 
            DIE("Invalid option\n");
        }
    }
    
    const char* file = argv[optind];
    if (!file) {
        DIE("Please specify file name\n");
    }
    
    int error = access(file, F_OK|R_OK);
    if (error) {
        DIE("File \"%s\" does not exist or cannot be read by you\n", file);
    }
    
    if (!ro) {
        error = access(file, W_OK);
        if (error) {
            DIE("You cannot write to file \"%s\", please try again with -r option\n", file);
        }
    }

    struct stat st;
    if (0 != stat(file, &st)) {
        DIE("stat on file \"%s\" failed\n", file);
    }

    uint64_t nblocks = st.st_size / kLoopBlockSize;
    if (st.st_size & (kLoopBlockSize - 1)) {
        fprintf(stderr, "Warning: file size %llu is not a multiple of the loop device block size. Will truncate down to %llu\n", 
                st.st_size, nblocks * kLoopBlockSize);
    }
    
 
    // Send controller command and wait for our new loop driver
    error = loop_attach(nblocks, ro);
    if (error) {
        DIE("Failed attaching new loop device: 0x%x\n", error);
    }
    
    io_service_t driver = waitForLoopDevice();
    if (!driver) {
        DIE("Could not wait for new loop device\n");
    }
    
    // Create a request port and start servicing
    signal(SIGKILL, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGSTOP, sighandler);
    signal(SIGQUIT, sighandler);
    
    beginRequestQueue(driver, file, ro);
    
    return EXIT_SUCCESS;
}
