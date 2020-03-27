// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "main.h"

#define FUSE_USE_VERSION 27
#include <fuse.h>
#include "modules/filesystem/fs.h"
#include "modules/util/files.h"
#include <fcntl.h>
#include <stdio.h>
#include <chrono>

#ifndef _WIN32
#include <linux/fs.h>
#include <sys/types.h>
#include <unistd.h>
#else 
#include <time.h>
#endif
#include <cstring>


int error_to_fuse(filesystem::error & in) {
	return - (int)in;
}

#ifdef _WIN32
#define LOG_OPERATION() FS->srvDEBUG("\n\n>>operation ",__FUNCTION__,": ",path)
#define LOG_OPERATION_NOPATH() FS->srvDEBUG("operation ",__FUNCTION__)
#else
#define LOG_OPERATION() FS->srvDEBUG("\n\n>>operation ",__PRETTY_FUNCTION__,": ",path)
#define LOG_OPERATION_NOPATH() FS->srvDEBUG("operation ",__PRETTY_FUNCTION__)
#endif

using namespace filesystem;

timeHolder currentTime() {//@todo: can be put in utility file.

	using namespace std::chrono;

	time_point<system_clock,nanoseconds> tp = system_clock::now();


	timeHolder t;

	auto secs = time_point_cast<seconds>(tp);
	auto ns = time_point_cast<nanoseconds>(tp) - time_point_cast<nanoseconds>(secs);
	

	t.tv_sec = secs.time_since_epoch().count();
	t.tv_nsec = ns.count();

	return t;
}

static void copyTime(const timeHolder & in, struct timespec & out) {
	out.tv_sec = in.tv_sec.load();
	out.tv_nsec = in.tv_nsec.load();
}

static unique_ptr<filesystem::context> getContext(bool getGroups = false) {
	auto * ctx = fuse_get_context();
	
	
	auto ptr = make_unique<filesystem::context>();
	//CLOG("ctx:uid:",ctx->uid, "pid:",ctx->pid );
	ptr->uid =ctx->uid;
	ptr->gid =ctx->gid;
	if(getGroups) {
#ifndef _WIN32
		std::vector<my_gid_t> gids;
		gids.resize(64);
		auto res = fuse_getgroups(gids.size(),&gids[0]);
		if(res>0) {
			for(int a=0;a<res;a++) {
				ptr->gids.insert(gids[a]);
			}
			FS->srvDEBUG("getContext: Got ",ptr->gids.size()," groups");
		} else if(res<0) {
			FS->srvDEBUG("getContext: Got error code ",res," getting groups");
			/*char path[128];
			sprintf(path, "/proc/%i/task/%i/status", ctx->pid,ctx->pid);
			auto r = util::getSystemString(path);
			CLOG("From ",path,":\n",r);*/

		} else {
			FS->srvDEBUG("getContext: no groups");
		}
#endif
	}
	
	
	return ptr;
}

static filesystem::access flagsToAccess(int flags) {
	if((flags & O_RDWR) == O_RDWR) {
		return filesystem::access::RW;
	}

	if((flags & O_WRONLY) == O_WRONLY) {
		return filesystem::access::W;
	}
	if((flags & O_TRUNC) == O_TRUNC) {
		return filesystem::access::RW;
	}
	return filesystem::access::R;
}

static int getattr_callback(const char *path, MYSTAT *stbuf) {
	
	memset(stbuf, 0, sizeof(MYSTAT));
	
	my_err_t errCode;
	auto F = FS->get(path,&errCode);
	if(F->valid()) {
		auto ctx = getContext();
		//should have at least read access to parent to stat a node. 
		if(F->validate_access(ctx.get(),access::NONE,access::X) == false) {
			return -EACCES;
		}
		timeHolder at,mt,ct;
		fileType t;
		F->loadStat(
				&t,
				&stbuf->st_mode,
				&stbuf->st_size,
				&stbuf->st_gid,
				&stbuf->st_uid,
				&at,&mt,&ct,&stbuf->st_ino
				);
		stbuf->st_nlink = (nlink_t)F->getNumLinks();
		if(str(path)!="/")FS->srvDEBUG("getattr_callback ",path," size:",stbuf->st_size," mode: ",stbuf->st_mode," t:",(int)t);
		
		copyTime(at,stbuf->st_atim);
		copyTime(mt,stbuf->st_mtim);
		copyTime(ct,stbuf->st_ctim);
#ifdef _WIN32
		copyTime(ct, stbuf->st_birthtim);
#endif
		//stbuf->st_ino = 0;
		stbuf->st_blocks = stbuf->st_size / 4096;
		stbuf->st_blksize = 4096;
		//if(str(path)!="/")CLOG("getattr_callback ",path," ",stbuf->st_size," mode: ",stbuf->st_mode);
		return 0;
	}

	if(errCode) {
//return -ENOSYS;
		return error_to_fuse(errCode);
	}

	return -ENOENT;
}

