/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

/** @file
 *
 * This file system mirrors the existing file system hierarchy of the
 * system, starting at the root file system. This is implemented by
 * just "passing through" all requests to the corresponding user-space
 * libc functions. Its performance is terrible.
 *
 * Compile with
 *
 *     gcc -Wall passthrough.c `pkg-config fuse3 --cflags --libs` -o passthrough
 *
 * ## Source code ##
 * \include passthrough.c
 */


#define FUSE_USE_VERSION 31

#define _GNU_SOURCE

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "passthrough_helpers.h"
#include "files.h"
#include "dskdata.h"

static int fill_dir_plus = 0;
static char *dskFile;

static DskInfo *dskInfo;

#define MAX_OPEN_FILES MAX_FILE_COUNT
typedef struct _FuseFileInfo
{
    bool open;
    int index;
    off_t pos;
    struct _FuseFileInfo *next;
}
FuseFileInfo;

static FuseFileInfo fuseFileInfo[MAX_OPEN_FILES];
static FuseFileInfo *fuseFileHandleList;

static void *xmp_init(struct fuse_conn_info *conn,
		      struct fuse_config *cfg)
{
    printf ("%s\n", __func__);

    /*  Build a list of free file handles */
    for (int i = 0; i < MAX_OPEN_FILES - 1; i++)
        fuseFileInfo[i].next = &fuseFileInfo[i+1];

    fuseFileHandleList = &fuseFileInfo[0];
        #if 0
	(void) conn;
	cfg->use_ino = 1;

	/* Pick up changes from lower filesystem right away. This is
	   also necessary for better hardlink support. When the kernel
	   calls the unlink() handler, it does not know the inode of
	   the to-be-removed entry and can therefore not invalidate
	   the cache of the associated inode - resulting in an
	   incorrect st_nlink value being reported for any remaining
	   hardlinks to this inode. */
	cfg->entry_timeout = 0;
	cfg->attr_timeout = 0;
	cfg->negative_timeout = 0;
        #endif

    return NULL;
}

static int xmp_getattr(const char *path, struct stat *stbuf,
		       struct fuse_file_info *fi)
{
    #if 1
    int index = 0;

    if (*path == '/')
        path++;

    printf ("%s %s\n", __func__, path);

    (void) fi;

    if (!strcmp (path, ""))
        stbuf->st_mode = S_IFDIR;
    else
    {
        if ((index = dskCheckFileAccess (dskInfo, path, 0)) < 0)
            return -ENOENT;

        stbuf->st_mode = S_IFREG;
    }

    stbuf->st_mode |= S_IRUSR | S_IRGRP | S_IROTH;
    
    if (!dskFileProtected (dskInfo, index))
        stbuf->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;

    stbuf->st_size = dskFileLength (dskInfo, index);
    stbuf->st_blocks = 1; // TODO
    stbuf->st_blksize = 512;
    stbuf->st_ino = index;
    stbuf->st_ctime = 0;
    stbuf->st_atime = 0;
    stbuf->st_mtime = 0;
    stbuf->st_dev = 45; // ??
    stbuf->st_nlink = 1;
    // stbuf->st_uid = 1000; // TODO get these from running process
    // stbuf->st_gid = 1000;
    stbuf->st_uid = 0;
    stbuf->st_gid = 0;
    stbuf->st_rdev = 0;

    return 0;

        #else
	(void) fi;
	int res;

	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

        return 0;
        #endif
}

static int xmp_access(const char *path, int mask)
{
    printf ("%s %s\n", __func__, path);

    if (*path == '/')
        path++;

    if (!strcmp (path, ""))
        return 0;

    if (dskCheckFileAccess (dskInfo, path, mask) < 0)
        return -EACCES;

    return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
    printf ("%s %s TODO\n", __func__, path);
    return -EINVAL;
}


static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi,
		       enum fuse_readdir_flags flags)
{
    printf ("%s %s\n", __func__, path);

