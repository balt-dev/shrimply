CC=g++

TARGET=simply

SRCDIR=./src
INCLUDEDIR=./include
OBJDIR=./obj
OUTDIR=./lib

SOURCES=$(wildcard $(SRCDIR)/*.cpp)
OBJECTS=$(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))
HEADERS=$(wildcard $(INCLUDEDIR)/*.h)

LDFLAGS=-I$(INCLUDEDIR)
CPPFLAGS=-g -Wall

$(OUTDIR)/$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CPPFLAGS) -c -o $@ $^

all: $(SOURCES) $(OBJECTS) $(HEADERS)
	make $(OBJECTS)
	make $(OUTDIR)/$(TARGET)

clean:
	rm -r $(OBJDIR)/*
	rm -r $(OUTDIR)/*