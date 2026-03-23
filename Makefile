CC = gcc
CFLAGS = -O3 -march=native -flto -Wall -Wextra -std=c11
LDFLAGS = -flto -lm

TARGET = fib
TEST_TARGET = test_math

SRCS = main.c bigint.c
OBJS = $(SRCS:.c=.o)

TEST_SRCS = test_math.c bigint.c
TEST_OBJS = $(TEST_SRCS:.c=.o)

all: $(TARGET) $(TEST_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o $(TARGET) $(TEST_TARGET)
