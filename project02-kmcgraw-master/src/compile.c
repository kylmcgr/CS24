#include "compile.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int if_counter = 0;
int while_counter = 0;

typedef struct {
  /** Whether this returned value is an int */
    bool has_value;
  /** The returned value (only valid if `has_value` is true) */
    int64_t value;
} optional_int_t;

optional_int_t arithmetic(binary_node_t *bin) {
    optional_int_t result = {.has_value = true};
    optional_int_t temp = {.has_value = false};
    int64_t left = 0;
    int64_t right = 0;
    if (bin->left->type == BINARY_OP){
        temp = arithmetic(((binary_node_t *) bin->left));
        if (temp.has_value){
            left = temp.value;
            temp.has_value = false;
        } else {
            return temp;
        }
    } else if (bin->left->type == NUM) {
        left = ((num_node_t *) bin->left)->value;
    } else {
        return temp;
    }
    if (bin->right->type == BINARY_OP){
        temp = arithmetic(((binary_node_t *) bin->right));
        if (temp.has_value){
            right = temp.value;
        } else {
            return temp;
        }
    } else if (bin->right->type == NUM) {
        right = ((num_node_t *) bin->right)->value;
    } else {
        return temp;
    }
    if (bin->op == '+') {
        result.value = left + right;
    } else if (bin->op == '-') {
        result.value = left - right;
    } else if (bin->op == '*') {
        result.value = left * right;
    } else if (bin->op == '/') {
        result.value = left / right;
    }
    return result;
}

bool compile_ast(node_t *node) {
    if (node->type == NUM){
        printf("movq $%ld, %%rax\n", ((num_node_t *) node)->value);
    } else if (node->type == PRINT) {
        compile_ast(((print_node_t *) node)->expr);
        printf("movq %%rax, %%rdi\n");
        printf("callq print_int\n");
    } else if (node->type == SEQUENCE) {
        sequence_node_t *sequence = (sequence_node_t *) node;
        for (size_t i = 0; i < sequence->statement_count; i++) {
            compile_ast(sequence->statements[i]);
        }
    } else if (node->type == BINARY_OP) {
        binary_node_t *bin = (binary_node_t *) node;
        optional_int_t result = {.has_value = false};
        if (bin->op == '+' || bin->op == '-' || bin->op == '*' || bin->op == '/') {
            result = arithmetic(bin);
        }
        if (result.has_value){
            printf("movq $%ld, %%rax\n", result.value);
        } else {
            compile_ast(bin->right);
            printf("push %%rax\n");
            compile_ast(bin->left);
            printf("pop %%r10\n");
            if (bin->op == '+') {
                printf("addq %%r10, %%rax\n");
            } else if (bin->op == '-') {
                printf("subq %%r10, %%rax\n");
            } else if (bin->op == '*') {
                if (bin->right->type == NUM && ((num_node_t *) bin->right)->value > 0) {
                    float j = log2(((num_node_t *) bin->right)->value);
                    int k = (int) j;
                    if (k == j) {
                        printf("shlq $%d, %%rax\n", k);
                    } else {
                        printf("imulq %%r10, %%rax\n");
                    }
                } else {
                    printf("imulq %%r10, %%rax\n");
                }
            } else if (bin->op == '/') {
                printf("cqto\n");
                printf("idiv %%r10\n");
            } else if (bin->op == '=') {
                printf("cmp %%r10, %%rax\n");
            } else if (bin->op == '<') {
                printf("cmp %%r10, %%rax\n");
            } else if (bin->op == '>') {
                printf("cmp %%r10, %%rax\n");
            }
        }
    } else if (node->type == VAR) {
        printf("movq -%d(%%rbp), %%rax\n", 8 * (((var_node_t *) node)->name - 'A' + 1));
    } else if (node->type == LET) {
        let_node_t *let = (let_node_t *) node;
        compile_ast(let->value);
        printf("movq %%rax,-%d(%%rbp)\n", 8 * (let->var - 'A' + 1));
    } else if (node->type == IF) {
        if_node_t *if_node = (if_node_t *) node;
        if_counter++;
        int if_cur = if_counter;
        compile_ast(if_node->condition);
        if (((binary_node_t *) if_node->condition)->op == '='){
            printf("je IF_TRUE_%d\n", if_cur);
        }
        if (((binary_node_t *) if_node->condition)->op == '<'){
            printf("jl IF_TRUE_%d\n", if_cur);
        }
        if (((binary_node_t *) if_node->condition)->op == '>'){
            printf("jg IF_TRUE_%d\n", if_cur);
        }
        if (if_node->else_branch != NULL) {
            compile_ast(if_node->else_branch);
        }
        printf("jmp IF_SKIP_%d\n", if_cur);
        printf("IF_TRUE_%d:\n", if_cur);
        compile_ast(if_node->if_branch);
        printf("IF_SKIP_%d:\n", if_cur);
    } else if (node->type == WHILE) {
        while_node_t *while_node = (while_node_t *) node;
        while_counter++;
        int while_cur = while_counter;
        printf("WHILE_%d:\n", while_cur);
        compile_ast(while_node->condition);
        if (((binary_node_t *) while_node->condition)->op == '='){
            printf("jne WHILE_SKIP_%d\n", while_cur);
        }
        if (((binary_node_t *) while_node->condition)->op == '<'){
            printf("jge WHILE_SKIP_%d\n", while_cur);
        }
        if (((binary_node_t *) while_node->condition)->op == '>'){
            printf("jle WHILE_SKIP_%d\n", while_cur);
        }
        compile_ast(while_node->body);
        printf("jmp WHILE_%d\n", while_cur);
        printf("WHILE_SKIP_%d:\n", while_cur);
    }
    return true;
}
