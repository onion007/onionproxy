EXECUTABLE := local
LIBDIR :=
LIBS :=
INCLUDES := .
SRCDIR :=

CC := g++
CFLAGS := -g -lpthread -O3
CPPFLAGS := $(CFLAGS)

RM := rm -f

SRCS := $(wildcard *.cpp) $(wildcard $(addsuffix /*.cpp, $(SRCDIR)))
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

.PHONY : all build clean

all: clean build

build:
	@echo "build ..."
	$(CC) -o $(EXECUTABLE) $(SRCS)

clean:
	@echo "clean ..."
	@$(RM) *.o *.d $(EXECUTABLE)
