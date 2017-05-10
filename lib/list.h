#ifndef _LIST_
#define _LIST_

struct list_head {
	struct list_head *next, *prev;
};

#undef offsetof
#define offsetof(TYPE, MEMBER) (size_t)( &((TYPE *)0)->MEMBER )

#define container_of(ptr, type, member)					\
	((type *)( (char *)(ptr) - offsetof(type, member) ))

#define list_entry(ptr, type, member)					\
	container_of(ptr, type, member)

#define list_next_entry(pos, member)					\
	list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_prev_entry(pos, member)					\
	list_entry((pos)->member.prev, typeof(*(pos)), member)

#define list_first_entry(head, type, member)				\
	list_entry((head)->next, type, member)

#define list_last_entry(head, type, member)				\
	list_entry((head)->prev, type, member)

#define list_for_each_entry(pos, head, member)				\
	for (pos = list_first_entry(head, typeof(*(pos)), member);	\
	     &(pos->member) != head;					\
	     pos = list_next_entry(pos, member))

#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_first_entry(head, typeof(*(pos)), member),	\
		n = list_next_entry(pos, member);			\
	     &(pos->member) != head;					\
	     pos = n, n = list_next_entry(n, member))

#define list_for_each_entry_continue(pos, head, member)			\
	for (pos = list_next_entry(pos, member);			\
	     &(pos->member) != head;					\
	     pos = list_next_entry(pos, member))

#define list_for_each_entry_safe_continue(pos, n, head, member)		\
	for (pos = list_next_entry(pos, member),			\
		n = list_next_entry(pos, member);			\
	     &(pos->member) != head;					\
	     pos = n, n = list_next_entry(n, member))

#define list_for_each_entry_from(pos, head, member)			\
	for (; &(pos->member) != head;					\
	     pos = list_next_entry(pos, member))

#define list_for_each_entry_safe_from(pos, n, head, member)		\
	for (n = list_next_entry(pos, member);				\
	     &(pos->member) != head;					\
	     pos = n, n = list_next_entry(n, member))

static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->prev = prev;
	new->next = next;
	prev->next = new;
}

static inline void list_add(struct list_head *new,
			    struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void list_add_tail(struct list_head *new,
				 struct list_head *head)
{
	__list_add(new, head, head->next);
}

static inline void __list_del(struct list_head *prev,
			      struct list_head *next)
{
	prev->next = next;
	next->prev = prev;
}

static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->prev = entry;
	entry->next = entry;
}

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->prev = list;
	list->next = list;
}

#endif
