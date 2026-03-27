CC=/opt/homebrew/bin/gcc-14
CFLAGS=-O3 -Wall -Wextra -Werror -std=c11 -D_POSIX_C_SOURCE=200809L -march=native -flto -fopenmp
LDFLAGS=-flto -fopenmp

all: fib

fib: main.o bigint.o verify.o arena.o
	$(CC) $(CFLAGS) -o fib main.o bigint.o verify.o arena.o $(LDFLAGS)

arena.o: arena.c arena.h
	$(CC) $(CFLAGS) -c arena.c -o arena.o

bigint.o: bigint.c bigint.h constants.h arena.h error.h
	$(CC) $(CFLAGS) -c bigint.c -o bigint.o

verify.o: verify.c verify.h bigint.h
	$(CC) $(CFLAGS) -c verify.c -o verify.o

main.o: main.c bigint.h constants.h arena.h error.h
	$(CC) $(CFLAGS) -c main.c -o main.o

test-mem: tests/test_mem.c arena.o
	$(CC) $(CFLAGS) -I. tests/test_mem.c arena.o -o test_mem $(LDFLAGS)
	./test_mem

test-atomic: tests/test_atomic.c arena.o bigint.o verify.o
	$(CC) $(CFLAGS) -I. tests/test_atomic.c arena.o bigint.o verify.o -o test_atomic $(LDFLAGS)
	./test_atomic

test-kernels: tests/test_kernels.c
	$(CC) $(CFLAGS) -I. tests/test_kernels.c -o test_kernels $(LDFLAGS)
	./test_kernels

test-bigint-low: tests/test_bigint_low.c arena.o bigint.o verify.o
	$(CC) $(CFLAGS) -I. tests/test_bigint_low.c arena.o bigint.o verify.o -o test_bigint_low $(LDFLAGS)
	./test_bigint_low

test-ssa-suite: tests/test_ssa_suite.c arena.o bigint.o verify.o
	$(CC) $(CFLAGS) -I. tests/test_ssa_suite.c arena.o bigint.o verify.o -o test_ssa_suite $(LDFLAGS)
	./test_ssa_suite

test-engine: tests/test_engine.c arena.o bigint.o verify.o
	$(CC) $(CFLAGS) -I. tests/test_engine.c arena.o bigint.o verify.o -o test_engine $(LDFLAGS)
	./test_engine

test-stress-concurrent: tests/test_stress_concurrent.c arena.o bigint.o verify.o
	$(CC) $(CFLAGS) -I. tests/test_stress_concurrent.c arena.o bigint.o verify.o -o test_stress_concurrent $(LDFLAGS)
	./test_stress_concurrent

test-fuzzer: tests/test_fuzzer.c arena.o bigint.o verify.o
	$(CC) $(CFLAGS) -I. tests/test_fuzzer.c arena.o bigint.o verify.o -o test_fuzzer $(LDFLAGS)
	./test_fuzzer

test: test-mem test-atomic test-kernels test-bigint-low test-ssa-suite test-engine test-stress-concurrent test-fuzzer
	@echo "[SUCCESS] All tests passed!"

clean:
	rm -f *.o fib test_mem test_atomic test_kernels test_bigint_low test_ssa_suite test_engine test_stress_concurrent test_fuzzer
