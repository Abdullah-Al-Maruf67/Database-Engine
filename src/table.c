#include "table.h"

typedef struct {
  uint32_t num_tables;
  struct {
    char table_name[MAX_TABLE_NAME];
    uint32_t root_page_num;
    uint32_t schema_page_num;
  } table_entries[MAX_TABLES];
} CatalogPage;

Catalog* db_open(const char* filename) {
  Pager* pager = pager_open(filename);
  Catalog* catalog = malloc(sizeof(Catalog));
  catalog->pager = pager;
  catalog->num_tables = 0;
  pthread_mutex_init(&catalog->db_mutex, NULL);

  if (pager->num_pages == 0) {
    /* New database. Page 0 is reserved for the Catalog. */
    void* page = get_page(pager, 0);
    memset(page, 0, PAGE_SIZE);
  } else {
    /* Load existing catalog from Page 0. */
    CatalogPage* cat_page = (CatalogPage*)get_page(pager, 0);
    catalog->num_tables = cat_page->num_tables;
    for (uint32_t i = 0; i < catalog->num_tables; i++) {
      Table* table = &catalog->tables[i];
      table->pager = pager;
      table->root_page_num = cat_page->table_entries[i].root_page_num;
      table->schema_page_num = cat_page->table_entries[i].schema_page_num;
      
      // Load schema from schema_page_num
      void* schema_data = get_page(pager, table->schema_page_num);
      memcpy(&table->schema, schema_data, sizeof(TableSchema));
    }
  }

  return catalog;
}


void db_close(Catalog* catalog) {
  catalog_save(catalog); // Final save before closing
  Pager* pager = catalog->pager;

  for (uint32_t i = 0; i < pager->num_pages; i++) {
    if (pager->pages[i] == NULL) {
      continue;
    }
    pager_flush(pager, i);
    free(pager->pages[i]);
    pager->pages[i] = NULL;
  }

  int result = close(pager->file_descriptor);
  if (result == -1) {
    printf("Error closing db file.\n");
    exit(EXIT_FAILURE);
  }
  
  pthread_mutex_destroy(&catalog->db_mutex);
  free(pager);
  free(catalog);
}

void catalog_save(Catalog* catalog) {
  void* page = get_page(catalog->pager, 0);
  CatalogPage* cat_page = (CatalogPage*)page;
  cat_page->num_tables = catalog->num_tables;
  for (uint32_t i = 0; i < catalog->num_tables; i++) {
    Table* table = &catalog->tables[i];
    strncpy(cat_page->table_entries[i].table_name, table->schema.table_name, MAX_TABLE_NAME);
    cat_page->table_entries[i].root_page_num = table->root_page_num;
    cat_page->table_entries[i].schema_page_num = table->schema_page_num;
  }
  pager_flush(catalog->pager, 0);
}


void catalog_add_table(Catalog* catalog, TableSchema* schema, uint32_t root_page_num, uint32_t schema_page_num) {
  if (catalog->num_tables >= MAX_TABLES) return;
  Table* table = &catalog->tables[catalog->num_tables++];
  table->pager = catalog->pager;
  table->root_page_num = root_page_num;
  table->schema_page_num = schema_page_num;
  memcpy(&table->schema, schema, sizeof(TableSchema));
}

Table* catalog_get_table(Catalog* catalog, const char* table_name) {
  for (uint32_t i = 0; i < catalog->num_tables; i++) {
    if (strcmp(catalog->tables[i].schema.table_name, table_name) == 0) {
      return &catalog->tables[i];
    }
  }
  return NULL;
}

Cursor* table_start(Table* table) {
  Cursor* cursor = table_find(table, 0);

  void* node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  cursor->end_of_table = (num_cells == 0);

  return cursor;
}

Cursor* internal_node_find(Table* table, uint32_t page_num, uint32_t key) {
  void* node = get_page(table->pager, page_num);

  uint32_t child_index = internal_node_find_child(node, key);
  uint32_t child_num = *internal_node_child(node, child_index);
  void* child = get_page(table->pager, child_num);
  switch (get_node_type(child)) {
    case NODE_LEAF: {
      Cursor* cursor = malloc(sizeof(Cursor));
      cursor->table = table;
      cursor->page_num = child_num;
      cursor->cell_num = leaf_node_find_cell(child, key);
      return cursor;
    }
    case NODE_INTERNAL:
      return internal_node_find(table, child_num, key);
  }
  return NULL;
}

Cursor* table_find(Table* table, uint32_t key) {
  uint32_t root_page_num = table->root_page_num;
  void* root_node = get_page(table->pager, root_page_num);

  if (get_node_type(root_node) == NODE_LEAF) {
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = root_page_num;
    cursor->cell_num = leaf_node_find_cell(root_node, key);
    return cursor;
  } else {
    return internal_node_find(table, root_page_num, key);
  }
}

Cursor* internal_node_find_end(Table* table, uint32_t page_num) {
  void* node = get_page(table->pager, page_num);
  uint32_t child_num = *internal_node_right_child(node);
  void* child = get_page(table->pager, child_num);
  switch (get_node_type(child)) {
    case NODE_LEAF: {
      Cursor* cursor = malloc(sizeof(Cursor));
      cursor->table = table;
      cursor->page_num = child_num;
      cursor->cell_num = *leaf_node_num_cells(child);
      cursor->end_of_table = true;
      return cursor;
    }
    case NODE_INTERNAL:
      return internal_node_find_end(table, child_num);
  }
  return NULL;
}

Cursor* table_end(Table* table) {
  uint32_t root_page_num = table->root_page_num;
  void* root_node = get_page(table->pager, root_page_num);

  if (get_node_type(root_node) == NODE_LEAF) {
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = root_page_num;
    cursor->cell_num = *leaf_node_num_cells(root_node);
    cursor->end_of_table = true;
    return cursor;
  } else {
    return internal_node_find_end(table, root_page_num);
  }
}

void* cursor_value(Cursor* cursor) {
  uint32_t page_num = cursor->page_num;
  void* page = get_page(cursor->table->pager, page_num);
  return leaf_node_value(page, cursor->cell_num);
}

void cursor_advance(Cursor* cursor) {
  uint32_t page_num = cursor->page_num;
  void* node = get_page(cursor->table->pager, page_num);

  cursor->cell_num += 1;
  if (cursor->cell_num >= (*leaf_node_num_cells(node))) {
    /* Next leaf node */
    uint32_t next_page_num = *leaf_node_next_leaf(node);
    if (next_page_num == 0) {
      /* This was rightmost leaf */
      cursor->end_of_table = true;
    } else {
      cursor->page_num = next_page_num;
      cursor->cell_num = 0;
    }
  }
}

uint32_t get_tree_depth(Table* table) {
  uint32_t depth = 0;
  uint32_t page_num = table->root_page_num;
  void* node = get_page(table->pager, page_num);

  while (get_node_type(node) == NODE_INTERNAL) {
    depth++;
    page_num = *internal_node_child(node, 0);
    node = get_page(table->pager, page_num);
  }
  return depth + 1;
}

uint32_t count_rows(Table* table) {
  uint32_t count = 0;
  Cursor* cursor = table_start(table);
  while (!cursor->end_of_table) {
    count++;
    cursor_advance(cursor);
  }
  free(cursor);
  return count;
}
