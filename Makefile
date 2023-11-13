PROG=pennfat

PROMPT='"$(PROG)> "'

# Remove -DNDEBUG during development if assert(3) is used
#
override CPPFLAGS += -DNDEBUG -DPROMPT=$(PROMPT)

CC = clang

# Replace -O1 with -g for a debug version during development
#
CFLAGS = -Wall -Werror -g

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard *.h)

.PHONY : clean

$(PROG) : $(OBJS) $(HEADERS)
	$(CC) -o $@ $(OBJS) parser.o

%.o: %.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

clean :
	$(RM) $(OBJS) $(PROG)