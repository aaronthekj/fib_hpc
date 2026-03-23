CC = gcc
CFLAGS = -O3 -march=native -flto -Wall -Wextra -std=c11
LDFLAGS = -flto -lm

TARGET = fib
TEST_TARGET = test_math

SRCS = main.c formatting.c math_engine.c bigint_ops.c fast_div.c negacyclic.c
OBJS = $(SRCS:.c=.o)

TEST_SRCS = test_math.c formatting.c math_engine.c bigint_ops.c fast_div.c negacyclic.c
TEST_OBJS = $(TEST_SRCS:.c=.o)

all: $(TARGET) $(TEST_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGET)
