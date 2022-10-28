#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define DEBUG 0

void xargs_exec(char* program, char** paraments);



void
getArgs(char** first_arg, int size, char* program_name)
{
    char buf[1024];
    if (DEBUG) {
        for (int i = 0; i < size; ++i) {
            printf("first_arg[%d] = %s\n", i, first_arg[i]);
        }
    }
    char *arg[MAXARG];
    int m = 0;
    while (read(0, buf+m, 1) == 1) {
        if (m >= 1024) {
            fprintf(2, "xargs: arguments too long.\n");
            exit(1);
        }
        if (buf[m] == '\n') {
            buf[m] = '\0';
            if (DEBUG) {
                printf("this line is %s\n", buf);
            }
            memmove(arg, first_arg, sizeof(*first_arg)*size);
            arg[size] = malloc(sizeof(char)*(m+1));
            memmove(arg[size], buf, m+1);
            if (DEBUG) {
                for (int j = 0; j <= size; ++j)
                    printf("arg[%d] = %s\n", j, arg[j]);
            }
            arg[size+1] = 0;
            xargs_exec(program_name, arg);
            free(arg[size]);
            m = 0;
        } else {
            m++;
        }
    }
}

void
xargs_exec(char* program, char** paraments)
{
    if (fork() > 0) {
        wait(0);
    } else {
        if (DEBUG) {
            printf("child process\n");
            printf("program = %s\n", program);
            
            for (int i = 0; paraments[i] != 0; ++i) {
                printf("paraments[i] = %s\n", paraments[i]);
            }
        }
        if (exec(program, paraments) == -1) {
            fprintf(2, "xargs: Error exec %s\n", program);
        }
    }
}

int
main(int argc, char* argv[])
{
    if(DEBUG) {
        printf("main func\n");
    }
    if (argc <= 1) {
        if(DEBUG) {
            printf("argc <= 1\n");
        }
        getArgs(argv+1, argc - 1, "echo");
    }
    else if (argc >= 3) {
        if(DEBUG) {
            printf("argc >= 3\n");
            printf("argv[1] = %s\n", argv[1]);
        }
        getArgs(argv+1, argc - 1, argv[1]);
    }
    exit(0);
}
