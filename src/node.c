#include "node.h"
#include "table.h"

uint32_t* leaf_node_num_cells(void* node) {
  return node + LEAF_NODE_NUM_CELLS_OFFSET;
}

uint32_t* leaf_node_next_leaf(void* node) {
  return node + LEAF_NODE_NEXT_LEAF_OFFSET;
}

uint32_t* leaf_node_free_space_pointer(void* node) {
  return node + LEAF_NODE_FREE_SPACE_POINTER_OFFSET;
}

Slot* leaf_node_slot(void* node, uint32_t slot_num) {
  return node + LEAF_NODE_HEADER_SIZE + slot_num * LEAF_NODE_SLOT_SIZE;
}

uint32_t* leaf_node_key(void* node, uint32_t slot_num) {
  // For index nodes, the 'key' is the start of the data. 
  // For data nodes, it's the primary key ID.
  return (uint32_t*)leaf_node_value(node, slot_num);
}


void* leaf_node_value(void* node, uint32_t slot_num) {
  Slot* slot = leaf_node_slot(node, slot_num);
  return node + slot->offset;
}

void initialize_leaf_node(void* node) {
  *(uint8_t*)(node + NODE_TYPE_OFFSET) = NODE_LEAF;
  *(uint8_t*)(node + IS_ROOT_OFFSET) = 0;
  *leaf_node_num_cells(node) = 0;
  *leaf_node_next_leaf(node) = 0;
  *leaf_node_free_space_pointer(node) = PAGE_SIZE;
  *node_parent(node) = 0;
}

uint32_t* node_parent(void* node) { return node + PARENT_POINTER_OFFSET; }

bool is_node_root(void* node) {
  uint8_t value = *((uint8_t*)(node + IS_ROOT_OFFSET));
  return (bool)value;
}

void set_node_root(void* node, bool is_root) {
  uint8_t value = is_root;
  *((uint8_t*)(node + IS_ROOT_OFFSET)) = value;
}

NodeType get_node_type(void* node) {
  uint8_t value = *((uint8_t*)(node + NODE_TYPE_OFFSET));
  return (NodeType)value;
}

uint32_t get_node_max_key(void* node) {
  switch (get_node_type(node)) {
    case NODE_INTERNAL:
      return *internal_node_key(node, *internal_node_num_keys(node) - 1);
    case NODE_LEAF:
      return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
  }
  return 0;
}

uint32_t* internal_node_num_keys(void* node) {
  return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}

uint32_t* internal_node_right_child(void* node) {
  return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}

