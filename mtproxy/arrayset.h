/*
 * this is an array of pointers, so you can use it for anything
 * when you add an element to this set, it is placed to the end of the array.
 * If the array is full, then it's reallocated with twice the size.
 * When you delete an element, then the last element is placed instead of
 * the deleted element, so that the array doesn't have holes.
 * */
#ifndef MY_ARRAY_SET
#define MY_ARRAY_SET
#include "consts.h"

#define ARRAY_SET_INITIALIZER { NULL, 0, 0 }

struct arrayset {
    void **arr;
    int data_size;  //number of elements
    int arr_size;   //size of the allocated array
};

void arrayset_init(struct arrayset *set);

int arrayset_add(struct arrayset *set, void *element);

int arrayset_remove(struct arrayset *set, void *element);

void arrayset_free(struct arrayset *set, void (*free_element) (void*));

#endif //MY_ARRAY_SET