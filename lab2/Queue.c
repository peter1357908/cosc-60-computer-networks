/*
 * Queue.c
 *
 * Generic queue, implemented with linked list.
 *
 * Shengsong Gao, 2020
 */


#include <stdio.h>
#include <stdlib.h>  // for malloc()
#include "Queue.h"

typedef struct qnode {
	void *item;
	struct qnode *node_behind;
} qnode_t;

typedef struct queue {
	qnode_t *head;
	qnode_t *tail;
} q_t;

/* -------- basic queue functions -------- */

q_t *make_q() {
	q_t *queue = (q_t *)malloc(sizeof(q_t));
	if (queue != NULL) {
		queue->head = NULL;
		queue->tail = NULL;
	}
	return queue;
}

void delete_q(q_t *queue, void (*itemdelete)(void *item)) {
	if (queue == NULL) { return; }
	void *item;
	while(queue->head != NULL) {
		item = deq_q(queue);  // deq_q() frees the node it deq'd
		if (itemdelete != NULL) {
			(*itemdelete)(item);
		}
	}
	free(queue);
}

int enq_q(q_t *queue, void *item) {
	if (queue == NULL) {
		perror("enq_q: queue is NULL\n");
		return -1;
	}
	
	qnode_t *node = (qnode_t *) malloc(sizeof(qnode_t));
	if (node == NULL) {
		perror("enq_q: error malloc'ing node\n");
		return -1;
	}
	node->item = item;
	node->node_behind = NULL;
	
	if (queue->head == NULL) {
		queue->head = node;
		queue->tail = node;
	} else {
		queue->tail->node_behind = node;
		queue->tail = node;
	}
	
	return 0;
}

void *deq_q(q_t *queue) {
	void *item = NULL;
	if (queue != NULL && queue->head != NULL) {
		item = queue->head->item;
		qnode_t *new_head = queue->head->node_behind;
		free(queue->head);
		queue->head = new_head;
		if (queue->head == NULL) {
			queue->tail = NULL;
		}
	}
	return item;
}


/* -------- unconventional queue functions -------- */

void iterate_q(q_t *queue, void (*itemfunc)(void *item)) {
	if (queue == NULL) { return; }
	qnode_t *currNode = queue->head;
	while(currNode != NULL) {
		(*itemfunc)(currNode->item);
		currNode = currNode->node_behind;
	}
}


void *peek_q(q_t *queue) {
	if (queue == NULL || queue->head == NULL) return NULL;
	return queue->head->item;
}



/* -------- implementation-specific functions -------- */
void delete_integer_targets_q(q_t *queue, int target) {
	if (queue == NULL) { return; }
	int curr_number;
	qnode_t *currNode = queue->head;
	qnode_t *lastNode;
	while(currNode != NULL) {
		curr_number = *((int *) currNode->item);
		// when a match is found...
		if (curr_number == target) {
			// if the current node is a head node...
			if (queue->head == currNode) {
				// AND a tail node...
				if (queue->tail == currNode) {
					queue->head = NULL;
					queue->tail = NULL;
					free(currNode->item);
					free(currNode);
					currNode = NULL;
				}
				// but NOT a tail node...
				else {
					queue->head = currNode->node_behind;
					free(currNode->item);
					free(currNode);
					currNode = queue->head;
				}
			}
			// if the current node is a tail node but not a head node
			else if (queue->tail == currNode) {
				// safe to assume that lastNode is not NULL
				lastNode->node_behind = NULL;
				queue->tail = lastNode;
				free(currNode->item);
				free(currNode);
				currNode = NULL;
			}
			// if the current node is neither the head nor the tail
			else {
				// safe to assume that lastNode is not NULL
				lastNode->node_behind = currNode->node_behind;
				free(currNode->item);
				free(currNode);
				currNode = lastNode->node_behind;
			}
		}
		// not a match, move on...
		else {
			lastNode = currNode;
			currNode = currNode->node_behind;
		}
	}
}


