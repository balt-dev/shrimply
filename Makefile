CC=g++

TARGET=shrimply

SRCDIR=./src
INCLUDEDIR=./include
OBJDIR=./obj
OUTDIR=./lib
EXECUTABLE=$(OUTDIR)/$(TARGET)

SRCS=$(wildcard $(SRCDIR)/*.cpp)
OBJECTS=$(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
HEADS=$(wildcard $(INCLUDEDIR)/*.h)

LDFLAGS=-g
CPPFLAGS=-g -I$(INCLUDEDIR)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CPPFLAGS) -c -o $@ $^

all: $(SRCS) $(OBJECTS) $(HEADS)
	make $(OBJECTS)
	make $(EXECUTABLE)

clean:
	rm -rf $(OBJDIR)/*
	rm -rf $(OUTDIR)/*

test:
	$(EXECUTABLE) ./samples/test.spl