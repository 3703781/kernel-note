# kfifo

这个目录里的文件由linux 5.15内核中的链表数据结构的相关代码组成，经过一点点删除操作后，文件大致对应关系：

| kernel                              | 文件                  |
| ----------------------------------- | --------------------- |
| include/linux/bitops.h              | bitops.h              |
| include/linux/const.h               | const.h               |
| include/linux/compiler_attributes.h | compiler_attributes.h |
| include/linux/log2.h                | log2.h                |
| include/linux/kfifo.h               | kfifo.h               |
| lib/kfifo.c                         | kfifo.c               |
| include/linux/minmax.h              | minmax.h              |

内核的*kfifo*实现了一个循环队列，它利用*in*和*out*指针来标记放入和读出的位置。

 - *out*不会大于*in*
 - 每放入*n*个元素：`in += n`
 - 每读出*k*个元素：`out += k`

*kfifo*的空间只能是2的幂，所以*in*和*out*两个地址只要和*mask*(*kfifo*的总元素或记录个数-1)计算位与，就能被约束在队列的地址空间内，形成循环队列的结构

*kfifo*接受的最小数据单元是元素，元素可以是简单的基本类型，也可以是自定义的数据结构。*kfifo*分为记录型和非记录型，划分依据是成员*recsize*的值

- 非记录型接受定长的元素，*recsize*为0
- 记录型则支持多个元素组成的不定长记录。所谓不定长的记录，可以是结构体，也可以是数组等等，*recsize*为1或者2

> recsize的含义：如果用一个变量来表示一条记录包含的元素个数，这个变量自身所占的字节数就是recsize

由于记录型*kfifo*的*recsize*为1或者2，所以自然分为两种记录型*kfifo*，一种为`kfifo_rec_ptr_1`（或者用`STRUCT_KFIFO_REC_1`来定义），*recsize*为1，即支持最大255字节长度的记录

另一种为`kfifo_rec_ptr_2`（或者用`STRUCT_KFIFO_REC_2`来定义），*recsize*为2，即支持最大65535字节长度的记录

那么*kfifo*是怎么实现不定长元素的：调用用于写入的函数`kfifo_put()`、`kfifo_in()`等的时候会判断*recsize*的值，确定是记录型*kfifo*后，会首先写入记录的字节数（小端序），然后再写入元素本身。在使用`kfifo_get()`、`kfifo_out()`或者`kfifo_peek()`等读出元素时，先根据*recsize*读取记录的字节数，然后再根据长度读出元素

需要注意的是，*recsize*在初始化一个*kfifo*的时候设定，而一个*kfifo*中每条记录的长度是可以不一样的，要理清两者的关系



## 非记录型kfifo
### 初始化

动态初始化有两种方式，一种是用固定元素类型为`unsigned char`的*kfifo*类型，另一种是用`DECLARE_KFIFO_PTR`来指定元素类型。动态初始化后要用`kfifo_alloc`或者`kfifo_init`的方式分配内存空间

静态初始化只需用`DEFINE_KFIFO`来定义局部或者全局变量，在定义时就已经分配好了内存空间

1. 动态初始化

   ```c
   struct kfifo fifo1;
   ```
   如果想指定元素类型，可以用`DECLARE_KFIFO_PTR`，不要被后缀PTR迷惑了，这里的PTR指的不是*fifo2*是个指针，而是指*fifo2.buf*指向未分配的内存

   ```c
   DECLARE_KFIFO_PTR(fifo2, unsigned char);
   ```

   这里用`kfifo_alloc`设定最大128个元素，不是2的幂会roundup

   ```c
   int ret = kfifo_alloc(&fifo1, 128, GFP_KERNEL);
   if (ret)
       printf("%s, %d\r\n", strerror(-ret), __LINE__);
   ```

   也可以用`kfifo_init`来指定预分配的内存空间，设定最大为128个元素，不是2的幂会roundup

   ```c
   char buf[128] = {0};
   ret = kfifo_init(&fifo2, buf, 128);
   if (ret)
       printf("%s, %d\r\n", strerror(-ret), __LINE__);
   ```

2. 静态初始化
   用`DECLARE_KFIFO`创建非记录型*kfifo*，元素类型为`my_element`的*fifo*，设置最多包含128个元素，这里**必须**是2的幂

   ```c
   DECLARE_KFIFO(fifo3, struct my_element, 128);
   INIT_KFIFO(fifo3);
   ```

   上面两句可以一步到位:

   ```c
   DEFINE_KFIFO(fifo4, struct my_element, 128);
   ```

### 写入

`kfifo_in`放入多个元素，最后一个参数*n*为元素个数，需要带自旋锁就用`kfifo_in_locked`或`kfifo_in_spinlocked`或`kfifo_in_spinlocked_noirqsave`

