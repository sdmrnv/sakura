CC      = cc
CFLAGS  = -Wall -Werror
LIBS    = -L/opt/instantclient_21_1/ -lclntsh #-locci
TARGET  = sakura
OPT     =  -DMULTI -DNF

#---------------------------------------------------------------------------------
#DO NOT EDIT BELOW THIS LINE
#---------------------------------------------------------------------------------

all:  $(TARGET)

soc.o: soc.c soc.h debug.h
        $(CC) $(CFLAGS) $(OPT) -c $< -o $@

ora.o:  ora.c ora.h debug.h
        $(CC) $(CFLAGS) $(OPT) -c $< -o $@

$(TARGET): soc.o ora.o
        $(CC) $(LIBS) soc.o ora.o -o $@

install:
        install $(TARGET) /usr/local/bin

clean:
      	rm -rf *.o $(TARGET)

cli: client/cli.c
        cc client/cli.c -o cli $(OPT)

cli_perf: client/cli_perf.c
        cc client/cli_perf.c -o cli_perf -lpthread $(OPT)

.PHONY: clean directories client
