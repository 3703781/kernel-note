#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H
typedef _Bool			bool;

struct list_head {
	struct list_head *next, *prev;
};

struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};
#endif
