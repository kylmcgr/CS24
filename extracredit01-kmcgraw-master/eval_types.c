#include "eval_types.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include "config.h"
#include "eval_dict.h"
#include "eval_list.h"
#include "eval_refs.h"
#include "exception.h"
#include "refs.h"

//// TYPE-SPECIFIC FUNCTIONS ////

char *type_to_str(value_type_t type) {
    switch (type) {
        case VAL_NONE:    return "NoneType";
        case VAL_BOOL:    return "bool";
        case VAL_INTEGER: return "int";
        case VAL_STRING:  return "str";
        case VAL_LIST:    return "list";
        case VAL_DICT:    return "dict";
        default:          return "<unknown>";
    }
}

// SINGLETON FUNCTIONS //

static reference_t singleton_to_ref_borrow(SingletonType type) {
    switch (type) {
        case S_NONE:  return NONE_REF;
        case S_TRUE:  return TRUE_REF;
        case S_FALSE: return FALSE_REF;
    }

    exception_set(EXC_INTERNAL, "unknown singleton type");
    return NULL_REF;
}
reference_t singleton_to_ref(SingletonType type) {
    reference_t result = singleton_to_ref_borrow(type);
    incref(result);
    return result;
}

static bool singleton_bool(value_t *obj) {
    assert(obj->type == VAL_NONE || obj->type == VAL_BOOL);

    /* Of all the singletons, only True is truthy. */
    return obj == deref(TRUE_REF);
}

static uint64_t singleton_hash(value_t *obj) {
    return obj->type == VAL_NONE ? -1 : singleton_bool(obj);
}

static int singleton_cmp(value_t *l, value_t *r) {
    assert((l->type == VAL_NONE || l->type == VAL_BOOL) && l->type == r->type);

    return singleton_bool(l) - singleton_bool(r);
}
static bool singleton_eq(value_t *l, value_t *r) {
    assert(l->type == VAL_NONE || l->type == VAL_BOOL);
    assert(r->type == VAL_NONE || r->type == VAL_BOOL);

    return l == r;
}

/*! Implements printing for singletons (None and boolean values). */
static void singleton_print(value_t *obj, FILE *stream, size_t depth, bool repr) {
    (void) depth;
    (void) repr;

    assert(obj->type == VAL_NONE || obj->type == VAL_BOOL);

    /* Otherwise, print an appropriate textual representation. */
    fprintf(stream,
        obj->type == VAL_NONE
            ? "None"
            : singleton_bool(obj) ? "True" : "False"
    );
}

/*! Returns the appropriate reference to a bool value. */
reference_t bool_ref(bool value) {
    reference_t result = value ? TRUE_REF : FALSE_REF;
    incref(result);
    return result;
}

// INTEGER FUNCTIONS //

static int64_t integer_coerce(value_t *obj) {
    assert(obj->type == VAL_INTEGER);
    return ((integer_value_t *) obj)->integer_value;
}

static bool integer_bool(value_t *obj) {
    /* An integer is truthy if it is not zero. */
    return integer_coerce(obj) != 0;
}

static uint64_t integer_hash(value_t *obj) {
    return integer_coerce(obj);
}

static reference_t integer_negate(value_t *obj) {
    return make_reference_int(-integer_coerce(obj));
}
static reference_t integer_identity(value_t *obj) {
    return make_reference_int(+integer_coerce(obj));
}

static int integer_cmp(value_t *l, value_t *r) {
    int64_t lval = integer_coerce(l),
            rval = integer_coerce(r);
    return (lval > rval) - (lval < rval);
}
static bool integer_eq(value_t *l, value_t *r) {
    return integer_cmp(l, r) == 0;
}

