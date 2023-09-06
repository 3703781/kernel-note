# list

这个目录里的文件由linux 5.15内核中的链表数据结构的相关代码组成，经过一点点删除操作后，文件对应关系：

|  kernel    |   文件   |
| ---- | ---- |
|   include/linux/build_bug.h   |   build_bug.h   |
|   include/linux/compiler_types.h   |   compiler_types.h   |
|   include/linux/compiler_attributes.h   |   compiler_attributes.h   |
|   include/linux/stddef.h<br>include/uapi/linux/stddef.h   | kernel_stddef.h |
| include/linux/kernel.h | kernel.h |
| include/linux/list.h | list.h |
| include/asm-generic/rwonce.h | rwonce.h |
| include/linux/types.h | types.h |


内核里的list数据结构为双向循环列表，节点在*types.h*里定义如下：
```c
struct list_head {
	struct list_head *next, *prev;
};
```
节点定义里没有任何指向实际数据的指针，因此它不是把数据结构塞入链表。而是自定义数据结构后，把链表塞入数据结构，比如在测试文件*list_test.c*中定义*my_node*：
```c
struct my_node
{
    const char *data;
    long *define_whatever_types_of_member_you_need;
    int define_as_many_members_as_you_need;
    struct list_head head;
};
```
在正常的数据结构塞入链表的实现中，用链表节点就能找到指向这个数据结构的成员变量。对于编译器来说，也就是链表节点所在地址加上一个固定的偏移量，然后用指向数据结构的指针类型来解释这个偏移后的地址。

内核的实现是相反的，所以要解决这样一个问题：通过*head*成员怎么找到自定义的数据结构？ 前面说过*head*成员距离所在结构体*my_node*首地址的偏移是固定的，将*head*所在地址减去这个固定的偏移就能得到自定义的数据结构：

```c
(struct my_node *)(my_node_ptr - __builtin_offsetof(struct my_node, head))
```

其中`__builtin_offsetof`为*gnu c extension*的内置扩展，编译期确定两个对象的地址偏移：https://gcc.gnu.org/onlinedocs/gcc/Offsetof.html

*kernel.h*用这个功能实现了`container_of`:

```c
/* cast a member of a structure out to the containing structure */
#define container_of(ptr, type, member)
```

- ptr - the pointer to the member.
- type - the type of the container struct this is embedded in.
- member - the name of the member within the struct.

## 内核API

头文件：*list.h*

### 初始化
有不少初始化的方式，所谓初始化，就是搞出一个自己指向自己head，
比如

1.  利用`LIST_HEAD_INIT`静态初始化
    ```c
    struct my_node node1 = {
        .data = "node1",
        .head = LIST_HEAD_INIT(node1.head)};
    ```
2.  利用`INIT_LIST_HEAD`动态初始化
    ```c
    struct my_node node2 = {
        .data = "node2"};
    INIT_LIST_HEAD(&node2.head);
    ```

3.  还可以直接利用`LIST_HEAD`生成一个新的head
    ```c
    LIST_HEAD(head3);
    struct my_node node3 = {
        .data = "node3",
        .head = head3};
    ```

>用于后面演示，初始化node4和5
>struct my_node node4 = {
>    .data = "node4",
>    .head = LIST_HEAD_INIT(node4.head)};
>struct my_node node5 = {
>    .data = "node5",
>    .head = LIST_HEAD_INIT(node5.head)};


### 添加节点

两个插入节点到链表的方法，区别在于插入的位置。

把*node3*添加为*node1*的下一个：

```c
list_add(&node3.head, &node1.head);
```
把*node2*添加为*node3*上一个，即插入到*node1*和*node3*之间
```c
list_add_tail(&node2.head, &node3.head);
```
用于后面演示，把*node4*添加为*node3*的下一个，把*node5*添加为*node4*的下一个

### 查询my_node对象
*list.h*里定义了`list_entry`宏，本质上就是前面提到的`container_of`
所以通过这个宏可以直接获取*head*指向的自定义数据结构对象：

```c
struct my_node *node_ptr = list_entry(&node2.head, struct my_node, head);
```
打印*node_ptr*和*&node2*，确认两者都指向同样的地址
> node_ptr=0x7ffd68d48ef0, &node2=0x7ffd68d48ef0

这个宏还有一些变体，区别在于获取给定节点的next还是prev：
1. 获取以*node2*为首的链表的最后一个node
    ```c
    node_ptr = list_last_entry(&node2.head, struct my_node, head);
    ```
    打印*node_ptr->data*
    > node1
2.  获取以*node2*为首的链表中除了*node2*外的第一个node
    ```c
    node_ptr = list_first_entry(&node2.head, struct my_node, head);
    ```
    打印*node_ptr->data*
    > node3
3. 获取以*node2*的下一个node
    ```c
    node_ptr = list_next_entry(&node2, head);
    ```
    打印*node_ptr->data*
    > node3
