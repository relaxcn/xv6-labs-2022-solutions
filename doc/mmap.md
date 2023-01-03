# mmap

在本次实验中我们要求实现 `mmap` 和 `munmap` 系统调用，在实现之前，我们首先需要了解一下 `mmap` 系统调用是做什么的。根据 `mmap` 的描述，`mmap` 是用来将文件或设备内容映射到内存的。`mmap` 使用懒加载方法，因为需要读取的文件内容大小很可能要比可使用的物理内存要大，当用户访问页面会造成页错误，此时会产生异常，此时程序跳转到内核态由内核态为错误的页面读入文件并返回用户态继续执行。当文件不再需要的时候需要调用 `munmap` 解除映射，如果存在对应的标志位的话，还需要进行文件写回操作。`mmap` 可以由用户态直接访问文件或者设备的内容而不需要内核态与用户态进行拷贝数据，极大提高了 IO 的性能。

接下来，我们来研究一下实现，首先我们需要一个结构体用来保存 mmap 的映射关系，也就是文档中的映射关系，用于在产生异常的时候映射与解除映射，我们添加了 `virtual_memory_area` 这个结构体:

```c
// 记录 mmap 信息的结构体
struct virtual_memory_area {
    // 映射虚拟内存起始地址
    uint64 start_addr;
    // 映射虚拟内存结束地址
    uint64 end_addr;
    // 映射长度
    uint64 length;
    // 特权
    int prot;
    // 标志位
    int flags;
    // 文件描述符
    // int fd;
    struct file* file;
    // 文件偏移量
    uint64 offset;
};
```

每个进程都需要有这样一个结构体用于记录信息，因此我们也需要在 `proc` 中添加这个结构体，由于其属于进程的私有域，所以不需要加锁访问。之后我们就将要去实现 `mmap` 系统调用:

```c
uint64 sys_mmap(void) {
  uint64 addr, length, offset;
  int prot, flags, fd;
  argaddr(0, &addr);
  argaddr(1, &length);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argaddr(5, &offset);
  uint64 ret;
  if((ret = (uint64)mmap((void*)addr, length, prot, flags, fd, offset)) < 0){
    printf("[Kernel] fail to mmap.\n");
    return -1;
  }
  return ret;
}

void* mmap(void* addr, uint64 length, int prot, int flags, int fd, uint64 offset) {
  // 此时应当从该进程中发现一块未被使用的虚拟内存空间
  uint64 start_addr = find_unallocated_area(length);
  if(start_addr == 0){
    printf("[Kernel] mmap: can't find unallocated area");
    return (void*)-1;
  }
  // 构造 mm_area
  struct virtual_memory_area m;
  m.start_addr = start_addr;
  m.end_addr = start_addr + length;
  m.length = length;
  m.file = myproc()->ofile[fd];
  m.flags = flags;
  m.prot = prot;
  m.offset = offset;
  // 检查权限位是否满足映射要求
  if((m.prot & PROT_WRITE) && (m.flags == MAP_SHARED) && (!m.file->writable)){
    printf("[Kernel] mmap: prot is invalid.\n");
    return (void*)-1;
  }
  // 增加文件的引用
  struct file* f = myproc()->ofile[fd];
  filedup(f);
  // 将 mm_area 放入结构体中
  if(push_mm_area(m) == -1){
    printf("[Kernel] mmap: fail to push memory area.\n");
    return (void*)-1;
  }
  return (void*)start_addr;
}
```

在实现中我们首先从虚拟内存域中找到一块可用的内存，然后不实际分配内存而只是构造 `virtual_memory_area` 结构体并将其存到进程的 `mm_area` 中去，然后增加文件引用并返回。如果用户程序访问这段内存，我们的操作系统将会触发异常并调用 `map_file` 来进行处理：

