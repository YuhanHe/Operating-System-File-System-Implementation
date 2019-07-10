#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>
#include <proc.h>

/*
 * Add your file-related functions here ...
 */

int
of_init() {
	int i;
    of_table = (struct of_ele **)kmalloc(sizeof(struct of_ele *) * __OPEN_MAX);
	for (i = 0; i < __OPEN_MAX; i++) {
        of_table[i] = NULL;
	}
	//of_lock = lock_create("open file table lock");
    //if (of_lock == NULL) return ENOMEM;
    of_sem = sem_create("of_table semaphore", 1); //sem_destroy(the_sem);
    if (of_sem == NULL) return ENOMEM;  /* Out of memory */    
    return 0;
}

void
of_destory() {
    kfree(of_table);
}

int of_find_empty(){
    int i;
    for (i = 0; i < __OPEN_MAX; i++) {
        if (of_table[i] == NULL){
            return i;
        }
    }
    return -1;  /* Too many open files in system */
}

int
__open(char * filename, int flags, mode_t mode, int index) {
    struct vnode *v_node;
    int v_ret;
    v_ret = vfs_open(filename, flags, mode, &v_node);
    if(v_ret) {
        return v_ret; /* check if vfs_open() works correctly */
    }
    //kprintf("index = %d\n", index);
    of_table[index] = (struct of_ele *)kmalloc(sizeof(struct of_ele));
    if(of_table[index]== NULL){
        vfs_close(v_node);
        kfree(v_node);
        return ENOMEM; /* Out of memory */
    }
    //of_table[index]->file_lock = lock_create("opend file lock");
    of_table[index]->file_sem = sem_create("opend file semaphore", 1);
    of_table[index]->flag = flags;
    struct stat info;
    off_t ret2 = VOP_STAT(v_node, &info);
    if(ret2) {
        return ret2;
    }
    off_t len = info.st_size;
    if ((int)flags/(int)O_APPEND == 1) {
        of_table[index]->offset = len;
    }
    else {
        of_table[index]->offset = 0;
    }
    //of_table[index]->offset = 0;
    of_table[index]->ref_num = 1;
    of_table[index]->v_ptr = v_node; 
    return 0;
}
 
int
sys_open(userptr_t filename, int flags, mode_t mode, int * err_num) {
    if (filename == NULL) {
        *err_num = EFAULT; /* Bad memory reference */
        return -1;
    }
    int i, j;
    /* check if per process fd table is full */
    for (j = 0; j < __OPEN_MAX; j++){
        if (curproc->fd_table[j] == -1) {
            break;
        }
    }
    if (j == __OPEN_MAX){
        *err_num = ENFILE; /* Too many open files */
        return -1; 
    }


    /* 
     * Now we got the j which is the fd, 
     * fd here is the index of fd table,
     * fd_table[fd] store an index of of_table 
     */
    
    int open_ret, copy_ret;
    char * kfilename = NULL;
    size_t * got = NULL;
    kfilename = (char *)kmalloc(PATH_MAX);
    copy_ret = copyinstr((const_userptr_t)filename, kfilename, PATH_MAX, got);
    if (copy_ret) {
        kfree(kfilename);
        *err_num = copy_ret;
        return -1;
    }
    //kprintf("\nkfilename = %s\n", kfilename);

    P(of_sem);
    for (i = 0; i < __OPEN_MAX; i++) {
        if (of_table[i] == NULL) {
            open_ret = __open(kfilename, flags, mode, i);
            if (open_ret) {
                /* handle error of __open() */
                V(of_sem);
                *err_num  = open_ret;
                kfree(kfilename);
                return -1;  
            };
            break;
        }
    }
    V(of_sem);
    kfree(kfilename);
    if (i == __OPEN_MAX) {
        *err_num = ENFILE; /* Too many open files in system */
        return -1;  
    }
    curproc->fd_table[j] = i;
    //kprintf("\nreturn j = %d\n",j);
    return j;
}

