#pragma once
#include <stdlib.h>
#include "ezgrpc2_defs.h"

/** An opaque linked-list context. */
typedef struct ezgrpc2_list ezgrpc2_list;

/** Create a list. Returns nullptr if allocation fails. */
ezgrpc2_list *ezgrpc2_list_new(void);

/** Free the list nodes and context; the caller owns the stored values. */
void ezgrpc2_list_free(ezgrpc2_list *list);

/** Return the number of elements in the list. */
size_t ezgrpc2_list_count(ezgrpc2_list *list);

/** Add ``userdata`` to the back of ``list``; return 0 on success or nonzero on allocation failure. */
int ezgrpc2_list_push_back(ezgrpc2_list *list, void *userdata);

/** Add ``userdata`` to the front of ``list``; return 0 on success or nonzero on allocation failure. */
int ezgrpc2_list_push_front(ezgrpc2_list *list, void *userdata);

/** Remove and return the first value, or nullptr if the list is empty. */
void *ezgrpc2_list_pop_front(ezgrpc2_list *list);

/** Remove and return the last value, or nullptr if the list is empty. */
void *ezgrpc2_list_pop_back(ezgrpc2_list *list);

/** Return the first value without removing it, or nullptr if the list is empty. */
void *ezgrpc2_list_peek_front(ezgrpc2_list *list);

/** Return the last value without removing it, or nullptr if the list is empty. */
void *ezgrpc2_list_peek_back(ezgrpc2_list *list);

/** Append ``src`` to ``dst`` and leave ``src`` empty. */
void ezgrpc2_list_concat_and_empty_src(ezgrpc2_list *dst, ezgrpc2_list *src);

/**
 * Return the first value for which ``cmp(value, userdata)`` returns 0,
 * following the usual ``strcmp()`` convention. Return nullptr if none matches.
 */
void *ezgrpc2_list_find(ezgrpc2_list *list, int (*cmp)(const void *data, const void *userdata),
                        void *userdata);

/**
 * Remove and return the first value for which ``cmp(value, userdata)`` returns
 * 0. Return nullptr if none matches.
 */
void *ezgrpc2_list_remove(ezgrpc2_list *list, int (*cmp)(const void *data, const void *userdata),
                          void *userdata);

/** Return nonzero if ``list`` is empty. */
int ezgrpc2_list_is_empty(ezgrpc2_list *list);

/** Call ``func`` for each value in ``list`` from front to back. */
void ezgrpc2_list_foreach(ezgrpc2_list *list, void (*func)(const void *data, const void *userdata),
                          void *userdata);