uint32_t* internal_node_cell(void* node, uint32_t cell_num) {
  return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

uint32_t* internal_node_child(void* node, uint32_t child_num) {
  uint32_t num_keys = *internal_node_num_keys(node);
  if (child_num > num_keys) {
    printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
    exit(EXIT_FAILURE);
  } else if (child_num == num_keys) {
    return internal_node_right_child(node);
  } else {
    return internal_node_cell(node, child_num);
  }
}

uint32_t* internal_node_key(void* node, uint32_t key_num) {
  return internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}

void initialize_internal_node(void* node) {
  *(uint8_t*)(node + NODE_TYPE_OFFSET) = NODE_INTERNAL;
  *(uint8_t*)(node + IS_ROOT_OFFSET) = 0;
  *internal_node_num_keys(node) = 0;
  *node_parent(node) = 0;
}

uint32_t leaf_node_find_cell(void* node, uint32_t key) {
  uint32_t num_cells = *leaf_node_num_cells(node);

  // Binary search to find the first matching key or the insertion point
  uint32_t min_index = 0;
  uint32_t one_past_max_index = num_cells;
  while (min_index < one_past_max_index) {
    uint32_t index = (min_index + one_past_max_index) / 2;
    uint32_t key_at_index = *leaf_node_key(node, index);
    if (key_at_index >= key) {
      one_past_max_index = index;
    } else {
      min_index = index + 1;
    }
  }
  return min_index;
}

uint32_t internal_node_find_child(void* node, uint32_t key) {
  uint32_t num_keys = *internal_node_num_keys(node);

  /* Binary search */
  uint32_t min_index = 0;
  uint32_t max_index = num_keys;

  while (min_index != max_index) {
    uint32_t index = (min_index + max_index) / 2;
    uint32_t key_to_to_right = *internal_node_key(node, index);
    if (key_to_to_right >= key) {
      max_index = index;
    } else {
      min_index = index + 1;
    }
  }

  return min_index;
}

void internal_node_insert(Table* table, uint32_t parent_page_num,
                          uint32_t child_page_num) {
  void* parent = get_page(table->pager, parent_page_num);
  void* child = get_page(table->pager, child_page_num);
  uint32_t child_max_key = get_node_max_key(child);
  uint32_t index = internal_node_find_child(parent, child_max_key);

  uint32_t original_num_keys = *internal_node_num_keys(parent);

  if (original_num_keys >= INTERNAL_NODE_MAX_KEYS) {
    internal_node_split_and_insert(table, parent_page_num, child_page_num);
    return;
  }

  *internal_node_num_keys(parent) = original_num_keys + 1;

  uint32_t right_child_page_num = *internal_node_right_child(parent);
  void* right_child = get_page(table->pager, right_child_page_num);

  if (child_max_key > get_node_max_key(right_child)) {
    *internal_node_child(parent, original_num_keys) = right_child_page_num;
    *internal_node_key(parent, original_num_keys) =
        get_node_max_key(right_child);
    *internal_node_right_child(parent) = child_page_num;
  } else {
    for (uint32_t i = original_num_keys; i > index; i--) {
      void* destination = internal_node_cell(parent, i);
      void* source = internal_node_cell(parent, i - 1);
      memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
    }
    *internal_node_child(parent, index) = child_page_num;
    *internal_node_key(parent, index) = child_max_key;
  }
  *node_parent(child) = parent_page_num;
}

void update_internal_node_key(void* node, uint32_t old_key, uint32_t new_key) {
  uint32_t old_child_index = internal_node_find_child(node, old_key);
  *internal_node_key(node, old_child_index) = new_key;
}

void internal_node_split_and_insert(Table* table, uint32_t parent_page_num,
                                    uint32_t child_page_num) {
  uint32_t old_page_num = parent_page_num;
  void* old_node = get_page(table->pager, old_page_num);
  uint32_t old_max_key = get_node_max_key(old_node);

  void* child = get_page(table->pager, child_page_num);
  uint32_t child_max_key = get_node_max_key(child);

  uint32_t new_page_num = get_unused_page_num(table->pager);
  void* new_node = get_page(table->pager, new_page_num);
  initialize_internal_node(new_node);
  *node_parent(new_node) = *node_parent(old_node);

  uint32_t cur_right_child_page_num = *internal_node_right_child(old_node);
  void* cur_right_child = get_page(table->pager, cur_right_child_page_num);

  bool child_is_right_child = child_max_key > get_node_max_key(cur_right_child);

  for (int32_t i = INTERNAL_NODE_MAX_KEYS; i >= 0; i--) {
    void* destination_node;
    if (i >= (int32_t)INTERNAL_NODE_LEFT_SPLIT_COUNT) {
      destination_node = new_node;
    } else {
      destination_node = old_node;
    }
    uint32_t index_within_node = i % INTERNAL_NODE_LEFT_SPLIT_COUNT;

    if (child_is_right_child && i == (int32_t)INTERNAL_NODE_MAX_KEYS) {
      *internal_node_right_child(destination_node) = child_page_num;
      if (i > 0) {
        *internal_node_child(destination_node, index_within_node - 1) =
            cur_right_child_page_num;
        *internal_node_key(destination_node, index_within_node - 1) =
            get_node_max_key(cur_right_child);
      }
    } else {
      uint32_t child_idx = internal_node_find_child(old_node, child_max_key);
      if (i == (int32_t)child_idx) {
        if (i == (int32_t)INTERNAL_NODE_MAX_KEYS) {
          *internal_node_right_child(destination_node) = child_page_num;
        } else {
          *internal_node_child(destination_node, index_within_node) =
              child_page_num;
          *internal_node_key(destination_node, index_within_node) =
              child_max_key;
        }
      } else if (i > (int32_t)child_idx) {
        if (i == (int32_t)INTERNAL_NODE_MAX_KEYS) {
          *internal_node_right_child(destination_node) =
              cur_right_child_page_num;
        } else {
          void* destination = internal_node_cell(destination_node, index_within_node);
          void* source = internal_node_cell(old_node, i - 1);
          memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
        }
      } else {
        void* destination = internal_node_cell(destination_node, index_within_node);
        void* source = internal_node_cell(old_node, i);
        memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
      }
    }
  }

  *internal_node_num_keys(old_node) = INTERNAL_NODE_LEFT_SPLIT_COUNT;
  *internal_node_num_keys(new_node) = INTERNAL_NODE_RIGHT_SPLIT_COUNT;

  for (uint32_t i = 0; i < *internal_node_num_keys(new_node); i++) {
    uint32_t child_num = *internal_node_child(new_node, i);
    *node_parent(get_page(table->pager, child_num)) = new_page_num;
  }
  *node_parent(get_page(table->pager, *internal_node_right_child(new_node))) =
      new_page_num;

  if (is_node_root(old_node)) {
    create_new_root(table, new_page_num);
  } else {
    uint32_t parent_page_num = *node_parent(old_node);
    void* parent = get_page(table->pager, parent_page_num);
    update_internal_node_key(parent, old_max_key, get_node_max_key(old_node));
    internal_node_insert(table, parent_page_num, new_page_num);
  }
}

void create_new_root(Table* table, uint32_t right_child_page_num) {
  void* root = get_page(table->pager, table->root_page_num);
  void* right_child = get_page(table->pager, right_child_page_num);
  uint32_t left_child_page_num = get_unused_page_num(table->pager);
  void* left_child = get_page(table->pager, left_child_page_num);

  memcpy(left_child, root, PAGE_SIZE);
  set_node_root(left_child, false);

  initialize_internal_node(root);
  set_node_root(root, true);
  *internal_node_num_keys(root) = 1;
  *internal_node_child(root, 0) = left_child_page_num;
  uint32_t left_child_max_key = get_node_max_key(left_child);
  *internal_node_key(root, 0) = left_child_max_key;
  *internal_node_right_child(root) = right_child_page_num;

  *node_parent(left_child) = table->root_page_num;
  *node_parent(right_child) = table->root_page_num;
}

bool leaf_node_has_space(void* node, uint32_t required_size) {
  uint32_t num_cells = *leaf_node_num_cells(node);
  uint32_t slot_array_end = LEAF_NODE_HEADER_SIZE + (num_cells + 1) * LEAF_NODE_SLOT_SIZE;
  uint32_t free_space = *leaf_node_free_space_pointer(node) - slot_array_end;
  return free_space >= required_size;
}

void leaf_node_insert(Cursor* cursor, uint32_t key, void* data, uint32_t length) {
  void* node = get_page(cursor->table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);

  uint32_t required_space = length + LEAF_NODE_SLOT_SIZE;

  if (!leaf_node_has_space(node, required_space)) {
    leaf_node_split_and_insert(cursor, key, data, length);
    return;
  }

  if (cursor->cell_num < num_cells) {
    for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
      memcpy(leaf_node_slot(node, i), leaf_node_slot(node, i - 1),
             LEAF_NODE_SLOT_SIZE);
    }
  }

  *(leaf_node_num_cells(node)) += 1;
  uint32_t new_free_ptr = *leaf_node_free_space_pointer(node) - length;
  *leaf_node_free_space_pointer(node) = new_free_ptr;
  memcpy(node + new_free_ptr, data, length);

  Slot* slot = leaf_node_slot(node, cursor->cell_num);
  slot->offset = new_free_ptr;
  slot->length = length;
}

