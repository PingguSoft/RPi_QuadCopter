# specifies where to install the binaries after compilation
# to use another directory you can specify it with:
# $ sudo make DESTDIR=/some/path install
DESTDIR = /usr/local

# set the compiler to use
CC = g++

SVNDEV := -D'SVN_REV="$(shell svnversion -c .)"'
CFLAGS += $(SVNDEV)

# general compile flags, enable all warnings to make compile more verbose
CFLAGS += -DLINUX -D_GNU_SOURCE -Wall
CFLAGS += -g -Wuninitialized
#CFLAGS +=  -DDEBUG

# we are using the libraries "libpthread" and "libdl"
# libpthread is used to run several tasks (virtually) in parallel
# libdl is used to load the plugins (shared objects) at runtime
LFLAGS += -lpthread -ldl

# define the name of the program
APP_BINARY = QuadCopter

# define the names of object files
OBJECTS=main.o SerialProtocol.o WiFiProtocol.o pwm.o

# this is the first target, thus it will be used implictely if no other target
# was given. It defines that it is dependent on the application target and
# the plugins
all: $(APP_BINARY)

$(APP_BINARY): main.cpp main.o SerialProtocol.cpp SerialProtocol.o WiFiProtocol.cpp WiFiProtocol.o pwm.c pwm.o
	$(CC) $(CFLAGS) $(OBJECTS) $(LFLAGS) -o $(APP_BINARY)

# cleanup
clean:
	rm -f *.a *.o $(APP_BINARY) core *~ *.so *.lo
