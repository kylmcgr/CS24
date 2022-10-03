#include "eval_dict.h"

#include <assert.h>
#include "eval_refs.h"
#include "eval_types.h"
#include "exception.h"
#include "refs.h"

static dict_value_t *dict_coerce(value_t *obj) {
    assert(obj->type == VAL_DICT);
    return (dict_value_t *) obj;
}

static ref_array_value_t *dict_keyarray(dict_value_t *dict) {
    value_t *obj = deref(dict->keys);
    assert(obj->type == VAL_REF_ARRAY);
    return (ref_array_value_t *) obj;
}
static ref_array_value_t *dict_valuearray(dict_value_t *dict) {
    value_t *obj = deref(dict->values);
    assert(obj->type == VAL_REF_ARRAY);
    return (ref_array_value_t *) obj;
}

static size_t keys_find(
    ref_array_value_t *keys,
    uint64_t hash,
    reference_t subscr,
    bool skip_tombstones
) {
    size_t capacity = keys->capacity;
    /* Probe buckets starting at hash % capacity
     * until either subscr or an empty slot is found. */
    for (size_t bucket = hash; true; bucket++) {
        /* Bucket index wraps around at capacity. */
        bucket %= capacity;
        reference_t key = keys->values[bucket];
        if (skip_tombstones && key == TOMBSTONE_REF) {
            continue;
        }
        if (key == NULL_REF || key == TOMBSTONE_REF || ref_eq(subscr, key)) {
            return bucket;
        }
    }
}

static void dict_upsize(dict_value_t *dict) {
    ref_array_value_t *keys = dict_keyarray(dict);
    ref_array_value_t *values = dict_valuearray(dict);
    size_t capacity = keys->capacity;

    size_t new_capacity = capacity * 2;
    reference_t ref_keys = make_reference_refarray(new_capacity);
    if (exception_occurred()) {
        return;
    }
    reference_t ref_values = make_reference_refarray(new_capacity);
    if (exception_occurred()) {
        decref(ref_keys);
        return;
    }

    ref_array_value_t *new_keys = (ref_array_value_t *) deref(ref_keys);
    ref_array_value_t *new_values = (ref_array_value_t *) deref(ref_values);

    for (size_t idx = 0; idx < capacity; idx++) {
        reference_t key = keys->values[idx];
        if (key == NULL_REF || key == TOMBSTONE_REF) {
            continue;
        }

        /* Add the key-value pair to the new ref arrays.
         * Increment the reference counts for the key and value
         * because the new ref arrays now refer to them. */
        size_t new_idx = keys_find(new_keys, ref_hash(key), key, false);
        reference_t value = values->values[idx];
        incref(key);
        incref(value);
        new_keys->values[new_idx] = key;
        new_values->values[new_idx] = value;
    }

    /* Now replace the old key and value arrays. */
    decref(dict->keys);
    decref(dict->values);
    dict->keys = ref_keys;
    dict->values = ref_values;

    /* This process removes all tombstones, so the number of occupied slots
     * become equal to the number of elements in the dictionary. */
    dict->occupied = dict->size;
}
static void dict_maybe_upsize(dict_value_t *dict) {
    if (dict->occupied * 2 >= dict_keyarray(dict)->capacity) {
        dict_upsize(dict);
    }
}

bool dict_bool(value_t *obj) {
    return dict_len(obj) > 0;
}

size_t dict_len(value_t *obj) {
    return dict_coerce(obj)->size;
}

bool dict_eq(value_t *l, value_t *r) {
    dict_value_t *ldict = dict_coerce(l),
                 *rdict = dict_coerce(r);

    if (ldict->size != rdict->size) {
        return false;
    }

    ref_array_value_t *lkeys = dict_keyarray(ldict),
                      *rkeys = dict_keyarray(rdict);
    ref_array_value_t *lvalues = dict_valuearray(ldict),
                      *rvalues = dict_valuearray(rdict);

    size_t capacity = lkeys->capacity;
    for (size_t idx = 0; idx < capacity; idx++) {
        reference_t key = lkeys->values[idx];
        if (key == NULL_REF || key == TOMBSTONE_REF) {
            continue;
        }

        size_t ridx = keys_find(rkeys, ref_hash(key), key, true);
        if (
            rkeys->values[ridx] == NULL_REF ||
            !ref_eq(lvalues->values[idx], rvalues->values[ridx])
        ) {
            return false;
        }
    }

    return true;
}