void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, void* data,
                                uint32_t length) {
  void* old_node = get_page(cursor->table->pager, cursor->page_num);
  uint32_t old_max_key = get_node_max_key(old_node);
  
  uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
  void* new_node = get_page(cursor->table->pager, new_page_num);
  initialize_leaf_node(new_node);
  *node_parent(new_node) = *node_parent(old_node);
  *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
  *leaf_node_next_leaf(old_node) = new_page_num;

  uint32_t num_cells = *leaf_node_num_cells(old_node);
  
  /* Temporarily store all records to redistribute them */
  typedef struct {
    void* data;
    uint32_t length;
    uint32_t key;
  } TempRecord;
  
  TempRecord temp_records[num_cells + 1];
  
  for (uint32_t i = 0, j = 0; i <= num_cells; i++) {
    if (i == cursor->cell_num) {
      temp_records[i].data = malloc(length);
      memcpy(temp_records[i].data, data, length);
      temp_records[i].length = length;
      temp_records[i].key = key;
    } else {
      Slot* slot = leaf_node_slot(old_node, j);
      temp_records[i].length = slot->length;
      temp_records[i].data = malloc(slot->length);
      memcpy(temp_records[i].data, old_node + slot->offset, slot->length);
      temp_records[i].key = *leaf_node_key(old_node, j);
      j++;
    }
  }


  /* Split roughly in half by record count */
  uint32_t left_node_num_cells = (num_cells + 1) / 2;
  uint32_t right_node_num_cells = (num_cells + 1) - left_node_num_cells;

  /* Preserve root status of old_node */
  bool old_is_root = is_node_root(old_node);
  initialize_leaf_node(old_node);
  set_node_root(old_node, old_is_root);

  for (uint32_t i = 0; i < left_node_num_cells; i++) {
    uint32_t row_size = temp_records[i].length;
    uint32_t new_free_ptr = *leaf_node_free_space_pointer(old_node) - row_size;
    *leaf_node_free_space_pointer(old_node) = new_free_ptr;
    memcpy(old_node + new_free_ptr, temp_records[i].data, row_size);
    Slot* slot = leaf_node_slot(old_node, i);
    slot->offset = new_free_ptr;
    slot->length = row_size;
    free(temp_records[i].data);
  }
  *leaf_node_num_cells(old_node) = left_node_num_cells;

  for (uint32_t i = 0; i < right_node_num_cells; i++) {
    uint32_t idx = left_node_num_cells + i;
    uint32_t row_size = temp_records[idx].length;
    uint32_t new_free_ptr = *leaf_node_free_space_pointer(new_node) - row_size;
    *leaf_node_free_space_pointer(new_node) = new_free_ptr;
    memcpy(new_node + new_free_ptr, temp_records[idx].data, row_size);
    Slot* slot = leaf_node_slot(new_node, i);
    slot->offset = new_free_ptr;
    slot->length = row_size;
    free(temp_records[idx].data);
  }
  *leaf_node_num_cells(new_node) = right_node_num_cells;

  if (old_is_root) {
    create_new_root(cursor->table, new_page_num);
  } else {
    uint32_t parent_page_num = *node_parent(old_node);
    void* parent = get_page(cursor->table->pager, parent_page_num);
    update_internal_node_key(parent, old_max_key, get_node_max_key(old_node));
    internal_node_insert(cursor->table, parent_page_num, new_page_num);
  }
}


