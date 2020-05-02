/*
 * Queue.h
 *
 * Generic queue, implemented with linked list.
 *
 * Shengsong Gao, 2020
 */

#ifndef _Queue_h
#define _Queue_h

typedef struct queue q_t;

/* -------- basic queue functions -------- */

// returns NULL if malloc failed.
q_t *make_q();


// allows NULL as the itemdelete(); does nothing for a NULL queue.
void delete_q(q_t *queue, void (*itemdelete)(void *item));


// returns -1 if the queue is NULL or malloc failed; otherwise 0.
int enq_q(q_t *queue, void *item);


/* deq_q() frees the node it deq'd, but the caller is still 
 * responsible for freeing the return item. Returns NULL if
 * the queue is NULL or if the queue is empty.
 */
void *deq_q(q_t *queue);


/* -------- unconventional queue functions -------- */

// does nothing for a NULL queue.
void iterate_q(q_t *queue, void (*itemfunc)(void *item, void *argument), void *argument);


// returns NULL if the queue is NULL or it's empty.
void *peek_q(q_t *queue);


/* returns the first item that makes item_matcher() return 1;
 * returns NULL if the queue is NULL or item_matcher never 
 * returned 1.
 */
void *get_item_q(q_t *queue, int (*item_matcher)(void *item, void *target), void *target);

/* pops the first item that makes item_matcher() return 1;
 * returns NULL if the queue is NULL or item_matcher never 
 * returned 1.
 */
void *pop_item_q(q_t *queue, int (*item_matcher)(void *item, void *target), void *target);


/* -------- implementation-specific functions -------- */

/* assumes that the queue's items point to integers and
 * delete all nodes with an integer matching the target.
 *
 * does nothing for a NULL queue.
 */
void delete_integer_targets_q(q_t *queue, int target);


#endif  /* _Queue_h */
