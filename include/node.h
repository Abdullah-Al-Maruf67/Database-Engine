#ifndef NODE_H
#define NODE_H

#include "common.h"
#include "row.h"
#include "pager.h"

typedef struct Table_t Table;
typedef struct Cursor_t Cursor;

typedef enum { NODE_INTERNAL, NODE_LEAF } NodeType;

typedef struct {
  uint32_t primary_key;
  char data[0]; // Variable length value
} IndexEntry;

typedef struct {
  uint16_t offset;
  uint16_t length;
} Slot;


void leaf_node_insert_index(Cursor* cursor, uint32_t primary_key, void* data, uint32_t length);
void internal_node_insert_index(Table* table, uint32_t parent_page_num, uint32_t child_page_num);


/*
 * Common Node Header Layout
 */
#define NODE_TYPE_SIZE sizeof(uint8_t)
#define NODE_TYPE_OFFSET 0
#define IS_ROOT_SIZE sizeof(uint8_t)
#define IS_ROOT_OFFSET NODE_TYPE_SIZE
#define PARENT_POINTER_SIZE sizeof(uint32_t)
#define PARENT_POINTER_OFFSET (IS_ROOT_OFFSET + IS_ROOT_SIZE)
#define COMMON_NODE_HEADER_SIZE (NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE)

/*
 * Leaf Node Header Layout
 */
#define LEAF_NODE_NUM_CELLS_SIZE sizeof(uint32_t)
#define LEAF_NODE_NUM_CELLS_OFFSET COMMON_NODE_HEADER_SIZE
#define LEAF_NODE_NEXT_LEAF_SIZE sizeof(uint32_t)
#define LEAF_NODE_NEXT_LEAF_OFFSET (LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE)
#define LEAF_NODE_FREE_SPACE_POINTER_SIZE sizeof(uint32_t)
#define LEAF_NODE_FREE_SPACE_POINTER_OFFSET (LEAF_NODE_NEXT_LEAF_OFFSET + LEAF_NODE_NEXT_LEAF_SIZE)
#define LEAF_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE + LEAF_NODE_FREE_SPACE_POINTER_SIZE)

/*
 * Leaf Node Body Layout
 */
#define LEAF_NODE_KEY_SIZE sizeof(uint32_t)
#define LEAF_NODE_SLOT_SIZE sizeof(Slot)

/*
 * Internal Node Header Layout
 */
#define INTERNAL_NODE_NUM_KEYS_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_NUM_KEYS_OFFSET COMMON_NODE_HEADER_SIZE
#define INTERNAL_NODE_RIGHT_CHILD_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)
#define INTERNAL_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE)

/*
 * Internal Node Body Layout
 */
#define INTERNAL_NODE_KEY_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_CHILD_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_CELL_SIZE (INTERNAL_NODE_KEY_SIZE + INTERNAL_NODE_CHILD_SIZE)
#define INTERNAL_NODE_SPACE_FOR_CELLS (PAGE_SIZE - INTERNAL_NODE_HEADER_SIZE)
#define INTERNAL_NODE_MAX_KEYS (INTERNAL_NODE_SPACE_FOR_CELLS / INTERNAL_NODE_CELL_SIZE)

#define INTERNAL_NODE_RIGHT_SPLIT_COUNT ((INTERNAL_NODE_MAX_KEYS + 1) / 2)
#define INTERNAL_NODE_LEFT_SPLIT_COUNT ((INTERNAL_NODE_MAX_KEYS + 1) - INTERNAL_NODE_RIGHT_SPLIT_COUNT)

uint32_t* leaf_node_num_cells(void* node);
uint32_t* leaf_node_next_leaf(void* node);
uint32_t* leaf_node_free_space_pointer(void* node);
Slot* leaf_node_slot(void* node, uint32_t slot_num);
uint32_t* leaf_node_key(void* node, uint32_t slot_num);
void* leaf_node_value(void* node, uint32_t slot_num);
void initialize_leaf_node(void* node);
bool leaf_node_has_space(void* node, uint32_t required_size);


uint32_t* node_parent(void* node);
bool is_node_root(void* node);
void set_node_root(void* node, bool is_root);

NodeType get_node_type(void* node);
uint32_t get_node_max_key(void* node);

uint32_t* internal_node_num_keys(void* node);
uint32_t* internal_node_right_child(void* node);
uint32_t* internal_node_cell(void* node, uint32_t cell_num);
uint32_t* internal_node_child(void* node, uint32_t child_num);
uint32_t* internal_node_key(void* node, uint32_t key_num);

void initialize_internal_node(void* node);
uint32_t leaf_node_find_cell(void* node, uint32_t key);
uint32_t internal_node_find_child(void* node, uint32_t key);

void internal_node_insert(Table* table, uint32_t parent_page_num, uint32_t child_page_num);
void internal_node_split_and_insert(Table* table, uint32_t parent_page_num, uint32_t child_page_num);
void update_internal_node_key(void* node, uint32_t old_key, uint32_t new_key);
void create_new_root(Table* table, uint32_t right_child_page_num);

void leaf_node_insert(Cursor* cursor, uint32_t key, void* data, uint32_t length);
void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, void* data, uint32_t length);


void print_constants(FILE* out);
void print_leaf_node(void* node, FILE* out);
void print_tree(Pager* pager, uint32_t page_num, uint32_t indentation_level, FILE* out);


#endif