static int readdir_callback(const char *path, void *buf, fuse_fill_dir_t filler,my_off_t offset, struct fuse_file_info *fi) {
	LOG_OPERATION();
	(void) offset;
	(void) fi;


	
	//auto ctx = getContext();

	auto D = FS->get(path);
	/*if(!D->validate_access(ctx.get(),flagsToAccess(fi->flags))) {
		return -EACCES;
	}*/
	if(D->valid() && D->type()==fileType::DIR) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		auto meta = script::ComplexType::newComplex();
		if(!D->readDirectoryContent(meta)) {
			return -ENOENT;
		}
		for(auto & it: *meta) {
			if (it.second.getI(0,1) > 0) {
				filler(buf, it.first.c_str(), nullptr, 0);
			}
		}
	}
	//filler(buf, filename, NULL, 0);

	return 0;
}

static int open_callback(const char *path, struct fuse_file_info *fi) {
	LOG_OPERATION();
	auto F = FS->get(path);
	auto ctx = getContext();
	if(!F->validate_access(ctx.get(),flagsToAccess(fi->flags))) {
		return -EACCES;
	}
	if(!F->valid()) {
		return -ENOENT;
	} 
	F->open();
	fi->fh = FS->open(F);
	return 0;
}

static int opendir_callback(const char *path, struct fuse_file_info *fi) {
	LOG_OPERATION();
	auto F = FS->get(path);
	auto ctx = getContext();
	if(!F->validate_access(ctx.get(),flagsToAccess(fi->flags))) {
		return -EACCES;
	}
	if(!F->valid()) {
		return -ENOENT;
	} 
	return 0;
}

static int releasedir_callback(const char *path, struct fuse_file_info *fi) {
	return 0;
}



static int release_callback(const char *path, struct fuse_file_info *fi) {
	LOG_OPERATION();
	auto F = FS->get(path,nullptr,fi->fh);
	if(F->valid()) {
		F->close();
		FS->close(F,fi->fh);
		fi->fh = 0;
		return 0;
	} 
	return -ENOENT;	
}

static int read_callback(const char *path, char *buf, my_size_t size, my_off_t offset,struct fuse_file_info *fi) {
	LOG_OPERATION();
	auto D = FS->get(path,nullptr,fi->fh);
	if(D->valid() && D->type()==fileType::FILE) {
		_ASSERT(buf != nullptr);
		auto ret =  (int)D->read((unsigned char *)buf,size,offset);
		//CLOG("read returns: ",ret);
		return ret;
	}
	
	return -ENOENT;
}

static int write_callback(const char * path, const char * buf,my_size_t size, my_off_t offset, struct fuse_file_info *fi) {
	//LOG_OPERATION();
	//CLOG("write ", path);
	auto D = FS->get(path,nullptr,fi->fh);
	if(D->valid() && D->type()==fileType::FILE) {
		return (int)D->write((unsigned char *)buf,size,offset);
	}
		
	return -ENOENT;
}

static int mkdir_callback (const char * path, my_mode_t mode) {
	LOG_OPERATION();

	auto ctx = getContext();
	auto rr = FS->_mkdir(path,mode,ctx.get());
	if(rr) {
		return error_to_fuse(rr);
	}
	return 0;
}

/** Create a file node
 * 
 * This is called for creation of all non-directory, non-symlink
 * nodes.  If the filesystem defines a create() method, then for
 * regular files that will be called instead.
 */
int mknod_callback (const char * path, my_mode_t mode, my_dev_t dev) {
	LOG_OPERATION();
	auto ctx = getContext();
	auto rr = FS->mknod(path,mode,dev,ctx.get());
	if(rr) {
		return error_to_fuse(rr);
	}
	return 0;
}

/** Change the permission bits of a file */
int chmod_callback (const char * path, my_mode_t mode) {
	LOG_OPERATION();
	auto ctx = getContext();
	auto e = FS->get(path)->chmod(mode,ctx.get());
	if(e) {
		return error_to_fuse(e);
	}

	FS->srvDEBUG("Evict: ",FS->evictCache(path));
	return 0;
}

/** Change the owner and group of a file */
int chown_callback (const char * path, my_uid_t uid, my_gid_t gid) {
	LOG_OPERATION();
	
	auto ctx = getContext(true);
	auto e = FS->get(path)->chown(uid,gid,ctx.get());
	if(e) {
		return error_to_fuse(e);
	}
	FS->srvDEBUG("Evict: ",FS->evictCache(path));
	FS->srvDEBUG("CHOWN returns success!");
	return 0;
}

