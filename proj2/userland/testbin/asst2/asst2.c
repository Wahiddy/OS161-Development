#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>

#define MAX_BUF 500
char teststr[] = "The quick brown fox jumps over the lazy dog. ";
char buf[MAX_BUF];

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
        fd = open("test.file", O_RDWR | O_CREAT, 0600); /* mode u=rw in octal */
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

        // int nextFd = open("wahid.test", O_RDWR | O_CREAT, 0777);
        // if (nextFd < 0) {
        //         printf("ERROR opening file: %s\n", strerror(errno));
        // }

        // int myWritten = write(nextFd, teststr, strlen(teststr));
        // if (myWritten < 0) {
        //         printf("ERROR writing file: %s\n", strerror(errno));
        // }

        // int appendFd = open("wahid.test", O_WRONLY | O_APPEND);
        // if (appendFd < 0) {
        //         printf("ERROR opening file: %s\n", strerror(errno));
        // }

        // int myWritten = write(appendFd, teststr, strlen(teststr));
        // if (myWritten < 0) {
        //         printf("ERROR writing file: %s\n", strerror(errno));
        // }

        printf("* writing test string again\n");
        r = write(fd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                exit(1);
        }
        printf("* closing file\n");
        close(fd);

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
        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                exit(1);
        }
        k = j = 0;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
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

        printf("**********\n* testing dup2\n");

        int newfd = 0;
        int oldfd = 0;
        int err = 0;

        // test 1
        // same oldfd newfd -> newfd
        oldfd = open("file1", O_RDONLY | O_CREAT);
        if (oldfd < 0) { printf("test 1 open error: oldfd %s\n", strerror(errno)); exit(1); }

        err = dup2(oldfd, oldfd);
        printf("test 1: dup2 got newfd %d\n", err);
        if (err < 0) {
                printf("test 1 error %s\n", strerror(errno));
                exit(1);
        } else {
                printf("test 1 success\n");
        }
        
        // test 2
        // null oldfd -> error
        if (close(oldfd) < 0) { printf("test 2 close error: oldfd "); }
        newfd = 4;
        
        err = dup2(oldfd, newfd);
        printf("test 2: dup2 got newfd %d\n", err);
        if (err > 0) {
                printf("test 2 error %s\n", strerror(errno));
                exit(1);
        } else {
                printf("test 2 success\n");
        }

        // test 3
        // oldfd, null newfd -> newfd
        oldfd = open("file1", O_RDONLY | O_CREAT);
        if (oldfd < 0) { printf("test 3 open error: oldfd\n"); exit(1); }

        newfd = 5;
        err = dup2(oldfd, newfd);
        printf("test 3: dup2 got newfd %d\n", err);
        if (err < 0) {
                printf("test 3 error %s\n", strerror(errno));
                exit(1);
        } else {
                printf("test 3 success\n");
        }

        /*
        test 4
        dup2 oldfd into an opened file
        -> success if it does (return 0)
        */
        close(newfd);
        close(oldfd);
        
        oldfd = open("file1", O_RDONLY | O_CREAT);
        if (oldfd < 0) { printf("test 4 open error: oldfd\n"); exit(1); }
        newfd = open("file2", O_RDONLY | O_CREAT); 
        if (newfd < 0) { printf("test 4 open error: newfd\n"); exit(1); }
        
        err = dup2(oldfd, newfd);
        printf("test 4: dup2 got newfd %d\n", err);
        if (err < 0) {
                printf("test 4 dup error %s\n", strerror(errno));
                exit(1);
        } else {
                printf("test 4 dup success\n");
        }

        close(oldfd);
        close(newfd);
        
        /*
        test 5
        dup2
        write on newfd
        check if it is writing to oldfd
        -> success if it does 
        */

        oldfd = open("test_dup.txt", O_RDWR | O_CREAT);
        if (oldfd < 0) { printf("test 4 open error: oldfd\n"); exit(1); }
        newfd = 4;
        
        err = dup2(oldfd, newfd);
        printf("test 5: dup2 got newfd %d\n", err);
        if (err < 0) {
                printf("test 5 dup error %s\n", strerror(errno));
                exit(1);
        } else {
                printf("test 5 dup success\n");
        }
        
        err = write(oldfd, teststr, strlen(teststr));
        if (err < 0) { 
                printf("test 5 write error: oldfd %s\n", strerror(errno)); 
                exit(1); 
        }
        err = write(newfd, teststr, strlen(teststr));
        if (err < 0) { 
                printf("test 5 write error: newfd %s\n", strerror(errno)); 
                exit(1); 
        }
        
        close(oldfd);
        close(newfd);

        /*
        test 6
        redirecting to stdout aka.
        oldfd = n+3
        newfd = 1
        -> success if it does (dup2 returns 0)
        */
        oldfd = open("test.file", O_RDONLY | O_CREAT);
        if (oldfd < 0) { printf("test 4 open error: oldfd\n"); exit(1); }
        newfd = 1;
       
        err = dup2(oldfd, newfd);
        printf("test 6: dup2 got newfd %d\n", err);
        if (err < 0) {
                printf("test 6 dup error %s\n", strerror(errno));
                exit(1);
        } else {
                printf("test 6 dup success\n");
        }

        return 0;


}