4. 获取以*node2*的上一个node
    ```c
    node_ptr = list_prev_entry(&node2, head);
    ```
    打印*node_ptr->data*
    > node1
    
### 遍历
> 如果要在遍历时删除节点，用带*safe*字样的`list_for_each_xxx`宏

1. 用`list_for_each`遍历整个链表，从*node1*开始，不包括*node1*<br>反向遍历用list_for_each_**reverse**

    ```c
        struct list_head *p;
        list_for_each(p, &node1.head)
        {
        	printf(" > %s", list_entry(p, struct my_node, head)->data);
        }
        printf("\t%d\r\n", __LINE__);
    ```
    输出： 
    >  \> node2 > node3 > node4 > node5
    
2. 用`list_for_each_entry`遍历整个链表，省去了手动使用`list_entry`宏来获取*my_node*对象，写法更简单<br>从*node1*开始，不包括*node1*<br>反向遍历用list_for_each_entry_**reverse**

    ```c
    struct my_node *n;
    list_for_each_entry(n, &node1.head, head)
    {
        printf(" > %s", n->data);
    }
    printf("\t%d\r\n", __LINE__);
    ```
    输出： 
    >   \> node2 > node3 > node4 > node5


3. 用`list_for_each_continue`从给定*p*开始遍历到*node4*，开区间<br>反向遍历用list_for_each_continue_**reverse**

    ```c
    p = &node1.head;
    list_for_each_continue(p, &node4.head)
    {
        printf(" > %s", list_entry(p, struct my_node, head)->data);
    }
    printf("\t%d\r\n", __LINE__);
    ```
    输出： 
    >   \ > node2 > node3


4. `list_for_each_entry_continue`省去了手动使用`list_entry`宏来获取*my_node*对象，写法更简单<br>从已有n开始遍历到node4，开区间<br>反向遍历用list_for_each_entry_continue_**reverse**

    ```c
    n = &node1;
    list_for_each_entry_continue(n, &node4.head, head)
    {
        printf(" > %s", n->data);
    }
    printf("\t%d\r\n", __LINE__);
    ```
    输出： 
    >   \> node2 > node3
    
5. `list_for_each_entry_from`从给定自定义数据结构*n*开始遍历到*node3*，左闭右开<br>反向遍历用list_for_each_entry_from_**reverse**

    ```c
    n = &node1;
    list_for_each_entry_from(n, &node3.head, head)
    {
        printf("%s > ", n->data);
    }
    printf("\t%d\r\n", __LINE__);
    ```
    输出： 
    >   node1 > node2 > 

### 删除节点

有两个删除宏，区别在于删除后对被删除对象的操作
> 如果要在遍历时删除节点，用带safe字样的list_for_each_xxx宏

从链表中删除*node3*
```c
list_del(&node3.head);
```

从链表中删除*node2*，并把*node2*重新初始化
```c
list_del_init(&node2.head);
```
上面两个删除操作前后变化如下：
> 删除前：node1 > node2 > node3 > node4 > node5
> 删除后：node1 > node4 > node5

### 比较和测试

判断*node2*和*node3*是否为的空列表，所谓空列表，就是next和prev都指向自己
```c
printf("%s\t%d\r\n", list_empty(&node2.head) ? "node2 empty" : "node2 not empty", __LINE__);
printf("%s\t%d\r\n", list_empty(&node3.head) ? "node3 empty" : "node3 not empty", __LINE__);
```
输出：
> node2 empty
> node3 not empty

判断*node4*是不是以*node1*为起始的循环链表中的第一项，即比较两个*head*是否为同一个
```c
printf("%s\t%d\r\n", list_is_head(&node4.head, &node1.head) ? "same" : "not same", __LINE__);
printf("%s\t%d\r\n", list_entry_is_head((&node4), &node1.head, head) ? "same" : "not same", __LINE__);
```
输出：
> not same
> not same

判断*node1*是不是以*node5*为起始的循环链表中的第二项，由于兼顾到*hlist*的语义，宏中的*first*字样表达的是以给定*head*起始的链表中除了起始项之外的第一项
```c
printf("%s\t%d\r\n", list_is_first(&node1.head, &node5.head) ? "the first entry after head" : "not the first after head", __LINE__);
```
输出：
> the first entry after head

判断*node5*是不是以*node1*为起始的循环链表中的最后一项，其实就是比较*node1.head*的*prev*
```c
printf("%s\t%d\r\n", list_is_last(&node5.head, &node1.head) ? "last" : "not last", __LINE__);
```
输出：
> last

判断*node2*链表除了*head*之外是否只有一项，即链表是否具有两项
```c
printf("%s\t%d\r\n", list_is_singular(&node2.head) ? "singular" : "not singular", __LINE__);
```
判断前链表结构是 node2 > node3
输出：
> singular

### 移动节点

有3个移动宏，其中两个的区别在于插入位置不同。另一个区别在于操作的对象是一个区间而不是一个节点