```c
int map_file(uint64 addr) {
    struct proc* p = myproc();
    struct virtual_memory_area* mm_area = find_area(addr);
    if(mm_area == 0){
      printf("[Kernel] map_file: fail to find mm_area.\n");
      return -1;
    }
    // 从文件中读一页的地址并映射到页中
    char* page = kalloc();
    // 将页初始化成0
    memset(page, 0, PGSIZE);
    if(page == 0){
      printf("[Kernel] map_file: fail to alloc kernel page.\n");
      return -1;
    }
    int flags = PTE_U;
    if(mm_area->prot & PROT_READ){
      flags |= PTE_R;
    }
    if(mm_area->prot & PROT_WRITE){
      flags |= PTE_W;
    }
    if(mappages(p->pagetable, addr, PGSIZE, (uint64)page, flags) != 0) {
      printf("[Kenrel] map_file: map page fail");
      return -1;
    }

    struct file* f = mm_area->file;
    if(f->type == FD_INODE){
      ilock(f->ip);
      // printf("[Kernel] map_file: start_addr: %p, addr: %p\n", mm_area->start_addr, addr);
      uint64 offset = addr - mm_area->start_addr;
      if(readi(f->ip, 1, addr, offset, PGSIZE) == -1){
        printf("[Kernel] map_file: fail to read file.\n");
        iunlock(f->ip);
        return -1;
      }
      // mm_area->offset += PGSIZE;
      iunlock(f->ip);
      return 0;
    }
    return -1;
}
```

在此函数中我们检查权限位并实际分配物理内存并进行映射，随后将文件内容读入到内存中来。当用户态不再需要文件内容的时候就会调用 `munmap` 进行取消映射:

```c
uint64 sys_munmap(void){
  uint64 addr, length;
  argaddr(0, &addr);
  argaddr(1, &length);
  uint64 ret;
  if((ret = munmap((void*)addr, length)) < 0){
    return -1;
  }
  return ret;
}

int munmap(void* addr, uint64 length){
  // 找到地址对应的区域
  struct virtual_memory_area* mm_area = find_area((uint64)addr);
  // 根据地址进行切割，暂时进行简单地考虑
  uint64 start_addr = PGROUNDDOWN((uint64)addr);
  uint64 end_addr = PGROUNDUP((uint64)addr + length);
  if(end_addr == mm_area->end_addr && start_addr == mm_area->start_addr){
    struct file* f = mm_area->file;
    if(mm_area->flags == MAP_SHARED && mm_area->prot & PROT_WRITE){
      // 将内存区域写回文件
      printf("[Kernel] start_addr: %p, length: 0x%x\n", mm_area->start_addr, mm_area->length);
      if(filewrite(f, mm_area->start_addr, mm_area->length) < 0){
        printf("[Kernel] munmap: fail to write back file.\n");
        return -1;
      }
    }
    // 对虚拟内存解除映射并释放
    for(int i = 0; i < mm_area->length / PGSIZE; i++){
      uint64 addr = mm_area->start_addr + i * PGSIZE;
      uint64 pa = walkaddr(myproc()->pagetable, addr);
      if(pa != 0){
        uvmunmap(myproc()->pagetable, addr, PGSIZE, 1);
      }
    }
    // 减去文件引用
    fileclose(f);

    // 将内存区域从表中删除
    if(rm_area(mm_area) < 0){
      printf("[Kernel] munmap: fail to remove memory area from table.\n");
      return -1;
    }
    return 0;
  }else if(end_addr <= mm_area->end_addr && start_addr >= mm_area->start_addr){
    // 此时表示该区域只是一个子区域
    struct file* f = mm_area->file;
    if(mm_area->flags == MAP_SHARED && mm_area->prot & PROT_WRITE){
      // 写回文件
      // 获取偏移量
      uint offset = start_addr - mm_area->start_addr;
      uint len = end_addr - start_addr;
      if(f->type == FD_INODE){
        begin_op(f->ip->dev);
        ilock(f->ip);
        if(writei(f->ip, 1, start_addr, offset, len) < 0){
          printf("[Kernel] munmap: fail to write back file.\n");
          iunlock(f->ip);
          end_op(f->ip->dev);
          return -1;
        }
        iunlock(f->ip);
        end_op(f->ip->dev);
      }
    }
    // 对虚拟内存解除映射并释放
    uint64 len = end_addr - start_addr;
    for(int i = 0; i < len / PGSIZE; i++){
      uint64 addr = start_addr + i * PGSIZE;
      uint64 pa = walkaddr(myproc()->pagetable, addr);
      if(pa != 0){
        uvmunmap(myproc()->pagetable, addr, PGSIZE, 1);
      }
    }
    // 修改 mm_area 结构体
    if(start_addr == mm_area->start_addr) {
      mm_area->offset = end_addr - mm_area->start_addr;
      mm_area->start_addr = end_addr;
      mm_area->length = mm_area->end_addr - mm_area->start_addr;
    }else if(end_addr == mm_area->end_addr){
      mm_area->end_addr = start_addr;
      mm_area->length = mm_area->end_addr - mm_area->start_addr;
    }else{
      // 此时需要进行分块
      panic("[Kernel] munmap: no implement!\n");
    }
    return 0;
  }else if(end_addr > mm_area->end_addr){
    panic("[Kernel] munmap: out of current range.\n");
  }else{
    panic("[Kernel] munmap: unresolved solution.\n");
  }
}
```

