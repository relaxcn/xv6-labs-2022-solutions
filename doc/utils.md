# Lab: Xv6 and Unix utilities

这个实验将会熟悉 xv6 操作系统和它的系统调用。

## Boot xv6

首先参考 [Tools Used in 6.1810](https://pdos.csail.mit.edu/6.828/2022/tools.html) 配置以下系统环境。这里我用的是 Windows 下的 WSL2 Ubuntu 20.04.

~~~bash
sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu 
~~~

克隆 xv6 代码，并测试。

~~~bash
git clone git://g.csail.mit.edu/xv6-labs-2022
cd xv6-labs-2022
make qemu
~~~

可以运行以下 `ls` 命令查看其文件。

xv6 中没有 `ps` 命令，但是你可以输入 `Ctrl-P`，内核将会打印出每一个进程（process）的信息。

想要退出 qemu ，输入 `Ctrl-a x` （同时按住 `Ctrl`和 `x` ，然后松开再按 `x`）。

## sleep

注意事项：

1. 你的程序名为 `sleep`，其文件名为 `sleep.c`，保存在 `user/sleep.c` 中。
2. 开始之前，你应该阅读 [xv6 book](https://pdos.csail.mit.edu/6.828/2022/xv6/book-riscv-rev3.pdf) 的第一章。
3. 使用系统调用 `sleep`。
4. `main` 函数应该以 `exit(0)` 结束。
5. 将你的`sleep` 函数添加到 `Makefile` 中的 `UPROGS`中，只有这样 `make qemu`时 才会编译它们。

~~~c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char* argv[])
{
    if (argc != 2) {
        fprintf(2, "Usage: sleep times\n");
    }
    int time = atoi(*++argv);
    if (sleep(time) != 0) {
        fprintf(2, "Error in sleep sys_call!\n");
    }
    exit(0);
}
~~~

将 `$U/_sleep\` 添加到 `Makefile` 中，如下图：

![image-20221030211552460](C:\Users\chen2\AppData\Roaming\Typora\typora-user-images\image-20221030211552460.png)

然后运行 `make qemu` ，之后执行 `sleep` 函数。

~~~bash
make qemu
sleep 10
(nothing happens for a little while)
~~~

你可以运行以下命令来测试你写的程序：

~~~bash
./grade-lab-util sleep
== Test sleep, no arguments == sleep, no arguments: OK (3.1s)
== Test sleep, returns == sleep, returns: OK (0.6s)
== Test sleep, makes syscall == sleep, makes syscall: OK (0.6s)
~~~

在此时前你可能需要赋予 `grade-lab-util ` 可执行权限。

~~~bash
sudo chmod +x ./grade-lab-util
~~~

## pingpong

~~~c
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
~~~

参考：https://www.geeksforgeeks.org/pipe-system-call/

## primes

这个比较难，理解递归会稍微好一点。其本质上是一个素数筛，使用的递归。需要看一个文章：http://swtch.com/~rsc/thread/

~~~c
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
~~~

## find

注意事项：

1. 查看 `user/ls.c` 的实现，看看它如何读取文件夹（本质是一个文件）。
2. 使用递归去读取子文件夹。
3. 不要递归 `.` 和 `..` 这两个文件。（这个特别重要，不然会无限循环了）
4. 文件系统中的更改在 qemu 中是一直保持的，所以使用 `make clean && make qemu` 清理文件系统。
5. 字符串的比较不能使用（==），要使用 `strcmp`。（这是 C，所以 == 字符串比较的是地址）

强烈建议弄明白 `user/ls.c` ，读取文件本质上没什么区别。程序中的 `printf` 只是为了方便我调试，lab2 将学习如何使用 GDB 进行调试。

~~~c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

// 去除字符串后面的空格
char*
rtrim(char* path)
{
    static char newStr[DIRSIZ+1];
    int whiteSpaceSize = 0;
    int bufSize = 0;
    for(char* p = path + strlen(path) - 1; p >= path && *p == ' '; --p) {
        ++whiteSpaceSize;
    }
    bufSize = DIRSIZ - whiteSpaceSize;
    memmove(newStr, path, bufSize);
    newStr[bufSize] = '\0';
    return newStr;
}

void
find(char* path, char* name)
{
    char buf[512], *p;
    int fd;
    // dir descriptor
    struct dirent de;
    // file descriptor
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) == -1) {
        fprintf(2, "find: cannot fstat %s\n", path);
        close(fd);
        return;
    }
    // printf("switch to '%s'\n", path);
    switch (st.type) {
        
        case T_DEVICE:
        case T_FILE:
            fprintf(2, "find: %s not a path value.\n", path);
            close(fd);
            // printf("==='%s' is a File\n", path);
            break;
        case T_DIR:
            // printf("==='%s' is a dir\n", path);
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
                printf("ls: path too long\n");
                break;
            }
            // create full path
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            // read dir infomation for file and dirs
            while (read(fd, &de, sizeof(de)) == sizeof de) {
                if (de.inum == 0)
                    continue;
                if (strcmp(".", rtrim(de.name)) == 0 || strcmp("..", rtrim(de.name)) == 0)
                    continue;
                // copy file name to full path
                memmove(p, de.name, DIRSIZ);
                // create a string with zero ending.
                p[DIRSIZ] = '\0';
                // stat each of files
                if (stat(buf, &st) == -1) {
                    fprintf(2, "find: cannot stat '%s'\n", buf);
                    continue;
                }
                // printf("===file:'%s'\n", buf);
                if (st.type == T_DEVICE || st.type == T_FILE) {
                    if (strcmp(name, rtrim(de.name)) == 0) {
                        printf("%s\n", buf);
                        // for (int i = 0; buf[i] != '\0'; ++i) {
                        //     printf("'%d'\n", buf[i]);
                        // }
                    }
                }
                else if (st.type == T_DIR) {
                    find(buf, name);
                }
            }
        
    }
}

int
main(int argc, char* argv[])
{
    if (argc != 3) {
        fprintf(2, "Usage: find path file.\n");
        exit(0);
    }
    char* path = argv[1];
    char* name = argv[2];
    // printf("path is '%s'\n", path);
    find(path, name);
    exit(0);
}
~~~

## xargs

`xargs` 程序将从 pipe （管道）中传递的值转换为其他程序的命令行参数（command line arguments)。

那么如何 `xargs` 如何从管道 (pipe) 中读取数据呢？其本质上就是从文件描述符 0 也就是 file descriptor 0 (standard input) 标准输入读取数据。这在  [xv6 book](https://pdos.csail.mit.edu/6.828/2022/xv6/book-riscv-rev3.pdf) 1.2章节有说明。

`xargs` 程序将从管道中读取的参数作为其他程序的命令行参数。

例如 `echo a | xargs echo b` ，它将输出 `b a`。`xargs` 将前面程序的输出 `a` 作为后面 `echo` 程序的命令行参数。所以上述命令等价于 `echo b a`。

注意事项：

1. 每一行输出都使用 `fork` 和 `exec` ，并且使用 `wait` 等待子程序完成。
2. 通过每次读取一个字符直到一个新行(`'\n'`) 出现，来读取每一行输入。
3. `kernel/param.h` 定义了 `MAXARG`，这在你定义一个参数数组时会有用。
4. 运行 `make clean` 清理文件系统。然后再运行 `make qemu`。

要测试你的程序，运行一个 shell 脚本。正确的程序将会显示如下结果：

~~~bash
make qemu
...
xv6 kernel is booting

hart 1 starting
hart 2 starting
init: starting sh
$ sh < xargstest.sh
$ $ $ $ $ $ hello
hello
hello
$ $
~~~

这个脚本实际就是 `find . b | xargs grep hello` ，寻找所有文件 b 中包含 `hello`  的行。

~~~c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

// 1 为打印调试信息
#define DEBUG 0

// 宏定义
#define debug(codes) if(DEBUG) {codes}

void xargs_exec(char* program, char** paraments);

void
xargs(char** first_arg, int size, char* program_name)
{
    char buf[1024];
    debug(
        for (int i = 0; i < size; ++i) {
            printf("first_arg[%d] = %s\n", i, first_arg[i]);
        }
    )
    char *arg[MAXARG];
    int m = 0;
    while (read(0, buf+m, 1) == 1) {
        if (m >= 1024) {
            fprintf(2, "xargs: arguments too long.\n");
            exit(1);
        }
        if (buf[m] == '\n') {
            buf[m] = '\0';
            debug(printf("this line is %s\n", buf);)
            memmove(arg, first_arg, sizeof(*first_arg)*size);
            // set a arg index
            int argIndex = size;
            if (argIndex == 0) {
                arg[argIndex] = program_name;
                argIndex++;
            }
            arg[argIndex] = malloc(sizeof(char)*(m+1));
            memmove(arg[argIndex], buf, m+1);
            debug(
                for (int j = 0; j <= argIndex; ++j)
                    printf("arg[%d] = *%s*\n", j, arg[j]);
            )
            // exec(char*, char** paraments): paraments ending with zero
            arg[argIndex+1] = 0;
            xargs_exec(program_name, arg);
            free(arg[argIndex]);
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
        debug(
            printf("child process\n");
            printf("    program = %s\n", program);
            
            for (int i = 0; paraments[i] != 0; ++i) {
                printf("    paraments[%d] = %s\n", i, paraments[i]);
            }
        )
        if (exec(program, paraments) == -1) {
            fprintf(2, "xargs: Error exec %s\n", program);
        }
        debug(printf("child exit");)
    }
}

int
main(int argc, char* argv[])
{
    debug(printf("main func\n");)
    char *name = "echo";
    if (argc >= 2) {
        name = argv[1];
        debug(
            printf("argc >= 2\n");
            printf("argv[1] = %s\n", argv[1]);
        )
    }
    else {
        debug(printf("argc == 1\n");)
    }
    xargs(argv + 1, argc - 1, name);
    exit(0);
}
~~~