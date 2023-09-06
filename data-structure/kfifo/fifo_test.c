#include "kfifo.h"
#include <stdio.h>
#include <string.h>

/* 用户空间编译，魔改GFP_KERNEL */
#define GFP_KERNEL (0)

struct my_element
{
    int a;
    char *b;
    long long c;
};

/*
 * 内核的kfifo实现了一个循环队列，它利用in和out指针来标记放入和读出的位置。
 * out不会大于in，每放入n个元素或记录：in+=n，每读出k个元素或记录：out+=k
 * kfifo的空间只能是2的幂，所以in和out两个地址只要和(kfifo的总元素或记录个数-1)计算位与，就能被约束在队列的地址空间内，形成循环队列的结构
 *
 * kfifo接受的最小数据单元是元素，元素可以是简单的基本类型，也可以是自定义的数据结构。
 * kfifo分为记录型和非记录型，非记录型接受定长的元素，记录型kfifo则支持多个元素组成的不定长记录。所谓不定长的记录，可以是结构体，也可以是数组等等。
 *
 * kfifo通过recsize来确定是记录型还是非记录型，非记录型kfifo的recsize为0，记录型的recsize为1或者2。
 * 解释一下recsize：如果用一个变量来表示一条记录包含的元素个数，这个变量自身所占的字节数就是recsize。
 * 由于记录型kfifo的recsize为1或者2，所以自然分为两种记录型kfifo，一种为kfifo_rec_ptr_1（或者用STRUCT_KFIFO_REC_1来定义），recsize为1，即支持最大255字节长度的记录。
 * 另一种为kfifo_rec_ptr_2（或者用STRUCT_KFIFO_REC_2来定义），recsize为2，即支持最大65535字节长度的记录。
 *
 * 那么kfifo是怎么实现不定长元素的：调用用于写入的函数kfifo_put()、kfifo_in()等的时候会判断recsize的值，确定是记录型kfifo后，会首先写入记录的字节数（小端序），然后再写入元素本身。
 * 在使用kfifo_get()、kfifo_out()或者kfifo_peek()等读出元素时，先根据recsize读取记录的字节数，然后再根据长度读出元素。
 *
 * 需要注意的是，recsize在初始化一个kfifo的时候设定，而一个kfifo中每条记录的长度是可以不一样的
 */

/**
 * 这个函数演示了非记录型kfifo
 */
