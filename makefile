CC = gcc
CFLAGS = -fopenmp -Wall -Wextra -O2
TARGET = escalonador
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET) $(CORES)

clean:
	rm -f $(TARGET)
