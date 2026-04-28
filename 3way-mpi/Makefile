CC      = mpicc
CFLAGS  = -O2 -Wall

PROGRAM = 3way-mpi
SRCS    = 3way-mpi.c
OBJS    = $(SRCS:.c=.o)

default: all

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(PROGRAM)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	/bin/rm -f $(OBJS) $(PROGRAM)
