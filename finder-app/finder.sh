#!/bin/bash

if [ "$#" -ne 2 ]; then
	echo "Wrong number of arguments"
	exit 1
fi

filesdir="$1"
searchstr="$2"

if [ ! -d "$filesdir" ]; then
	echo "Error: Argument 1 is not a directory"
	exit 1
fi

countfiles=$(grep -rl $searchstr $filesdir | wc -l)
countmatches=$(grep -Iro $searchstr $filesdir | wc -l)

echo "The number of files are $countfiles and the number of matching lines are $countmatches"