```c
 unsigned char b[] = {1, 2, 3};
 int in_num = kfifo_in(&fifo1, b, ARRAY_SIZE(b));
 printf("input counts: %d\r\n", in_num);
```

`kfifo_put`放入单个元素

```c
kfifo_put(&fifo1, 4);
kfifo_put(&fifo2, 4);
```

如果要从用户空间写入，直接用`kfifo_from_user`

```c
static ssize_t fifo_write(struct file *file, const char __user *buf,
                            size_t count, loff_t *ppos)
{
    int ret;
    unsigned int copied;

       if (mutex_lock_interruptible(&write_access))
           return -ERESTARTSYS;

        ret = kfifo_from_user(&test, buf, count, &copied);

        mutex_unlock(&write_access);
        if (ret)
            return ret;

        return copied;
}
```

### 查看状态

是否已经初始化
```c
printf("%s\r\n", kfifo_initialized(&fifo1) ? "initialized" : "not initialized");
```

查看单个元素的字节数
```c
 printf("element size: %d bytes\r\n", kfifo_esize(&fifo1));
```

查看*recsize*, 对于非记录型*kfifo*，返回0
```c
printf("record size: %ld bytes\r\n", kfifo_recsize(&fifo1));
```

查看最大元素个数
```c
printf("max element count: %d\r\n", kfifo_size(&fifo1));
```

查看队列中已经存储的元素个数
```c
printf("used element count: %d\r\n", kfifo_len(&fifo1));
```

查看队列中还空闲的，能够继续存储的元素个数
```c
printf("unused element count: %d\r\n", kfifo_avail(&fifo1));
```

查看队列中下一个记录的字节数，对于非记录型*kfifo*，返回已经存储的元素的总字节数
```c
printf("next record size: %d bytes\r\n", kfifo_peek_len(&fifo1));
```

是否为空队列
```c
printf("%s\r\n", kfifo_is_empty(&fifo1) ? "empty" : "not empty");
```

> 用`kfifo_is_empty_spinlocked`或`kfifo_is_empty_spinlocked_noirqsave`可以带锁查看

是否为满队列
```c
printf("%s\r\n", kfifo_is_full(&fifo1) ? "full" : "not full");
```

### 读出

`kfifo_peek`查看但不读出下一次要读出的元素，返回查看到的元素数

```c
ret = kfifo_peek(&fifo1, b);
for (int i = 0; i < ret; i++)
	printf("%d ", b[i]);
```

`kfifo_out_peek`查看但不读出*kfifo*中的元素，第三个参数指定查看的最大元素数，返回查看到的元素数

```c
ret = kfifo_out_peek(&fifo1, b, sizeof(b));
for (int i = 0; i < ret; i++)
    printf("%d ", b[i]);
```

`kfifo_skip`跳过下一次要读出的元素

```c
kfifo_skip(&fifo1);
```

`kfifo_out`读出min(指定的最大元素数, 队列中已有元素数)，返回值是读出元素个数，空队列返回0

```c
ret = kfifo_out(&fifo1, b, 100);
for (int i = 0; i < ret; i++)
    printf("%d ", b[i]);
```

`kfifo_get`读出单个元素，返回读出的元素数，空队列返回0

```c
ret = kfifo_get(&fifo2, b);
for (int i = 0; i < ret; i++)
    printf("%d ", b[i]);
```

如果要读出到用户空间，直接用`kfifo_to_user`
```c
static ssize_t fifo_read(struct file *file, char __user *buf,
                    size_t count, loff_t *ppos)
{
   int ret;
   unsigned int copied;

    if (mutex_lock_interruptible(&read_access))
        return -ERESTARTSYS;

    ret = kfifo_to_user(&test, buf, count, &copied);

    mutex_unlock(&read_access);
    if (ret)
        return ret;

    return copied;
}
```

### 释放

释放所有动态分配的空间

```c
kfifo_free(&fifo1);
```

## 记录型kfifo

### 初始化

两种不同最大记录长度的*kfifo*都支持动态和静态初始化

 - `kfifo_rec_ptr_1`或者用`STRUCT_KFIFO_REC_1`来定义的*recsize*为1，即支持最大255字节长度的记录。
 - `kfifo_rec_ptr_2`或者用`STRUCT_KFIFO_REC_2`来定义的*recsize*为2，即支持最大65535字节长度的记录。

1. 动态初始化
   `kfifo_rec_ptr_x`的元素类型固定为`unsigned char`，这里用`kfifo_alloc`设定最大128个元素（不是2的幂会roundup）
   ```c
   int ret = kfifo_alloc(&fifo1, 128, GFP_KERNEL);
   if (ret)
       printf("%s, %d\r\n", strerror(-ret), __LINE__);
   ```
   也可以用kfifo_init来指定预分配的内存空间，设定最大为128个元素，不是2的幂会roundup
   ```c
   char buf[128] = {0};
   ret = kfifo_init(&fifo2, buf, 128);
   if (ret)
       printf("%s, %d\r\n", strerror(-ret), __LINE__);
   ```