在 `munmap` 中我们判断是否写回页面并取消映射，减少文件引用。注意这里需要根据系统调用的地址和长度来判断不同的取消映射方式，一共有三种情况：

- 要取消映射区间的一个端点或者两个端点与内存区域重合
- 要取消映射区间的两个端点都在内存区域范围内
- 要取消映射区间的一个端点或者两个端点在内存区域外

这里由于测试用例没给太多我就没有全部实现，不过实现起来应该也不难。

在最后我们需要在 `exit` 的时候对所有内存取消映射和对文件减少引用；在 `fork` 的时候子进程需要从父进程这里拿到 `mm_area` 并对文件增加引用:

```c
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  // 释放所有 mmap 区域的内存
  for(int i = 0; i < MM_SIZE; i++){
    if(p->mm_area[i].start_addr != 0){
      munmap((void*)p->mm_area[i].start_addr, p->mm_area[i].length);
    }
  }

  begin_op(ROOTDEV);
  iput(p->cwd);
  end_op(ROOTDEV);
  p->cwd = 0;

  // we might re-parent a child to init. we can't be precise about
  // waking up init, since we can't acquire its lock once we've
  // acquired any other proc lock. so wake up init whether that's
  // necessary or not. init may miss this wakeup, but that seems
  // harmless.
  acquire(&initproc->lock);
  wakeup1(initproc);
  release(&initproc->lock);

  // grab a copy of p->parent, to ensure that we unlock the same
  // parent we locked. in case our parent gives us away to init while
  // we're waiting for the parent lock. we may then race with an
  // exiting parent, but the result will be a harmless spurious wakeup
  // to a dead or wrong process; proc structs are never re-allocated
  // as anything else.
  acquire(&p->lock);
  struct proc *original_parent = p->parent;
  release(&p->lock);

  // we need the parent's lock in order to wake it up from wait().
  // the parent-then-child rule says we have to lock it first.
  acquire(&original_parent->lock);

  acquire(&p->lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup1(original_parent);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&original_parent->lock);


  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  np->parent = p;

  // copy saved user registers.
  *(np->tf) = *(p->tf);

  // Cause fork to return 0 in the child.
  np->tf->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  // 复制 mmap 结构体
  for(int i = 0; i < MM_SIZE; i++){
    if(p->mm_area[i].start_addr != 0){
      np->mm_area[i] = p->mm_area[i];
      // 增加文件引用
      filedup(p->mm_area[i].file);
    }
  }


  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  np->state = RUNNABLE;

  release(&np->lock);

  return pid;
}
```
