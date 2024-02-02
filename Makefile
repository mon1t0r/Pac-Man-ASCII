# variables
CC=gcc
CFLAGS=-std=c11 -Wall -Werror -Wno-vla-parameter
LDLIBS=-lm -lcurses
OUTPUT=pacman

# targets
all: $(OUTPUT)

$(OUTPUT): program.o game.o graphics.o world.o timer.o menu.o file_render.o
	cppcheck --enable=performance,unusedFunction --error-exitcode=1 *.c
	$(CC) $(CFLAGS) program.o game.o graphics.o world.o timer.o menu.o file_render.o $(LDLIBS) -o $(OUTPUT)

program.o: program.c
	$(CC) $(CFLAGS) -c program.c $(LDLIBS) -o program.o

game.o: game.c
	$(CC) $(CFLAGS) -c game.c $(LDLIBS) -o game.o

graphics.o: graphics.c
	$(CC) $(CFLAGS) -c graphics.c $(LDLIBS) -o graphics.o

world.o: world.c
	$(CC) $(CFLAGS) -c world.c $(LDLIBS) -o world.o

timer.o: timer.c
	$(CC) $(CFLAGS) -c timer.c $(LDLIBS) -o timer.o

menu.o: menu.c
	$(CC) $(CFLAGS) -c menu.c $(LDLIBS) -o menu.o

file_render.o: file_render.c
	$(CC) $(CFLAGS) -c file_render.c $(LDLIBS) -o file_render.o

# remove compiled files
clean: 
	rm -rf $(OUTPUT) *.o