2. 静态初始化
   用`STRUCT_KFIFO_REC_1`创建最大255字节长度的记录的*kfifo*，元素类型固定为`unsigned char`，设置最多包含128个元素，不是2的幂会roundup
   ```c
   STRUCT_KFIFO_REC_1(128)
   fifo3 __maybe_unused;
   ```
   
### 写入

`kfifo_put`在记录型*kfifo*中不常用，放入单个元素形成的记录
```c
char bu = 'a';
kfifo_put(&fifo1, bu);
```

`kfifo_in`，放入单个记录，最后一个参数为记录的元素个数，需要带自旋锁就用`kfifo_in_locked`或`kfifo_in_spinlocked`或`kfifo_in_spinlocked_noirqsave`

```c
for (int i = 1; i < 10; i++)
{
    memset(buf, 'a' + i, i + 1);
    kfifo_in(&fifo1, buf, i + 1);
}
```

如果要从用户空间写入，直接用`kfifo_from_user`

```c
static ssize_t fifo_write(struct file *file, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    int ret;
    unsigned int copied;

    if (mutex_lock_interruptible(&write_access))
        return -ERESTARTSYS;

    ret = kfifo_from_user(&test, buf, count, &copied);

    mutex_unlock(&write_access);
    if (ret)
        return ret;

    return copied;
}
```

### 查看状态

是否已经初始化
```c
printf("%s\r\n", kfifo_initialized(&fifo1) ? "initialized" : "not initialized");
```

查看单个元素的字节数
```c
printf("element size: %d bytes\r\n", kfifo_esize(&fifo1));
```
查看recsize
```c
printf("record size: %ld bytes\r\n", kfifo_recsize(&fifo1));
```
查看最大元素个数
```c
printf("max element count: %d\r\n", kfifo_size(&fifo1));
```
查看队列中已经存储的元素个数
```c
printf("used element count: %d\r\n", kfifo_len(&fifo1));
```
查看队列中还空闲的，能够继续存储的元素个数
```c
printf("unused element count: %d\r\n", kfifo_avail(&fifo1));
```
查看队列中下一个记录的字节数
```c
printf("next record size: %d bytes\r\n", kfifo_peek_len(&fifo1));
```
是否为空队列，用`kfifo_is_empty_spinlocked`或`kfifo_is_empty_spinlocked_noirqsave`带锁查看
```c
printf("%s\r\n", kfifo_is_empty(&fifo1) ? "empty" : "not empty");
```
是否为满队列
```c
printf("%s\r\n", kfifo_is_full(&fifo1) ? "full" : "not full");
```

### 读出
`kfifo_peek`查看但不读出下一次要读出的记录，返回查看到的元素数
```c
char b[1000] = {0};
ret = kfifo_out_peek(&fifo1, b, sizeof(b));
b[ret] = '\0';
printf("%d elements: %s line %d\r\n", ret, b, __LINE__);
```

`kfifo_peek`在记录型*kfifo*中不常用，查看但不读出记录中的第一个元素，返回查看到的元素数
```c
ret = kfifo_peek(&fifo1, b);
printf("%d elements: %c line %d\r\n", ret, b[0], __LINE__);
```

`kfifo_skip`跳过下一次要读出的记录
```c
kfifo_skip(&fifo1);
```

`kfifo_get`在记录型*kfifo*中不常用，读出记录中的第一个元素，返回读出的元素数，空队列返回0，*out*指向下一条记录
```c
ret = kfifo_get(&fifo1, b);
printf("%d elements: %c line %d\r\n", ret, b[0], __LINE__);
```

`kfifo_out`读出一条记录，读出的元素个数为min(指定的最大元素数, 下一条记录包含的元素数)，返回值是读出元素个数，空队列返回0
```c
while (!kfifo_is_empty(&fifo1))
{
    ret = kfifo_out(&fifo1, b, sizeof(b));
    b[ret] = '\0';
    printf("%d elements: %s line %d\r\n", ret, b, __LINE__);
}
printf("%d elements: %c line %d\r\n", ret, b[0], __LINE__);
```

如果要读出到用户空间，直接用`kfifo_to_user`
```c
static ssize_t fifo_read(struct file *file, char __user *buf,
                        size_t count, loff_t *ppos)
{
    int ret;
    unsigned int copied;

    if (mutex_lock_interruptible(&read_access))
        return -ERESTARTSYS;

    ret = kfifo_to_user(&test, buf, count, &copied);

    mutex_unlock(&read_access);
    if (ret)
        return ret;

    return copied;
}
```

### 释放

释放所有动态分配的空间
```c
kfifo_free(&fifo1);
```