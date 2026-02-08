## Assignment 2 (CS 464/564) - Remote Shell (rsshell)
## Base framework by Dr. Stefan D. Bruda (Assignment 1)
## Extended with TCP/IP networking for Assignment 2
## tcp-utils and tokenize libraries by Dr. Stefan D. Bruda

## Uncomment this line (and comment out the following line) to produce
## an executable that provides some debug output.
#CXXFLAGS = -g -Wall -pedantic -DDEBUG
CXXFLAGS = -g -Wall -pedantic

all: rsshell

tcp-utils.o: tcp-utils.h tcp-utils.cc
	$(CXX) $(CXXFLAGS) -c -o tcp-utils.o tcp-utils.cc

tokenize.o: tokenize.h tokenize.cc
	$(CXX) $(CXXFLAGS) -c -o tokenize.o tokenize.cc

sshell.o: tcp-utils.h tokenize.h sshell.cc
	$(CXX) $(CXXFLAGS) -c -o sshell.o sshell.cc

rsshell: sshell.o tcp-utils.o tokenize.o
	$(CXX) $(CXXFLAGS) -o rsshell sshell.o tcp-utils.o tokenize.o

clean:
	rm -f rsshell sshell *~ *.o *.bak core \#*
