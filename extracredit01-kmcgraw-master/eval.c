#include "eval.h"

#include <assert.h>
#include <string.h>

#include "ast.h"
#include "config.h"
#include "eval_refs.h"
#include "eval_types.h"
#include "exception.h"
#include "mm.h"
#include "refs.h"

/* Global variable information. */

typedef struct {
    char *name;
    reference_t ref;
} global_variable_t;

static global_variable_t *global_vars;
static size_t num_vars;
static size_t max_vars;

//////////// EVALUATION ENGINE ////////////


//// GLOBAL VARIABLES ////

static reference_t globals_get(const char *name);
static void globals_set(const char *name, reference_t value);
static void globals_delete(const char *name);

//// AST EVALUATION FUNCTIONS ////

static reference_t eval_stmt_sequence(NodeStmtSequence *sequence);
static void eval_stmt_assign(NodeStmtAssign *assign);
static void eval_stmt_del(NodeStmtDel *del);
static void eval_stmt_if(NodeStmtIf *ifn);
static void eval_stmt_while(NodeStmtWhile *ifn);

static reference_t eval_expr(Node *node);

static reference_t eval_identifier(NodeExprIdentifier *ident);
static reference_t eval_not_test(NodeExprNotTest *test);
static reference_t eval_and_test(NodeExprAndTest *test);
static reference_t eval_or_test(NodeExprOrTest *test);
static reference_t eval_builtin(NodeExprBuiltin *builtin);
static reference_t eval_call(NodeExprCall *call);
static reference_t eval_subscript(NodeExprSubscript *subscript);
static reference_t eval_literal_list(NodeExprLiteralList *list);
static reference_t eval_literal_dict(NodeExprLiteralDict *dist);

//// AST EVALUATION ////

/*! Initialize the evaluation engine.
 *
 *  This function initializes the standard Python global singletons
 *  None, True and False, which are tied to to respective names and cannot
 *  be deleted. */
void eval_init() {
    /* Initialize an empty array of globals. */
    global_vars = NULL;
    num_vars = 0;
    max_vars = 0;

    NONE_REF  = make_reference_none();
    TRUE_REF  = make_reference_bool();
    FALSE_REF = make_reference_bool();
    if (exception_occurred()) {
        fprintf(stderr, "Failed to initialize evaluator\n");
        abort();
    }

    globals_set("None", NONE_REF);
    decref(NONE_REF);
    globals_set("True", TRUE_REF);
    decref(TRUE_REF);
    globals_set("False", FALSE_REF);
    decref(FALSE_REF);

    eval_types_init();
}

/*! Clean up the evaluator state. */
void eval_close() {
    for (size_t i = 0; i < num_vars; i++) {
        free(global_vars[i].name);
    }
    free(global_vars);
    global_vars = NULL;
}

/*!
 * Evaluates the provided AST node as a statement. If the AST node is a sequence
 * then returns a reference to the result of the final statement in the
 * sequence. If the AST node is an expression, then returns a reference to the
 * result of executing that expression.
 *
 * If an exception occurs during the evaluation of this AST node, then
 * NULL_REF will be returned and the exception will be set.
 *
 * If this function returns a reference, then it is a new reference to the
 * value. Otherwise, this function will return NULL_REF and the reference
 * should not be decremented.
 */
reference_t eval_root(Node *node) {
    assert(node != NULL);

    switch (node->type) {
        case STMT_SEQUENCE:
            return eval_stmt_sequence((NodeStmtSequence *) node);

        case STMT_ASSIGN:
            eval_stmt_assign((NodeStmtAssign *) node);
            break;

        case STMT_DEL:
            eval_stmt_del((NodeStmtDel *) node);
            break;

        case STMT_IF:
            eval_stmt_if((NodeStmtIf *) node);
            break;

        case STMT_WHILE:
            eval_stmt_while((NodeStmtWhile *) node);
            break;

        default:
            /* If this is not a statement, then attempt to evaluate it as a
             * expression in a regular context. */
            return eval_expr(node);
    }

    if (exception_occurred()) {
        return NULL_REF;
    }

    /* For statements that don't return a value, produce None. */
    incref(NONE_REF);
    return NONE_REF;
}

