#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    long long sz;

    char buf[256];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, sizeof(buf));
        // printf("sz = %lld\n", sz);
        buf[sz] = 0;
        // printf("fuk my life %s\n",buf);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, sizeof(buf));
        buf[sz] = '\0';
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }
    /*int i=0;
    xs f[3];
    int i;

    f[0] = *xs_tmp("0");
    f[1] = *xs_tmp("1");
    f[2] = *xs_tmp("1");
    int tmp;
    for (i = 2; i <= k; i++) {
        tmp = i%3;
        string_number_add(&f[(tmp+1)%3], &f[(tmp+2)%3], &f[tmp]);
        printf("%s\n", xs_data(&f[tmp]));
    }
    tmp = k%3;*/
    // n = xs_size(&f[tmp]);
    close(fd);
    return 0;
}
