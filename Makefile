# compiler to use
CC = gcc

# compiler flags
CFLAGS = -Wall -Wextra

# source files
SRCS = project.c userInput.c validation.c udp_Queue.c tcp.c interruptions.c exp_Content.c messageHandling.c

# header files
HDRS = project.h

# object files
OBJS = $(SRCS:.c=.o)

# name of executable
EXEC = cot

# archive name
ARCHIVE = proj08.zip

# default rule
all: $(EXEC)

# rule to build executable
$(EXEC): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# clean rule
clean:
	rm -f $(EXEC)

# rule to create archive
archive: $(SRCS) $(HDRS) Makefile
	tar czf $(ARCHIVE) $^

.SECONDARY: $(OBJS)

.PHONY: all clean archive