static reference_t eval_stmt_sequence(NodeStmtSequence *sequence) {
    reference_t result = NULL_REF;

    for (NodeListEntry *current = sequence->statements->head; current; current = current->next) {
        if (result != NULL_REF) {
            decref(result);
        }
        result = eval_root(current->node);
        if (exception_occurred()) {
            return NULL_REF;
        }
    }

    assert(result != NULL_REF);
    return result;
}

static void eval_stmt_assign(NodeStmtAssign *assign) {
    /* Determine what needs to happen based on the type of the left side of
     * the assignment. */
    switch (assign->left->type) {
        case EXPR_LITERAL_STRING:
        case EXPR_LITERAL_INTEGER:
        case EXPR_LITERAL_FLOAT:
        case EXPR_LITERAL_SINGLETON:
            exception_set(EXC_SYNTAX_ERROR, "can't assign to literal");
            break;

        case EXPR_LITERAL_DICT:
        case EXPR_LITERAL_LIST:
            exception_set(EXC_SYNTAX_ERROR, "assignment destructuring not implemented");
            break;

        case EXPR_IDENTIFIER: {
            reference_t right = eval_expr(assign->right);
            if (!exception_occurred()) {
                globals_set(((NodeExprIdentifier *) assign->left)->name, right);
                decref(right);
            }
            break;
        }

        case EXPR_NOT_TEST:
        case EXPR_AND_TEST:
        case EXPR_OR_TEST:
        case EXPR_BUILTIN:
            exception_set(EXC_SYNTAX_ERROR, "can't assign to operator");
            break;

        case EXPR_CALL:
            exception_set(EXC_SYNTAX_ERROR, "can't assign to function call");
            break;

        case EXPR_SUBSCRIPT: {
            NodeExprSubscript *subscript = (NodeExprSubscript *) assign->left;

            reference_t right = eval_expr(assign->right);
            if (!exception_occurred()) {
                reference_t target = eval_expr(subscript->obj);
                if (!exception_occurred()) {
                    reference_t index = eval_expr(subscript->index);
                    if (!exception_occurred()) {
                        ref_subscr_set(target, index, right);
                        decref(index);
                    }
                    decref(target);
                }
                decref(right);
            }

            break;
        }

        default:
            exception_set_format(EXC_INTERNAL,
                    "assignment target node type '%d' not implemented",
                    assign->left->type);
    }
}

static void eval_stmt_del(NodeStmtDel *del) {
    /* For the deletion statement, we need to check the type of the right
     * hand parse in order to know what to do. */
    switch (del->arg->type) {
        case EXPR_LITERAL_STRING:
        case EXPR_LITERAL_INTEGER:
        case EXPR_LITERAL_FLOAT:
        case EXPR_LITERAL_SINGLETON:
            exception_set(EXC_SYNTAX_ERROR, "can't delete literal");
            break;

        case EXPR_LITERAL_DICT:
        case EXPR_LITERAL_LIST:
            exception_set(EXC_SYNTAX_ERROR, "deletion destructuring not implemented");
            break;

        case EXPR_IDENTIFIER:
            globals_delete(((NodeExprIdentifier *) del->arg)->name);
            break;

        case EXPR_NOT_TEST:
        case EXPR_AND_TEST:
        case EXPR_OR_TEST:
        case EXPR_BUILTIN:
            exception_set(EXC_SYNTAX_ERROR, "can't delete operator");
            break;

        case EXPR_CALL:
            exception_set(EXC_SYNTAX_ERROR, "can't delete function call");
            break;

        case EXPR_SUBSCRIPT: {
            // TODO: REFCNT EXCEPTION
            NodeExprSubscript *subscript = (NodeExprSubscript *) del->arg;

            reference_t target = eval_expr(subscript->obj);
            if (!exception_occurred()) {
                reference_t index = eval_expr(subscript->index);
                if (!exception_occurred()) {
                    ref_subscr_del(target, index);
                    decref(index);
                }
                decref(target);
            }

            break;
        }

        default:
            exception_set_format(EXC_INTERNAL,
                    "deletion target node type '%d' not implemented",
                    del->arg->type);
    }
}

