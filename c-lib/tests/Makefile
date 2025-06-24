CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I../include
LIBS = -pthread

all: generate_test_data

generate_test_data: generate_test_data.c ../src/cJSON.c
	$(CC) $(CFLAGS) -o generate_test_data generate_test_data.c ../src/cJSON.c $(LIBS)

clean:
	rm -f generate_test_data *.json *.txt

.PHONY: all clean