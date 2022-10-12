CFLAGS := -Wall -Werror -Wshadow -Wextra -pthread --std=c11
OBJS := config.hpp logs.hpp network.hpp types.hpp utils.hpp file.hpp main.c
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	CC = gcc
endif
ifeq ($(UNAME_S),Darwin)
	CC = gcc-12
endif

server: $(OBJS)
	$(CC) $(CFLAGS) main.c -o server
	echo "make success\n"

.PHONY: clean
clean:
	rm server