ZLIB_PATH = ./zlib-1.2.8
MINIZIP_PATH = $(ZLIB_PATH)/contrib/minizip
CFLAGS = -I$(MINIZIP_PATH)

ZIP_UNZ_OBJS = test.o zip_manager.o $(MINIZIP_PATH)/unzip.o $(MINIZIP_PATH)/ioapi.o\
							 $(MINIZIP_PATH)/zip.o $(ZLIB_PATH)/libz.a

.c.o:
	g++ -c $(CFLAGS) *.cpp

test: .c.o $(ZIP_UNZ_OBJS)
	g++ -o $@ $(CFLAGS) $(ZIP_UNZ_OBJS)
