#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

/*
 * Add your file-related functions here ...
 */


/*
 * Return errno on failure. On success set retval to amount written.
*/
int sys_open(userptr_t fbuffer, int flags, mode_t mode, int32_t *retval) {
    int err = 0;
    int fd = -1;                            // The return value of sys_open

    struct fd_desc *file_info = NULL;
    char fname[NAME_MAX + 1];
    size_t got;

    err = copyinstr(fbuffer, fname, NAME_MAX + 1, &got);

    if (err || fbuffer == NULL) {
        return EFAULT;                     // Check if buffer specified was ok
    } 

    // Need to start from availble file descriptor
    for (int i = 3; i < OPEN_MAX; i++) {

        if (curproc->p_table[i] == NULL) {

            // Allocate file info in the file table
            file_info = (struct fd_desc *)kmalloc(sizeof(struct fd_desc));
            if (file_info == NULL) {
                return ENFILE;             // No heap memory left, so file table is full
            }
            
            struct vnode *vnode;
            err = vfs_open(fname, flags, mode, &vnode);
            if (err) {
                kfree(file_info);
                return err;                  // Return the error code from vfs
            }

            off_t fp = 0;             

            // Check if fp needs to change
            if (flags & O_APPEND) {
                
                struct stat *statbuf = (struct stat *)kmalloc(sizeof(struct stat));
                err = VOP_STAT(vnode, statbuf);
                if (err) {
                    kfree(file_info);
                    kfree(statbuf);
                    return err;              // Return error code from stat
                }
                fp = statbuf->st_size;      // Fp set to end of file
            }

            file_info->vnode = vnode;
            file_info->fp = fp;
            file_info->flags = flags;
            file_info->refnum = 1;          // Unique fd, so start at 1

            fd = i;                         // Set fd return value
            break;
        }
    }

    // No error - set retval = fd and err = 0 when return
    curproc->p_table[fd] = file_info;
    *retval = fd;               
    return 0;
}

/*
 * Return errno on failure. On success set retval to amount written.
*/
int sys_write(int fd, void *buf, size_t buflen, int32_t *retval) {
    int err = 0;

    // Invalid buffer
    if (buf == NULL) {                    
        return EFAULT;             
    }

    // No such file descriptors
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    if (curproc->p_table[fd] == NULL) {
        return EBADF;
    }
    
    // Read only
    if ((curproc->p_table[fd]->flags & O_ACCMODE) == O_RDONLY) {
        return EBADF;                      
    }

    // Init uio for memory checking
    struct iovec io;
    struct uio u;

    uio_uinit(&io, &u, buf, buflen, curproc->p_table[fd]->fp, UIO_WRITE);
    err = VOP_WRITE(curproc->p_table[fd]->vnode, &u);

    // Return err from write
    if (err) {
        return err;
    }

    // Amount written
    size_t amt_writ = buflen - u.uio_resid;

    curproc->p_table[fd]->fp += amt_writ;        // Set to knew offset after write
    *retval = amt_writ;
    return 0;                                    // Return how much has been written
}


/*
 * Return errno on failure. On success set retval to new seek position.
*/
int sys_lseek(int fd, off_t pos, int whence, int32_t *retval) {
    int err;
    int temp_pos;
    struct stat *statbuf;

    // No such file descriptors
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    if (curproc->p_table[fd] == NULL) {
        return EBADF;
    }

    // Not seekable
    if (!VOP_ISSEEKABLE(curproc->p_table[fd]->vnode)) {
        return ESPIPE;
    }    
    
    switch(whence) {
        case SEEK_SET:
            if (pos < 0) return EINVAL;
            curproc->p_table[fd]->fp = pos;
            break;

        case SEEK_CUR:
            temp_pos = curproc->p_table[fd]->fp + pos;
            if (temp_pos < 0) return EINVAL;
            curproc->p_table[fd]->fp = temp_pos;
            break;
        
        case SEEK_END:
            statbuf = (struct stat *)kmalloc(sizeof(struct stat));
            err = VOP_STAT(curproc->p_table[fd]->vnode, statbuf);
            if (err) {
                kfree(statbuf);
                return err;  
            }
            temp_pos = pos + statbuf->st_size;
            if (temp_pos < 0) {
                kfree(statbuf);
                return EINVAL;
            }

            curproc->p_table[fd]->fp = temp_pos;
            kfree(statbuf);
            break;
        
        default:
            return EINVAL;      // Not valid whence
            break;
    }

    *retval = curproc->p_table[fd]->fp;
    return 0;
}

