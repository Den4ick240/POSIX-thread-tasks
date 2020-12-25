#include "arrayset.h"

void arrayset_init(struct arrayset *set) {
    set->arr = NULL;
    set->arr_size = 0;
    set->data_size = 0;
}

int arrayset_add(struct arrayset *set, void *element) {
    if (set->data_size == set->arr_size) {
        set->arr_size += (set->arr_size == 0 ? 1 : set->arr_size);
        set->arr = (void **)realloc(set->arr, set->arr_size * sizeof(void*));
        if (set->arr == NULL)
            return -1;
    }
    set->arr[set->data_size] = element;
    set->data_size++;
    return 0;
}

int arrayset_remove(struct arrayset *set, void *element) {
    int i;
    for (i = 0; i < set->data_size; i++) {
        if (set->arr[i] == element) {
            set->data_size--;
            set->arr[i] = set->arr[set->data_size];
            return 0;
        }
    }
    return -1;
}

void arrayset_free(struct arrayset *set, void (*free_element) (void*)) {
    int i;
    for (i = 0; i < set->data_size; i++) {
        free_element(set->arr[i]);
    }
    free(set->arr);
    arrayset_init(set);
}