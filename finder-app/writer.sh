#!/bin/bash

if [ "$#" -ne 2 ]; then
	echo "Wrong number of arguments"
	exit 1
fi

writefile=$1
writestr=$2

parentfolder=$(dirname $writefile)

if [ ! -d $parentfolder ]; then
	mkdir -p $parentfolder

	if [ $? -ne 0 ]; then
		echo "Failed to create containing directory"
		exit 1
	fi
fi

echo $writestr > $writefile

if [ $? -ne 0 ]; then
	echo "Failed to create file"
	exit 1
fi

