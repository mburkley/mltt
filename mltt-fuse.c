/*
 * Copyright (c) 2004-2024 Mark Burkley.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 *  Mount a sector dump disk file using FUSE
 */

#define FUSE_USE_VERSION 31

#define _GNU_SOURCE

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/xattr.h>

#include "files.h"
#include "dskdata.h"

static int fill_dir_plus = 0;
static char *dskFile;

static DskInfo *dskInfo;

#define MAX_OPEN_FILES MAX_FILE_COUNT

typedef struct _FuseFileInfo
{
    DskFileInfo *file;
    struct _FuseFileInfo *next;
}
FuseFileInfo;

static FuseFileInfo fuseFileInfo[MAX_OPEN_FILES];
static FuseFileInfo *fuseFileHandleList;

static void *tidsk_init(struct fuse_conn_info *conn,
		      struct fuse_config *cfg)
{
    printf ("%s\n", __func__);

    /*  Build a list of free file handles */
    for (int i = 0; i < MAX_OPEN_FILES - 1; i++)
        fuseFileInfo[i].next = &fuseFileInfo[i+1];

    fuseFileHandleList = &fuseFileInfo[0];

    return NULL;
}

static int tidsk_getattr(const char *path, struct stat *stbuf,
		       struct fuse_file_info *fi)
{
    if (*path == '/')
        path++;

    printf ("%s %s\n", __func__, path);

    (void) fi;

    DskFileInfo *file;
    if (!strcmp (path, ""))
    {
        stbuf->st_mode = S_IFDIR;

        // TODO set protected to be disk protected
        stbuf->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
        stbuf->st_size = 256; // random number
        stbuf->st_blocks = 256; // random number
        stbuf->st_ino = 99; // random number
    }
    else
    {
        if ((file = dskFileAccess (dskInfo, path, 0)) == NULL)
            return -ENOENT;

        stbuf->st_mode = S_IFREG;

        if (!dskFileProtected (dskInfo, file))
            stbuf->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;

        stbuf->st_size = dskFileLength (dskInfo, file);
        stbuf->st_blocks = dskFileSecCount (dskInfo, file);
        stbuf->st_ino = dskFileInode (dskInfo, file);
    }

    stbuf->st_mode |= S_IRUSR | S_IRGRP | S_IROTH;
    
    stbuf->st_blksize = BYTES_PER_SECTOR;
    stbuf->st_ctime = 0;
    stbuf->st_atime = 0;
    stbuf->st_mtime = 0;
    stbuf->st_dev = 1; // Assuming mount points can have a common value here
    stbuf->st_nlink = 1;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_uid = 0;
    stbuf->st_gid = 0;
    stbuf->st_rdev = 0;

    return 0;
}

static int tidsk_access(const char *path, int mask)
{
    printf ("%s %s\n", __func__, path);

    if (*path == '/')
        path++;

    if (!strcmp (path, ""))
        return 0;

    if (dskFileAccess (dskInfo, path, mask) == NULL)
        return -EACCES;

    return 0;
}

static int tidsk_readlink(const char *path, char *buf, size_t size)
{
    printf ("%s %s unsupported\n", __func__, path);
    return -EINVAL;
}

static int tidsk_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi,
		       enum fuse_readdir_flags flags)
{
    struct stat st;
    memset(&st, 0, sizeof(st));

    printf ("%s %s\n", __func__, path);

    if (strcmp (path, "/"))
        return -ENOTDIR;

    for (DskFileInfo *file = dskFileFirst (dskInfo);
         file != NULL;
         file = dskFileNext (dskInfo, file))
    {
        st.st_ino = dskFileInode (dskInfo, file);
        st.st_mode = DT_REG << 12;

        if (filler(buf, dskFileName (dskInfo, file), &st, 0, fill_dir_plus))
            break;
    }

    return 0;
}

static int tidsk_mknod(const char *path, mode_t mode, dev_t rdev)
{
    printf ("%s\n", __func__);
    return -EPERM;
}

static int tidsk_mkdir(const char *path, mode_t mode)
{
    printf ("%s\n", __func__);
    return -ENOTDIR;
}

static int tidsk_unlink(const char *path)
{
    if (*path == '/')
        path++;

    printf ("%s %s\n", __func__, path);

    if (dskUnlinkFile (dskInfo, path) != 0)
        return -EACCES;

    return 0;
}

/* No directories in TI */
static int tidsk_rmdir(const char *path)
{
    printf ("%s not supported\n", __func__);
    return -ENOTDIR;
}

static int tidsk_symlink(const char *from, const char *to)
{
    printf ("%s not supported\n", __func__);
    return -EPERM;
}