__attribute__((no_sanitize("integer")))
static reference_t integer_add(value_t *l, value_t *r) {
    return make_reference_int(integer_coerce(l) + integer_coerce(r));
}
__attribute__((no_sanitize("integer")))
static reference_t integer_subtract(value_t *l, value_t *r) {
    return make_reference_int(integer_coerce(l) - integer_coerce(r));
}
__attribute__((no_sanitize("integer")))
static reference_t integer_multiply(value_t *l, value_t *r) {
    return make_reference_int(integer_coerce(l) * integer_coerce(r));
}
__attribute__((no_sanitize("integer")))
static reference_t integer_divide(value_t *l, value_t *r) {
    return make_reference_int(integer_coerce(l) / integer_coerce(r));
}
__attribute__((no_sanitize("integer")))
static reference_t integer_modulo(value_t *l, value_t *r) {
    return make_reference_int(integer_coerce(l) % integer_coerce(r));
}

/*! Implements printing for integers. */
static void integer_print(value_t *obj, FILE *stream, size_t depth, bool repr) {
    (void) depth;
    (void) repr;

    fprintf(stream, "%" PRIi64, integer_coerce(obj));
}

// STRING FUNCTIONS //

static char *string_coerce(value_t *obj) {
    assert(obj->type == VAL_STRING);
    return ((string_value_t *) obj)->string_value;
}

static bool string_bool(value_t *obj) {
    return string_coerce(obj)[0] != '\0';
}

static uint64_t string_hash(value_t *obj) {
    /* Adapted from Java's String.hashCode(). */
    uint64_t hash = 1125899906842597; // prime
    for (char *sp = string_coerce(obj); *sp != '\0'; sp++) {
        hash = 31 * hash + *sp;
    }
    return hash;
}

static size_t string_len(value_t *obj) {
    return strlen(string_coerce(obj));
}

static int string_cmp(value_t *l, value_t *r) {
    return strcmp(string_coerce(l), string_coerce(r));
}
static bool string_eq(value_t *l, value_t *r) {
    return string_cmp(l, r) == 0;
}

static reference_t string_add(value_t *l, value_t *r) {
    return make_reference_string_concat(string_coerce(l), string_coerce(r));
}

static void string_print(value_t *obj, FILE *stream, size_t depth, bool repr) {
    (void) depth;

    /* Print out the string, surrounded with quotes if this is a repr() print. */
    fprintf(stream, repr ? "\"%s\"" : "%s", string_coerce(obj));
}

//// TYPE FUNCTION LISTINGS ////

/*! The number of unary operations. */
#define UNARY_OPS (MAX_UNARY_OP - MIN_UNARY_OP + 1)
/*! The number of binary operations. */
#define BINARY_OPS (MAX_BINARY_OP - MIN_BINARY_OP + 1)

/*!
 * A function pointer table for all the possible operations.
 * Not all operations may be implemented for each value type,
 * so some entries may be NULL.
 */
typedef struct {
    bool        (*f_bool)(value_t *);
    size_t      (*f_len )(value_t *);
    uint64_t    (*f_hash)(value_t *);

    int         (*f_cmp)(value_t *, value_t *);
    bool        (*f_eq )(value_t *, value_t *);

    reference_t (*f_unary_ops[UNARY_OPS]  )(value_t *);
    reference_t (*f_binary_ops[BINARY_OPS])(value_t *, value_t *);

    reference_t (*f_subscr_get)(value_t *obj, reference_t subscript);
    void        (*f_subscr_set)(value_t *obj, reference_t subscript, reference_t value);
    void        (*f_subscr_del)(value_t *obj, reference_t subscript);

    void        (*f_print)(value_t *obj, FILE *stream, size_t depth, bool repr);
} func_table_t;

static func_table_t table[NUM_TYPES];

static func_table_t *get_type_table(value_t *value) {
    value_type_t type = value->type;
    assert(0 <= type && type < NUM_TYPES);
    return &table[type];
}

/*!
 * This function setups the function tables used to handle operations that vary
 * between the different types of values supported by Subpython.
 *
 * Note that the singleton references are actually set by `eval_init`, not
 * this function.
 */
