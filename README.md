The seL4 microkernel for x86_UINTR
====================

# 新增特性

## 新增系统调用

针对x86架构的用户态中断，增加下列系统调用：

- seL4_uintr_register_handler()。注册为用户态中断处理函数的接收方。
- seL4_uintr_unregister_handler()。销毁用户态中断处理函数。
- seL4_uintr_vector_fd()。创建一个用户态中断的文件描述符。
- seL4_uintr_register_sender()。注册为进程间用户态中断的发送方。
- seL4_uintr_unregister_sender()。销毁发送方。
- seL4_uintr_wait()。暂无实现。
- seL4_uintr_register_self()。暂无实现。
- seL4_uintr_alt_stack()。暂无实现。
- seL4_uintr_ipi_fd()。暂无实现。

## 由sel4的特性引起的使用注意事项

因为sel4不提供使用页表的虚拟内存管理，其在内核中是使用pptr进行操作（pptr = paddr + OFFSET），pptr本质上是仅加了一个偏移量后映射到最高位的物理地址，因此本质上在内核中的所有地址操作都是对真实物理地址进行操作。

但用户程序会要求使用经过页表映射而非简单的偏移映射的虚拟地址，这在sel4的内核中是无法实现的，但是用户态中断的处理流程中需要CPU读取内存中的相应数据结构的信息，向对应地址写入标志位以触发用户态的中断（因为用户态中断本身要求不经过委托给内核处理，因此需要CPU自行处理相关内存的读写，同时需要在CPL = 3的要求也使得必须使用页表映射的虚拟地址）。

因此与linux不需要关心地址不同，sel4要求用户程序自行创建页帧和页表映射关系，并在发起系统调用时将对应的页的物理地址传入内核，由内核代为处理此页。同时为了CPU能够在用户态下访问，此页帧的虚拟地址也需要传入给内核。

值得一提的是，如果sel4即将运行在一个不使用页表寻址，而是默认所有地址为物理地址的CPU上，那么只需要让传入的虚拟地址等于物理地址即可。

## 详细说明与示例

详细请见Intel手册与Intel的linux示例内核：https://github.com/intel/uintr-linux-kernel

sel4的测试用例见含有UINTR用例的sel4test: projects/sel4test/apps/sel4test-tests/src/tests/uintr.c

## seL4_uintr_register_handler

输入参数：
- seL4_Uint64 handler_address, 处理函数的地址。
- seL4_Uint32 flags, 标志位，目前做保留以供未来拓展，目前必须为0
- seL4_Uint64* addr, 长度应为2的数组，保存：0:paddr, 1:vaddr

## seL4_uintr_unregister_handler

输入参数：
- seL4_Uint32 flags, 标志位，目前做保留以供未来拓展，目前必须为0

## seL4_uintr_vector_fd

输入参数：
- seL4_Uint64 vector, 需要在接受方处理向量中，为哪一个索引创建fd（fd需大于等于0，小于64）。
- seL4_Uint32 flags, 标志位，目前做保留以供未来拓展，目前必须为0

返回值：
- seL4_Int64 fd, 返回的fd值。

需要说明的是，sel4中不含有文件系统相关的定义和实现，系统调用中的fd为宏内核迁移到sel4的残留命名方式，其不支持文件系统对fd的操作。
此处返回的fd仅仅是一个token，不含有任何能力。

## seL4_uintr_register_sender

输入参数：
- seL4_Int32 uvec_fd, 需要传入的fd token，以此 fd 来定位和区分发送后哪一个进程会收到用户态中断。
- seL4_Uint32 flags, 标志位，目前做保留以供未来拓展，目前必须为0
- seL4_Uint64* addr, 长度应为3的数组，保存：0:vaddr1, 1:paddr2, 2:vaddr2

此处需要做说明，对发送方需要做两次页表映射，分别称为paddr1 & vaddr1与paddr2 & vaddr2。其中 paddr1 需与接收方的 paddr 相同（可通过复制页帧能力进行重新映射），vaddr1 无需相同。paddr2 与 vaddr2 为发送方所特有的页帧，无要求。

返回值：
- seL4_Int64 index, 返回的index值，用于发送用户态中断：'SENDUIPI  <uipi_index>'。一个程序最多接受256个发送程序对其注册，index值即代表在此256个值中为第几个。

## unregister_sender

输入参数：
- seL4_Uint32 flags, 标志位，目前做保留以供未来拓展，目前必须为0

# 已知问题与BUG

## 多层函数嵌套后再进行系统调用导致在栈帧RSP错误时触发UINTR

目前在单核情况下，也就是接收线程需要阻塞后让出调度去执行发送线程，此时用户态中断不会触发，会通过puir位置1来遗留一个中断，在接收线程即将进入用户态时（有点类似于信号的处理时机），会检测其puir位，如果不为0，说明有一个用户态中断被挂起了，需要重新触发一个中断。

这种情况下，需要使用apic给自己发一个中断，代码为 `apic_send_ipi_core(UINTR_NOTIFICATION_VECTOR, cur_cpu);` 。这个中断会在硬件的执行过程中（在陷入内核态触发中断进入内核代码之前），提前检测并拦截此中断号，进入用户态中断处理流程。

而此过程的跳转到中断处理函数是通过函数调用的方式完成的（也就是使用RSP压栈出栈），因此需要严格保证栈顶的正确性，否则会出现写入异常（VM FAULT）。

这里经过验证，初步验证结论为，正确的栈顶应该是：在定义中断处理函数的文件中（也就是定义`static void __attribute__((interrupt)) uintr_handler(){}`的文件），pc指针必须位于此文件下的任意函数中（说人话就是必须完成系统调用的所有函数嵌套返回），此时的RSP指针是正确的。

但是在sel4中，大概率会发生以下情况导致出现异常：

Step    cpu 0 (receiver task)           cpu 0 (sender task)
----    ---------------------           -------------------
1       task is running
2       function call1
3       function call2
4       block and schedule
5                                       task is running
6                                       executes SENDUIPI
7                                       IPI sent
8                                       task is end, schedule
9       waken up, ret to user
10      function call2 return
11      IPI delivered
12      go to uintr_handler

这样会在function call1里执行用户态中断处理，导致RSP错误。

正确的预期行为是：

Step    cpu 0 (receiver task)           cpu 0 (sender task)
----    ---------------------           -------------------
1       task is running
2       function call1
3       function call2
4       block and schedule
5                                       task is running
6                                       executes SENDUIPI
7                                       IPI sent
8                                       task is end, schedule
9       waken up, ret to user
10      function call2 return
11      function call1 return   <================
12      IPI delivered
13      go to uintr_handler

全部函数嵌套返回后IPI被发送过来，此时是正确的。

> 经过验证，如果你的电脑性能比较好，qemu执行的快，那么大概率你会碰到异常情况，但是如果你的电脑比较卡，那么有一定的概率能执行成功（不会碰到异常）。不改变任何代码和环境，只是重复执行qemu run。

> 此问题目前无解，这是硬件的问题。