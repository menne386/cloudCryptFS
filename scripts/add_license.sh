#!/bin/bash

HEADER=$1

shift

for i in $@ # or whatever other pattern...
do
		if ! grep -q Copyright "$i"
		then
				cat $HEADER "$i" >"$i.new" && mv "$i.new" "$i"
		fi
done
