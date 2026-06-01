#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include "common.h"
#include "table.h"
#include "statement.h"
#include "input_buffer.h"

#define PORT 8080
#define BUFFER_SIZE 4096

Catalog* global_catalog = NULL;

void sigint_handler(int sig) {
  if (global_catalog) {
    printf("\nShutting down server gracefully...\n");
    db_close(global_catalog);
  }
  exit(0);
}

typedef struct {
  int client_socket;
  Catalog* catalog;
} ClientHandlerArgs;

void* handle_client(void* args) {
  ClientHandlerArgs* handler_args = (ClientHandlerArgs*)args;
  int client_socket = handler_args->client_socket;
  Catalog* catalog = handler_args->catalog;
  free(handler_args);

  printf("New client connected\n");

  while (1) {
    uint32_t request_length;
    ssize_t bytes_read = recv(client_socket, &request_length, sizeof(uint32_t), 0);
    if (bytes_read <= 0) break;

    char* buffer = malloc(request_length + 1);
    bytes_read = recv(client_socket, buffer, request_length, 0);
    if (bytes_read <= 0) {
      free(buffer);
      break;
    }
    buffer[request_length] = '\0';

    printf("Received query: %s\n", buffer);

    /* Collect results in a string buffer to send back */
    /* For now, we need to redirect stdout or use a buffer-based execution */
    /* Let's implement a simple wrapper that captures execution output */
    
    // For this prototype, we'll just send "Executed" back.
    // In Phase 2, we will redirect output correctly.
    
    InputBuffer input_buffer;
    input_buffer.buffer = buffer;
    input_buffer.input_length = request_length;
    input_buffer.buffer_length = request_length + 1;

    Statement statement;
    memset(&statement, 0, sizeof(Statement));
    char* response_ptr;
    size_t response_size;
    FILE* out = open_memstream(&response_ptr, &response_size);

    pthread_mutex_lock(&catalog->db_mutex);
    if (buffer[0] == '.') {
      MetaCommandResult meta_result = do_meta_command(&input_buffer, catalog, out);
      if (meta_result == META_COMMAND_UNRECOGNIZED_COMMAND) {
        fprintf(out, "Unrecognized command '%s'\n", buffer);
      }
    } else {
      PrepareResult prepare_result = prepare_statement(&input_buffer, &statement, catalog);
      if (prepare_result == PREPARE_SUCCESS) {
        ExecuteResult execute_result = execute_statement(&statement, catalog, out);
        if (execute_result == EXECUTE_SUCCESS) {
          fprintf(out, "Executed success\n");
        } else if (execute_result == EXECUTE_DUPLICATE_KEY) {
          fprintf(out, "Error: Duplicate key\n");
        } else {
          fprintf(out, "Execution error\n");
        }
      } else {
        fprintf(out, "Syntax error or unrecognized statement\n");
      }
    }
    pthread_mutex_unlock(&catalog->db_mutex);

    fclose(out); // Flushes to response_ptr

    uint32_t response_len = (uint32_t)response_size;
    send(client_socket, &response_len, sizeof(uint32_t), 0);
    send(client_socket, response_ptr, response_len, 0);

    free(response_ptr);
    free(buffer);

  }

  printf("Client disconnected\n");
  close(client_socket);
  return NULL;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf("Must supply a database filename.\n");
    exit(EXIT_FAILURE);
  }

  char* filename = argv[1];
  global_catalog = db_open(filename);
  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);
  Catalog* catalog = global_catalog;

  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("Database server listening on port %d...\n", PORT);

  while (1) {
    if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
      perror("accept");
      continue;
    }

    pthread_t thread_id;
    ClientHandlerArgs* args = malloc(sizeof(ClientHandlerArgs));
    args->client_socket = new_socket;
    args->catalog = catalog;

    if (pthread_create(&thread_id, NULL, handle_client, (void*)args) != 0) {
      perror("pthread_create failed");
      free(args);
    }
    pthread_detach(thread_id);
  }

  db_close(catalog);
  return 0;
}
