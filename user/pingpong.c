#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{
    // parent to child
    int fd[2];

    if (pipe(fd) == -1) {
        fprintf(2, "Error: pipe(fd) error.\n");
    }
    // child process
    if (fork() == 0){
        char buffer[1];
        read(fd[0], buffer, 1);
        close(fd[0]);
        fprintf(0, "%d: received ping\n", getpid());
        write(fd[1], buffer, 1);
    }
    // parent process
    else {
        char buffer[1];
        buffer[0] = 'a';
        write(fd[1], buffer, 1);
        close(fd[1]);
        read(fd[0], buffer, 1);
        fprintf(0, "%d: received pong\n", getpid());

    }


    exit(0);
}