/** Remove a file */
int unlink_callback (const char *path) {
	LOG_OPERATION();
	auto ctx = getContext();
	auto err = FS->unlink(path,ctx.get());
	if(err) {
		return error_to_fuse(err);
	}
	return 0;
}

int utimens_callback (const char *path, const struct timespec tv[2]) {
	LOG_OPERATION();
	timeHolder tv2[2];
	for(int a=0;a<2;a++) {
		tv2[a].tv_sec = tv[a].tv_sec;
		tv2[a].tv_nsec = tv[a].tv_nsec;
	}

	if(!FS->get(path)->setTimes(tv2)) {
		return -ENOENT;
	}
	return 0;
}

static int truncate_callback (const char *path, my_off_t newSize) {
	LOG_OPERATION();
	auto F = FS->get(path);
	if(F->valid() ) {
		auto ctx = getContext();
		if(F->validate_access(ctx.get(),access::W)==false) {
			return -EACCES;
		}
		F->truncate(newSize);
		F->rest();
		return 0;
	}
	
	return -ENOENT;
}

static int ftruncate_callback (const char * path, my_off_t newSize, struct fuse_file_info *) {
	LOG_OPERATION();
	return truncate_callback(path,newSize);
}

static void _destroy_callback(void * in) {
	const char * path = "/";
	LOG_OPERATION();
	services::stop_all_services();
}
int symlink_callback (const char *path, const char *b) {
	LOG_OPERATION();
	auto ctx = getContext();
	auto err = FS->softlink(path,b,ctx.get());
	if(err) {
		return error_to_fuse(err);
	}
	return 0;
}
int link_callback (const char *path, const char *b) {
	LOG_OPERATION();
	auto ctx = getContext();
	auto err = FS->hardlink(path,b,ctx.get());
	if(err) {
		return error_to_fuse(err);
	}
	return 0;
}

int readlink_callback(const char * path,char * buffer, my_size_t bufferSize) {
	LOG_OPERATION();
	auto F = FS->get(path);
	if(F->valid() && F->readlnk(buffer,bufferSize) ) {
		return 0;
	}
	FS->srvERROR("READLNK FAIL");
	return -ENOENT;
}


int statfs_callback (const char *path , struct statvfs * buf) {
	LOG_OPERATION();
	auto fs = FS->getStatFS();
#ifdef _WIN32
	ULARGE_INTEGER lpFreeBytesAvailableToCaller, lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes;
	if (GetDiskFreeSpaceExA((FS->getPath()).c_str(),&lpFreeBytesAvailableToCaller,&lpTotalNumberOfBytes,&lpTotalNumberOfFreeBytes)) {

	}
	buf->f_bavail = lpFreeBytesAvailableToCaller.QuadPart / fs.first;
	buf->f_bfree = lpFreeBytesAvailableToCaller.QuadPart / fs.first;
	buf->f_blocks = lpFreeBytesAvailableToCaller.QuadPart / fs.first;
	buf->f_bsize = fs.first;

	return 0;
#else
	 auto ret =  statvfs(str(FS->getPath()).c_str(), buf);
	 buf->f_bsize = fs.first;
	 buf->f_frsize = fs.first;
	 return ret;
#endif
}

int rename_callback(const char * path, const char * dst){
	LOG_OPERATION();

	auto ctx = getContext();
	auto res = FS->renamemove(path,dst,ctx.get());
	if(res) {
		return error_to_fuse(res);
	}
	return 0;
}


void *init_callback(struct fuse_conn_info *conn) {
	LOG_OPERATION_NOPATH();
#ifndef _WIN32
	conn->want |= FUSE_CAP_BIG_WRITES;
#endif
	return nullptr;
}

enum {
	KEY_HELP,
	KEY_VERSION,
};

#define MYFS_OPT(t, p, v) { t, offsetof(struct myfs_config, p), v }

static struct fuse_opt myfs_opts[] = {
	MYFS_OPT("--src %s",           source, 0),
	MYFS_OPT("src=%s",             source, 0),
	
	MYFS_OPT("--loglevel %s",      loglevel, 0),
	MYFS_OPT("loglevel=%s",        loglevel, 0),
	MYFS_OPT("--keyfile %s",       keyfile, 0),
	MYFS_OPT("keyfile=%s",         keyfile, 0),
	MYFS_OPT("--key %s",           keybase64, 0),
	MYFS_OPT("key=%s",             keybase64, 0),
	MYFS_OPT("--pass %s",          password, 0),
	MYFS_OPT("pass=%s",            password, 0),
	MYFS_OPT("--create %s",        create, 0),
	MYFS_OPT("--migrateto %s",     migrate, 0),
	FUSE_OPT_KEY("-V",             KEY_VERSION),
	FUSE_OPT_KEY("--version",      KEY_VERSION),
	FUSE_OPT_KEY("-h",             KEY_HELP),
	FUSE_OPT_KEY("--help",         KEY_HELP),
	{nullptr,0,0}
};


