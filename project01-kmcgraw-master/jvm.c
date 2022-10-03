#include "jvm.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "read_class.h"

const int LOAD = 26;
const int STORE = 59;

/** The name of the method to invoke to run the class file */
const char MAIN_METHOD[] = "main";
/**
 * The "descriptor" string for main(). The descriptor encodes main()'s signature,
 * i.e. main() takes a String[] and returns void.
 * If you're interested, the descriptor string is explained at
 * https://docs.oracle.com/javase/specs/jvms/se12/html/jvms-4.html#jvms-4.3.2.
 */
const char MAIN_DESCRIPTOR[] = "([Ljava/lang/String;)V";

/**
 * Represents the return value of a Java method: either void or an int.
 * (In a real JVM, methods could also return object references or other primitives.)
 */
typedef struct {
    /** Whether this returned value is an int */
    bool has_value;
    /** The returned value (only valid if `has_value` is true) */
    int32_t value;
} optional_int_t;

/**
 * Runs a method's instructions until the method returns.
 *
 * @param method the method to run
 * @param locals the array of local variables, including the method parameters.
 *   Except for parameters, the locals are uninitialized.
 * @param class the class file the method belongs to
 * @return an optional int containing the method's return value
 */
optional_int_t execute(method_t *method, int32_t *locals, class_file_t *class) {
    int pc = 0;
    int stack_length = 0;
    int32_t *stack = calloc(method->code.max_stack, sizeof(int));
    optional_int_t result = {.has_value = false};
    while (pc < (int) method->code.code_length) {
        if (method->code.code[pc] == i_bipush) {
            stack[stack_length] = (int32_t) (int8_t) method->code.code[pc + 1];
            stack_length++;
            pc += 2;
        }
        else if (method->code.code[pc] == i_return) {
            break;
        }
        else if (method->code.code[pc] == i_getstatic) {
            pc += 3;
        }
        else if (method->code.code[pc] == i_invokevirtual) {
            stack_length--;
            printf("%d\n", stack[stack_length]);
            pc += 3;
        }
        else if (method->code.code[pc] >= i_iconst_m1 &&
                 method->code.code[pc] <= i_iconst_5) {
            stack[stack_length] = method->code.code[pc] - 3;
            stack_length++;
            pc += 1;
        }
        else if (method->code.code[pc] == i_sipush) {
            stack[stack_length] =
                (int16_t)(method->code.code[pc + 1] << 8) | method->code.code[pc + 2];
            stack_length++;
            pc += 3;
        }
        else if (method->code.code[pc] == i_iadd) {
            stack_length--;
            stack[stack_length - 1] += stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_isub) {
            stack_length--;
            stack[stack_length - 1] = stack[stack_length - 1] - stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_imul) {
            stack_length--;
            stack[stack_length - 1] = stack[stack_length - 1] * stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_idiv) {
            stack_length--;
            assert(stack[stack_length] != 0);
            stack[stack_length - 1] = stack[stack_length - 1] / stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_irem) {
            stack_length--;
            assert(stack[stack_length] != 0);
            stack[stack_length - 1] = stack[stack_length - 1] % stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_ineg) {
            stack[stack_length - 1] = -stack[stack_length - 1];
            pc += 1;
        }
        else if (method->code.code[pc] == i_ishl) {
            stack_length--;
            stack[stack_length - 1] = stack[stack_length - 1] << stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_ishr) {
            stack_length--;
            stack[stack_length - 1] = stack[stack_length - 1] >> stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_iushr) {
            stack_length--;
            stack[stack_length - 1] =
                ((uint32_t) stack[stack_length - 1]) >> stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_iand) {
            stack_length--;
            stack[stack_length - 1] = stack[stack_length - 1] & stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_ior) {
            stack_length--;
            stack[stack_length - 1] |= stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_ixor) {
            stack_length--;
            stack[stack_length - 1] = stack[stack_length - 1] ^ stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_iload) {
            stack[stack_length] = locals[method->code.code[pc + 1]];
            stack_length++;
            pc += 2;
        }
        else if (method->code.code[pc] >= i_iload_0 &&
                 method->code.code[pc] <= i_iload_3) {
            stack[stack_length] = locals[method->code.code[pc] - LOAD];
            stack_length++;
            pc += 1;
        }
        else if (method->code.code[pc] == i_istore) {
            stack_length--;
            locals[method->code.code[pc + 1]] = stack[stack_length];
            pc += 2;
        }
        else if (method->code.code[pc] >= i_istore_0 &&
                 method->code.code[pc] <= i_istore_3) {
            stack_length--;
            locals[method->code.code[pc] - STORE] = stack[stack_length];
            pc += 1;
        }
        else if (method->code.code[pc] == i_iinc) {
            locals[method->code.code[pc + 1]] += (int8_t) method->code.code[pc + 2];
            pc += 3;
        }
        else if (method->code.code[pc] == i_ldc) {
            stack[stack_length] =
                ((CONSTANT_Integer_info
                      *) class->constant_pool[method->code.code[pc + 1] - 1]
                     .info)
                    ->bytes;
            stack_length++;
            pc += 2;
        }
        else if ((method->code.code[pc] == i_ifeq && stack[stack_length - 1] == 0) ||
                 (method->code.code[pc] == i_ifne && stack[stack_length - 1] != 0) ||
                 (method->code.code[pc] == i_iflt && stack[stack_length - 1] < 0) ||
                 (method->code.code[pc] == i_ifge && stack[stack_length - 1] >= 0) ||
                 (method->code.code[pc] == i_ifgt && stack[stack_length - 1] > 0) ||
                 (method->code.code[pc] == i_ifle && stack[stack_length - 1] <= 0)) {
            stack_length--;
            pc += (int16_t)(method->code.code[pc + 1] << 8) | method->code.code[pc + 2];
        }
        else if ((method->code.code[pc] == i_if_icmpeq &&
                  stack[stack_length - 2] == stack[stack_length - 1]) ||
                 (method->code.code[pc] == i_if_icmpne &&
                  stack[stack_length - 2] != stack[stack_length - 1]) ||
                 (method->code.code[pc] == i_if_icmplt &&
                  stack[stack_length - 2] < stack[stack_length - 1]) ||
                 (method->code.code[pc] == i_if_icmpge &&
                  stack[stack_length - 2] >= stack[stack_length - 1]) ||
                 (method->code.code[pc] == i_if_icmpgt &&
                  stack[stack_length - 2] > stack[stack_length - 1]) ||
                 (method->code.code[pc] == i_if_icmple &&
                  stack[stack_length - 2] <= stack[stack_length - 1])) {
            stack_length -= 2;
            pc += (int16_t)(method->code.code[pc + 1] << 8) | method->code.code[pc + 2];
        }
        else if (method->code.code[pc] == i_goto) {
            pc += (int16_t)(method->code.code[pc + 1] << 8) | method->code.code[pc + 2];
        }
        else if (method->code.code[pc] == i_ifeq || method->code.code[pc] == i_ifne ||
                 method->code.code[pc] == i_iflt || method->code.code[pc] == i_ifge ||
                 method->code.code[pc] == i_ifgt || method->code.code[pc] == i_ifle) {
            stack_length--;
            pc += 3;
        }
        else if (method->code.code[pc] == i_if_icmpeq ||
                 method->code.code[pc] == i_if_icmpne ||
                 method->code.code[pc] == i_if_icmplt ||
                 method->code.code[pc] == i_if_icmpge ||
                 method->code.code[pc] == i_if_icmpgt ||
                 method->code.code[pc] == i_if_icmple) {
            stack_length -= 2;
            pc += 3;
        }
        else if (method->code.code[pc] == i_ireturn) {
            stack_length--;
            result.has_value = true;
            result.value = stack[stack_length];
            break;
        }
        else if (method->code.code[pc] == i_invokestatic) {
            method_t *new_method = find_method_from_index(
                (int16_t)(method->code.code[pc + 1] << 8) | method->code.code[pc + 2],
                class);
            int num_locals = get_number_of_parameters(new_method);
            int *new_locals = malloc(sizeof(int) * new_method->code.max_locals);
            for (int i = stack_length - num_locals; i < stack_length; i++) {
                new_locals[i - (stack_length - num_locals)] = stack[i];
            }
            stack_length -= num_locals;
            optional_int_t result = execute(new_method, new_locals, class);
            free(new_locals);
            if (result.has_value) {
                stack[stack_length] = result.value;
                stack_length++;
            }
            pc += 3;
        }
    }

    free(stack);
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <class file>\n", argv[0]);
        return 1;
    }

    // Open the class file for reading
    FILE *class_file = fopen(argv[1], "r");
    assert(class_file != NULL && "Failed to open file");

    // Parse the class file
    class_file_t *class = get_class(class_file);
    int error = fclose(class_file);
    assert(error == 0 && "Failed to close file");

    // Execute the main method
    method_t *main_method = find_method(MAIN_METHOD, MAIN_DESCRIPTOR, class);
    assert(main_method != NULL && "Missing main() method");
    /* In a real JVM, locals[0] would contain a reference to String[] args.
     * But since TeenyJVM doesn't support Objects, we leave it uninitialized. */
    int32_t locals[main_method->code.max_locals];
    optional_int_t result = execute(main_method, locals, class);
    assert(!result.has_value && "main() should return void");

    // Free the internal data structures
    free_class(class);
}
