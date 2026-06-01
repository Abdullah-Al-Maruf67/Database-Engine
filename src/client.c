#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "input_buffer.h"

#define PORT 8080
#define BUFFER_SIZE 4096

int main() {
  int sock = 0;
  struct sockaddr_in serv_addr;
  
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("\n Socket creation error \n");
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    printf("\nInvalid address/ Address not supported \n");
    return -1;
  }

  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("\nConnection Failed \n");
    return -1;
  }

  printf("Connected to database server.\n");

  InputBuffer* input_buffer = new_input_buffer();
  while (1) {
    print_prompt();
    read_input(input_buffer);

    if (strcmp(input_buffer->buffer, ".exit") == 0) {
      break;
    }

    uint32_t request_length = strlen(input_buffer->buffer);
    send(sock, &request_length, sizeof(uint32_t), 0);
    send(sock, input_buffer->buffer, request_length, 0);

    uint32_t response_length;
    ssize_t bytes_read = recv(sock, &response_length, sizeof(uint32_t), 0);
    if (bytes_read <= 0) {
      printf("Server disconnected.\n");
      break;
    }

    char* response = malloc(response_length + 1);
    bytes_read = recv(sock, response, response_length, 0);
    if (bytes_read <= 0) {
      free(response);
      break;
    }
    response[response_length] = '\0';
    printf("%s", response);
    free(response);
  }

  close(sock);
  return 0;
}
