NVCC    = nvcc
NVFLAGS = -O2

PROGRAM = 3way-cuda
SRCS    = cuda3Way.cu
OBJS    = $(SRCS:.cu=.o)

default: all

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(NVCC) $(NVFLAGS) $(OBJS) -o $(PROGRAM)

%.o: %.cu
	$(NVCC) $(NVFLAGS) -c $< -o $@

clean:
	/bin/rm -f $(OBJS) $(PROGRAM)
