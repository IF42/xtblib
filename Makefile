CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c18 -O3 $$(pkg-config --cflags json-c openssl)
LIBS = $$(pkg-config --libs json-c openssl) -lws2_32 -L/mingw64/lib/ -l:libvector.so

TARGET = libxtb.a
CACHE = .cache
OUTPUT = $(CACHE)/release

ID = 14497513
PASS = 4xl74fx0.H

INCLUDE_PATH = /usr/include
LIB_PATH = /usr/lib64

MODULES += xtb.o

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
	$(CC) -MM  test/*.c src/*.c  | sed 's|[a-zA-Z0-9_-]*\.o|$(CACHE)/&|' > dep.list


env:
	mkdir -pv $(CACHE)
	mkdir -pv $(OUTPUT)


install:
	cp -v $(OUTPUT)/$(TARGET) $(LIB_PATH)/$(TARGET)
	cp -v src/xtb.h $(INCLUDE_PATH)/xtb.h


clean: 
	rm -rvf $(CACHE)



