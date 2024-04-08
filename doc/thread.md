# lab thread

本文不再像前文那样进行细致的翻译和讲解，我想一个有追求的人应该具备独立思考的基本能力。

开始前，应该切换到 `thread` 分支，注意需要阅读 xv6-book 的 Chapter 7: Scheduling 章节。我建议先阅读，后面再来做lab，这样你会具有一个更加具体的概念。

## Uthread: switching between threads

在这个练习中，你需要在用户级别的线程系统中设计一个上下文(context)切换机制。

xv6 提供了两个文件 `user/uthread.c` 和 `user/uthread_switch.S`，首先阅读这两个文件，尤其是 `uthread.c`，知道它是如何运行的。

> 你的任务：想出一个方法创建thread，并且在线程切换的时候保存和恢复CPU上下文。

### 思路

#### 0x1

根据xv6-book，切换线程需要保存和恢复CPU上下文（也就是寄存器）的相关信息，所以在 struct thread 中，需要有CPU寄存器的相关信息，你手动添加它。我们不需要保存全部的寄存器，只需要保存 **[callee-save registers](https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf)**，你可以在 `kernel/proc.h` 中找到相关的内容，不过还是建议你阅读前面的超链接，你便你更好的了解。

所以在 `user/uthread.c` 中定义一个 struct context 来保存 callee-save registers 的信息。

```cpp
struct context {
  uint64 ra; // return address
  uint64 sp; // stack pointer

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct context context;       // register context
};
```

#### 0x2

`user/uthread_switch.S` 中，需要用asm编写保存和恢复寄存器的汇编代码，如果你做过前面的lab，或者认真读了xv6 book，那么这个函数 `thread_switch(uint64, uint64)` 和 `kernel/swtch.S` 中的`swtch`函数不能说一模一样，简直是没什么区别。


#### 0x3

在 `thread_schedule` 中，很显然需要调用 `thread_swtich` 函数来保存寄存器和恢复CPU上下文为即将运行的进程的上下文信息。

xv6 book 中说道，thread switch 的原理就是通过保存原本的CPU上下文，然后恢复想要调度运行的进程的CPU上下文信息，其中最重要的就是寄存器 `ra` 的值，因为它保存着函数将要返回的地址 return address. 所以此时 `ra` 中的地址是什么，CPU就会跳转到这个地址进行运行。这就是所谓的 thread switch. 不过为了保持原本线程中的数据的完整性，需要一并恢复它所需要的寄存器的信息，也就是 callee-save resigers.

```cpp
void 
thread_schedule(void)
{
  ...
  if (current_thread != next_thread) {         /* switch threads?  */
    ...
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
    // switch old to new thread
    // save old thread context and restore new thread context
    thread_switch(&t->context, &current_thread->context);
  } else
    next_thread = 0;
}
```

这里为了方便，我修改了 `thread_switch` 函数的参数原型：

```cpp
extern void thread_switch(struct context *old, struct context *new);
```

#### 0x4

通过上述0x3的讲解，你应该清楚了线程切换的根本原理。如何在一开始 `thread_schedule` 函数调度的时候，切换的新的CPU上下文信息中的 `ra` 寄存器为我们想要调转的那个函数的地址呢？答案就是在 `thread_create` 的时候，提前将对应的 thread 的 `ra` 寄存器设置为对应的函数地址。

同时需要注意，每个 thread 拥有一个独立的 stack 空间。所以需要同时将寄存器 `sp` (stack pointer) 设置为 `thread.stack` 的地址。

```cpp
void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  // save stack pointer to stack address
  t->context.sp = (uint64)&t->stack[0];
  t->context.ra = (uint64)(*func);
}
```
是不是挺有道理的？不过就是结果不正确，你会发现，经过一次两次 thread_switch 之后 `thread[0\1\2..]` 中的某些数据被修改为一个随机值了。比如 `thread[0].state` 在调度到 `thread_a` 之后，莫名的被修改为了一个随机的值，它正确的值应该为 `1`，因为它是 main 函数的线程，后面不会再进行调度。

