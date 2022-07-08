CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c18 -O3 $$(pkg-config --cflags json-c openssl)
LIBS = $$(pkg-config --libs json-c openssl) -lws2_32

TARGET = libxtb.a
OUTPUT = release

INCLUDE_PATH = /usr/include
LIB_PATH = /usr/lib64

MODULES += xtb.o


TEST += test.o
TEST += xtb.o


all: env $(MODULES)
	ar -crs $(OUTPUT)/$(TARGET) $(MODULES)


%.o:
	$(CC) $(CFLAGS) -c $<


-include dep.list


exec: all
	$(OUTPUT)/$(TARGET)

build_test: env $(TEST)
	$(CC) $(CFLAGS) $(TEST) $(LIBS) -o $(OUTPUT)/test

test: build_test
	$(OUTPUT)/test


.PHONY: env dep clean install


dep:
	$(CC) -MM test/*.c src/*.c > dep.list


env:
	mkdir -pv $(OUTPUT)


install:
	cp -v $(OUTPUT)/$(TARGET) $(LIB_PATH)/$(TARGET)
	cp -v src/xtb.h $(INCLUDE_PATH)/xtb.h


clean: 
	rm -rvf $(OUTPUT)
	rm -vf ./*.o



