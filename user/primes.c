#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void new_proc(int p[2]) {
    int prime;
    int n;
    // close the write side of p
    close(p[1]);
    if (read(p[0], &prime, 4) != 4) {
        fprintf(2, "Error in read.\n");
        exit(1);
    }
    printf("prime %d\n", prime);
    // if read return not zero
    // if it still need next process
    if (read(p[0], &n, 4) == 4){
        int newfd[2];
        pipe(newfd);
        // father
        if (fork() != 0) {
            close(newfd[0]);
            if (n % prime) write(newfd[1], &n, 4);
            while (read(p[0], &n, 4) == 4) {
                if (n % prime) write(newfd[1], &n, 4);
            }
            close(p[0]);
            close(newfd[1]);
            wait(0);
        }
        // child
        else {
            new_proc(newfd);
        }
    }
}

int
main(int argc, char* argv[])
{
    int p[2];
    pipe(p);
    // child process
    if (fork() == 0) {
        new_proc(p);
    }
    // father process
    else {
        // close read port of pipe
        close(p[0]);
        for (int i = 2; i <= 35; ++i) {
            if (write(p[1], &i, 4) != 4) {
                fprintf(2, "failed write %d into the pipe\n", i);
                exit(1);
            }
        }
        close(p[1]);
        wait(0);
        exit(0);
    }
    return 0;
}