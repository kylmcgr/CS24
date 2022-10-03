#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "directory_tree.h"
const size_t ARRAY_SIZE = 1000;

void init_node(node_t *node, char *name, node_type_t type) {
    if (name == NULL) {
        name = strdup("ROOT");
        assert(name != NULL);
    }
    node->name = name;
    node->type = type;
}

file_node_t *init_file_node(char *name, size_t size, uint8_t *contents) {
    file_node_t *node = malloc(sizeof(file_node_t));
    assert(node != NULL);
    init_node((node_t *) node, name, FILE_TYPE);
    node->size = size;
    node->contents = contents;
    return node;
}

directory_node_t *init_directory_node(char *name) {
    directory_node_t *node = malloc(sizeof(directory_node_t));
    assert(node != NULL);
    init_node((node_t *) node, name, DIRECTORY_TYPE);
    node->num_children = 0;
    node->children = NULL;
    return node;
}

void add_child_directory_tree(directory_node_t *dnode, node_t *child) {
    dnode->num_children++;
    dnode->children = realloc(dnode->children, sizeof(node_t *)*dnode->num_children);
    node_t *curr = child;
    for (size_t i = 0; i < dnode->num_children - 1; i++) {
        if (strcmp(curr->name, dnode->children[i]->name) < 0){
            node_t *temp = dnode->children[i];
            dnode->children[i] = curr;
            curr = temp;
        }
    }
    dnode->children[dnode->num_children - 1] = curr;
}

void print_directory_tree_helper(int depth, node_t *node) {
    printf("%*s%s\n", depth*4, "", node->name);
    if (node->type == DIRECTORY_TYPE && ((directory_node_t *) node)->num_children > 0){
        for (size_t i = 0; i < ((directory_node_t *) node)->num_children; i++){
            print_directory_tree_helper(depth + 1, ((directory_node_t *) node)->children[i]);
        }
    }
}

void print_directory_tree(node_t *node) {
    print_directory_tree_helper(0, node);
}

void create_directory_tree_helper(char *path, node_t *node) {
    char temp[ARRAY_SIZE] = "";
    strcat(strcpy(temp, path), node->name);
    if (node->type == DIRECTORY_TYPE){
        directory_node_t *directory_node = (directory_node_t *) node;
        assert(mkdir(temp, 0777) == 0);
        if (directory_node->num_children > 0){
            strcat(temp, "/");
            for (size_t i = 0; i < directory_node->num_children; i++){
                create_directory_tree_helper(temp, directory_node->children[i]);
            }
        }
    } else {
        FILE *file = fopen(temp, "w");
        file_node_t *file_node = (file_node_t *) node;
        size_t written = fwrite(file_node->contents, file_node->size, 1, file);
        assert(written == 1);
        int close_result = fclose(file);
        assert(close_result == 0);
    }
}

void create_directory_tree(node_t *node) {
    char path[ARRAY_SIZE];
    if (node->type == DIRECTORY_TYPE){
        directory_node_t *directory_node = (directory_node_t *) node;
        assert(mkdir(node->name, 0777) == 0);
        if (directory_node->num_children > 0){
            for (size_t i = 0; i < directory_node->num_children; i++){
                create_directory_tree_helper(strcat(strcpy(path, node->name), "/"), directory_node->children[i]);
            }
        }
    } else {
        FILE *file = fopen(node->name, "w");
        file_node_t *file_node = (file_node_t *) node;
        size_t written = fwrite(file_node->contents, file_node->size, 1, file);
        assert(written == 1);
        int close_result = fclose(file);
        assert(close_result == 0);
    }
}

void free_directory_tree(node_t *node) {
    if (node->type == FILE_TYPE) {
        file_node_t *fnode = (file_node_t *) node;
        free(fnode->contents);
    }
    else {
        assert(node->type == DIRECTORY_TYPE);
        directory_node_t *dnode = (directory_node_t *) node;
        for (size_t i = 0; i < dnode->num_children; i++) {
            free_directory_tree(dnode->children[i]);
        }
        free(dnode->children);
    }
    free(node->name);
    free(node);
}