static void discard_eval(Node *node) {
    reference_t result = eval_root(node);
    if (!exception_occurred()) {
        decref(result);
    }
}

static void eval_stmt_if(NodeStmtIf *ifn) {
    /* First evaluate the condition and coerce it to a boolean. */
    reference_t cond_ref = eval_expr(ifn->cond);
    if (!exception_occurred()) {
        bool cond = ref_bool(cond_ref);
        decref(cond_ref);

        /* If the condition is true, then execute the body. */
        if (cond) {
            discard_eval(ifn->left);
        } else {
            Node *right = ifn->right;
            if (right != NULL) {
                discard_eval(ifn->right);
            }
        }
    }
}

static void eval_stmt_while(NodeStmtWhile *whilen) {
    Node *condition = whilen->cond,
         *body = whilen->body;
    while (true) {
        /* First evaluate the condition and coerce it to a boolean. */
        reference_t cond_ref = eval_expr(condition);
        if (exception_occurred()) {
            break;
        }

        bool cond = ref_bool(cond_ref);
        decref(cond_ref);
        if (!cond) {
            break;
        }

        discard_eval(body);
        if (exception_occurred()) {
            break;
        }
    }
}

/*!
 * This function takes an AST expression node and evaluates it, returning the
 * value that it generates as a reference. This returned reference is a new
 * reference to the result value.
 */
static reference_t eval_expr(Node *node) {
    assert(node != NULL);

    switch (node->type) {
        case EXPR_LITERAL_STRING:
            return make_reference_string(((NodeExprLiteralString *) node)->value);
        case EXPR_LITERAL_INTEGER:
            return make_reference_int(((NodeExprLiteralInteger *) node)->value);
        case EXPR_LITERAL_LIST:
            return eval_literal_list((NodeExprLiteralList *) node);
        case EXPR_LITERAL_DICT:
            return eval_literal_dict((NodeExprLiteralDict *) node);

        case EXPR_LITERAL_SINGLETON:
            return singleton_to_ref(((NodeExprLiteralSingleton *) node)->singleton);

        case EXPR_IDENTIFIER:
            return eval_identifier((NodeExprIdentifier *) node);

        case EXPR_NOT_TEST:
            return eval_not_test((NodeExprNotTest *) node);
        case EXPR_AND_TEST:
            return eval_and_test((NodeExprAndTest *) node);
        case EXPR_OR_TEST:
            return eval_or_test((NodeExprOrTest *) node);

        case EXPR_BUILTIN:
            return eval_builtin((NodeExprBuiltin *) node);
        case EXPR_CALL:
            return eval_call((NodeExprCall *) node);
        case EXPR_SUBSCRIPT:
            return eval_subscript((NodeExprSubscript *) node);

        default:
            exception_set_format(EXC_INTERNAL, "ast node type '%d' not implemented", node->type);
            return NULL_REF;
    }
}


static reference_t eval_identifier(NodeExprIdentifier *ident) {
    /* Push the loaded reference onto the stack. */
    return globals_get(ident->name);
}

static reference_t eval_not_test(NodeExprNotTest *test) {
    /* First evaluate the operand. */
    reference_t operand = eval_expr(test->operand);

    /* Ensure that no exception has occurred. */
    if (exception_occurred()) {
        return NULL_REF;
    }

    /* Then invert the truth value to get the result. */
    bool bool_value = ref_bool(operand);
    decref(operand);
    return bool_ref(!bool_value);
}

static reference_t eval_and_test(NodeExprAndTest *test) {
    /* First evaluate the first operand. */
    reference_t left = eval_expr(test->left);

    /* Ensure that no exception has occurred. */
    if (exception_occurred()) {
        return NULL_REF;
    }

    /* Check to see if we should short-circuit evaluation. */
    if (!ref_bool(left)) {
        return left;
    }

    decref(left);
    return eval_expr(test->right);
}