    for (int i = 0; i < dskFileCount (dskInfo); i++)
    {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = i + 42;
        // st.st_mode = S_IFREG;
        st.st_mode = DT_REG << 12;
        #if 0
        st.st_size = 1;
        st.st_blocks = 1;
        st.st_blksize = 512;
        st.st_ctime = 0;
        st.st_atime = 0;
        st.st_mtime = 0;
        st.st_dev = 45; // ??
        st.st_nlink = 1;
        st.st_uid = 1000; // TODO get these from running process
        st.st_gid = 1000;
        st.st_rdev = 0;
        #endif

        if (filler(buf, dskFileName (dskInfo, i), &st, 0, fill_dir_plus))
            break;
    }

        #if 0
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;
	(void) flags;

	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0, fill_dir_plus))
			break;
	}

	closedir(dp);
        #endif
    return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
    printf ("%s\n", __func__);
    return -EPERM;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
    printf ("%s\n", __func__);
    return -ENOTDIR;
}

static int xmp_unlink(const char *path)
{
    if (*path == '/')
        path++;

    printf ("%s %s TODO\n", __func__, path);

        #if 0
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

        #endif
    return 0;
}

/* No directories in TI */
static int xmp_rmdir(const char *path)
{
    printf ("%s\n", __func__);
    return -ENOTDIR;
}

static int xmp_symlink(const char *from, const char *to)
{
    printf ("%s\n", __func__);
    return -EPERM;
}

static int xmp_rename(const char *from, const char *to, unsigned int flags)
{
    if (*from == '/')
        from++;

    printf ("%s %s %s TODO\n", __func__, from, to);

        #if 0
	int res;

	if (flags)
		return -EINVAL;

	res = rename(from, to);
	if (res == -1)
		return -errno;

        #endif
    return 0;
}

static int xmp_link(const char *from, const char *to)
{
    printf ("%s %s %s TODO\n", __func__, from, to);

        #if 0
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

        #endif
    return 0;
}

static int xmp_chmod(const char *path, mode_t mode,
		     struct fuse_file_info *fi)
{
    printf ("%s %s TODO\n", __func__, path);

        #if 0
	(void) fi;
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

        #endif
    return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid,
		     struct fuse_file_info *fi)
{
    printf ("%s\n", __func__);
    return -EPERM;
}

static int xmp_truncate(const char *path, off_t size,
			struct fuse_file_info *fi)
{
    if (*path == '/')
        path++;

    printf ("%s %s TODO\n", __func__, path);

        #if 0
	int res;

	if (fi != NULL)
		res = ftruncate(fi->fh, size);
	else
		res = truncate(path, size);
	if (res == -1)
		return -errno;

        #endif
    return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2],
		       struct fuse_file_info *fi)
{
    printf ("%s\n", __func__);

        #if 0
	(void) fi;
	int res;

	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

        #endif
    return 0;
}
#endif

static int xmp_create(const char *path, mode_t mode,
		      struct fuse_file_info *fi)
{
    int index;

    if (*path == '/')
        path++;

    printf ("%s %s\n", __func__, path);

    /*  Error if file exists */
    if (dskCheckFileAccess (dskInfo, path, fi->flags) >= 0)
        return -EACCES;

    FuseFileInfo *f = fuseFileHandleList;
    if (f == NULL)
        return -EBADF;

    fuseFileHandleList = f->next;

    if ((index = dskCreateFile (dskInfo, path, mode)) < 0)
        return -EACCES;

    fi->fh = f - fuseFileInfo;
    printf ("file info array index=%ld\n", fi->fh);
    f->pos = 0;
    f->index = index;
    f->open = true;
    return 0;

        #if 0
	int res;

	res = open(path, fi->flags, mode);
	if (res == -1)
		return -errno;

	fi->fh = res;
        return 0;
        #endif
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
    int index;

    if (*path == '/')
        path++;

    printf ("%s %s\n", __func__, path);