void eval_types_init() {
    /* Default all function pointers to NULL. */
    memset(table, 0, sizeof(table));

    table[VAL_NONE] = (func_table_t) {
        .f_bool  = singleton_bool,
        .f_hash  = singleton_hash,
        .f_cmp   = singleton_cmp,
        .f_eq    = singleton_eq,
        .f_print = singleton_print
    };
    table[VAL_BOOL] = table[VAL_NONE];
    table[VAL_INTEGER] = (func_table_t) {
        .f_bool  = integer_bool,
        .f_hash  = integer_hash,
        .f_cmp   = integer_cmp,
        .f_eq    = integer_eq,
        .f_print = integer_print,
    };
    table[VAL_INTEGER].f_unary_ops[UOP_NEGATE - MIN_UNARY_OP] = integer_negate;
    table[VAL_INTEGER].f_unary_ops[UOP_IDENTITY - MIN_UNARY_OP] = integer_identity;
    table[VAL_INTEGER].f_binary_ops[OP_ADD - MIN_BINARY_OP] = integer_add;
    table[VAL_INTEGER].f_binary_ops[OP_SUBTRACT - MIN_BINARY_OP] = integer_subtract;
    table[VAL_INTEGER].f_binary_ops[OP_MULTIPLY - MIN_BINARY_OP] = integer_multiply;
    table[VAL_INTEGER].f_binary_ops[OP_DIVIDE - MIN_BINARY_OP] = integer_divide;
    table[VAL_INTEGER].f_binary_ops[OP_MODULO - MIN_BINARY_OP] = integer_modulo;
    table[VAL_STRING] = (func_table_t) {
        .f_bool  = string_bool,
        .f_len   = string_len,
        .f_hash  = string_hash,
        .f_cmp   = string_cmp,
        .f_eq    = string_eq,
        .f_print = string_print
    };
    table[VAL_STRING].f_binary_ops[OP_ADD - MIN_BINARY_OP] = string_add;
    table[VAL_LIST] = (func_table_t) {
        .f_bool       = list_bool,
        .f_len        = list_len,
        .f_cmp        = list_cmp,
        .f_eq         = list_eq,
        .f_subscr_get = list_subscr_get,
        .f_subscr_set = list_subscr_set,
        .f_subscr_del = list_subscr_del,
        .f_print      = list_print
    };
    table[VAL_DICT] = (func_table_t) {
        .f_bool       = dict_bool,
        .f_len        = dict_len,
        .f_eq         = dict_eq,
        .f_subscr_get = dict_subscr_get,
        .f_subscr_set = dict_subscr_set,
        .f_subscr_del = dict_subscr_del,
        .f_print      = dict_print
    };
}

//// GENERIC DISPATCH FUNCTIONS ////

bool ref_is_none(reference_t r) {
    return r == NONE_REF;
}
bool ref_is_true(reference_t r) {
    return r == TRUE_REF;
}
bool ref_is_false(reference_t r) {
    return r == FALSE_REF;
}

/*! Return the result of coercing the reference to a bool. */
bool ref_bool(reference_t r) {
    /* Attempt to dereference the provided reference. */
    value_t *obj = deref(r);

    /* Dispatch to function. All types implement bool() conversion. */
    return get_type_table(obj)->f_bool(obj);
}

/*!
 * Return the result of calling len() on the provided reference.
 */
size_t ref_len(reference_t r) {
    /* Attempt to dereference the provided reference. */
    value_t *obj = deref(r);

    /* If the len function is not set, then error out. */
    size_t (*f_len)(value_t *) = get_type_table(obj)->f_len;
    if (f_len == NULL) {
        exception_set_format(EXC_TYPE_ERROR,
                "object of type '%s' has no len()", type_to_str(obj->type));
        return 0;
    }

    /* Otherwise, dispatch to function. */
    return f_len(obj);
}

/*!
 * Return the result of calling hash() on the provided reference.
 */
