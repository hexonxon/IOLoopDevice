//
//  Copyright (c) 2012 ACME, Inc. All rights reserved.
//
//  This file contains all definitions for loop kernel control protocol.
//  It is included by both kernel and user space code.
//  LOOP_KERNEL macro is used to compile only kernel-specific parts.
//

#ifndef LOOP_KEXT_CTL_H
#define LOOP_KEXT_CTL_H

#include <stdint.h>
#include <mach/mach_types.h>


#define kLoopControllerMatchKey		"org_acme_LoopController"   // Loop controller IORegistry match key
#define kLoopDriverMatchKey         "org_acme_LoopDriver"       // Loop driver IORegistry match key
#define kLoopDriverPIDKey           "pid"                       // Loop driver pid property key


enum {
    kLoopBlockSize      = 512,                      // Size of the loop block size
    kLoopMaxBufferSize  = kLoopBlockSize * 20480,   // Max request buffer size
};


enum {
    kLoopIODirection_Read   = 0,            // Read from file
    kLoopIODirection_Write  = 1,            // Write to file
};
typedef uint32_t LoopIODirection;



/******************************************************************************
 *
 * User -> Kernel
 * IOCTLs through IOUserClient instances from user space to kernel
 *
 ******************************************************************************/


enum {
    kLoopCTL_Magic          = 0x1243,       // Magic code for all our ioctls
    kLoopCTL_Attach         = 0x01,         // LoopController ioctl to attach a new loop device
    kLoopDriverCTL_Complete = 0x02,         // LoopDriver ioctl to complete io request from user space
};


struct LoopAttachCtl {
    uint64_t    size;
    int         readonly;
    int         pid;
};



/******************************************************************************
 *
 * Kernel -> User
 * Mach port ipc messages from kernel to user space
 *
 ******************************************************************************/


enum {
    kLoopUserIONotification = 0,            // New IO request, LoopIONotification as data
    kLoopUserTerminateNotification = 1,     // Notifying device is about to be ejected, user space needs to close the connection, no data
};

// User process io request description send through a mach port
struct UserIORequest {
    uint64_t            offset;     // File block offset
    uint64_t            nblocks;    // Total blocks to process
    uint64_t            buffer;     // Pointer to the kernel data buffer mapped into task virtual address space
    uint32_t            direction;  // Read or write as in kLoopIODirection_XXX
    uint32_t            result;     // kIOReturnXXX code, set by user once request is completed
    uint64_t            priv;       // Private request handle
};


// Enclosing mach message request structure
struct UserRequestNotification {
    mach_msg_header_t       header;     // Standard message header
    struct UserIORequest    data;
};

#endif
