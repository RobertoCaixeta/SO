CC = gcc
CFLAGS = -fopenmp -Wall -Wextra -O2
TARGET = escalonador
SRC = main.c
CORES = 2

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	echo $(SRC)
	./$(TARGET) $(CORES)

clean:
	rm -f $(TARGET)