int
sys_close(int fd, int * err_num) {
    // check if fd is valid
    if (fd < 0 || fd >= __OPEN_MAX || curproc->fd_table[fd] == -1) {
        *err_num = EBADF; /* Bad file number */
        return -1;
    }
    int of_index = curproc->fd_table[fd];

    P(of_table[of_index]->file_sem);
    of_table[of_index]->ref_num--;
    if (of_table[of_index]->ref_num == 0 && of_index > 2) {
        P(of_sem);
        V(of_table[of_index]->file_sem);
        vfs_close(of_table[of_index]->v_ptr);
        sem_destroy(of_table[of_index]->file_sem);
        kfree(of_table[of_index]);
        of_table[of_index] = NULL;
        V(of_sem);
    }
    else {
        V(of_table[of_index]->file_sem);
    }
    
    curproc->fd_table[fd] = -1;
    return 0;
}

/* 
 * return actual read bytes, -1 if error
 */
ssize_t
sys_read(int fd, userptr_t buf, size_t buflen, int * err_num) {
    /* check if buffer ptr is valid */
    if (buf == NULL) {
        *err_num = EFAULT; /* Bad memory reference */
        return -1;
    }
    /* check if fd is valid */
    if (fd >= __OPEN_MAX  || 0 > fd  || -1 == curproc->fd_table[fd]) {
        *err_num = EBADF; /* Bad file number */
        return -1;
    }
    
    /* check if the file is already opend and not write only */
    P(of_table[curproc->fd_table[fd]]->file_sem);
    int baseflags = of_table[curproc->fd_table[fd]]->flag & O_ACCMODE;
    if (NULL == of_table[curproc->fd_table[fd]] || O_WRONLY == baseflags) {
        *err_num = EBADF; /* Permission denied */
        V(of_table[curproc->fd_table[fd]]->file_sem);
        return -1;
    }
    V(of_table[curproc->fd_table[fd]]->file_sem);

    void * kbuf = kmalloc(sizeof(void *) * buflen);
    if(kbuf == NULL) {
        *err_num = EFAULT; /* Bad memory reference */
        return -1;
    }

    struct iovec myiov;
    struct uio myuio;
    int res;
    myiov.iov_ubase = buf;
    P(of_table[curproc->fd_table[fd]]->file_sem);
    uio_kinit(&myiov, &myuio, kbuf, buflen, of_table[curproc->fd_table[fd]]->offset, UIO_READ);
    res = VOP_READ(of_table[curproc->fd_table[fd]]->v_ptr, &myuio);
    if (res) {
        V(of_table[curproc->fd_table[fd]]->file_sem);
        kfree(kbuf);
        *err_num = res;
        return -1;
    }
    int len = buflen - myuio.uio_resid;
    res = copyout(kbuf, buf, len);
    kfree(kbuf);
    //f (res) {
    if (res && len) {
        V(of_table[curproc->fd_table[fd]]->file_sem);
        *err_num = res;
        return -1;
    }
    of_table[curproc->fd_table[fd]]->offset += len;
    V(of_table[curproc->fd_table[fd]]->file_sem);
    return len;
}

