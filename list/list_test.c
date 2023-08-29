
#include <stdio.h>
#include <stdlib.h>

#include <list.h>

#define PRINT_FROM_NODE(node)                        \
    do                                               \
    {                                                \
        struct my_node *__n;                         \
        printf("%s", (node).data);                   \
        list_for_each_entry(__n, &(node).head, head) \
            printf(" > %s", __n->data);              \
        printf("\t%d\r\n", __LINE__);                \
    } while (0)
/*
 * 内核里的list数据结构为双向循环列表
 * 反常识的是，它不是把数据结构塞入链表，而是把链表塞入数据结构
 * rcu功能依赖linux内核，移植到用户空间是个大工程，所以这里没有保留rcu，不要用链表的rcu功能
 * 如果要在用户空间用rcu，看这个https://liburcu.org/
 */

/* 要用内核中的list数据结构，自定义的节点必须包含list_head成员，如下 */
struct my_node
{
    const char *data;
    long *define_whatever_types_of_member_you_need;
    int define_as_many_members_as_you_need;
    struct list_head head;
};

int main(void)
{
    /*
     * 1.
     * 有不少初始化的方式，所谓初始化，就是搞出一个自己指向自己head
     */
    /* 利用LIST_HEAD_INIT静态初始化 */
    struct my_node node1 = {
        .data = "node1",
        .head = LIST_HEAD_INIT(node1.head)};

    /* 也可以利用INIT_LIST_HEAD动态初始化 */
    struct my_node node2 = {
        .data = "node2"};
    INIT_LIST_HEAD(&node2.head);

    /* 还可以直接利用LIST_HEAD生成一个新的head */
    LIST_HEAD(head3);
    struct my_node node3 = {
        .data = "node3",
        .head = head3};

    /* 初始化node4和5用于后面演示 */
    struct my_node node4 = {
        .data = "node4",
        .head = LIST_HEAD_INIT(node4.head)};
    struct my_node node5 = {
        .data = "node5",
        .head = LIST_HEAD_INIT(node5.head)};

    /*
     * 2.
     * 添加节点到链表中
     */
    /* 把node3添加为node1的下一个 */
    list_add(&node3.head, &node1.head);
    /* 把node2添加为node3上一个，即插入到node1和node3之间 */
    list_add_tail(&node2.head, &node3.head);
    /* 把node4添加为node3的下一个，把node5添加为node4的下一个 */
    list_add(&node4.head, &node3.head);
    list_add(&node5.head, &node4.head);

    /*
     * 3.
     * 可以通过head反向获取指向自定义node的指针
     */
    /* 获取head指向的node */
    struct my_node *node_ptr = list_entry(&node2.head, struct my_node, head);
    printf("node_ptr=%p, &node2=%p\t%d\r\n", node_ptr, &node2, __LINE__);

    /* 获取以node2为首的链表的最后一个node */
    node_ptr = list_last_entry(&node2.head, struct my_node, head);
    printf("last_node=%s\t%d\r\n", node_ptr->data, __LINE__);

    /* 获取以node2为首的链表中除了node2外的第一个node */
    node_ptr = list_first_entry(&node2.head, struct my_node, head);
    printf("first_node=%s\t%d\r\n", node_ptr->data, __LINE__);

    /* 获取以node2的下一个node */
    node_ptr = list_next_entry(&node2, head);
    printf("next_node=%s\t%d\r\n", node_ptr->data, __LINE__);

    /* 获取以node2的上一个node */
    node_ptr = list_prev_entry(&node2, head);
    printf("prev_node=%s\t%d\r\n", node_ptr->data, __LINE__);

    /*
     * 4.
     * 遍历链表
     * 如果要在遍历时删除节点，用带safe字样的list_for_each_xxx宏
     */
    /* 用list_for_each遍历整个链表，从node1开始，不包括node1；反向遍历用list_for_each_reverse */
    struct list_head *p;
    list_for_each(p, &node1.head)
    {
        printf(" > %s", list_entry(p, struct my_node, head)->data);
    }
    printf("\t%d\r\n", __LINE__);

    /* 用list_for_each_entry遍历整个链表，写法更简单，从node1开始，不包括node1；反向遍历用list_for_each_entry_reverse */
    struct my_node *n;
    list_for_each_entry(n, &node1.head, head)
    {
        printf(" > %s", n->data);
    }
    printf("\t%d\r\n", __LINE__);

    /* 用list_for_each_continue从已有p开始遍历到node4，左开右开；反向遍历用list_for_each_continue_reverse */
    p = &node1.head;
    list_for_each_continue(p, &node4.head)
    {
        printf(" > %s", list_entry(p, struct my_node, head)->data);
    }
    printf("\t%d\r\n", __LINE__);

    /* list_for_each_entry_continue，写法更简单，从已有n开始遍历到node4，左开右开；反向遍历用list_for_each_entry_continue_reverse */
    n = &node1;
    list_for_each_entry_continue(n, &node4.head, head)
    {
        printf(" > %s", n->data);
    }
    printf("\t%d\r\n", __LINE__);

    /* list_for_each_entry_from，从已有n开始遍历到node3，左闭右开；反向遍历用list_for_each_entry_from_reverse */
    n = &node1;
    list_for_each_entry_from(n, &node3.head, head)
    {
        printf("%s > ", n->data);
    }
    printf("\t%d\r\n", __LINE__);

    /*
     * 5.
     * 删除节点
     * 如果要在遍历时删除节点，用带safe字样的list_for_each_xxx宏
     */
    /* 从链表中删除node3 */
    list_del(&node3.head);
    /* 从链表中删除node2，并把node2重新初始化 */
    list_del_init(&node2.head);
    PRINT_FROM_NODE(node1);

    /*
     * 6.
     * 比较和测试
     */
    /* 判断node2和nodde3是否为初始化后的空列表 */
    printf("%s\t%d\r\n", list_empty(&node2.head) ? "node2 empty" : "node2 not empty", __LINE__);
    printf("%s\t%d\r\n", list_empty(&node3.head) ? "node3 empty" : "node3 not empty", __LINE__);

    /* 判断node4是不是以node1为起始的循环链表中的第一项，即比较两个head是否为同一个head */
    printf("%s\t%d\r\n", list_is_head(&node4.head, &node1.head) ? "same" : "not same", __LINE__);
    printf("%s\t%d\r\n", list_entry_is_head((&node4), &node1.head, head) ? "same" : "not same", __LINE__);

    /* 判断node1是不是以node5为起始的循环链表中的第二项 */
    printf("%s\t%d\r\n", list_is_first(&node1.head, &node5.head) ? "the first entry after head" : "not the first after head", __LINE__);

    /* 判断node5是不是以node1为起始的循环链表中的最后一项 */
    printf("%s\t%d\r\n", list_is_last(&node5.head, &node1.head) ? "last" : "not last", __LINE__);

    /* 判断node2链表除了head之外是否只有一项，即链表是否具有两项 */
    INIT_LIST_HEAD(&node3.head);
    list_add(&node3.head, &node2.head);
    printf("%s\t%d\r\n", list_is_singular(&node2.head) ? "singular" : "not singular", __LINE__);

    /*
     * 7.
     * 移动节点
     */
    /* 将node1从所在链表中移除，插入到node2的后面，node1所在链表和node2所在链表可以是同一个链表，也可以是不同链表*/
    list_move(&node1.head, &node2.head);
    PRINT_FROM_NODE(node4);
    PRINT_FROM_NODE(node2);

    /* 将node1从所在链表中移除，插入到node4的前面，node1所在链表和node4所在链表可以是同一个链表，也可以是不同链表*/
    list_move_tail(&node1.head, &node4.head);
    PRINT_FROM_NODE(node1);
    PRINT_FROM_NODE(node2);

    /* 将node4到node5的闭区间移动到node3之前， 可以发生在同一个链表内，也可以是不同链表 */
    list_bulk_move_tail(&node3.head, &node4.head, &node5.head);
    PRINT_FROM_NODE(node1);
    PRINT_FROM_NODE(node2);

    /*
     * 8.
     * 合并和切分链表
     */

    /*
     * 合并
     * 循环链表node1 > node4 > node5 > node7
     * 循环链表node2 > node3 > node6
     * 下面的list_splice以后==>
     * 第一个列表从node4处打断，node4被删除（不要访问node4），node1和node5形成不循环链表node5 > node7 > node1
     * 第二个列表从node3和node6中间打断变成不循环链表node6 > node2 > node3，
     * 把node5 > node7 > node1 和 node6 > node2 > node3 组成一个循环链表：
     * node5 > node7 > node1
     *  ^               v
     * node3 < node2 < node6
     * 如果要把node4初始化为空列表，就用list_splice_init
     */
    struct my_node node6 = {
        .data = "node6",
        .head = LIST_HEAD_INIT(node6.head)};
    struct my_node node7 = {
        .data = "node7",
        .head = LIST_HEAD_INIT(node7.head)};
    list_del_init(&node1.head);
    list_add(&node4.head, &node1.head);
    list_add(&node5.head, &node4.head);
    list_add(&node7.head, &node5.head);
    list_del_init(&node2.head);
    list_add(&node3.head, &node2.head);
    list_add(&node6.head, &node3.head);

    list_splice(&node4.head, &node3.head);
    PRINT_FROM_NODE(node5);

    /*
     * 合并
     * 循环链表node1 > node4 > node5 > node7
     * 循环链表node2 > node3 > node6
     * list_splice_tail==>
     * 第一个列表从node4处打断，node4被删除（不要访问node4），node1和node5形成不循环链表node5 > node7 > node1
     * 第二个列表从node2和node3中间打断变成不循环链表node3 > node6 > node2，
     * 把node5 > node7 > node1 和 node3 > node6 > node2 组成一个循环链表：
     * node5 > node7 > node1
     *  ^               v
     * node2 < node6 < node3
     * 如果要把node4初始化为空列表，就用list_splice_tail_init
     */
    list_del_init(&node1.head);
    list_add(&node4.head, &node1.head);
    list_add(&node5.head, &node4.head);
    list_add(&node7.head, &node5.head);
    list_del_init(&node2.head);
    list_add(&node3.head, &node2.head);
    list_add(&node6.head, &node3.head);
    list_splice_tail(&node4.head, &node3.head);
    PRINT_FROM_NODE(node5);

    /*
     * list_cut_before分割
     * 循环链表node5 > node7 > node1 > node3 > node6 > node2
     * 在node2后打断，在node3前打断，形成第一个循环列表node2 > node3 > node6，
     * 和不循环链表node5 > node7 > node1
     * 在node5和node1之间插入node4
     * 形成第二个循环链表node1 > node4 > node5 > node7
     * 综上：node3到node2的闭区间范围为切分出来的链表1
     * 将原列表中的链表1部分替换为node4形成切分后的链表2
     */
    list_cut_before(&node4.head, &node2.head, &node3.head);
    PRINT_FROM_NODE(node2);
    PRINT_FROM_NODE(node1);

    /*
     * list_cut_position分割
     * 循环链表node5 > node7 > node1 > node3 > node6 > node2
     * 在node2后打断，在node1后打断，形成第一个循环列表node2 > node3 > node6，
     * 和不循环链表node5 > node7 > node1
     * 在node5和node1之间插入node4
     * 形成第二个循环链表node1 > node4 > node5 > node7
     * 综上：node1到node2的左开右闭区间范围为切分出来的链表1
     * 将原列表中的链表1部分替换为node4形成切分后的链表2
     */
    list_splice_tail(&node4.head, &node3.head);
    list_cut_position(&node4.head, &node2.head, &node1.head);
    PRINT_FROM_NODE(node2);
    PRINT_FROM_NODE(node1);

    exit(0);
}