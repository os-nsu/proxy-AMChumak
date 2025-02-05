all: config.o parser.o
	ar -rcs libconfig.a config.o parser.o
	mv ./libconfig.a install

config.o:
	gcc -c src/backend/config/config.c

parser.o:
	gcc -c src/backend/config/parser.c