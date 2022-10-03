#include <inttypes.h>
#include <stdio.h>

#include "ast.h"

// A function that can be called from assembly to print an integer
void print_int(value_t value) {
    printf("%" PRId64 "\n", value);
    // Clobber all caller-save registers
    asm("movq $0x6A2CFE91073BD845, %%rax\n"
        "movq $0x03BAD7C14F2E6589, %%rdi\n"
        "movq $0x5D41EA960C72F8B3, %%rsi\n"
        "movq $0xEC364B2D5A7F9810, %%rdx\n"
        "movq $0xFC85AD49320BE167, %%rcx\n"
        "movq $0x529A48CDB7163E0F, %%r8\n"
        "movq $0x92E1587A4BDCF630, %%r9\n"
        "movq $0x47DC36501F89AEB2, %%r10\n"
        "movq $0xAF4B29785C61ED30, %%r11\n"
        :
        :
        : "rax", "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11");
}