static struct fuse_operations cloudcryptops;

static int myfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	switch(key) {
		case KEY_HELP:
			CLOG(
			"usage: ",outargs->argv[0]," --src /path/to/encrypted/source/ mountpoint [options]\n"
			"\n"
			"general options:\n"
			"    -o opt,[opt...]  mount options\n"
			"    -h   --help      print help\n"
			"    -V   --version   print version\n"
			"\n"
			"cloudCryptFS options:\n"
			"    --src [path] -OR- -osrc=[...]\n"
			"    --keyfile [filename] -OR- -okeyfile=[...]\n"
			"    --key [base64 key] -OR- -okey=[...]\n"
			"    --pass [password] -OR- -opass=[...]\n"
			"    --create yes\n"
			"    --migrateto [protocol version (or latest)]\n"
			"    --loglevel N  -OR- -ologlevel=N\n"
			
			);
			fuse_opt_add_arg(outargs, "-ho");
      fuse_main(outargs->argc, outargs->argv, &cloudcryptops, NULL);
			
			exit(EXIT_SUCCESS);
			break;
		case KEY_VERSION:
			CLOG("No versionnr yet...");
			
			exit(EXIT_SUCCESS);
	}
	return 1;
}

void * parseArgs(int argc, char * argv[], struct myfs_config * conf) {
	static struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	
	
	memset(conf, 0, sizeof(myfs_config));
	
	fuse_opt_parse(&args, conf, myfs_opts, myfs_opt_proc);
	
	return (void*)&args;
}

int flush_callback(const char *, struct fuse_file_info *) { FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int fsync_callback(const char *,int, struct fuse_file_info *) { FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int setxattr_callback(const char *, const char *, const char *, size_t, int){ FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int getxattr_callback(const char *, const char *, char *, size_t){ FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int listxattr_callback(const char *, char *, size_t){ FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int removexattr_callback(const char *, const char *){ FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int access_callback(const char *, int){ FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int lock_callback(const char *, struct fuse_file_info *, int cmd, struct flock *){ FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int flock_callback(const char *, struct fuse_file_info *, int op){ FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int fallocate_callback(const char *, int, off_t, off_t, struct fuse_file_info *){ FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int ioctl_callback(const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data){ FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }
int poll_callback(const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp){ FS->srvDEBUG(__FUNCTION__);	return -ENOSYS; }



int _realMain( void * parsedArgs) {
	auto * args = reinterpret_cast<fuse_args*>(parsedArgs);

	//Stubs:
	
	cloudcryptops.access = access_callback;
#ifndef _WIN32	
	cloudcryptops.poll = poll_callback;
#endif	
	cloudcryptops.lock = lock_callback;
#ifndef _WIN32	

	cloudcryptops.ioctl = ioctl_callback;
	cloudcryptops.flock = flock_callback;
	cloudcryptops.fallocate = fallocate_callback;
#endif	
	cloudcryptops.flush = flush_callback;
	cloudcryptops.fsync = fsync_callback;
	cloudcryptops.setxattr = setxattr_callback;
	cloudcryptops.getxattr = getxattr_callback;
	cloudcryptops.listxattr = listxattr_callback;
	cloudcryptops.removexattr = removexattr_callback;
	
	cloudcryptops.mknod = mknod_callback;
	cloudcryptops.chmod = chmod_callback;
	cloudcryptops.chown = chown_callback;
	cloudcryptops.unlink = unlink_callback;
	cloudcryptops.rmdir = unlink_callback;
	cloudcryptops.utimens = utimens_callback;
	cloudcryptops.truncate = truncate_callback;
	cloudcryptops.ftruncate = ftruncate_callback;
	
	cloudcryptops.getattr = getattr_callback;
	//cloudcryptops.create = create_callback;
	cloudcryptops.open = open_callback;
	cloudcryptops.release = release_callback;
	cloudcryptops.opendir = opendir_callback;
	cloudcryptops.releasedir = releasedir_callback;
	cloudcryptops.read = read_callback;
	cloudcryptops.write = write_callback;
	cloudcryptops.readdir = readdir_callback;
	
	cloudcryptops.mkdir = mkdir_callback; 
	cloudcryptops.destroy = _destroy_callback;
	cloudcryptops.symlink = symlink_callback; 
	cloudcryptops.link = link_callback; 
	cloudcryptops.readlink = readlink_callback; 

	cloudcryptops.rename = rename_callback; 
	
	cloudcryptops.statfs =statfs_callback;
	cloudcryptops.init = init_callback;

	auto result = fuse_main(args->argc, args->argv, &cloudcryptops, NULL);
	if(result == EXIT_FAILURE) {
		CLOG("Failed to lauch the filesystem, is FUSE installed?");
	}
	return result;
}


