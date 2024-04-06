SRCS = axpty.c
OBJS = $(SRCS:.c=.o)

all: axpty

axpty: $(OBJS)
	$(CC) -o $@ $(OBJS)


install: axpty
	install -m 755 axpty /usr/sbin/axpty
