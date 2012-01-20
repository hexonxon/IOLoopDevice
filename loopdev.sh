#!/bin/sh

LOOP_KEXT=./build/IOLoopDevice.kext
case $1 in

    start) 
        echo "Starting loop device.."
        prev_owner=`stat -f %u:%g $LOOP_KEXT`
        sudo chown -R root:wheel $LOOP_KEXT
        sudo chmod -R 0755 $LOOP_KEXT
        sudo kextutil -v $LOOP_KEXT
        sudo chown -R $prev_owner $LOOP_KEXT
        ;;

    stop)
        echo "Stopping loop device.."
        sudo kextunload -b org.acme.IOLoopDevice
        ;;

    *)
        echo "invalid command"
        echo "usage: loopdev start|stop"
        ;;

esac

exit 0;