static reference_t eval_or_test(NodeExprOrTest *test) {
    /* First evaluate the first operand. */
    reference_t left = eval_expr(test->left);

    /* Ensure that no exception has occurred. */
    if (exception_occurred()) {
        return NULL_REF;
    }

    /* Check to see if we should short-circuit evaluation. */
    if (ref_bool(left)) {
        return left;
    }

    decref(left);
    return eval_expr(test->right);
}

static reference_t eval_builtin(NodeExprBuiltin *builtin) {
    /* First evaluate the operands to the builtin in order. */
    reference_t left = eval_expr(builtin->left);
    if (exception_occurred()) {
        return NULL_REF;
    }

    /* If this builtin is unary then the right operand will be NULL. */
    Node *right_expr = builtin->right;
    reference_t right;
    if (right_expr != NULL) {
        right = eval_expr(right_expr);
        if (exception_occurred()) {
            decref(left);
            return NULL_REF;
        }
    }

    /* Then, once we have the operands, we can dispatch to the particular
     * types being operated on. */
    NodeExprBuiltinType type = builtin->builtin_type;
    reference_t result;
    switch (type) {
        case MIN_UNARY_OP ... MAX_UNARY_OP:
            result = ref_unary(type, left);
            break;
        case MIN_BINARY_OP ... MAX_BINARY_OP:
            result = ref_binary(type, left, right);
            break;
        case COMP_EQUALS:
            result = bool_ref(ref_eq(left, right));
            break;
        default: { /* COMP_LT ... COMP_GE */
            bool compare_result = ref_compare(type, left, right);
            result = exception_occurred() ? NULL_REF : bool_ref(compare_result);
        }
    }

    decref(left);
    if (right_expr != NULL) {
        decref(right);
    }
    return result;
}

static reference_t eval_subscript(NodeExprSubscript *subscript) {
    /* First evaluate the subscript target and the subscript itself. */
    reference_t target = eval_expr(subscript->obj);
    if (exception_occurred()) {
        return NULL_REF;
    }

    reference_t index = eval_expr(subscript->index);
    if (exception_occurred()) {
        decref(target);
        return NULL_REF;
    }

    /* Then dispatch to the object for accessing the subscript value. */
    reference_t result = ref_subscr_get(target, index);
    decref(target);
    decref(index);

    return result;
}

static reference_t eval_literal_list(NodeExprLiteralList *list) {
    /* First figure out the number of elements. */
    size_t length = list->values ? ast_nodelist_length(list->values) : 0;

    /* Then allocate space for the references. */
    reference_t ref_array = make_reference_refarray(length);
    if (exception_occurred()) {
        return NULL_REF;
    }
    ref_array_value_t *values = (ref_array_value_t *) deref(ref_array);

    /* Then create a new list. */
    reference_t ref_list = make_reference_list(ref_array);
    if (exception_occurred()) {
        decref(ref_array);
        return NULL_REF;
    }

    /* Then compute and store the elements. */
    if (length > 0) {
        list_value_t *list_value = (list_value_t *) deref(ref_list);
        for (NodeListEntry *entry = list->values->head; entry; entry = entry->next) {
            values->values[list_value->size++] = eval_expr(entry->node);
            if (exception_occurred()) {
                list_value->size--;
                decref(ref_list);
                return NULL_REF;
            }
        }
    }

    /* If all is well, then we have a new list. */
    return ref_list;
}