void test_nonrec(void)
{
    /*
     * 1.
     * 初始化
     * 动态初始化有两种方式，一种是用固定元素类型为unsigned char的kfifo类型，另一种是用DECLARE_KFIFO_PTR来指定元素类型
     * 动态初始化后要用kfifo_alloc或者kfifo_init的方式分配内存空间
     * 静态初始化只需用DEFINE_KFIFO来定义局部或者全局变量，在定义时就已经分配好了内存空间
     */

    /*
      动态初始化
    */
    struct kfifo fifo1;
    /* 如果想指定元素类型，可以用DECLARE_KFIFO_PTR，不要被后缀PTR迷惑了，这里的PTR指的不是fifo2是个指针，而是指fifo2.buf指向未分配的内存 */
    DECLARE_KFIFO_PTR(fifo2, unsigned char);
    /* 这里用kfifo_alloc设定最大128个元素（不是2的幂会roundup） */
    int ret = kfifo_alloc(&fifo1, 128, GFP_KERNEL);
    if (ret)
        printf("%s, %d\r\n", strerror(-ret), __LINE__);

    /* 也可以用kfifo_init来指定预分配的内存空间，设定最大为128个元素（不是2的幂会roundup）*/
    char buf[128] = {0};
    ret = kfifo_init(&fifo2, buf, 128);
    if (ret)
        printf("%s, %d\r\n", strerror(-ret), __LINE__);

    /*
      静态初始化
    */
    /* 用DECLARE_KFIFO创建非记录型kfifo，元素类型为my_element的fifo，设置最多包含128个元素，这里必须是2的幂 */
    DECLARE_KFIFO(fifo3, struct my_element, 128);
    INIT_KFIFO(fifo3);

    /* 上面两句可以一步到位 */
    DEFINE_KFIFO(fifo4, struct my_element, 128);

    /*
     * 2.
     * 写入
     */
    /* kfifo_in，放入多个元素，最后一个参数n为元素个数
       需要带自旋锁就用kfifo_in_locked或kfifo_in_spinlocked或kfifo_in_spinlocked_noirqsave */
    unsigned char b[] = {1, 2, 3};
    int in_num = kfifo_in(&fifo1, b, ARRAY_SIZE(b));
    printf("input counts: %d\r\n", in_num);

    /* kfifo_put，放入单个元素 */
    kfifo_put(&fifo1, 4);
    kfifo_put(&fifo2, 4);

#if 0
    /* 如果要从用户空间写入，直接用kfifo_from_user */
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
#endif

    /*
     * 3.
     * 查看状态
     */
    /* 是否已经初始化 */
    printf("%s\r\n", kfifo_initialized(&fifo1) ? "initialized" : "not initialized");

    /* 查看单个元素的字节数 */
    printf("element size: %d bytes\r\n", kfifo_esize(&fifo1));

    /* 查看recsize, 对于非记录型kfifo，返回0 */
    printf("record size: %ld bytes\r\n", kfifo_recsize(&fifo1));

    /* 查看最大元素个数 */
    printf("max element count: %d\r\n", kfifo_size(&fifo1));

    /* 查看队列中已经存储的元素个数 */
    printf("used element count: %d\r\n", kfifo_len(&fifo1));

    /* 查看队列中还空闲的，能够继续存储的元素个数 */
    printf("unused element count: %d\r\n", kfifo_avail(&fifo1));

    /* 查看队列中下一个记录的字节数，对于非记录型kfifo，返回已经存储的元素的总字节数 */
    printf("next record size: %d bytes\r\n", kfifo_peek_len(&fifo1));

    /* 是否为空队列，用kfifo_is_empty_spinlocked或kfifo_is_empty_spinlocked_noirqsave带锁查看 */
    printf("%s\r\n", kfifo_is_empty(&fifo1) ? "empty" : "not empty");

    /* 是否为满队列 */
    printf("%s\r\n", kfifo_is_full(&fifo1) ? "full" : "not full");

    /*
     * 4.
     * 读出
     */
    /* kfifo_peek查看但不读出下一次要读出的元素， 返回查看到的元素数 */
    ret = kfifo_peek(&fifo1, b);
    for (int i = 0; i < ret; i++)
        printf("%d ", b[i]);
    printf(" line %d\r\n", __LINE__);

    /* kfifo_out_peek查看但不读出kfifo中的元素，第三个参数指定查看的最大元素数，返回查看到的元素数 */
    ret = kfifo_out_peek(&fifo1, b, sizeof(b));
    for (int i = 0; i < ret; i++)
        printf("%d ", b[i]);
    printf(" line %d\r\n", __LINE__);

    /* kfifo_skip跳过下一次要读出的元素 */
    kfifo_skip(&fifo1);

    /* kfifo_out读出min(指定的最大元素数, 队列中已有元素数)，返回值是读出元素个数，空队列返回0 */
    ret = kfifo_out(&fifo1, b, 100);
    for (int i = 0; i < ret; i++)
        printf("%d ", b[i]);
    printf(" line %d\r\n", __LINE__);

    /* kfifo_get读出单个元素，返回读出的元素数，空队列返回0 */
    ret = kfifo_get(&fifo2, b);
    for (int i = 0; i < ret; i++)
        printf("%d ", b[i]);
    printf(" line %d\r\n", __LINE__);

#if 0
    /* 如果要读出到用户空间，直接用kfifo_to_user */
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
#endif

    /*
     * 5.
     * 释放
     */
    /* 释放所有动态分配的空间 */
    kfifo_free(&fifo1);
}

/**
 * 这个函数演示了记录型kfifo
 */
