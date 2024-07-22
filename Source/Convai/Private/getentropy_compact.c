#pragma warning (disable:4668) // 'expression' : signed/unsigned mismatch

#ifdef PLATFORM_LINUX
    #if PLATFORM_LINUX

    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>

    int getentropy(void *buffer, size_t length) {
        if (length > 256) {
            errno = EIO;
            return -1;
        }

        int fd = open("/dev/urandom", O_RDONLY);
        if (fd == -1) {
            return -1;
        }

        ssize_t result = read(fd, buffer, length);
        int saved_errno = errno;
        close(fd);
        if (result != (ssize_t)length) {
            errno = saved_errno;
            return -1;
        }

        return 0;
    }
    #endif
#endif