ssize_t
sys_write(int fd, userptr_t buf, size_t nbytes, int * err_num) {
	// invalid fd
	if (fd < 0 || fd >= __OPEN_MAX || curproc->fd_table[fd]== -1) {
        *err_num = EBADF; /* Bad file number */
        return -1;
    }
    // check buffer
    if (buf == NULL) {
        *err_num = EFAULT; /* Bad memory reference */
        return -1;
    }
    /* check if the file is already opend and not read only */
    P(of_table[curproc->fd_table[fd]]->file_sem);
    int baseflags = of_table[curproc->fd_table[fd]]->flag & O_ACCMODE;
    if (NULL == of_table[curproc->fd_table[fd]] || O_RDONLY == baseflags) {
        *err_num = EBADF; /* Bad memory reference */
        V(of_table[curproc->fd_table[fd]]->file_sem);
        return -1;
    }
    V(of_table[curproc->fd_table[fd]]->file_sem);
	void * kbuf = kmalloc(sizeof(void *) * nbytes);
	if(kbuf == NULL) {
        *err_num = EBADF; /* Bad memory reference */
        return -1;
    }

    //copy the userland buffer to kernal buffer
    int result;
	result = copyin((const_userptr_t) buf, kbuf, nbytes);
	if (result && nbytes)
    //if (result)
	{
		kfree(kbuf);
        *err_num = result;
		return -1;
	}

    struct iovec myiov;
    struct uio myuio;
    myiov.iov_ubase = buf;

    int of_index = curproc->fd_table[fd];
    struct vnode * v_node;
    P(of_table[of_index]->file_sem);
    v_node = of_table[of_index]->v_ptr;
    uio_kinit(&myiov, &myuio, kbuf, nbytes, of_table[of_index]->offset, UIO_WRITE);
    result = VOP_WRITE(v_node, &myuio);

    if (result) {
        *err_num = result;
        kfree(kbuf);
        V(of_table[of_index]->file_sem);
        return -1;
    }

    // length of written
    int len = nbytes - myuio.uio_resid;
    // update offset
	of_table[of_index]->offset += len;
	V(of_table[of_index]->file_sem);
	kfree(kbuf);

    return len;
}

off_t
sys_lseek(int fd, off_t pos, int whence, int *err_num) {
	if (fd < 0 || fd >= __OPEN_MAX || curproc->fd_table[fd]== -1) {
        *err_num = EBADF; /* Bad file number */
        return -1;
    }

    struct vnode * node;
    int ret1;
    struct stat info;
    off_t len, ret2;

    P(of_table[curproc->fd_table[fd]]->file_sem);
    node = of_table[curproc->fd_table[fd]]->v_ptr;
    ret1 = VOP_ISSEEKABLE(node);
    if (!ret1) {
        V(of_table[curproc->fd_table[fd]]->file_sem);
        *err_num = ESPIPE; /* Illegal seek */
        return -1;
    }
    
    ret2 = VOP_STAT(node, &info);
    if(ret2) {
        V(of_table[curproc->fd_table[fd]]->file_sem);
        *err_num = ret2;
        return -1;
    }
    //V(of_table[curproc->fd_table[fd]]->file_sem);

    len = info.st_size;
    switch (whence)
    {
        case SEEK_END:
            ret2 = len + pos;
            break;
        case SEEK_CUR:
            ret2 = of_table[curproc->fd_table[fd]]->offset + pos;
            break;
        case SEEK_SET:
            ret2 = pos;
            break;
        default:
            V(of_table[curproc->fd_table[fd]]->file_sem);
            *err_num = EINVAL; /* Invalid argument */
            return -1;
    }
    if (ret2 < 0) {
        V(of_table[curproc->fd_table[fd]]->file_sem);
        *err_num = EINVAL; /* Invalid argument */
        return -1;
    }
    // update offset
    of_table[curproc->fd_table[fd]]->offset = ret2;
    V(of_table[curproc->fd_table[fd]]->file_sem);
    return ret2;
}

int sys_dup2(int oldfd, int new_fd, int* err_num) {
    // check if fd is valid
    if (oldfd < 0 || oldfd >= __OPEN_MAX || new_fd < 0 || new_fd >= __OPEN_MAX || curproc->fd_table[oldfd] == -1) {
        *err_num = EBADF; /* Bad file number */
        //kprintf("\ndup2 return -1 because badfilenum err\n");
        return -1;
    }
    // check if the same fd
    if (oldfd==new_fd) return new_fd;
    
    // copy fd
    int err = 0;
    //int32_t retval;
    if (curproc->fd_table[new_fd] != -1) {
        //need to close new_fd
        sys_close(new_fd, &err);
        if (err) {
            *err_num = err; /* Bad file number */
            //kprintf("\ndup2 return -1 because close err\n");
            return -1;
        }
    }

    curproc->fd_table[new_fd] = curproc->fd_table[oldfd];
    P(of_table[curproc->fd_table[new_fd]]->file_sem);
    of_table[curproc->fd_table[new_fd]]->ref_num++;
    V(of_table[curproc->fd_table[new_fd]]->file_sem);

    return new_fd;
}
