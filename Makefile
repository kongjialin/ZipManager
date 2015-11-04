ZLIB_PATH = ./zlib-1.2.8
MINIZIP_PATH = $(ZLIB_PATH)/contrib/minizip
CURL_INCLUDE = ./curl-7.45.0/include/curl
CFLAGS = -I$(MINIZIP_PATH) -I$(CURL_INCLUDE)

OBJS = test.o zip_manager.o uploader.o ThreadQueue.o $(MINIZIP_PATH)/unzip.o $(MINIZIP_PATH)/ioapi.o\
       $(MINIZIP_PATH)/zip.o $(ZLIB_PATH)/libz.a

.c.o:
	g++ -c $(CFLAGS) test.cpp zip_manager.cpp ThreadQueue.cpp uploader.cpp

test: .c.o $(OBJS)
	g++ -o $@ $(CFLAGS) $(OBJS) -L/usr/local/lib/ -lcurl
