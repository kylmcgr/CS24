#include "eval_list.h"

#include <assert.h>
#include "eval_types.h"
#include "exception.h"
#include "refs.h"

static list_value_t *list_coerce(value_t *obj) {
    assert(obj->type == VAL_LIST);
    return (list_value_t *) obj;
}
static size_t list_coerce_subscript(list_value_t *list, reference_t subscr) {
    value_t *obj = deref(subscr);
    if (obj->type != VAL_INTEGER) {
        exception_set(EXC_TYPE_ERROR, "list indices must be integers");
        return 0;
    }

    int64_t idx = ((integer_value_t *) obj)->integer_value;
    /* If the subscript is negative, then add the size of the list to get
     * the actual index to use. */
    if (idx < 0) {
        idx += list->size;
    }

    /* If the subscript is still invalid, then raise an error. */
    if ((size_t) idx >= list->size) {
        exception_set(EXC_INDEX_ERROR, "list index out of bounds");
        return 0;
    }

    return idx;
}

static ref_array_value_t *list_refarray(list_value_t *list) {
    value_t *obj = deref(list->values);
    assert(obj->type == VAL_REF_ARRAY);
    return (ref_array_value_t *) obj;
}

bool list_bool(value_t *obj) {
    return list_len(obj) > 0;
}

size_t list_len(value_t *obj) {
    return list_coerce(obj)->size;
}

int list_cmp(value_t *lobj, value_t *robj) {
    list_value_t *l = list_coerce(lobj),
                 *r = list_coerce(robj);
    reference_t *lvals = list_refarray(l)->values,
                *rvals = list_refarray(r)->values;

    size_t lsize = l->size,
           rsize = r->size;
    size_t min_size = lsize < rsize ? lsize : rsize;
    /* Compare items lexicographically. */
    for (size_t idx = 0; idx < min_size; idx++) {
        int comparison = compare(lvals[idx], rvals[idx]);
        if (exception_occurred()) {
            return 0;
        }
        /* The first pair of elements that don't match determines the result. */
        if (comparison != 0) {
            return comparison;
        }
    }

    /* If all elements match, decide which list is longer. */
    return (lsize > rsize) - (lsize < rsize);
}
bool list_eq(value_t *lobj, value_t *robj) {
    list_value_t *l = list_coerce(lobj),
                 *r = list_coerce(robj);

    /* If lists' sizes are different, they are definitely not equal. */
    size_t size = l->size;
    if (size != r->size) {
        return false;
    }

    ref_array_value_t *lvals = list_refarray(l),
                      *rvals = list_refarray(r);
    /* Check that all corresponding values are equal. */
    for (size_t idx = 0; idx < size; idx++) {
        if (!ref_eq(lvals->values[idx], rvals->values[idx])) {
            return false;
        }
    }

    return true;
}

/*! Implements subscript access for list types. */
reference_t list_subscr_get(value_t *obj, reference_t subscr) {
    /* First ensure that this is actually a list_value_t. */
    list_value_t *list = list_coerce(obj);

    /* Then check to make sure that the subscript is an integer. */
    size_t idx = list_coerce_subscript(list, subscr);
    if (exception_occurred()) {
        return NULL_REF;
    }

    /* Finally look up the appropriate reference in the list. */
    ref_array_value_t *array = list_refarray(list);
    reference_t value = array->values[idx];
    incref(value);
    return value;
}

/*! Implements subscript assignment for list types. */
void list_subscr_set(value_t *obj, reference_t subscr, reference_t value) {
    /* First ensure that this is actually a list_value_t. */
    list_value_t *list = list_coerce(obj);

    /* Then check to make sure that the subscript is an integer. */
    size_t idx = list_coerce_subscript(list, subscr);
    if (exception_occurred()) {
        return;
    }

    /* Finally set the appropriate reference in the list. */
    ref_array_value_t *array = list_refarray(list);
    decref(array->values[idx]);
    incref(value);
    array->values[idx] = value;
}

void list_subscr_del(value_t *obj, reference_t subscr) {
    /* First ensure that this is actually a list_value_t. */
    list_value_t *list = list_coerce(obj);

    /* Then check to make sure that the subscript is an integer. */
    size_t idx = list_coerce_subscript(list, subscr);
    if (exception_occurred()) {
        return;
    }

    /* Now get the value array so that we can actually perform the deletion
     * operation. */
    ref_array_value_t *array = list_refarray(list);

    /* Update the size of the list. */
    list->size--;

    /* Move any values after the element to be deleted up by one slot. */
    decref(array->values[idx]);
    for (size_t i = idx; i < list->size; i++) {
        array->values[i] = array->values[i + 1];
    }
    array->values[list->size] = NULL_REF;
}

/*! Implements printing of lists. */
void list_print(value_t *obj, FILE *stream, size_t depth, bool repr) {
    (void) repr;

    /* First ensure that this is actually a list_value_t. */
    list_value_t *list = list_coerce(obj);

    /* Then get the array of references. */
    ref_array_value_t *array = list_refarray(list);

    /* Then print out the contents. */
    fprintf(stream, "[");
    size_t size = list->size;
    for (size_t i = 0; i < size; i++) {
        if (i > 0) {
            fprintf(stream, ", ");
        }
        ref_print_repr(array->values[i], stream, depth);
    }
    fprintf(stream, "]");
}
