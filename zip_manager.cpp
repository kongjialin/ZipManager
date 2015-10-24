#if (!defined(_WIN32)) && (!defined(WIN32)) && (!defined(__APPLE__))
        #ifndef __USE_FILE_OFFSET64
                #define __USE_FILE_OFFSET64
        #endif
        #ifndef __USE_LARGEFILE64
                #define __USE_LARGEFILE64
        #endif
        #ifndef _LARGEFILE64_SOURCE
                #define _LARGEFILE64_SOURCE
        #endif
        #ifndef _FILE_OFFSET_BIT
                #define _FILE_OFFSET_BIT 64
        #endif
#endif

#ifdef __APPLE__
// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#define FTELLO_FUNC(stream) ftello(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>

#include <iostream>
#include <string>
using namespace std;

#include "zip_manager.h"

#define CASESENSITIVITY (0)
#define WRITEBUFFERSIZE (9182)
#define MAXFILENAME (256)

void ZipManager::test() {
  const char* zipfilename = NULL;
  char filename_try[MAXFILENAME+16] = "";
  const char *dirname=NULL;
  unzFile uf=NULL;

  string dir, file;
  cout << "out dir: \n";
  cin >> dir;
  dirname = dir.c_str();
  cout << "zip file: \n";
  cin >> file;
  zipfilename = file.c_str();
  strncpy(filename_try, zipfilename,MAXFILENAME-1);
  filename_try[ MAXFILENAME ] = '\0';
  uf = unzOpen64(zipfilename);
  if (uf==NULL)
  {
    strcat(filename_try,".zip");
    uf = unzOpen64(filename_try);
  }
  if (uf==NULL)
  {
    printf("Cannot open %s or %s.zip\n",zipfilename,zipfilename);
    return;
  }
  printf("%s opened\n",filename_try);

  if (chdir(dirname)) {
    printf("Error changing into %s, aborting\n", dirname);
    return;
  }
  DoExtract(uf);
  unzClose(uf);
}

int ZipManager::Makedir(char* newdir) {
  char* buffer;
  char* p;
  int len = (int)strlen(newdir);
  if (len <= 0) return 0;

  buffer = (char*)malloc(len + 1);
  if (buffer == NULL) {
    printf("Error allocating memory\n");
    return UNZ_INTERNALERROR;
  }
  strcpy(buffer, newdir);

  if (buffer[len - 1] == '/')
    buffer[len - 1] = '\0';
  if (Mymkdir(buffer) == 0) {
    free(buffer);
    return 1;
  }

  p = buffer + 1;
  while (1) {
    char hold;
    while (*p && *p != '\\' && *p != '/')
      ++p;
    hold = *p;
    *p = 0;
    if ((Mymkdir(buffer) == -1) && (errno == ENOENT)) {
      printf("Couldn't create directory %s\n", buffer);
      free(buffer);
      return 0;
    }
    if (hold == 0)
      break;
    *p++ = hold;
  }
  free(buffer);
  return 1;
}

int ZipManager::Mymkdir(const char* dirname) {
  return mkdir(dirname, 0755);
}

int ZipManager::DoExtract(unzFile uf) {
  uLong i;
  unz_global_info64 gi;
  int err;
  FILE* fout = NULL;

  err = unzGetGlobalInfo64(uf, &gi);
  if (err != UNZ_OK)
    printf("Error %d with zipfile in unzGetGlobalInfo\n", err);

  for (i = 0; i < gi.number_entry; ++i) {
    if (DoExtractCurrentFile(uf) != UNZ_OK)
      break;
    if ((i + 1) < gi.number_entry) {
      err = unzGoToNextFile(uf);
      if (err != UNZ_OK) {
        printf("Error %d with zipfile in unzGoToNextFile\n", err);
        break;
      }
    }
  }

  return err;
}

int ZipManager::DoExtractCurrentFile(unzFile uf) {
  char filename_inzip[256];
  char* filename_withoutpath;
  char* p;
  int err = UNZ_OK;
  FILE* fout = NULL;
  void* buf;
  uInt size_buf;

  unz_file_info64 file_info;
  err = unzGetCurrentFileInfo64(uf, &file_info, filename_inzip,
                                sizeof(filename_inzip), NULL, 0, NULL, 0);
  if (err != UNZ_OK) {
    printf("Error %d with zipfile in unzGetCurrentFileInfo\n", err);
    return err;
  }

  size_buf = WRITEBUFFERSIZE;
  buf = (void*)malloc(size_buf);
  if (buf == NULL) {
    printf("Error allocating memory\n");
    return UNZ_INTERNALERROR;
  }

  p = filename_withoutpath = filename_inzip;
  while (*p != '\0') {
    if ((*p == '/') || (*p == '\\'))
      filename_withoutpath = p + 1;
    ++p;
  }

  if (*filename_withoutpath == '\0') {
    printf("Creating directory: %s\n", filename_inzip);
    Mymkdir(filename_inzip);
  } else {
    err = unzOpenCurrentFile(uf);
    if (err != UNZ_OK) {
      printf("Error %d with zipfile in unzOpenCurrentFile\n", err);
    } else {
      fout = FOPEN_FUNC(filename_inzip, "wb");
      // some zipfile don't contain directory alone before file
      if ((fout == NULL) && (filename_withoutpath != (char*)filename_inzip)) {
        char c = *(filename_withoutpath - 1);
        *(filename_withoutpath - 1) = '\0';
        Makedir(filename_inzip);
        *(filename_withoutpath - 1) = c;
        fout = FOPEN_FUNC(filename_inzip, "wb");
      }
      if (fout == NULL) {
        printf("Error opening %s\n", filename_inzip);
      } else {
        printf("Extracting: %s\n", filename_inzip);
        do {
          err = unzReadCurrentFile(uf, buf, size_buf);
          if (err < 0) {
            printf("Error %d with zipfile in unzReadCurrentFile\n", err);
            break;
          }
          if (err > 0) {
            if (fwrite(buf, err, 1, fout) != 1) {
              printf("Error in writing extracted file\n");
              err = UNZ_ERRNO;
              break;
            }
          }
        } while (err > 0);
        if (fout)
          fclose(fout);
        if (err == 0)
          ChangeFileDate(filename_inzip, file_info.tmu_date);
      }
      
      if (err == UNZ_OK) {
        err = unzCloseCurrentFile(uf);
        if (err != UNZ_OK)
          printf("Error %d with zipfile in unzCloseCurrentFile\n", err);
      } else {
        unzCloseCurrentFile(uf);
      }
    }
  }
  free(buf);
  return err;
}

void ZipManager::ChangeFileDate(const char* filename, tm_unz tmu_date) {
  struct utimbuf ut;
  struct tm newdate;
  newdate.tm_sec = tmu_date.tm_sec;
  newdate.tm_min = tmu_date.tm_min;
  newdate.tm_hour = tmu_date.tm_hour;
  newdate.tm_mday = tmu_date.tm_mday;
  newdate.tm_mon = tmu_date.tm_mon;
  if (tmu_date.tm_year > 1900)
    newdate.tm_year = tmu_date.tm_year - 1900;
  else
    newdate.tm_year = tmu_date.tm_year;
  newdate.tm_isdst = -1;

  ut.actime = ut.modtime = mktime(&newdate);
  utime(filename, &ut);
}