将*node1*从所在链表中移除，插入到*node2*的**后面**，*node1*所在链表和*node2*所在链表可以是同一个链表，也可以是不同链表
```c
list_move(&node1.head, &node2.head);
```

将*node1*从所在链表中移除，插入到*node4*的**前面**，*node1*所在链表和*node4*所在链表可以是同一个链表，也可以是不同链表
```c
list_move_tail(&node1.head, &node4.head);
```

将`node4`到`node5`的闭区间移动到`node3`之前， 可以发生在同一个链表内，也可以是不同链表
```c
list_bulk_move_tail(&node3.head, &node4.head, &node5.head);
```

### 合并和切分链表

`list_splice`合并
合并前两个链表如下：
node1 > node4 > node5 > node7
node2 > node3 > node6

```c
list_splice(&node4.head, &node3.head);
```

- 第一个列表从*node4*的**前面和后面**分别被打断，*node4*被删除<br>*node1*和*node5*所在的部分形成不循环链表node5 > node7 > node1
- 第二个列表从*node3* **后面**打断变成不循环链表node6 > node2 > node3
- `list_splice`把node5 > node7 > node1 和 node6 > node2 > node3 组成一个循环链表：
  ```
  node5 > node7 > node1
    ^               v
  node3 < node2 < node6
  ```
  并且*node4*从原链表中脱离，如果合并的同时要对*node4*进行重新初始化，就用list_splice_**init**

`list_splice_tail`合并
合并前两个链表如下：
node1 > node4 > node5 > node7
node2 > node3 > node6

```c
list_splice_tail(&node4.head, &node3.head);
```

- 第一个列表从*node4* **前面和后面**分别被打断，*node4*被删除<br>*node1*和*node5*形成不循环链表node5 > node7 > node1
- 第二个列表从*node3* **前面**打断变成不循环链表node3 > node6 > node2
- `list_splice_tail`把node5 > node7 > node1 和 node3 > node6 > node2 组成一个循环链表：
   ```
   node5 > node7 > node1
    ^               v
   node2 < node6 < node3
   ```
   并且*node4*从原链表中脱离，如果合并的同时要对*node4*进行重新初始化，就用list_splice_tail_**init**

`list_cut_before`分割
循环链表node5 > node7 > node1 > node3 > node6 > node2

```c
list_cut_before(&node4.head, &node2.head, &node3.head);
```

- 在*node2*后打断，在*node3* **前**打断，形成循环列表node2 > node3 > node6和不循环链表node5 > node7 > node1
- 在node5和node1之间插入node4，形成第二个循环链表node1 > node4 > node5 > node7

总结来说，*node3*到*node2*的**闭区间**范围为切分出来的链表1，将原列表中的链表1部分替换为*node4*形成切分后的链表2

`list_cut_position`分割
循环链表node5 > node7 > node1 > node3 > node6 > node2

```c
list_cut_position(&node4.head, &node2.head, &node1.head);
```

- 在*node2*后打断，在*node1* **后**打断，形成第一个循环列表node2 > node3 > node6和不循环链表node5 > node7 > node1
- 在*node5*和*node1*之间插入*node4*，形成第二个循环链表node1 > node4 > node5 > node7

其实`list_cut_position`是将*node1*到*node2*的**左开右闭区**间范围为切分出来的链表1，将原列表中的链表1部分替换为node4形成切分后的链表2

## 链表的RCU支持
> rcu功能依赖linux内核，移植到用户空间是个大工程，所以这里没有保留rcu，如果要在用户空间用rcu，看这个https://liburcu.org/

内核的*include/linux/rculist.h*提供了利用rcu机制对链表进行增删查改操作的接口，下面的定义和非rcu方式接近。在用这些列表功能的时候，记得要包围在rcu临界区里。

需要说明的是，`list_splice_init_rcu`中参数*sync*应设置为*writer*发布*list*参数初始化完成消息的同步函数，也就是调用这个函数来开始list初始话操作后的GP时段。对于`list_splice_tail_init_rcu`的参数*sync*，也是一个道理。

还需要说明的是，`list_for_each_entry_rcu`的*cond*，咋查不到资料呢

```c
static inline void list_add_rcu(struct list_head *new, struct list_head *head);
static inline void list_add_tail_rcu(struct list_head *new, struct list_head *head);
static inline void list_del_rcu(struct list_head *entry);
static inline void list_replace_rcu(struct list_head *old, struct list_head *new);
static inline void list_splice_init_rcu(struct list_head *list, struct list_head *head, void (*sync)(void));
static inline void list_splice_tail_init_rcu(struct list_head *list, struct list_head *head, void (*sync)(void));

#define list_next_rcu(list)
#define list_tail_rcu(head)	
#define list_entry_rcu(ptr, type, member)
#define list_for_each_entry_rcu(pos, head, member, cond...)
#define list_for_each_entry_from_rcu(pos, head, member)
```

在*include/linux/list.h*里，竟然也提供了1个支持rcu的用于遍历的宏。

```c
#define list_for_each_rcu(pos, head)
```

