SOURCES=cli.cpp  configs.cpp  files.cpp  main.cpp  string.cpp  templates.cpp
CFLAGS=-Weverything -Wno-shorten-64-to-32 -Wno-padded -Wno-old-style-cast -Wno-zero-as-null-pointer-constant -Wno-c++98-compat-pedantic

bin/qs: _bindir
	clang $(CFLAGS) -O3 $(SOURCES) -o bin/qs

debug:
	clang $(CFLAGS) -fsanitize=address --debug -ggdb $(SOURCES) -o bin/qs

install: bin/qs
	cp bin/qs /usr/local/bin/qs
	rm -rf ./bin

uninstall:
	rm /usr/local/bin/qs

test: bin/qs
	qs test

clean:
	rm -rf ./bin

_bindir:
	mkdir -p ./bin

