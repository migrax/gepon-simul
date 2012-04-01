#### Compiler and tool definitions shared by all build targets #####
CC = c++
CCC = c++
CXX = c++
BASICOPTS = -g -O2 -Wall -fno-strict-aliasing
CFLAGS = $(BASICOPTS)
CCFLAGS = $(BASICOPTS)
CXXFLAGS = $(BASICOPTS)
CCADMIN = 


.PHONY: all
all: gepon-simul

.c.o:
	$(COMPILE.c) $(CFLAGS) -o $@ -c $<

.cc.o:
	$(COMPILE.cc) $(CCFLAGS) $(CPPFLAGS) -o $@ -c $<

gepon-simul: gepon-simul.o


.PHONY: clean
clean:
	rm -rf -- *.o