uint64_t ref_hash(reference_t r) {
    /* Attempt to dereference the provided reference. */
    value_t *obj = deref(r);

    /* If the hash function is not set, then error out. */
    uint64_t (*f_hash)(value_t *) = get_type_table(obj)->f_hash;
    if (f_hash == NULL) {
        exception_set_format(EXC_TYPE_ERROR,
                "unhashable type: '%s'", type_to_str(obj->type));
        return 0;
    }

    /* Otherwise, dispatch to function. */
    return f_hash(obj);
}

static char *builtin_to_str(NodeExprBuiltinType type) {
    switch (type) {
        case UOP_NEGATE:   return "-";
        case UOP_IDENTITY: return "+";

        case COMP_EQUALS:  return "==";
        case COMP_LT:      return "<";
        case COMP_GT:      return ">";
        case COMP_LE:      return "<=";
        case COMP_GE:      return ">=";

        case OP_ADD:       return "+";
        case OP_SUBTRACT:  return "-";
        case OP_MULTIPLY:  return "*";
        case OP_DIVIDE:    return "/";
        case OP_MODULO:    return "%";

        default:           return "<unkown>";
    }
}

reference_t ref_unary(NodeExprBuiltinType type, reference_t r) {
    assert(MIN_UNARY_OP <= type && type <= MAX_UNARY_OP);

    /* Attempt to dereference the provided reference. */
    value_t *obj = deref(r);

    /* If the unary function is not set, then error out. */
    reference_t (*f_op)(value_t *) =
        get_type_table(obj)->f_unary_ops[type - MIN_UNARY_OP];
    if (f_op == NULL) {
        exception_set_format(EXC_TYPE_ERROR,
                "bad operand type for unary %s: '%s'",
                builtin_to_str(type), type_to_str(obj->type));
        return NULL_REF;
    }

    /* Otherwise, dispatch to function. */
    return f_op(obj);
}

/*!
 * Return the result of executing the specified builtin operation.
 */
reference_t ref_binary(NodeExprBuiltinType type, reference_t l, reference_t r) {
    assert(MIN_BINARY_OP <= type && type <= MAX_BINARY_OP);

    /* Attempt to dereference the provided references. */
    value_t *lobj = deref(l),
            *robj = deref(r);

    /* First, we make the simplifying assumption that the two values must have
     * the same type, which holds for all operations current available in
     * Subpython. Then, if the appropriate builtin function is not set for this
     * type, error out. */
    reference_t (*f_op)(value_t *, value_t *) =
        get_type_table(lobj)->f_binary_ops[type - MIN_BINARY_OP];
    if (f_op == NULL || lobj->type != robj->type) {
        exception_set_format(EXC_TYPE_ERROR,
                "unsupported operand type(s) for %s: '%s' and '%s'",
                builtin_to_str(type), type_to_str(lobj->type), type_to_str(robj->type));
        return NULL_REF;
    }

    /* Otherwise, dispatch to function. */
    return f_op(lobj, robj);
}

/*!
 * Compares the values at two references.
 * Return value is negative if the first is less,
 * positive if the first is greater, and 0 if they are equal.
 */
int compare(reference_t l, reference_t r) {
    /* Attempt to dereference the provided references. */
    value_t *lobj = deref(l),
            *robj = deref(r);

    /* If the operands have different types or can't be compared, error out. */
    int (*f_cmp)(value_t *, value_t *) = get_type_table(lobj)->f_cmp;
    if (f_cmp == NULL || lobj->type != robj->type) {
        exception_set_format(EXC_TYPE_ERROR,
                "type(s) are not comparable: '%s' and '%s'",
                type_to_str(lobj->type), type_to_str(robj->type));
        return 0;
    }

    /* Otherwise, dispatch to function. */
    return f_cmp(lobj, robj);
}

/*! Returns the result of executing the builtin compare operation. */
bool ref_compare(NodeExprBuiltinType type, reference_t l, reference_t r) {
    int comparison = compare(l, r);
    if (exception_occurred()) {
        return NULL_REF;
    }

    /* Return whether the comparison result satisfies the comparison type. */
    switch (type) {
        case COMP_LT:
            return comparison < 0;
        case COMP_GT:
            return comparison > 0;
        case COMP_LE:
            return comparison <= 0;
        case COMP_GE:
            return comparison >= 0;
        default:
            UNREACHABLE();
    }
}

