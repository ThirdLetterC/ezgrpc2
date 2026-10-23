#pragma once
#include <stdlib.h>
#include "grpc_defs.h"

/** An opaque linked-list context. */
typedef struct grpc_list grpc_list;

/** Create a list. Returns nullptr if allocation fails. */
grpc_list *grpc_list_new(void);

/** Free the list nodes and context; the caller owns the stored values. */
void grpc_list_free(grpc_list *list);

/** Return the number of elements in the list. */
size_t grpc_list_count(grpc_list *list);

/** Add ``userdata`` to the back of ``list``; return 0 on success or nonzero on allocation failure. */
int grpc_list_push_back(grpc_list *list, void *userdata);

/** Add ``userdata`` to the front of ``list``; return 0 on success or nonzero on allocation failure. */
int grpc_list_push_front(grpc_list *list, void *userdata);

/** Remove and return the first value, or nullptr if the list is empty. */
void *grpc_list_pop_front(grpc_list *list);

/** Remove and return the last value, or nullptr if the list is empty. */
void *grpc_list_pop_back(grpc_list *list);

/** Return the first value without removing it, or nullptr if the list is empty. */
void *grpc_list_peek_front(grpc_list *list);

/** Return the last value without removing it, or nullptr if the list is empty. */
void *grpc_list_peek_back(grpc_list *list);

/** Append ``src`` to ``dst`` and leave ``src`` empty. */
void grpc_list_concat_and_empty_src(grpc_list *dst, grpc_list *src);

/**
 * Return the first value for which ``cmp(value, userdata)`` returns 0,
 * following the usual ``strcmp()`` convention. Return nullptr if none matches.
 */
void *grpc_list_find(grpc_list *list, int (*cmp)(const void *data, const void *userdata),
                        void *userdata);

/**
 * Remove and return the first value for which ``cmp(value, userdata)`` returns
 * 0. Return nullptr if none matches.
 */
void *grpc_list_remove(grpc_list *list, int (*cmp)(const void *data, const void *userdata),
                          void *userdata);

/** Return nonzero if ``list`` is empty. */
int grpc_list_is_empty(grpc_list *list);

/** Call ``func`` for each value in ``list`` from front to back. */
void grpc_list_foreach(grpc_list *list, void (*func)(const void *data, const void *userdata),
                          void *userdata);