    FuseFileInfo *f = fuseFileHandleList;
    if (f == NULL)
        return -EBADF;

    fuseFileHandleList = f->next;

    if ((index = dskCheckFileAccess (dskInfo, path, fi->flags)) < 0)
        return -EACCES;

    /*  We need to store the file info element as an integer so calculate the
     *  offset into the file info array and use that as the index */
    fi->fh = f - fuseFileInfo;
    printf ("file info array index=%ld\n", fi->fh);
    f->pos = 0;
    f->index = index;
    f->open = true;
    return 0;

        #if 0
	int res;

	res = open(path, fi->flags);
	if (res == -1)
		return -errno;

	fi->fh = res;
        return 0;
        #endif
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
    int index;

    if (*path == '/')
        path++;

    printf ("%s\n", __func__);

    if(fi == NULL)
    {
        if ((index = dskCheckFileAccess (dskInfo, path, O_RDONLY)) < 0)
            return -EACCES;
    }
    else
        index = fuseFileInfo[fi->fh].index;

    return dskReadFile (dskInfo, index, (uint8_t*) buf, offset, size);
}

static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
    int index;

    if (*path == '/')
        path++;

    printf ("%s\n", __func__);

    if(fi == NULL)
    {
        if ((index = dskCheckFileAccess (dskInfo, path, O_WRONLY)) < 0)
            return -EACCES;
    }
    else
        index = fuseFileInfo[fi->fh].index;

    return dskWriteFile (dskInfo, index, (uint8_t*) buf, offset, size);

        #if 0
	int fd;
	int res;

	(void) fi;
	if(fi == NULL)
		fd = open(path, O_WRONLY);
	else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
        #endif
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
    printf ("%s %s\n", __func__, path);

    stbuf->f_bsize = DSK_BYTES_PER_SECTOR;
    stbuf->f_frsize = DSK_BYTES_PER_SECTOR;
    stbuf->f_blocks = dskSectorCount (dskInfo);
    stbuf->f_bfree = dskSectorsFree (dskInfo);
    stbuf->f_bavail = dskSectorsFree (dskInfo);
    stbuf->f_files = MAX_FILE_COUNT;
    stbuf->f_ffree = MAX_FILE_COUNT - dskFileCount (dskInfo);
    stbuf->f_favail = MAX_FILE_COUNT - dskFileCount (dskInfo);
    stbuf->f_fsid = 42;
    stbuf->f_flag = 0;
    stbuf->f_namemax = 10;

    return 0;


        #if 0
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
        #endif
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
    if (*path == '/')
        path++;

    FuseFileInfo *f = &fuseFileInfo[fi->fh];

    // if (f == NULL)
    //     return -EBADF;

    f->next = fuseFileHandleList;
    fuseFileHandleList = f;
    f->open = false;

    return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
    printf ("%s\n", __func__);
    return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
    printf ("%s\n", __func__);
    return -EOPNOTSUPP;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
    if (*path == '/')
        path++;

    printf ("%s %s TODO\n", __func__, path);
    return 0;

        #if 0
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
        #endif
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
    if (*path == '/')
        path++;

    printf ("%s %s TODO\n", __func__, path);
    return 0;

        #if 0
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
        #endif
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
    if (*path == '/')
        path++;

    printf ("%s %s TODO\n", __func__, path);

        #if 0
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
        #endif
	return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
    if (*path == '/')
        path++;

    printf ("%s %s TODO\n", __func__, path);
    return 0;

        #if 0
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
        #endif
}
#endif /* HAVE_SETXATTR */

