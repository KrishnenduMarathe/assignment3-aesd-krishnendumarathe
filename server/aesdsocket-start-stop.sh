#!/bin/sh

case "$1" in
	start)
		echo "INIT: Starting AESD Socket Server!"
		start-stop-daemon -S -x /usr/bin/aesdsocket -- -d
		;;
	stop)
		echo "INIT: Stopping AESD Socker Server!"
		start-stop-daemon -K -x /usr/bin/aesdsocket
		;;
	*)
		echo "INIT: Wrong Argument passed. Valid Arguments: start/stop"
		;;
esac
