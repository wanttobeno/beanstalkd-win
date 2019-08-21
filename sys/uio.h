#ifndef SYS_UIO_H
#define SYS_UIO_H
#include <inttypes.h>
#include <unistd.h>

struct iovec
{
    void    *iov_base;  /* Base address of a memory region for input or output */
    size_t   iov_len;   /* The size of the memory pointed to by iov_base */
};

size_t readv(int fildes, const struct iovec *iov, int iovcnt);
size_t writev(int fildes, const struct iovec *iov, int iovcnt);

#endif /* SYS_UIO_H */