void test_rec(void)
{
    /*
     * 1.
     * 初始化
     * 两种不同最大记录长度的kfifo都支持动态和静态初始化
     * kfifo_rec_ptr_1（或者用STRUCT_KFIFO_REC_1来定义）的recsize为1，即支持最大255字节长度的记录。
     * kfifo_rec_ptr_2（或者用STRUCT_KFIFO_REC_2来定义）的recsize为2，即支持最大65535字节长度的记录。
     */

    /*
      动态初始化
    */
    /* kfifo_rec_ptr_x的元素类型固定为unsigned char */
    struct kfifo_rec_ptr_1 fifo1, fifo2 __maybe_unused;
    /* 这里用kfifo_alloc设定最大128个元素（不是2的幂会roundup） */
    int ret = kfifo_alloc(&fifo1, 128, GFP_KERNEL);
    if (ret)
        printf("%s, %d\r\n", strerror(-ret), __LINE__);
    /* 也可以用kfifo_init来指定预分配的内存空间，设定最大为128个元素（不是2的幂会roundup）*/
    char buf[128] = {0};
    ret = kfifo_init(&fifo2, buf, 128);
    if (ret)
        printf("%s, %d\r\n", strerror(-ret), __LINE__);

    /*
      静态初始化
    */
    /* 用STRUCT_KFIFO_REC_1创建最大255字节长度的记录的kfifo，元素类型固定为unsigned char，设置最多包含128个元素（不是2的幂会roundup） */
    STRUCT_KFIFO_REC_1(128)
    fifo3 __maybe_unused;

    /*
     * 2.
     * 写入
     */
    /* kfifo_put在记录型kfifo中不常用，放入单个元素形成的记录 */
    char bu = 'a';
    kfifo_put(&fifo1, bu);

    /* kfifo_in，放入单个记录，最后一个参数为记录的元素个数
       需要带自旋锁就用kfifo_in_locked或kfifo_in_spinlocked或kfifo_in_spinlocked_noirqsave */
    for (int i = 1; i < 10; i++)
    {
        memset(buf, 'a' + i, i + 1);
        kfifo_in(&fifo1, buf, i + 1);
    }
#if 0
    /* 如果要从用户空间写入，直接用kfifo_from_user */
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
#endif

    /*
     * 3.
     * 查看状态
     */
    /* 是否已经初始化 */
    printf("%s\r\n", kfifo_initialized(&fifo1) ? "initialized" : "not initialized");

    /* 查看单个元素的字节数 */
    printf("element size: %d bytes\r\n", kfifo_esize(&fifo1));

    /* 查看recsize */
    printf("record size: %ld bytes\r\n", kfifo_recsize(&fifo1));

    /* 查看最大元素个数 */
    printf("max element count: %d\r\n", kfifo_size(&fifo1));

    /* 查看队列中已经存储的元素个数 */
    printf("used element count: %d\r\n", kfifo_len(&fifo1));

    /* 查看队列中还空闲的，能够继续存储的元素个数 */
    printf("unused element count: %d\r\n", kfifo_avail(&fifo1));

    /* 查看队列中下一个记录的字节数 */
    printf("next record size: %d bytes\r\n", kfifo_peek_len(&fifo1));

    /* 是否为空队列，用kfifo_is_empty_spinlocked或kfifo_is_empty_spinlocked_noirqsave带锁查看 */
    printf("%s\r\n", kfifo_is_empty(&fifo1) ? "empty" : "not empty");

    /* 是否为满队列 */
    printf("%s\r\n", kfifo_is_full(&fifo1) ? "full" : "not full");

    /*
     * 4.
     * 读出
     */
    /* kfifo_peek查看但不读出下一次要读出的记录，返回查看到的元素数 */
    char b[1000] = {0};
    ret = kfifo_out_peek(&fifo1, b, sizeof(b));
    b[ret] = '\0';
    printf("%d elements: %s line %d\r\n", ret, b, __LINE__);

    /* kfifo_peek在记录型kfifo中不常用，查看但不读出记录中的第一个元素，返回查看到的元素数 */
    ret = kfifo_peek(&fifo1, b);
    printf("%d elements: %c line %d\r\n", ret, b[0], __LINE__);

    /* kfifo_skip跳过下一次要读出的记录 */
    kfifo_skip(&fifo1);

    /* kfifo_get在记录型kfifo中不常用，读出记录中的第一个元素，返回读出的元素数，空队列返回0，out指针指向下一条记录 */
    ret = kfifo_get(&fifo1, b);
    printf("%d elements: %c line %d\r\n", ret, b[0], __LINE__);

    /* kfifo_out读出一条记录，读出的元素个数为min(指定的最大元素数, 下一条记录包含的元素数)，返回值是读出元素个数，空队列返回0 */
    while (!kfifo_is_empty(&fifo1))
    {
        ret = kfifo_out(&fifo1, b, sizeof(b));
        b[ret] = '\0';
        printf("%d elements: %s line %d\r\n", ret, b, __LINE__);
    }

#if 0
    /* 如果要读出到用户空间，直接用kfifo_to_user */
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
#endif

    /*
     * 5.
     * 释放
     */
    /* 释放所有动态分配的空间 */
    kfifo_free(&fifo1);
}

int main(int argc, char const *argv[])
{
    printf("====nonrec kfifo====\r\n");
    test_nonrec();
    printf("\r\n\r\n\r\n=====rec kfifo======\r\n");
    test_rec();
    exit(0);
}
