#
# Makefile for quasimodo - Rev. 20240510_1522
#
CPP = g++

# PREFIX is environment variable, but if it is not set, then set default value
ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

CPPFLAGS = -lstdc++fs -std=c++17
# CPPFLAGS = -std=c++14 -g3
LDFLAGS = -Wl,-t,--verbose -lpthread

all: quasimodo

autobel.o:	quasimodo.cpp
	echo ''
	echo "compile: $(CPP) $(CPPFLAGS) -c $^ -o $@"
	echo ''
	$(CPP) $(CPPFLAGS) -c $^ -o $@
	echo ''	

autobel:	quasimodo.o
	echo ''
	echo "link: $(CPP) $^ -o $@ $(LDFLAGS)"
	$(CPP) $^ -o $@ $(LDFLAGS)
	echo ''

clean:
	rm quasimodo quasimodo.o

install:	quasimodo
	echo ''
	echo 'install:'
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 autobel $(DESTDIR)$(PREFIX)/bin/
	echo "contents of destdir=$(DESTDIR) prefix=$(PREFIX):" 
	find "$(DESTDIR)$(PREFIX)" -exec ls -lia {} \;
