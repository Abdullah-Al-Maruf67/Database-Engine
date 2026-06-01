#include "row.h"

uint32_t serialize_row(Row* source, void* destination) {
  uint32_t offset = 0;
  
  memcpy(destination + offset, &(source->id), ID_SIZE);
  offset += ID_SIZE;
  
  uint32_t username_len = strlen(source->username) + 1;
  memcpy(destination + offset, source->username, username_len);
  offset += username_len;
  
  uint32_t email_len = strlen(source->email) + 1;
  memcpy(destination + offset, source->email, email_len);
  offset += email_len;
  
  return offset;
}

void deserialize_row(void* source, Row* destination) {
  uint32_t offset = 0;
  
  memcpy(&(destination->id), source + offset, ID_SIZE);
  offset += ID_SIZE;
  
  strcpy(destination->username, source + offset);
  offset += strlen(destination->username) + 1;
  
  strcpy(destination->email, source + offset);
}


void print_row(Row* row) {
  printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}
