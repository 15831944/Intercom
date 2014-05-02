
DESTDIR=/usr
PREFIX=/local

STATIC=picassoLib.a
DYNAMIC=picassoLib.so

#DEBUG	= -g -O0
DEBUG	= -O2
CC      = $(GCC)
INCLUDE	= -I.
CFLAGS	= $(DEBUG) -Wall $(INCLUDE) -Winline -pipe -fPIC
LDFLAGS = -lm
LIBS    = -lm

SRC	=	picasso.c serial.c 

# May not need to  alter anything below this line
###############################################################################

OBJ	=	$(SRC:.c=.o)

all:	$(DYNAMIC)

static: $(STATIC)

$(STATIC):	$(OBJ)
	@echo "[Link (Static)]"
	@ar rcs $(STATIC) $(OBJ)
	@ranlib $(STATIC)
#	@size   $(STATIC)

$(DYNAMIC):     $(OBJ)
	@echo "[Link (Dynamic)]"
	@$(CC) -shared -Wl,-soname,picassoLib.so -o picassoLib.so -lpthread $(OBJ)

.c.o:
	@echo [Compile] $<
	@$(CC) -c $(CFLAGS) $< -o $@

.PHONEY:	clean
clean:
	rm -f $(OBJ) *~ core tags *.bak Makefile.bak picassoLib.*

.PHONEY:	tags
tags:	$(SRC)
	@echo [ctags]
	@ctags $(SRC)

.PHONEY:	install
install:	$(STATIC)
	@echo "[Install Static]"
	@cp picassoLib.a			../lib
	@cp picasso.h				../inc


.PHONEY:	depend
depend:
	makedepend -Y $(SRC)

# DO NOT DELETE

socket.o: picasso.h serial.h
