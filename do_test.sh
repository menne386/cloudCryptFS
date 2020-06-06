#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

echo '/output/core' > /proc/sys/kernel/core_pattern
ulimit -c unlimited

UIDGID="`id -u`:`id -g`"
echo "$@" > /output/cmdline


function compare_files {
	local FILEA=`cat $1 | md5sum`
	local FILEB=`cat $2 | md5sum`
	
	if [[ "$FILEA" != "$FILEB" ]];then
		echo "FAIL! $1 does not match $2 (\"$FILEA\" != \"$FILEB\")"
		echo "A:"
		tail -c 256 $1 | xxd
		echo "B:"
		tail -c 256 $2 | xxd	
	else
		echo "PASS: $1 matches $2"
	fi
}

function file_deleted {
	if [[ -f "$1" ]]; then
		echo "FAIL! $1 was not deleted as expected"
	else
		echo "PASS: $1 was deleted as expected"
	fi
}

function usage {
	echo "usage: $0 [--uidgid xxx:xxx] some test names"
	echo "Avaiable tests:"
	echo "create_read, dedup, fstest, crashresistant"
	cd /tests/ 
	echo "Available tests in fstest:"
	ls -d */ 
	cd - > /dev/null
}

function check_for_crash {
	for i in `ls /output/core* 2> /dev/null`; do
		echo "!!!!Found core dump $i!!!!"
		gdb /cloudCryptFS.docker $i -ex "thread apply all bt" -ex q --batch > /output/core_dump_bt
		chown -R $UIDGID /output
		exit 1
	done
}

function remount {
		echo "re-mounting for $1"
		fusermount -u /mnt
		sleep 2
		cp /srv/log.txt $OUTPUT
		chown -R $UIDGID /output
		check_for_crash
		/cloudCryptFS.docker -osrc=/srv/ -onegative_timeout=0 -ohard_remove -onoauto_cache -odirect_io,use_ino -oattr_timeout=0 -oentry_timeout=0 -opass=menne -o allow_other mnt > /output/remount_$1.txt  || quit
}

function quit {
	check_for_crash
	exit 1
}


#parse --uidgid parameter
if [[ "$1" == "--uidgid" ]]; then
	shift
	UIDGID=$1
	shift
fi

#display usage:
if [[ "$@" == "" ]]; then 
	usage
	exit 1
fi


echo "creating..."
/cloudCryptFS.docker -osrc=/srv/ -opass=menne -o allow_other --create yes --log-level 10  > /output/create_log.txt || quit

check_for_crash

rm -f /srv/log.txt
#echo "mounting:"
#/cloudCryptFS.docker -osrc=/srv/ -onegative_timeout=0 -ohard_remove -onoauto_cache -odirect_io,use_ino -oattr_timeout=0 -oentry_timeout=0 -opass=menne -o allow_other mnt || exit 1

#cp /cloudCryptFS.docker /mnt/
#fusermount -u /mnt
#echo "migrating:"
#/cloudCryptFS.docker -osrc=/srv/ -opass=menne -o allow_other --migrateto latest  || exit 1 

echo "mounting..."
/cloudCryptFS.docker -osrc=/srv/ -onegative_timeout=0 -ohard_remove -onoauto_cache -odirect_io,use_ino -oattr_timeout=0 -oentry_timeout=0 -opass=menne -o allow_other mnt --loglevel 10 > /output/mount_log.txt || quit
touch /mnt/._meta
touch /mnt/._stats

check_for_crash





OUTPUT="/output"
{


while (( "$#" )); do

echo "running test... $1"
if [[ "$1" == "crashresistant" ]]; then
		cp /testfile1 /mnt/tf1 -v
		cp /testfile2 /mnt/tf2 -v
		remount "crashresistant"
		rm -f /mnt/tf1
		cp /cloudCryptFS.docker /mnt/crashfile -v
		kill -9 `pidof cloudCryptFS.docker`
		cp -rv /srv/journal /output
		rm -fv /srv/._lock
		touch /output/crashresistant
elif [[ "$1" == "create_read" ]]; then
		cp /cloudCryptFS.docker /mnt/ccfstestfile -v
		cp /testfile1 /mnt -v
		cp /testfile2 /mnt -v
		touch /output/create_read
elif [[ "$1" == "dedup" ]]; then
		cp /testfile1 /bigfile /mnt -v
		mkdir /mnt/test
		sleep 6
		cp /testfile2 /bigfile /mnt/test -v
		sleep 6
		touch /output/dedup
elif [[ -d "/tests/$1" || -f "/tests/$1" ]]; then
		cd /mnt
		if [[ "$2" == "-v" ]]; then
			prove -e bash -r /tests/$1 -v 
			shift
		else
			prove -e bash -r /tests/$1  
		fi
		cd -
elif [[ "$1" == "fstest" ]]; then
		cd /mnt
		if [[ "$2" == "-v" ]]; then
			prove -e bash -r /tests/ -v 
			shift
		else
			prove -e bash -r /tests/  
		fi
		cd -
else 
	echo "test $1 not found!"
	usage
fi

shift


cp /srv/log.txt $OUTPUT


done

} | tee -a /srv/log.txt

cd /

cp /srv/log.txt $OUTPUT
cat /mnt/._meta > "$OUTPUT/_meta"
cat /mnt/._stats > "$OUTPUT/_stats"

remount "step2"

ls -lha /mnt > "$OUTPUT/ls.txt"
cat /mnt/._meta > "$OUTPUT/_meta_final"
cat /mnt/._stats > "$OUTPUT/_stats_final"
cp /srv/log.txt $OUTPUT

check_for_crash
chown -R $UIDGID /output


{

if [[ -f "/output/crashresistant" ]]; then
	compare_files /mnt/crashfile /cloudCryptFS.docker
	file_deleted /mnt/tf1
fi

if [[ -f "/output/create_read" ]]; then
	compare_files /mnt/testfile1 /testfile1
	compare_files /mnt/testfile2 /testfile2
	compare_files /mnt/ccfstestfile /cloudCryptFS.docker
fi

if [[ -f "/output/dedup" ]]; then
	A=`grep "de-dup" -B2 /output/_stats`
	B=`grep "de-dup" -B2 /output/_stats_final`
	C=`grep "de-dup" /output/_stats_final`
	
	
	if [[ "$A" != "$B" ]]; then 
		echo "FAIL! Dedup stats are not equal after re-mount"
		echo "A: $A"
		echo "B: $B"
	else
		echo "PASS: after re-mouting, deduplication stats are the same!"
	fi
	if [[ "$C" != "About ~153MB de-duplicated." && "$C" != "About ~155MB de-duplicated." && "$C" != "About ~157MB de-duplicated." ]]; then
			echo "FAILT! de-dup failed? $C"
	else
			echo "PASS: De-duplicated the expected amount"
	fi
	
	compare_files /mnt/bigfile /mnt/test/bigfile
	compare_files /bigfile /mnt/bigfile
fi

touch /srv/decrypt_fail
cp /srv/decrypt_fail $OUTPUT

find /srv > "$OUTPUT/find.txt"

#grep "ERROR" /srv/log.txt
#grep "WARNING" /srv/log.txt

} | tee -a /srv/log.txt

chown -R $UIDGID /output

echo "unmounting..."
fusermount -u /mnt

check_for_crash

cp /srv/log.txt $OUTPUT

chown -R $UIDGID /output

