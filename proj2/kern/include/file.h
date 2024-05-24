/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
    TODO: Locks to be added when implementing fork 
 */
struct fd_desc {
    struct vnode *vnode;    // Inode abstraction
    off_t fp;               // For the file offset
    int flags;              // Mode the file was opened in 
    int refnum;             // For dup2, how many ptrs reference this
};


/*
    Syscalls return the err code:
        - Error: return the errno
        - Success: return 0
    In case where errno is returned, users will recieve -1.
    In cases where 0 is returned retval will be set.

    Values such as amount written are modified in retval ptr, 
    passed in from syscall.c
*/
void sys_init(void);
int sys_open(userptr_t fbuffer, int flags, mode_t mode, int32_t *retval);
int sys_write(int fd, void *buf, size_t buflen, int32_t *retval);
int sys_lseek(int fd, off_t pos, int whence, int32_t *retval);
int sys_close(int fd, int32_t *retval);
int sys_read(int fd, void *buf, size_t buflen, int32_t *retval);
int sys_dup2(int oldfd, int newfd, int32_t *retval);

#endif /* _FILE_H_ */