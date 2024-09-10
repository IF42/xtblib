CC = gcc
CFLAGS = -Wall -Wextra -pedantic -Ofast $$(pkg-config --cflags openssl) -I/usr/include
LIBS = $$(pkg-config --libs openssl) -lm -lthr -ljson 

INCLUDE_PATH=
LIB_PATH=


ifeq ($(UNAME), Linux)	
	INCLUDE_PATH+=/usr/include/
	LIB_PATH+=/usr/lib64/
else
	INCLUDE_PATH+=/usr/include/
	LIB_PATH+=/usr/lib/
endif



TARGET = libxtblib.a
CACHE = .cache
OUTPUT = $(CACHE)/release

ID = 50285159
PASS = 4xl74fx0.H


MODULES += xtblib.o
TEST += test.o


OBJ=$(addprefix $(CACHE)/,$(MODULES))
T_OBJ=$(addprefix $(CACHE)/,$(TEST))


all: env $(OBJ)
	ar -crs $(OUTPUT)/$(TARGET) $(OBJ)


%.o:
	$(CC) $(CFLAGS) -c $< -o $@


-include dep.list


exec: env $(OBJ) $(T_OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(T_OBJ) $(LIBS) -o $(OUTPUT)/test	
	$(OUTPUT)/test $(ID) $(PASS)


.PHONY: env dep clean install


dep:
	$(CC) -MM  test/*.c -g src/*.c | sed 's|[a-zA-Z0-9_-]*\.o|$(CACHE)/&|' > dep.list


env:
	mkdir -pv $(CACHE)
	mkdir -pv $(OUTPUT)


install:
	cp -v $(OUTPUT)/$(TARGET) $(LIB_PATH)/$(TARGET)
	cp -v src/xtblib.h $(INCLUDE_PATH)/xtblib.h


clean: 
	rm -rvf $(CACHE)