/*! Returns the result of comparing two objects for equality. */
bool ref_eq(reference_t l, reference_t r) {
    /* Attempt to dereference the provided references. */
    value_t *lobj = deref(l),
            *robj = deref(r);

    /* If the operands have different types, then they are not equal. */
    if (lobj->type != robj->type) {
        return false;
    }

    /* Dispatch to function. All types implement equality checks. */
    return  get_type_table(lobj)->f_eq(lobj, robj);
}

/*!
 * Return the result of a subscript access to an object.
 */
reference_t ref_subscr_get(reference_t r, reference_t subscr) {
    /* Attempt to dereference the provided reference. */
    value_t *obj = deref(r);

    /* If the subscript get function is not set, then error out. */
    reference_t (*f_get)(value_t *, reference_t) =
        get_type_table(obj)->f_subscr_get;
    if (f_get == NULL) {
        exception_set_format(EXC_TYPE_ERROR,
                "'%s' object is not subscriptable", type_to_str(obj->type));
        return NULL_REF;
    }

    /* Otherwise, dispatch to function. */
    return f_get(obj, subscr);
}

/*!
 * Return the result of a subscript assignment to an object.
 */
void ref_subscr_set(reference_t r, reference_t subscr, reference_t value) {
    /* Attempt to dereference the provided reference. */
    value_t *obj = deref(r);

    /* If the subscript set function is not set, then error out. */
    void (*f_set)(value_t *, reference_t, reference_t) =
        get_type_table(obj)->f_subscr_set;
    if (f_set == NULL) {
        exception_set_format(EXC_TYPE_ERROR,
                "'%s' object does not support item assignment", type_to_str(obj->type));
        return;
    }

    /* Otherwise, dispatch to function. */
    f_set(obj, subscr, value);
}

/*!
 * Execute the deletion of an item from an object.
 */
void ref_subscr_del(reference_t r, reference_t subscr) {
    /* Attempt to dereference the provided reference. */
    value_t *obj = deref(r);

    /* If the subscript del function is not set, then error out. */
    void (*f_del)(value_t *, reference_t) =
        get_type_table(obj)->f_subscr_del;
    if (f_del == NULL) {
        exception_set_format(EXC_TYPE_ERROR,
                "'%s' object does not support item deletion", type_to_str(obj->type));
        return;
    }

    /* Otherwise, dispatch to function. */
    f_del(obj, subscr);
}

static void print(reference_t r, FILE *stream, size_t depth, bool repr) {
    /* If we have reached the maximum recursion depth, print a placeholder. */
    if (depth == 0) {
        fprintf(stream, "...");
        return;
    }

    /* Attempt to dereference the provided reference. */
    value_t *obj = deref(r);

    /* Dispatch to function. Decrease depth to account for this level of recursion. */
    get_type_table(obj)->f_print(obj, stream, depth - 1, repr);
}

/*!
 * Print the provided reference to the provided stream, recursing to some
 * limited depth.
 */
void ref_print_repr(reference_t r, FILE *stream, size_t depth) {
    print(r, stream, depth, true);
}

/*!
 * Print the provided reference to the provided stream, recursing to some
 * limited depth.
 */
void ref_print(reference_t r, FILE *stream, size_t depth) {
    print(r, stream, depth, false);
}

/*! Print as in ref_print_repr but with an additional newline. */
void ref_println_repr(reference_t r, FILE *stream, size_t depth) {
    /* First print the value. */
    ref_print_repr(r, stream, depth);

    /* Then a newline. */
    fprintf(stream, "\n");
}
/*! Print as in ref_print but with an additional newline. */
void ref_println(reference_t r, FILE *stream, size_t depth) {
    /* First print the value. */
    ref_print(r, stream, depth);

    /* Then a newline. */
    fprintf(stream, "\n");
}
