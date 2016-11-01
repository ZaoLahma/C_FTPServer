CCOMMAND = gcc
CFLAGS = -Wall -c -Wextra
LINKARGS = -lpthread
SOURCES = $(wildcard src/*.c)
TEST_SOURCES = $(wildcard test/*.c)
OBJECTS = $(SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)
INC_DIRS = -I./inc
EXE_NAME = ftp_server


$(EXE_NAME): $(OBJECTS) $(TEST_OBJECTS)
	$(CCOMMAND) $(OBJECTS) $(TEST_OBJECTS) $(LINKARGS) -o $(EXE_NAME)

%.o: %.cpp
	$(CCOMMAND) $(INC_DIRS) -c $(CFLAGS) $< -o $@

lib: $(OBJECTS)
	ar rvs thread_starter.a $(OBJECTS)

clean:
	rm -f $(EXE_NAME) $(OBJECTS) $(TEST_OBJECTS)