static int tidsk_rename(const char *from, const char *to, unsigned int flags)
{
    if (*from == '/')
        from++;

    printf ("%s %s %s TODO\n", __func__, from, to);

    return 0;
}

static int tidsk_link(const char *from, const char *to)
{
    printf ("%s %s %s not supported\n", __func__, from, to);
    return -EINVAL;
}

static int tidsk_chmod(const char *path, mode_t mode,
		     struct fuse_file_info *fi)
{
    printf ("%s %s TODO\n", __func__, path);
    return 0;
}

static int tidsk_chown(const char *path, uid_t uid, gid_t gid,
		     struct fuse_file_info *fi)
{
    printf ("%s\n", __func__);
    return -EPERM;
}

static int tidsk_truncate(const char *path, off_t size,
			struct fuse_file_info *fi)
{
    if (*path == '/')
        path++;

    printf ("%s %s TODO\n", __func__, path);

    return 0;
}

static int tidsk_create(const char *path, mode_t mode,
		      struct fuse_file_info *fi)
{
    if (*path == '/')
        path++;

    printf ("%s %s\n", __func__, path);

    /*  Error if file exists */
    if (dskFileAccess (dskInfo, path, fi->flags) != NULL)
        return -EACCES;

    FuseFileInfo *f = fuseFileHandleList;
    if (f == NULL)
        return -EBADF;

    Tifiles header;
    filesInitTifiles (&header, path, 0, BYTES_PER_SECTOR, 0, false, false);

    DskFileInfo *file;
    if ((file = dskCreateFile (dskInfo, path, &header)) == NULL)
        return -EACCES;

    fuseFileHandleList = f->next;
    fi->fh = f - fuseFileInfo;
    printf ("file info array index=%ld\n", fi->fh);
    f->file = file;
    return 0;
}

static int tidsk_open(const char *path, struct fuse_file_info *fi)
{
    if (*path == '/')
        path++;

    printf ("%s %s\n", __func__, path);

    FuseFileInfo *f = fuseFileHandleList;
    if (f == NULL)
        return -EBADF;

    DskFileInfo *file;
    if ((file = dskFileOpen (dskInfo, path, fi->flags)) == NULL)
        return -EACCES;

    /*  We need to store the file info element as an integer so calculate the
     *  offset into the file info array and use that as the index */
    fi->fh = f - fuseFileInfo;
    printf ("file info array index=%ld\n", fi->fh);
    f->file = file;
    fuseFileHandleList = f->next;
    return 0;
}

static int tidsk_read (const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi)
{
    if (*path == '/')
        path++;

    printf ("%s\n", __func__);

    DskFileInfo *file;
    if (fi == NULL)
    {
        if ((file = dskFileAccess (dskInfo, path, O_RDONLY)) == NULL)
            return -EACCES;
    }
    else
        file = fuseFileInfo[fi->fh].file;

    return dskReadFile (dskInfo, file, (uint8_t*) buf, offset, size);
}

static int tidsk_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
    if (*path == '/')
        path++;

    printf ("%s\n", __func__);

    DskFileInfo *file;
    if(fi == NULL)
    {
        if ((file = dskFileAccess (dskInfo, path, O_WRONLY)) == NULL)
            return -EACCES;
    }
    else
        file = fuseFileInfo[fi->fh].file;

    return dskWriteFile (dskInfo, file, (uint8_t*) buf, offset, size);
}

static int tidsk_statfs(const char *path, struct statvfs *stbuf)
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
}

static int tidsk_release(const char *path, struct fuse_file_info *fi)
{
    if (*path == '/')
        path++;

    FuseFileInfo *f = &fuseFileInfo[fi->fh];

    // if (f == NULL)
    //     return -EBADF;

    dskFileClose (dskInfo, f->file);
    f->next = fuseFileHandleList;
    f->file = NULL;
    fuseFileHandleList = f;

    return 0;
}

static int tidsk_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
    printf ("%s\n", __func__);
    return 0;
}

/*  Extended attributes are defined to provide meta data about a file when not
 *  using the TIFILES header.  Extended attributes supported are:
 *
 *  flags: a numeric value representing the flags bitmask
 *  reclen: the length of a record for VAR files
 */

#define XATTR_FLAGS "user.tifiles.flags"
#define XATTR_RECLEN "user.tifiles.reclen"

