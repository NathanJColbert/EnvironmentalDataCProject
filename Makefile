CC = gcc
CFLAGS = -Wall -Wextra -O2 -I$(SRC_DIR) -MMD -MP
LDFLAGS = -lwiringPi -lm -lmysqlclient

SRC_DIR = src
BUILD_DIR = build
TARGET_NAME = Environmental_Program
TARGET = $(BUILD_DIR)/$(TARGET_NAME)

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

all: $(TARGET)

# Link object files
$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

# Compile .c to .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Run
run: all
	./$(TARGET)

# Clean
clean:
	rm -rf $(BUILD_DIR)

-include $(OBJ:.o=.d)

.PHONY: all clean run
