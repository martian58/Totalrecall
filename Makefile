CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lsqlite3

SRC = totalRecall.c
TARGET = totalRecall

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