其实原因就是 `sp` 的值错误了。学过操作系统原理的同学应该清楚，栈空间是从高往低填充的，也就是说 stack pointer 应该指向这个栈内存空间的高地址，而不是低地址。

在 C 语言中，一个数组的空间是从低地址向高地址分配的，所以此时 `t->stack` 是这个数组的低地址，我们需要将它的高地址传给 `sp` resiger. 也就是 `&t->stack[STACK_SIZE-1];`。
所以正确的应该是：

```cpp
  // save stack pointer to stack address
  t->context.sp = (uint64)&t->stack[STACK_SIZE-1];
  t->context.ra = (uint64)(*func);
```

我们应该掌握最基本的调式方法，详细的GDB调式方法见 https://sourceware.org/gdb/current/onlinedocs/gdb.html/

例如希望显示源代码 `layout src` ，希望显示asm代码 `layout asm`，二者都显示 `layout split`，见 https://sourceware.org/gdb/current/onlinedocs/gdb.html/TUI-Commands.html#TUI-Commands

在源代码中，如果执行 `next` 会执行多行asm代码，如果向单行调试asm代码，需要执行 `si` 。 

详细的代码改动见 [GitHub Commit](https://github.com/relaxcn/xv6-labs-2022-solutions/commit/a965c2be0089a9fe8c827b2e873d76ade95a5008).

## Using threads

详见 [GitHub commit](https://github.com/relaxcn/xv6-labs-2022-solutions/commit/b95669c94e27e50427d105007d3ed1dae60b9e46)，注意我的注释即可，最好画图分析一下指针，知道为什么会丢失数据。

```cpp
static 
void put(int key, int value)
{
  int i = key % NBUCKET;

  // is the key already present?
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if(e){
    // update the existing key.
    e->value = value;
  } else {
  pthread_mutex_lock(&lock[i]);
    // the new is new.
    //  重要的是 table[i] 的值，如果 thread_1 刚进入，但是 thread_2 刚好完成修改了 table[i] 的操作，此时就会丢失后面的所有node
  insert(key, value, &table[i], table[i]);
  pthread_mutex_unlock(&lock[i]);
  }

}
```

## Barrier

Barrier 是一种机制，可以让所有线程阻塞，直到所有线程都到达这个位置，然后继续运行。详见 [Wikipedia](https://en.wikipedia.org/wiki/Barrier_(computer_science)).

`pthread_cond_wait` 在进入之后，会释放 mutex lock，返回之前会重新获取 mutex lock. 我们希望 `bstate.nthread` 记录阻塞的线程的数量仅在一个 thread 中递增一次，而不是多次增加。在所有线程到达之后，重置 `bstate.nthread` ，递增 `bstate.round` 表示一次回合。

首先我们需要使用 mutex lock 保护 `bstate.nthread` 的递增。因为如何不保护它，那么在多线程中会导致其结果最后不正确。如果其本来是 0，此时刚好计算到 `bstate.nthread + 1` 结束，结果是 1，但是还没有被赋值给 `bstate.nthread`，但是此时另一个线程刚好在此之前完成了对 `bstate.nthread` 的递增和赋值，此时 `bstate.nthread` = 1，然后回到刚才线程，执行赋值操作 `bstate.nthread = 1` 最终， `bstate.nthread` 的值为 1，显然是错误的，应该是 2.

```cpp
static void 
barrier()
{
  pthread_mutex_lock(&bstate.barrier_mutex); // Lock the mutex

  // Increment the number of threads that have reached this round
  bstate.nthread++;

  // If not all threads have reached the barrier, wait
  if (bstate.nthread < nthread) {
      pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }

  // If all threads have reached the barrier, increment the round and reset the counter
  if (bstate.nthread == nthread) {
      bstate.round++;
      bstate.nthread = 0;
      pthread_cond_broadcast(&bstate.barrier_cond); // Notify all waiting threads
  }

  pthread_mutex_unlock(&bstate.barrier_mutex); // Unlock the mutex
}
```

详细的代码改动见 [GitHub Commit](https://github.com/relaxcn/xv6-labs-2022-solutions/commit/098ee563a1f34897b0667981277adf30a743fa21).