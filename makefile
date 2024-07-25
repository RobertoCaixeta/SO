CC = gcc
CFLAGS = -fopenmp -Wall -Wextra -O0
TARGET = escalonador
SRC = test.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET) $(CORES)

clean:
	rm -f $(TARGET)
