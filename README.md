1. Using zlib under Linux 
----
(for zip and unzip)

    wget http://zlib.net/zlib-1.2.8.tar.gz
    tar -zxvf zlib-1.2.8.tar.gz
    cd zlib-1.2.8
    ./configure
    make
    make install

And **minizip** is under zlib-1.2.8/contrib/minizip

ToDo:

ZipManager

1) the include path of the source files and the compile flag (-I and so on) should be taken good care of.

2) maximum length of the pathname = ? (currently 4096)

3) build minizip to a static library

4) thread-safe queue ?
****


2. Using libcurl
----
(for sftp transport)  
[Adding sftp to curl](http://andrewberls.com/blog/post/adding-sftp-support-to-curl)

    1) Install libssh2
    download libssh2, unpack
    cd libssh2/
    ./configure (might need --with-libssl-prefix)(might need openssl-devel installed first)
    make
    sudo make install
    
    2) Install/configure/build cURL using libssh2
    download and unzip curl
    cd dir/of/curl
    ./configure --with-libssh2=/usr/local
    make
    sudo make install
    
    curl -V to check sftp is in the Protocols list

****

3. Processing Flow
----
**Directories needed:<br> source/, dest/, unz_out/, fail/, success/, dest/, backup/**

    unzip --(success)--> the extracted directory is under unz_out
          -----(fail)--> the zip file is moved from souce to fail
          
    zip   --(success)--> the zipped file is under dest, the corresponding directory
                         under unz_out is deleted, and the corresponding zip file in
                         source is moved to success.
          -----(fail)--> the corresponding directory under unz_out is deleted   
    upload--(success)--> the zip file in dest is moved to backup                 


There are two queues (thread-safe queue) needed: **prepare** and **inprocessing**.  
A `scan thread` finds a zip file in the source, checks and confirm this zip file is not in the **inprocessing** queue, and pushes the file name into the **prepare** queue.  
Many `worker threads` pop a file from the **prepare** queue and push it into the **inprocessing** queue, and begin unzipping it, then doing other processing work on the unzipped files. When processing done, the directory is zipped and the file in **inprocessing** queue is poped (with the file moved from source/ to success/).  
An `upload thread` scans the generated zipped files and upload them to a sftp server.
****

4. Downtime cases handling
----
Each worker thread takes a file and does unzipping-processing-zipping in a single work flow. The source zip file is moved to **success/** only when the whole flow succeeds. So if the machine goes down at any time during the working flow, the zip file is still under **source/**. And when the machine restarts, the program will redo the whole procedure for all the zip files under **source/**, thus no difference from the usual work flow, and no data will lose.
