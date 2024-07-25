CC = gcc
CFLAGS = -fopenmp -Wall -Wextra -O0
TARGET = escalonador
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET) $(CORES)

clean:
	rm -f $(TARGET)
