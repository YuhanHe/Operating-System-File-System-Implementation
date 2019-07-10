#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>

#define MAX_BUF 500
char teststr[] = "The quick brown fox jumped over the lazy dog.";
char buf[MAX_BUF];
char teststr2[] = "Testing Dup2, please pass, Amen!";
char teststr3[] = "I should be wrote into stdout, but since dup2 on same fd has no effect, so still that file\n";

int
main(int argc, char * argv[])
{
        int fd, r, i, j , k;
        (void) argc;
        (void) argv;

        printf("\n**********\n* File Tester\n");

        snprintf(buf, MAX_BUF, "**********\n* write() works for stdout\n");
        write(1, buf, strlen(buf));
        snprintf(buf, MAX_BUF, "**********\n* write() works for stderr\n");
        write(2, buf, strlen(buf));

        printf("**********\n* opening new file \"test.file\"\n");
        fd = open("test.file", O_RDWR | O_CREAT );
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* writing test string\n");
        r = write(fd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* writing test string again\n");
        r = write(fd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                exit(1);
        }
        printf("* closing file\n");
        close(fd);
        printf("* closed file\n");

        printf("**********\n* opening old file \"test.file\"\n");
        fd = open("test.file", O_RDONLY);
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* reading entire file into buffer \n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes\n", MAX_BUF -i);
                r = read(fd, &buf[i], MAX_BUF - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < MAX_BUF && r > 0);

        printf("* reading complete\n");
        printf("buffer now got :%s\n",buf);

        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                exit(1);
        }
        k = j = 0;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch,buf[k]:%s,teststr [j]:%s\n",buf+k,teststr+ j);
                        printf("the current k:%d,i:%d\n",k,i);
                        exit(1);
                }
                k++;
                j = k % r;
        } while (k < i);
        printf("* file content okay\n");

        printf("**********\n* testing lseek\n");
        r = lseek(fd, 5, SEEK_SET);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                exit(1);
        }

        printf("* reading 10 bytes of file into buffer \n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes\n", 10 - i );
                r = read(fd, &buf[i], 10 - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < 10 && r > 0);
        printf("* reading complete\n");
        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                exit(1);
        }

        k = 0;
        j = 5;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        exit(1);
                }
                k++;
                j = (k + 5)% r;
        } while (k < 5);

        printf("* file lseek  okay\n");
        printf("* closing file\n");
        close(fd);

// ===========================================================================
        printf("**********\n*0-th Dup2 test\n");
        printf(" * Open \"test_dup0.file\" for use\n");
        int fd0 = open("test_dup0.file", O_RDWR | O_CREAT);
        printf("* open() got fd %d\n", fd0);
        if (fd0 < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        printf(" * Open \"test_dup00.file\" for use\n");
        int fd00 = open("test_dup00.file", O_RDWR | O_CREAT);
        printf("* open() got fd %d\n", fd00);
        if (fd00 < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }
        printf("\nfd0 = %d, fd00 = %d\n",fd0, fd00);
        int dup2_ok = dup2(fd0, fd00);
        if (dup2_ok < 0) {
                printf("Error dup2: %s\n", strerror(errno));
                exit(1);
        }

        r = write(fd00, teststr, strlen(teststr));
        printf("* Just now %d bytes was written\n", r);
        if (r < 0) {
                printf(" We ain't able to file, ahhhhhh!: %s\n", strerror(errno));
                exit(1);
        }

        close(fd0);
        fd0 = open("test_dup0.file", O_RDWR);
        printf("* open() again got fd %d\n", fd0);
        if (fd0 < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        i = 0;
        do  {
                printf("* we are try'in to read %d bytes\n", MAX_BUF -i);
                r = read(fd0, &buf[i], MAX_BUF - i);
                printf("* %d bytes was read!!!\n", r);
                i += r;
        } while (i < MAX_BUF && r > 0);


// ============================================================================
        printf("**********\n*first Dup2 test\n");
        printf(" * Open \"test_dup1.file\" for use\n");
        int fd1 = open("test_dup1.file", O_RDWR | O_CREAT);
        printf("* open() got fd %d\n", fd);
        if (fd1 < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        // Copy fd to 100
        dup2_ok = dup2(fd1, 100);
        if (dup2_ok < 0) {
                printf("Error dup2: %s\n", strerror(errno));
                exit(1);
        }

        // These writes should be redirected to fd
        r = write(100, teststr2, strlen(teststr2));
        printf("* Just now %d bytes was written\n", r);
        if (r < 0) {
                printf(" We ain't able to file, ahhhhhh!: %s\n", strerror(errno));
                exit(1);
        }

        i = 0;
        do  {
                printf("* we are try'in to read %d bytes\n", MAX_BUF -i);
                r = read(fd1, &buf[i], MAX_BUF - i);
                printf("* %d bytes was read!!!\n", r);
                i += r;
        } while (i < MAX_BUF && r > 0);

// =================================================================
        // This test redirects stdout to file
        printf("***************\n* second test on dup2: Redirect stdout to a file\n");
        int fd2 = open("test_dup2.file", O_RDWR | O_CREAT);
        printf("* open() got fd2 %d\n", fd2);
        if (fd2 < 0) {
                printf("Error opening file: %s\n", strerror(errno));
                exit(1);
        }

        dup2_ok = dup2(fd2, 1);
        if (dup2_ok < 0) {
                printf("Error dup2: %s\n", strerror(errno));
                exit(1);
        }
        r = write(1, teststr2, strlen(teststr2));
        printf("* Just now %d bytes was written\n", r);
        if (r < 0) {
                printf(" We ain't able to file, ahhhhhh!: %s\n", strerror(errno));
                exit(1);
        }

// ====================================================================
        // Try dup2 on same fd, should have no effect
        printf("***************\n* Third test on dup2: Dup2 on same fds\n");
        dup2_ok = dup2(1, 1);
        if (dup2_ok < 0) {
                printf("Error dup2: %s\n", strerror(errno));
                exit(1);
        }
        r = write(1, teststr3, strlen(teststr3));
         printf("* Just now %d bytes was written\n", r);
        if (r < 0) {
                printf(" We ain't able to file, ahhhhhh!: %s\n", strerror(errno));
                exit(1);
        }

        return 0;
}