reference_t dict_subscr_get(value_t *obj, reference_t subscr) {
    dict_value_t *dict = dict_coerce(obj);
    ref_array_value_t *keys = dict_keyarray(dict);
    ref_array_value_t *values = dict_valuearray(dict);

    uint64_t hash = ref_hash(subscr);
    if (exception_occurred()) {
        return NULL_REF;
    }

    size_t idx = keys_find(keys, hash, subscr, true);
    if (keys->values[idx] == NULL_REF) {
        exception_set(EXC_KEY_ERROR, "no value found for key in dictionary");
        return NULL_REF;
    }

    incref(values->values[idx]);
    return values->values[idx];
}

void dict_subscr_set(value_t *obj, reference_t subscr, reference_t value) {
    dict_value_t *dict = dict_coerce(obj);
    ref_array_value_t *keys = dict_keyarray(dict);
    ref_array_value_t *values = dict_valuearray(dict);

    uint64_t hash = ref_hash(subscr);
    if (exception_occurred()) {
        return;
    }

    /* Look for an existing entry with this key. */
    size_t idx = keys_find(keys, hash, subscr, true);
    if (keys->values[idx] != NULL_REF) {
        /* If the keys match, just overwrite the value. */
        decref(values->values[idx]);
        incref(value);
        values->values[idx] = value;
        return;
    }

    /* Otherwise, look for a new slot to set. */
    idx = keys_find(keys, hash, subscr, false);
    assert(keys->values[idx] == NULL_REF || keys->values[idx] == TOMBSTONE_REF);
    dict->size++;
    if (keys->values[idx] == NULL_REF) {
        dict->occupied++;
    }


    incref(subscr);
    incref(value);

    keys->values[idx] = subscr;
    values->values[idx] = value;

    dict_maybe_upsize(dict);
}

void dict_subscr_del(value_t *obj, reference_t subscr) {
    dict_value_t *dict = dict_coerce(obj);
    ref_array_value_t *keys = dict_keyarray(dict);
    ref_array_value_t *values = dict_valuearray(dict);

    uint64_t hash = ref_hash(subscr);
    if (exception_occurred()) {
        return;
    }

    size_t idx = keys_find(keys, hash, subscr, true);
    if (keys->values[idx] == NULL_REF) {
        exception_set(EXC_KEY_ERROR, "can't delete nonexistant key in dictionary");
        return;
    }

    decref(keys->values[idx]);
    decref(values->values[idx]);

    dict->size--;
    keys->values[idx] = TOMBSTONE_REF;
    values->values[idx] = NULL_REF;
}

void dict_print(value_t *obj, FILE *stream, size_t depth, bool repr) {
    (void) repr;

    /* Then make sure that we are dealing with a dictionary. */
    dict_value_t *dict = dict_coerce(obj);
    ref_array_value_t *keys = dict_keyarray(dict);
    ref_array_value_t *values = dict_valuearray(dict);

    bool comma = false;
    fprintf(stream, "{");
    size_t capacity = keys->capacity;
    for (size_t idx = 0; idx < capacity; idx++) {
        reference_t key = keys->values[idx];
        if (key == NULL_REF || key == TOMBSTONE_REF) {
            continue;
        }

        if (comma) {
            fprintf(stream, ", ");
        }

        ref_print_repr(key, stream, depth);
        fprintf(stream, ": ");
        ref_print_repr(values->values[idx], stream, depth);

        comma = true;
    }
    fprintf(stream, "}");
}