#ifdef HAVE_COPY_FILE_RANGE
static ssize_t xmp_copy_file_range(const char *path_in,
				   struct fuse_file_info *fi_in,
				   off_t offset_in, const char *path_out,
				   struct fuse_file_info *fi_out,
				   off_t offset_out, size_t len, int flags)
{
    if (*path == '/')
        path++;

    printf ("%s %s TODO\n", __func__, path);
    return 0;

        #if 0
	int fd_in, fd_out;
	ssize_t res;

	if(fi_in == NULL)
		fd_in = open(path_in, O_RDONLY);
	else
		fd_in = fi_in->fh;

	if (fd_in == -1)
		return -errno;

	if(fi_out == NULL)
		fd_out = open(path_out, O_WRONLY);
	else
		fd_out = fi_out->fh;

	if (fd_out == -1) {
		close(fd_in);
		return -errno;
	}

	res = copy_file_range(fd_in, &offset_in, fd_out, &offset_out, len,
			      flags);
	if (res == -1)
		res = -errno;

	if (fi_out == NULL)
		close(fd_out);
	if (fi_in == NULL)
		close(fd_in);

	return res;
        #endif
}
#endif

static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi)
{
    if (*path == '/')
        path++;

    printf ("%s %s\n", __func__, path);

    if (fi == NULL)
    {
        errno = EBADF;
        return -1;
    }

    FuseFileInfo *f = &fuseFileInfo[fi->fh];

    if (!f->open)
    {
        errno = EBADF;
        return -1;
    }

    /*  We don't support sparse files so any seek for data is the same as a seek
     *  set and a seek hole is the same as a seek end */
    switch (whence)
    {
    case SEEK_DATA:
    case SEEK_SET: f->pos = off; break;
    case SEEK_CUR: f->pos += off; break;
    case SEEK_HOLE:
    case SEEK_END: f->pos = dskFileLength (dskInfo, f->index) + off; break;
    default: printf ("# whence=%d ?\n", whence); break;
    }

    return f->pos;
}

static const struct fuse_operations xmp_oper = {
	.init           = xmp_init,
	.getattr	= xmp_getattr,
	.access		= xmp_access,
	.readlink	= xmp_readlink,
	.readdir	= xmp_readdir,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
#ifdef HAVE_UTIMENSAT
	.utimens	= xmp_utimens,
#endif
	.open		= xmp_open,
	.create 	= xmp_create,
	.read		= xmp_read,
	.write		= xmp_write,
	.statfs		= xmp_statfs,
	.release	= xmp_release,
	.fsync		= xmp_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= xmp_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
#ifdef HAVE_COPY_FILE_RANGE
	.copy_file_range = xmp_copy_file_range,
#endif
	.lseek		= xmp_lseek,
};

int main(int argc, char *argv[])
{
        #if 0
	enum { MAX_ARGS = 10 };
	int i,new_argc;
	char *new_argv[MAX_ARGS];

	umask(0);
			/* Process the "--plus" option apart */
	for (i=0, new_argc=0; (i<argc) && (new_argc<MAX_ARGS); i++) {
		if (!strcmp(argv[i], "--plus")) {
			fill_dir_plus = FUSE_FILL_DIR_PLUS;
		} else {
			new_argv[new_argc++] = argv[i];
		}
	}
	return fuse_main(new_argc, new_argv, &xmp_oper, NULL);
        #endif
    if (argc<2)
    {
        fprintf (stderr, "Please specify DSK file\n");
        exit(1);
    }
    dskFile = strdup (argv[1]);
    #if 0
    if ((diskFp = fopen (dskFile, "r")) == NULL)
    {
        printf ("Can't open %s\n", dskFile);
        return 0;
    }

    uint8_t sector[DSK_BYTES_PER_SECTOR];
    fread (&sector, sizeof (sector), 1, diskFp);
    // diskDecodeVolumeHeader (sector);
    fread (&sector, sizeof (sector), 1, diskFp);
    fileCount = diskAnalyseDirectory (diskFp, 1, fileHeader);
    fclose (diskFp);
    #endif
    dskInfo = dskOpenVolume (dskFile);

    /*  Until we understand what fuse_main does with args we will use fixed
     *  args.  In time we can use getopt() */
    return fuse_main(argc-1, &argv[1], &xmp_oper, NULL);
}
