CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LIBS = -lcjson

all: generate_test_data

generate_test_data: generate_test_data.c
	$(CC) $(CFLAGS) -o generate_test_data generate_test_data.c $(LIBS)

clean:
	rm -f generate_test_data

.PHONY: all clean