static reference_t eval_literal_dict(NodeExprLiteralDict *dict) {
    /* First figure out the number of elements. */
    size_t length = dict->keys ? ast_nodelist_length(dict->keys) : 0;

    /* Initial capacity should be twice the number of elements, minimum 16. */
    size_t capacity = length * 2;
    if (capacity < 16) {
        capacity = 16;
    }

    /* Then allocate space for the key references. */
    reference_t ref_key_array = make_reference_refarray(capacity);
    if (exception_occurred()) {
        return NULL_REF;
    }

    /* Then allocate space for the value references. */
    reference_t ref_value_array = make_reference_refarray(capacity);
    if (exception_occurred()) {
        decref(ref_key_array);
        return NULL_REF;
    }

    /* Then create a new dict. */
    reference_t ref_dict = make_reference_dict(ref_key_array, ref_value_array);
    if (exception_occurred()) {
        decref(ref_key_array);
        decref(ref_value_array);
        return NULL_REF;
    }

    /* Then compute and store the elements. */
    if (length > 0) {
        NodeListEntry *key_entry = dict->keys->head,
                      *value_entry = dict->values->head;
        while (key_entry != NULL) {
            assert(value_entry != NULL);
            reference_t key = eval_expr(key_entry->node);
            if (!exception_occurred()) {
                reference_t value = eval_expr(value_entry->node);
                if (!exception_occurred()) {
                    ref_subscr_set(ref_dict, key, value);
                    decref(value);
                }
                decref(key);
            }

            if (exception_occurred()) {
                decref(ref_dict);
                return NULL_REF;
            }

            key_entry = key_entry->next;
            value_entry = value_entry->next;
        }
    }

    return ref_dict;
}

static reference_t eval_call_exit(size_t arity, reference_t *args) {
    if (arity > 1) {
        exception_set_format(EXC_TYPE_ERROR,
                "exit() takes from 0 to 1 positional arguments but %d were given", arity);
        return NULL_REF;
    }

    int code;
    if (arity > 0) {
        reference_t code_reference = args[0];
        value_t *code_value = deref(code_reference);

        if (code_value->type == VAL_INTEGER) {
            code = ((integer_value_t *) code_value)->integer_value;
        } else {
            ref_println(code_reference, stderr, MAX_DEPTH);
            code = 1;
        }
    } else {
        code = 0;
    }

    exit(code);
}

static reference_t eval_call_mem(size_t arity, reference_t *args) {
    (void) args;

    if (arity > 0) {
        exception_set_format(EXC_TYPE_ERROR,
                "mem() takes 0 positional arguments but %d were given", arity);
        return NULL_REF;
    }

    printf("%zu bytes in use; %zu refs in use\n", mem_used(), refs_used());

    incref(NONE_REF);
    return NONE_REF;
}

static reference_t eval_call_gc(size_t arity, reference_t *args) {
    (void) args;

    if (arity > 0) {
        exception_set_format(EXC_TYPE_ERROR,
                "gc() takes 0 positional arguments but %d were given", arity);
        return NULL_REF;
    }

    collect_garbage();

    incref(NONE_REF);
    return NONE_REF;
}

static reference_t eval_call_print(size_t arity, reference_t *args) {
    for (size_t i = 0; i < arity; i++) {
        if (i > 0) {
            fprintf(stdout, " ");
        }
        ref_print(args[i], stdout, MAX_DEPTH);
    }

    fprintf(stdout, "\n");

    incref(NONE_REF);
    return NONE_REF;
}

static reference_t eval_call_len(size_t arity, reference_t *args) {
    if (arity != 1) {
        exception_set_format(EXC_TYPE_ERROR,
                "len() takes 1 positional argument but %d were given", arity);
        return NULL_REF;
    }

    return make_reference_int(ref_len(args[0]));
}

static reference_t eval_call_bool(size_t arity, reference_t *args) {
    if (arity != 1) {
        exception_set_format(EXC_TYPE_ERROR,
                "bool() takes 1 positional argument but %d were given", arity);
        return NULL_REF;
    }

    return bool_ref(ref_bool(args[0]));
}

