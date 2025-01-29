all: wrapper.o syscall.o assembler.o
	gcc -shared -o libmy_time_wrapper.so time_wrap.o -L $(MAINDIR)/install -Wl,-rpath,$(MAINDIR)/install
	mv ./libmy_time_wrapper.so install

	gcc -shared -o libmy_time_syscall.so time_syscall.o -L $(MAINDIR)/install -Wl,-rpath,$(MAINDIR)/install
	mv ./libmy_time_syscall.so install

	gcc -shared -o libmy_time_assembler.so time_assembler.o -L $(MAINDIR)/install -Wl,-rpath,$(MAINDIR)/install
	mv ./libmy_time_assembler.so install

	ln -s ./libmy_time_assembler.so ./install/libmy_time.so

wrapper.o:
	gcc -fPIC -c src/backend/my_time/time_wrap.c

syscall.o:
	gcc -fPIC -c src/backend/my_time/time_syscall.c

assembler.o:
	gcc -fPIC -c src/backend/my_time/time_assembler.s