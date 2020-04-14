#VoxWar cross-platform makefile

ifneq ($(words $(MAKECMDGOALS)),1)
.DEFAULT_GOAL = all
%:
	@$(MAKE) $@ --no-print-directory -rRf $(firstword $(MAKEFILE_LIST))
else
ifndef ECHO
T := $(shell $(MAKE) $(MAKECMDGOALS) --no-print-directory \
	-nrRf $(firstword $(MAKEFILE_LIST)) \
	ECHO="COUNTTHIS" | grep -c "COUNTTHIS")
N := x
C = $(words $N)$(eval N := x $N)
ECHO = echo "`expr $C '*' 100 / $T`" %
endif

#The directory for your source
SRCDIR	:= src/* src/modules/*/* 
#The extension used for your source
SOURCESEXTENSION    := cpp

#The compiler used:
CC		    := g++
LD		    := g++ 
OBJCOPY		    := objcopy
STRIP		    := strip
GDB		    := gdb

#LTOFLAGS := -flto=5 -fuse-linker-plugin
LTOFLAGS :=

PLATFORMFLAGS := -march=native -mtune=native 
OPTIMIZEFLAGS := -O3

#-fno-strict-overflow -fno-strict-aliasing 
#Compiler and linker flags: -g Generate debugging, -Wall -Werror Display lotsa errors, standard to c++11 Optimize for size, dont do optimization that violate strict aliasing. 
GENERALCXXFLAGS:= -g -m64 -pipe -Wall -Wextra -Werror -Wno-unused-parameter -std=c++17 $(PLATFORMFLAGS) -Isrc/ -D_FILE_OFFSET_BITS=64 
#-fconcepts
#extra flags overridden per target
EXTRAFLAGS	:= 
#Flags used when compiling:
CXXFLAGS	:= $(GENERALCXXFLAGS) -Wl,-fuse-ld=gold
#Librarys used when linking:
LIBS 			:= -lfuse -pthread -lsodium -lstdc++fs 

EXECUTABLE 		:= cloudCryptFS
DEXECUTABLE 		:= cloudCryptFS.docker
#file that your project stores the version number in
VERSIONFILE 		:= src/buildn.h
VERSION			:= $(shell cat src/buildn.h | grep buildnr | cut -d\  -f3)
NEWVERSION		:= $(shell expr $(VERSION) + 1)


#Now follow the BUILDIN stuff: DO NOT CHANGE!
SRCS	:= $(foreach dir,$(SRCDIR),$(wildcard $(dir).$(SOURCESEXTENSION) ) )
OBJS 	:= $(patsubst %.$(SOURCESEXTENSION),lin/%.o,$(SRCS))
DEPS 	:= $(patsubst %.o,%.d,$(OBJS))

DOBJS 	:= $(patsubst %.$(SOURCESEXTENSION),dckr/%.o,$(SRCS))
DDEPS 	:= $(patsubst %.o,%.d,$(DOBJS))


.PHONY: all clean edit printopt printlib testandcopy
.SILENT: edit deps_dbg rundbg versionnr clean run  $(EXECUTABLE) $(DEXECUTABLE) $(DOBJS) $(OBJS) printopt printlib printunitobj testandcopy

all:  testandcopy 

printopt:
	echo $(GENERALCXXFLAGS)

printlib:
	echo $(STATICOBJS) $(LIBS)

printunitobj: 
	echo $(UOBJS)

testandcopy: $(OBJS) $(EXECUTABLE) #$(WEXECUTABLE)
	#$(ECHO) copying to server
	#cp -f $(WEXECUTABLE) $(EXECUTABLE) $(WEXECUTABLEU) /mnt/space/newtorrent &
	#scp -P 4040 $(WEXECUTABLE).dbg $(WEXECUTABLE) *.dll menne@192.168.178.230:/mnt/space/newtorrent
	$(ECHO) DONE

-include $(DEPS) $(DDEPS) 

lin/%.o : %.cpp
	$(ECHO) linux $< $(OPTIMIZEFLAGS) $(EXTRAFLAGS)
	mkdir -p $(dir $@)
	$(CC) -c $< $(CXXFLAGS) $(OPTIMIZEFLAGS) $(EXTRAFLAGS)  -MMD -MT '$@' -o $@ 

dckr/%.o : %.cpp
	$(ECHO) docker $< $(OPTIMIZEFLAGS) $(EXTRAFLAGS)
	mkdir -p $(dir $@)
	$(CC) -c $< $(CXXFLAGS) $(OPTIMIZEFLAGS) $(EXTRAFLAGS)  -MMD -MT '$@' -o $@ 

unittestout: $(UOBJS)
	touch unittestout

docker_test:
	docker run --name cloudcryptfs_backup --rm --cap-add SYS_ADMIN -p 873:873 --device /dev/fuse -it menne386/cloudcryptfs_backup

docker_fstest:
	docker run --name cloudcryptfs_fstest --rm -v "$(pwd)/test:/srv" --security-opt apparmor:unconfined --cap-add SYS_ADMIN --device /dev/fuse -it menne386/cloudcryptfs_fstest


docker_enter:
	docker exec -it cloudcryptfs_backup /bin/sh

$(EXECUTABLE): $(OBJS)
	$(ECHO) Linking $@
	$(LD) -o .$@.dbg  $(OBJS) $(CXXFLAGS) $(OPTIMIZEFLAGS)  $(LIBS) 
	$(OBJCOPY) --strip-all --add-gnu-debuglink=".$@.dbg" .$@.dbg $@	

$(DEXECUTABLE): $(DOBJS)
	$(ECHO) Linking $@
	$(LD) -o .$@.dbg  $(DOBJS) $(CXXFLAGS) $(OPTIMIZEFLAGS)  $(LIBS) 
	$(OBJCOPY) --strip-all --add-gnu-debuglink=".$@.dbg" .$@.dbg $@	

clean: 
	rm -rf $(EXECUTABLE) $(DEXECUTABLE) lin dckr 

rundbg : $(EXECUTABLE)
	$(GDB) ./$(EXECUTABLE)  

valgrind : $(EXECUTABLE)
	valgrind --trace-children=yes --leak-check=full --log-file=valg.log	./$(EXECUTABLE) -f --onegative_timeout=0 -ohard_remove -onoauto_cache -odirect_io,use_ino -oattr_timeout=0 -oentry_timeout=0 -opass=menne -o allow_other mntpoint 
	#+unit model/data/characters/goku/models/normal/goku\ normal.md5mesh

drd: $(EXECUTABLE)
	valgrind --tool=drd --exclusive-threshold=10 --log-file=drd.log	./$(EXECUTABLE) --noredirect --debug --nomods --valgrind +unit 

callgrind: $(EXECUTABLE)
	valgrind --tool=callgrind --log-file=calg.log	./$(EXECUTABLE) --noredirect --debug --valgrind 


	

endif
	