static reference_t eval_call(NodeExprCall *node) {
    /* First check to ensure this is a valid function call. */
    if (node->func->type != EXPR_IDENTIFIER) {
        exception_set(EXC_SYNTAX_ERROR, "calling non-identifiers not supported");
    }

    /* Compute function arity and arguments. */
    size_t arity = node->args ? ast_nodelist_length(node->args) : 0;

    /* Allocate space for arguments. */
    reference_t args[arity];
    size_t idx = 0;
    if (node->args) {
        for (NodeListEntry *entry = node->args->head; entry; entry = entry->next, idx++) {
            args[idx] = eval_expr(entry->node);
            if (exception_occurred()) {
                break;
            }
        }
    }

    reference_t result = NULL_REF;
    if (!exception_occurred()) {
        const char *name = ((NodeExprIdentifier *) node->func)->name;

        /* For the time being, it's enough to just hardcode the list of available
         * functions. */
        if (strcmp(name, "exit") == 0 || strcmp(name, "quit") == 0) {
            result = eval_call_exit(arity, args);
        } else if (strcmp(name, "mem") == 0) {
            result = eval_call_mem(arity, args);
        } else if (strcmp(name, "gc") == 0) {
            result = eval_call_gc(arity, args);
        } else if (strcmp(name, "print") == 0) {
            result = eval_call_print(arity, args);
        } else if (strcmp(name, "len") == 0) {
            result = eval_call_len(arity, args);
        } else if (strcmp(name, "bool") == 0) {
            result = eval_call_bool(arity, args);
        } else {
            exception_set_format(EXC_NAME_ERROR, "no such function '%s'", name);
        }
    }

    for (size_t ridx = 0; ridx < idx; ridx++) {
        decref(args[ridx]);
    }

    return result;
}

//// GLOBAL VAR FUNCTIONS ////

/*!
 * Tries to retrieve a global variable's reference. The returned reference is
 * a new reference to the stored value.
 */
static reference_t globals_get(const char *name) {
    for (size_t i = 0; i < num_vars; i++) {
        char *var_name = global_vars[i].name;
        if (var_name != NULL && strcmp(var_name, name) == 0) {
            reference_t ref = global_vars[i].ref;
            incref(ref);
            return ref;
        }
    }

    exception_set_format(EXC_NAME_ERROR, "name '%s' is not defined", name);
    return NULL_REF;
}

/*! Tries to set a global variable's reference, creating it if it does not exist. */
static void globals_set(const char *name, reference_t value) {
    /* Look for the existing variable in the globals array. */
    for (size_t i = 0; i < num_vars; i++) {
        char *var_name = global_vars[i].name;
        if (var_name != NULL && strcmp(var_name, name) == 0) {
            decref(global_vars[i].ref);
            incref(value);
            global_vars[i].ref = value;
            return;
        }
    }

    /* If we are out of space, increase the size of the globals array. */
    if (num_vars == max_vars) {
        /* Double its size (the JVM internal source said this
         * was a good resizing semantic, don't sue me!), and zero it out. */
        max_vars = max_vars == 0 ? INITIAL_SIZE : max_vars * 2;
        global_vars = realloc(global_vars, sizeof(global_variable_t[max_vars]));
        if (global_vars == NULL) {
            exception_set(EXC_INTERNAL, "allocation of global variable array failed");
            return;
        }
    }

    /* Add the new variable to the end of the globals array. */
    global_vars[num_vars].name = strdup(name);
    incref(value);
    global_vars[num_vars].ref = value;
    num_vars++;
}

/*! Delete the global variable with name `name`. Error if no such variable
    exists. */
static void globals_delete(const char *name) {
    for (size_t i = 0; i < num_vars; i++) {
        char *var_name = global_vars[i].name;
        if (strcmp(var_name, name) == 0) {
            // Found the variable.  Remove it by sliding the whole array down.
            free(var_name);
            decref(global_vars[i].ref);

            num_vars--;
            for (size_t j = i; j < num_vars; j++) {
                global_vars[j] = global_vars[j + 1];
            }

            global_vars[num_vars].name = NULL;
            global_vars[num_vars].ref = NULL_REF;
            return;
        }
    }

    exception_set_format(EXC_NAME_ERROR, "name '%s' is not defined", name);
}

size_t foreach_global(void (*f)(char *name, reference_t ref)) {
    /* Call the callback on each global. */
    for (size_t i = 0; i < num_vars; i++) {
        f(global_vars[i].name, global_vars[i].ref);
    }

    return num_vars;
}

static void print_global(char *name, reference_t ref) {
    fprintf(stdout, "%s = ref %d; value ", name, ref);
    ref_println_repr(ref, stdout, MAX_DEPTH);
}

void print_globals(void) {
    fprintf(stdout, "%zu Globals:\n", num_vars);
    foreach_global(print_global);
}