static int tidsk_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
    printf ("%s %s attr='%s' size=%ld\n", __func__, path, name, size);

    if (*path == '/')
        path++;

    /*  We report no data for the mount point itself */
    if (!*path)
        return -ENODATA;

    DskFileInfo *file;
    if ((file = dskFileAccess (dskInfo, path, O_WRONLY)) == NULL)
        return -EACCES;

    char data[11];
    memcpy (data, value, size < 10 ? size : 10);
    data[10] = 0;

    if (!strcmp (name, XATTR_FLAGS))
    {
        dskFileFlagsSet (dskInfo, file, atoi (data));
        return 0;
    }

    if (!strcmp (name, XATTR_RECLEN))
    {
        dskFileRecLenSet (dskInfo, file, atoi (data));
        return 0;
    }

    return -ENODATA;
}

static int tidsk_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
    printf ("%s %s attr='%s' size=%ld\n", __func__, path, name, size);

    if (*path == '/')
        path++;

    /*  We report no data for the mount point itself */
    if (!*path)
        return -ENODATA;

    DskFileInfo *file;
    if ((file = dskFileAccess (dskInfo, path, O_WRONLY)) == NULL)
        return -EACCES;

    /*  Note we must check if this is an attribute we have BEFORE we check the
     *  length.  Otherwise we will mislead the OS into thinking we have data
     *  when we actually don't */
    if (!strcmp (name, XATTR_FLAGS))
    {
        /*  Size of zero indicates the OS is asking us how much space it should
         *  allocated.  Return an arbitrary number for now that is big enough */
        if (size == 0)
            return 100;

        int flags = dskFileFlags (dskInfo, file);
        sprintf (value, "%d", flags);
        return strlen (value);
    }
    else if (!strcmp (name, XATTR_RECLEN))
    {
        if (size == 0)
            return 100;

        int len = dskFileRecLen (dskInfo, file);
        sprintf (value, "%d", len);
        return strlen (value);
    }

    return -ENODATA;
}

static int tidsk_listxattr(const char *path, char *list, size_t size)
{
    if (*path == '/')
        path++;

    printf ("%s %s listsize=%ld\n", __func__, path, size);

    int len = strlen (XATTR_FLAGS) + strlen (XATTR_RECLEN) + 2;

    if (size == 0)
        return len;

    if (size >= len)
    {
        strcpy (list, XATTR_FLAGS);
        list += strlen (XATTR_FLAGS) + 1; // include NUL termination
        strcpy (list, XATTR_RECLEN);
        return len;
    }
    return 0;
}

static int tidsk_removexattr(const char *path, const char *name)
{
    if (*path == '/')
        path++;

    printf ("%s %s attribute removal is not supported\n", __func__, path);
    return -ENODATA;
}

static off_t tidsk_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi)
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

    if (!f->file)
    {
        errno = EBADF;
        return -1;
    }

    /*  We don't support sparse files so any seek for data is the same as a seek
     *  set and a seek hole is the same as a seek end */
    if (whence == SEEK_DATA) 
        whence = SEEK_SET;

    if (whence == SEEK_HOLE) 
        whence = SEEK_END;

    return dskFileSeek (dskInfo, f->file, off, whence);
}

static const struct fuse_operations tidsk_oper = {
	.init           = tidsk_init,
	.getattr	= tidsk_getattr,
	.access		= tidsk_access,
	.readlink	= tidsk_readlink,
	.readdir	= tidsk_readdir,
	.mknod		= tidsk_mknod,
	.mkdir		= tidsk_mkdir,
	.symlink	= tidsk_symlink,
	.unlink		= tidsk_unlink,
	.rmdir		= tidsk_rmdir,
	.rename		= tidsk_rename,
	.link		= tidsk_link,
	.chmod		= tidsk_chmod,
	.chown		= tidsk_chown,
	.truncate	= tidsk_truncate,
	.open		= tidsk_open,
	.create 	= tidsk_create,
	.read		= tidsk_read,
	.write		= tidsk_write,
	.statfs		= tidsk_statfs,
	.release	= tidsk_release,
	.fsync		= tidsk_fsync,
	.setxattr	= tidsk_setxattr,
	.getxattr	= tidsk_getxattr,
	.listxattr	= tidsk_listxattr,
	.removexattr	= tidsk_removexattr,
	.lseek		= tidsk_lseek,
};

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf ("\nFUSE driver for TI99 sector dump files\n\n"
                "usage: %s <dsk-file> [fuse-options] <mount-point>\n", argv[0]);
        printf ("\twhere <dsk-file> is a sector dump disk file.\n\n");
        return 1;
    }

    dskFile = strdup (argv[optind]);

    if ((dskInfo = dskOpenVolume (dskFile)) == NULL)
        exit (1);

    return fuse_main(argc-optind, &argv[optind], &tidsk_oper, NULL);
}

