#include <stdlib.h>
#include <string.h>

struct linkedlist_node_t {
    struct linkedlist_node_t *next;              /*!< pointer to the next list node */
};

struct linkedlist_t{
	struct linkedlist_node_t *first;
	struct linkedlist_node_t *last;
	unsigned int size;
	unsigned int data_sz;
};


struct linkedlist_t *linkedlist_create(unsigned int sz);
void * linkedlist_insert_tail(struct linkedlist_t *l, void *e);
void linkedlist_remove_head(struct linkedlist_t *l);
void linkedlist_remove(struct linkedlist_t *l, void *e);
struct linkedlist_node_t* get_next(struct linkedlist_node_t* node, void **e);
int linkedlist_size(struct linkedlist_t *l);
void linkedlist_destroy(struct linkedlist_t **l);