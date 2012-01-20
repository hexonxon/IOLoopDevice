//
//  Copyright (c) 2012 ACME, Inc
//  All rights reserved.
//


#ifndef LOOP_KEXT_BUILD_H
#define LOOP_KEXT_BUILD_H

#include <IOKit/IOLib.h>

#define LOOP_IOLOG(fmt, args...)            IOLog(" [%s:%s] " fmt, this->getName(), __func__, ## args)

#ifdef LOOP_DEBUG
#  define MACH_ASSERT 1
#  include <kern/debug.h>
#  define LOOP_IOLOG_DEBUG(fmt, args...)    LOOP_IOLOG(fmt, args)
#  define LOOP_ASSERT(pred)                 if (!(pred)) panic("Loop Assertion %s failed", __STRINGIFY(pred));
#  define LOOP_TRACE                        LOOP_IOLOG("\n")
#else
#  define LOOP_IOLOG_DEBUG(fmt, args...)
#  define LOOP_ASSERT(pred)			
#  define LOOP_TRACE
#endif


#endif
