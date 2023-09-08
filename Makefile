CC = gcc
CFLAGS = -Wall -Wextra -pedantic -Ofast $$(pkg-config --cflags openssl)
LIBS = $$(pkg-config --libs openssl) -ljson

ifeq ($(OS),Windows_NT)
	LIBS += -l:libvector.so 
else
	LIBS += -lvector
endif

TARGET = libxtb.a
CACHE = .cache
OUTPUT = $(CACHE)/release

ID = 15074665
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



