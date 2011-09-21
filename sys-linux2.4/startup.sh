#! /bin/bash
# Copyright (c) 2011 Picture Elements, Inc.
# 
# This is a startup script for the cmpn device driver. Check if there are
# compatible devices, and if so load the module and create any nodes that
# are needed. To install, copy this file to this destination:
#
#   /etc/init.d/ise
#
# and enable its execute permissions for root. Then enable with this command:
#
#   chkconfig -a ise
#
### BEGIN INIT INFO
# Provides:       ISE
# Required-Start:
# Required-Stop:
# Default-Start:  2 3 5
# Default-Stop:
# Short-Description: Probe for ISE/JSE/EJSE devices
# Description:   Probe for ISE/JSE/EJSE devices and load drivers.
### END INIT INFO

. /etc/rc.status

# Load the cmp driver and probe for devices.
function start() {
    # How many ise devices?
    countISE=`lspci -n -d 0x12c5:0x007f | wc -l`
    countEJSE=`lspci -n -d 0x12c5:0x0091 | wc -l`
    countVJSE=`lspci -n -d 0x8086:0xb555 | wc -l`
    count=`expr $countISE + $countEJSE + $countVJSE`
    if [ $count = 0 ]; then
	return
    fi

    # Make sure the cmp module is loaded, then find the major number.
    /sbin/modprobe ise

    dev_major=`grep 'ise' /proc/devices | cut -d' ' -f1`
    if [ x$dev_major = x ]; then
        return
    fi

    # Create nodes for all the devices.
    N=0
    while [ $N -lt $count ]
    do
	mknod --mode=0666 /dev/ise${N}  c $dev_major $N
	mknod --mode=0666 /dev/isex${N} c $dev_major `expr 128 + $N`
	N=`expr $N + 1`
    done
}

function stop() {
    # How many ise devices?
    countISE=`lspci -n -d 0x12c5:0x007f | wc -l`
    countEJSE=`lspci -n -d 0x12c5:0x0091 | wc -l`
    countVJSE=`lspci -n -d 0x8086:0xb555 | wc -l`
    count=`expr $countISE + $countEJSE + $countVJSE`

    # Remove nodes for all the devices.
    N=0
    while [ $N -lt $count ]
    do
	rm -f /dev/ise${N} /dev/isex${N}
	N=`expr $N + 1`
    done

    /sbin/rmmod ise > /dev/null 2>&1 && true
}

case "$1" in
    start)
	echo -n "Starting ISE/JSE driver"
	start
	rc_status -v
        ;;
    stop)
        # Stop daemons.
        echo -n "Stopping ISE/JSE driver"
	stop
	rc_status -v
        ;;
    try-restart)
        $0 status >/dev/null && $0 restart
	rc_status
	;;
    restart)
	$0 stop
	$0 start
        rc_status
	;;
    force-reload)
	$0 stop && $0 start
	rc_status
	;;
    reload)
        rc_failed 3
	rc_status -v
	;;
    status)
	if $lsmod | grep -q ise; then
          echo -n "ISE driver loaded."
          rc_status -v
        else
          echo -n "ISE driver not loaded."
	  rc_status -u
        fi
        ;;
    *)
	echo "Usage: $0 {start|stop|try-restart|restart|force-reload|reload|status}"
        exit 1
esac

rc_exit
