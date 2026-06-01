CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -g
LDFLAGS = -lpthread
SRC_DIR = src
OBJ_DIR = obj
SOURCES = $(wildcard $(SRC_DIR)/*.c)
COMMON_OBJECTS = $(OBJ_DIR)/tokenizer.o $(OBJ_DIR)/parser.o $(OBJ_DIR)/schema.o $(OBJ_DIR)/statement.o $(OBJ_DIR)/table.o $(OBJ_DIR)/node.o $(OBJ_DIR)/pager.o $(OBJ_DIR)/row.o $(OBJ_DIR)/input_buffer.o $(OBJ_DIR)/expression.o $(OBJ_DIR)/evaluator.o


SERVER_TARGET = db-server
CLIENT_TARGET = db-client

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(COMMON_OBJECTS) $(OBJ_DIR)/server.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(CLIENT_TARGET): $(OBJ_DIR)/client.o $(OBJ_DIR)/input_buffer.o
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@



$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(SERVER_TARGET) $(CLIENT_TARGET)

.PHONY: all clean