int sys_close(int fd, int32_t *retval) 
{
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }

    if (curproc->p_table[fd] == NULL) {
        return EBADF;
    }
    
    // if there's more than 1 fd that has this file open
    // simply decrement the refnum
    if (curproc->p_table[fd]->refnum > 1) {
        curproc->p_table[fd]->refnum--;
        curproc->p_table[fd] = NULL;
        *retval = fd;
        return 0;
    }
    
    // if there's only 1 fd that has this file open
    // simply free the file from memory
    kfree(curproc->p_table[fd]);
    curproc->p_table[fd] = NULL;
    *retval = fd;
    return 0;
}

/*
 * Initialise STDOUT and STDERR
*/
ssize_t sys_read(int fd, void *buf, size_t buflen, int32_t *retval) {

    if (buf == NULL) {                    
        return EFAULT;             
    }

    // check if fd is within the limit
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }

    // check if fd exists
    if (curproc->p_table[fd] == NULL) {
        return EBADF;
    }

    // check if its not in read-only or read-write mode
    if ((curproc->p_table[fd]->flags & O_ACCMODE) == O_WRONLY)
    {
        return EBADF;
    }

    // TODO: verify if buffer is ok

    // might need to provide kernel supplied buffer
    struct iovec read_iov;
    struct uio read_uio;
    off_t read_oft = curproc->p_table[fd]->fp;

    uio_uinit(&read_iov, &read_uio, buf, buflen, read_oft, UIO_READ);

    
    int error;
    error = VOP_READ(curproc->p_table[fd]->vnode, &read_uio);
    if (error) {
        return EFAULT;
    }
    
    // Amount read
    *retval = buflen - read_uio.uio_resid;

    return 0;
}


int sys_dup2(int oldfd, int newfd, int32_t *retval) 
{
    if (curproc->p_table[oldfd] == NULL) {
        return EBADF;
    }

    if (oldfd >= OPEN_MAX ||
        oldfd < 0 ||
        newfd >= OPEN_MAX ||
        newfd < 0) 
    {
        return EBADF;
    }

    if (oldfd == newfd) {
        *retval = newfd;
        return 0;
    }

    // if there's a file open at newfd, close it
    if (curproc->p_table[newfd] != NULL) {
        if (curproc->p_table[newfd]->refnum > 1) {
            curproc->p_table[newfd]->refnum--;
            curproc->p_table[newfd] = NULL;
        } else {
            kfree(curproc->p_table[newfd]);
            curproc->p_table[newfd] = NULL;
        }
    }

    curproc->p_table[newfd] = curproc->p_table[oldfd];
    curproc->p_table[newfd]->refnum++;

    *retval = newfd;
    return 0;
}

void sys_init(void) {
    /* Set STDIN to NULL
        Later can be opened by user
    */

    /* Set STDOUT 1 by opening file and allocating memory in process table
        vnode - Created via vfs_open
        fp - Set to start of file
        flags - Set to write only
        refnum - Only single process
    */

    // Connected to console file
    char outname[] = "con:";
    struct fd_desc *desc_out;

    desc_out = (struct fd_desc *)kmalloc(sizeof(struct fd_desc));
    if (desc_out == NULL) {
        panic("Out of memory for stdout file descriptor\n");
    }

    struct vnode *vout;

    int out_err = vfs_open(outname, O_WRONLY, 0777, &vout);
    if (out_err) {
        panic("Error opening stdout\n");
    }

    desc_out->vnode = vout;         // 
    desc_out->fp = 0;               // Offset = 0; from the start
    desc_out->flags = O_WRONLY;     // WRITE to stdout
    desc_out->refnum = 1;           // 1 process pointing to this fd
    
    // Assign index 1
    curproc->p_table[1] = desc_out;

    // Sanity checking, should never happen
    KASSERT(desc_out != NULL);
    KASSERT(curproc->p_table[1] != NULL);

    /* Set STDERR 2 by opening file and allocating memory in process table
        vnode - Created via vfs_open
        fp - Set to start of file
        flags - Set to write only
        refnum - Only single process
    */
    struct fd_desc *desc_err;

    // Must be connected to console file
    char errname[] = "con:";

    desc_err = (struct fd_desc *)kmalloc(sizeof(struct fd_desc));
    if (desc_err == NULL) {
        panic("Out of memory for stderr file descriptor\n");
    }

    struct vnode *verr;

    int r_err = vfs_open(errname, O_WRONLY, 0777, &verr);
    if (r_err) {
        panic("Error opening stderr\n");
    }

    desc_err->vnode = verr;
    desc_err->fp = 0;
    desc_err->flags = O_WRONLY;
    desc_err->refnum = 1;

    // Assign index 2
    curproc->p_table[2] = desc_err;        

    // Sanity checking, should never happen
    KASSERT(desc_err != NULL);
    KASSERT(curproc->p_table[2] != NULL);
}
