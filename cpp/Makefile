EXECUTABLE := local
LIBDIR :=
LIBS :=
INCLUDES := .
SRCDIR :=

CC := g++
CFLAGS := -g -O3 -pthread
CPPFLAGS := $(CFLAGS) -std=c++11

RM := rm -f

SRCS := local.cpp
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

.PHONY : all build clean

all: clean build

build:
	@echo "build ..."
	$(CC) -o $(EXECUTABLE) $(CPPFLAGS) $(SRCS)

clean:
	@echo "clean ..."
	@$(RM) *.o *.d $(EXECUTABLE)
