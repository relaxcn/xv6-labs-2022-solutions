# xv6-labs-2022-solutions

本分支是 [Lab: page tables](https://pdos.csail.mit.edu/6.828/2022/labs/pgtbl.html) 的代码。

## Speed up system calls 代码改动

见 [commit](https://github.com/relaxcn/xv6-labs-2022-solutions/commit/a4609f4237e864ce3c5085c8d4be4b3d00d637d8)

> 注：此 commit 的 `PTE_R | PTE_U | PTE_W` 错误，应该改为 `PTE_R | PTE_U`。

## Print a page table 代码改动

见 [commit](https://github.com/relaxcn/xv6-labs-2022-solutions/commit/3343757349ffeff0c1d49a605bb4b6561a8d8ac5)

## Detect which pages have been accessed 代码改动

见 [commit](https://github.com/relaxcn/xv6-labs-2022-solutions/commit/4489995876d00a8b4de22b7e06d93daef25d09e5)