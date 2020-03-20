#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.



echo '/output/core.%h.%e.%t' > /proc/sys/kernel/core_pattern
ulimit -c unlimited




echo "creating:"
./cloudCryptFS.docker -osrc=/srv/ -opass=menne -o allow_other --create yes --log-level 10  > /output/create_log.txt || exit 1 

rm -f /srv/log.txt
#echo "mounting:"
#./cloudCryptFS.docker -osrc=/srv/ -onegative_timeout=0 -ohard_remove -onoauto_cache -odirect_io,use_ino -oattr_timeout=0 -oentry_timeout=0 -opass=menne -o allow_other mnt || exit 1

#cp /cloudCryptFS.docker /mnt/
#fusermount -u /mnt
#echo "migrating:"
#./cloudCryptFS.docker -osrc=/srv/ -opass=menne -o allow_other --migrateto latest  || exit 1 

echo "mounting:"
./cloudCryptFS.docker -osrc=/srv/ -onegative_timeout=0 -ohard_remove -onoauto_cache -odirect_io,use_ino -oattr_timeout=0 -oentry_timeout=0 -opass=menne -o allow_other mnt --loglevel 10 || exit 1
touch /mnt/._meta
touch /mnt/._stats

cd /mnt 


OUTPUT="/output"
{

echo "running tests:"
if [[ "$@" == "create_read" ]]; then
		cp /cloudCryptFS.docker /mnt/ccfstestfile -v
		cp /testfile1 /mnt -v
		cp /testfile2 /mnt -v
elif [[ "$@" == "dedup" ]]; then
		cp /testfile1 /bigfile /mnt -v
		mkdir /mnt/test
		sleep 6
		cp /testfile2 /bigfile /mnt/test -v
		sleep 6
else
		prove -e bash -r /tests/$@ 
fi

} | tee -a /srv/log.txt



cd /

cp /srv/log.txt $OUTPUT
cat /mnt/._meta > "$OUTPUT/_meta"
cat /mnt/._stats > "$OUTPUT/_stats"

echo "unmounting: "
fusermount -u /mnt

sleep 5
cp /srv/log.txt $OUTPUT



echo "re-mounting: "
./cloudCryptFS.docker -osrc=/srv/ -onegative_timeout=0 -ohard_remove -onoauto_cache -odirect_io,use_ino -oattr_timeout=0 -oentry_timeout=0 -opass=menne -o allow_other mnt 

ls -lha /mnt > "$OUTPUT/ls.txt"
cat /mnt/._meta > "$OUTPUT/_meta_final"
cat /mnt/._stats > "$OUTPUT/_stats_final"
cp /srv/log.txt $OUTPUT

{

if [[ "$@" == "create_read" ]]; then
		md5sum /mnt/testfile1 /testfile1 /mnt/testfile2 /testfile2 /mnt/ccfstestfile /cloudCryptFS.docker
elif [[ "$@" == "dedup" ]]; then
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
		if [[ "$C" != "About ~153MB de-duplicated." ]]; then
				echo "FAILT! de-dup failed? $C"
		else
				echo "PASS: $C"
		fi
		
		FILEA=`cat /mnt/bigfile | md5sum`
		FILEB=`cat /mnt/test/bigfile | md5sum`
		FILEC=`cat /bigfile | md5sum`

		if [[ "$FILEA" != "$FILEB" ]];then
			echo "FAIL! Files are not equal"
		else
			echo "PASS: Files are equal"
		fi

		if [[ "$FILEA" != "$FILEC" ]];then
			echo "FAIL! File does not match original (\"$FILEA\" != \"$FILEC\")"
			echo "original:"
			tail -c 256 /bigfile | xxd
			echo "copy:"
			tail -c 256 /mnt/bigfile | xxd	
		else
			echo "PASS: File matches original"
		fi

fi
} | tee -a /srv/log.txt


echo "unmounting: "
fusermount -u /mnt

cp /srv/log.txt $OUTPUT