void print_constants(FILE* out) {
  fprintf(out, "ID_SIZE: %zu\n", ID_SIZE);
  fprintf(out, "COMMON_NODE_HEADER_SIZE: %zu\n", COMMON_NODE_HEADER_SIZE);
  fprintf(out, "LEAF_NODE_HEADER_SIZE: %zu\n", LEAF_NODE_HEADER_SIZE);
  fprintf(out, "LEAF_NODE_SLOT_SIZE: %zu\n", LEAF_NODE_SLOT_SIZE);
}

void print_leaf_node(void* node, FILE* out) {
  uint32_t num_cells = *leaf_node_num_cells(node);
  fprintf(out, "leaf (size %d)\n", num_cells);
  for (uint32_t i = 0; i < num_cells; i++) {
    uint32_t key = *leaf_node_key(node, i);
    fprintf(out, "  - %d : %d\n", i, key);
  }
}

void indent(uint32_t level, FILE* out) {
  for (uint32_t i = 0; i < level; i++) {
    fprintf(out, "  ");
  }
}

void print_tree(Pager* pager, uint32_t page_num, uint32_t indentation_level, FILE* out) {
  void* node = get_page(pager, page_num);
  uint32_t num_keys, child;

  switch (get_node_type(node)) {
    case (NODE_LEAF):
      num_keys = *leaf_node_num_cells(node);
      indent(indentation_level, out);
      fprintf(out, "- leaf (size %d)\n", num_keys);
      for (uint32_t i = 0; i < num_keys; i++) {
        indent(indentation_level + 1, out);
        fprintf(out, "- %d\n", *leaf_node_key(node, i));
      }
      break;
    case (NODE_INTERNAL):
      num_keys = *internal_node_num_keys(node);
      indent(indentation_level, out);
      fprintf(out, "- internal (size %d)\n", num_keys);
      for (uint32_t i = 0; i < num_keys; i++) {
        child = *internal_node_child(node, i);
        print_tree(pager, child, indentation_level + 1, out);

        indent(indentation_level + 1, out);
        fprintf(out, "- key %d\n", *internal_node_key(node, i));
      }
      child = *internal_node_right_child(node);
      print_tree(pager, child, indentation_level + 1, out);
      break;
  }
}

