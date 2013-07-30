#Debug
ifeq ($(DEBUG),y)
	DEBFLAGS = -O -g -DDEBUG
else
	DEBFLAGS = -O
endif

#Compiler and Linker
CC = gcc
LD = gcc

#Flags
CFLAGS += $(DEBFLAGS) -g
LDFLAGS = -lrt

#Install Variables
INSTALL = install
INSTALLBIN = -s -m 755
BINDIR = $(CURDIR)/../bin

#Include
INCLUDEDIR = ../include
INCLUDE = -I$(INCLUDEDIR)

#Program Specific Variables
OBJS = main.o
PROG = gizmo

all: $(PROG)

$(PROG): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(PROG)

%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -c $<

clean:
	$(RM) -rf $(PROG) $(OBJS) *~ \#*

moreclean: clean
	$(RM) -rf $(BINDIR)/$(PROG)
	$(RM) -rf $(INCLUDEDIR)/*~ $(INCLUDEDIR)/\#*

install:
	$(INSTALL) $(INSTALLBIN) $(PROG) $(BINDIR)/$(PROG)

