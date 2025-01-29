export MAINDIR = $(CURDIR)

export COPT := ${COPT} -g
export CFLAGS := ${CFLAGS} -export-dynamic -Wall -Wextra -Werror
export CPPFLAGS := ${CPPFLAGS}

export CC = gcc

SUBDIRS = src/backend plugins


all: clean_install prepare_dir
	for target_dir in $(SUBDIRS); do \
		make -f $(MAINDIR)/$$target_dir/Makefile; \
	done

prepare_dir:
	mkdir install
	mkdir install/plugins

clean:
	rm -rf *.o
	rm -rf *.so
	rm -rf *.a

clean_install:
	rm -rf install
