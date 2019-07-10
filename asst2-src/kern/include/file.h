/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
#include <synch.h> /* include for mutex funcs */
#include <types.h>

/*
 * Put your function declarations and data types here ...
 */

/*
 * of_ele is the data structure of each opened file
 */
struct of_ele
{
    off_t offset;                  /* current file offset */
    int ref_num;                 /* open file reference count */
	int flag;                    /* open mode */
    struct vnode *v_ptr;         /* vnode ptr */
    //struct lock * file_lock;     /* mutex */
    struct semaphore * file_sem; /* each open file has a semaphore */
};

//struct lock * of_lock;     /* open file table mutex lock */
struct semaphore * of_sem; /* open file table semaphore */
struct of_ele ** of_table; /* open file table ptr */

int
of_init(void);

void
of_destory(void);

int 
of_find_empty(void);

int
__open(char * filename, int flags, mode_t mode, int index);

int
sys_open(userptr_t filename, int flags, mode_t mode, int * err_num);

int
sys_close(int fd, int * err_num);

ssize_t
sys_read(int fd, userptr_t buf, size_t buflen, int * err_num);

ssize_t
sys_write(int fd, userptr_t buf, size_t nbytes, int * err_num);

off_t
sys_lseek(int fd, off_t pos, int whence, int * err_num);

int
sys_dup2(int oldfd, int newfd, int * err_num);



#endif /* _FILE_H_ */
