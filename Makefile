CCOMMAND = gcc
CFLAGS = -Wall -c -Wextra -g #-fno-stack-protector
LINKARGS = -lpthread
SOURCES = $(wildcard src/*.c)
RELEASE_SOURCES = $(wildcard src/main/*.c)
TEST_SOURCES = $(wildcard test/*.c)
OBJECTS = $(SOURCES:.c=.o)
RELEASE_OBJECTS = $(RELEASE_SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)
INC_DIRS = -I./inc
EXE_NAME = ftp_server
EXE_NAME_TEST = ftp_server_test

release: $(OBJECTS) $(RELEASE_OBJECTS)
	$(CCOMMAND) $(OBJECTS) $(RELEASE_OBJECTS) $(LINKARGS) -o $(EXE_NAME)

test: $(OBJECTS) $(TEST_OBJECTS)
	$(CCOMMAND) $(OBJECTS) $(TEST_OBJECTS) $(LINKARGS) -o $(EXE_NAME_TEST)

%.o: %.cpp
	$(CCOMMAND) $(INC_DIRS) -c $(CFLAGS) $< -o $@

lib: $(OBJECTS)
	ar rvs thread_starter.a $(OBJECTS)

clean:
	rm -f $(EXE_NAME) $(EXE_NAME_TEST) $(OBJECTS) $(TEST_OBJECTS) $(RELEASE_OBJECTS)
