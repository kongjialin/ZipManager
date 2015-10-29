#if (!defined(_WIN33)) && (!defined(WIN32)) && (!defined(__APPLE__))
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

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <algorithm>

#include "zip_manager.h"

#define WRITEBUFFERSIZE (9182)

ZipManager::ZipManager(const string& source_path, const string& dest_path,
                       const string& inqueue_path, const string& unz_out_path,
                       const string& unz_success_path, const string& unz_fail_path,
                       const string& zip_fail_path) :
    source_path_(source_path), dest_path_(dest_path),
    inqueue_path_(inqueue_path), unz_out_path_(unz_out_path),
    unz_success_path_(unz_success_path), unz_fail_path_(unz_fail_path),
    zip_fail_path_(zip_fail_path) {
}

ZipManager::~ZipManager() {
}

/** Scan the zip files in source_path, push the name of the zip file(string)
 * into the queue, and moved the in-queue zip files to inqueue_path.
 * @return the count of scanned zip files
 **/
int ZipManager::ScanZip(vector<string>& v) {
  string zip(".zip");
  int count = 0;
  DIR* dir = opendir(source_path_.c_str());
  if (dir == NULL) {
    printf("Error open directory %s\n", source_path_.c_str());
  } else {
    dirent* entry;
    while ((entry = readdir(dir))) {
      string name(entry->d_name);
      if (std::equal(zip.rbegin(), zip.rend(), name.rbegin())) {
        v.push_back(name);
        ++count;
        string old_path(source_path_ + '/' + name);
        string new_path(inqueue_path_ + '/' + name);
        if (rename(old_path.c_str(), new_path.c_str()) == -1) {
          printf("Error moving the in-queue zip file %s to inqueue_path\n",
              name.c_str());
        }
      }
    }
    closedir(dir);
  }
  return count;
}

/** Unzip the zip file. If succeed, the zip file is moved to unz_success_path,
 * and the unzipped directory is in unz_out_path. If fail, the zip file is 
 * moved to unz_fail_path.
 * @return 0(success), others(failure)
 **/
int ZipManager::Unzip(string zipfile) {
  string zipfile_fullname = inqueue_path_ +'/' + zipfile;
  unzFile uf = unzOpen64(zipfile_fullname.c_str());
  if (uf == NULL) {
    printf("Cannot open %s\n", zipfile_fullname.c_str());
    return -1;
  }
  printf("%s opened\n", zipfile_fullname.c_str());
  zipfile.erase(zipfile.find(".zip"));
  string extract_path(unz_out_path_ + '/' + zipfile);
  int err_code = DoExtract(uf, extract_path);
  if (err_code == 0) { // sucessful unzip
    string success_path(unz_success_path_ + '/' + zipfile + ".zip");
    if (rename(zipfile_fullname.c_str(), success_path.c_str()) == -1)
      printf("Error moving the successfully-unzipped zip file %s from "
          "inqueue_path to unz_success_path\n", zipfile_fullname.c_str());
  } else { // failed unzip
    if (Removedir(extract_path.c_str()) == -1)
      printf("Error removing the failed unzipped directory %s from "
          "unz_out_path\n", extract_path.c_str());
    string fail_path(unz_fail_path_ + '/' + zipfile + ".zip");
    if (rename(zipfile_fullname.c_str(), fail_path.c_str()) == -1)
      printf("Error moving the failed-unzipped zip file %s from "
          "inqueue_path to unz_fail_path", zipfile_fullname.c_str());
  }
  unzClose(uf);
  return err_code;
}

/** Zip all the files in dir_to_zip to one zip file. If failed, the 
 * dir_to_zip is moved to zip_fail_path.
 * @param dir_to_zip The relative directory under the unz_out_path
 * @return 0(success), others(failure)
 **/
int ZipManager::Zip(const string& dir_to_zip) {
  // unz_out_path_ or other path ?
  string absolute_dir(unz_out_path_ + '/' + dir_to_zip);
  string zipfile_name(dest_path_ + '/' + dir_to_zip + ".zip");
  zipFile zf = zipOpen64(zipfile_name.c_str(), 0);
  int err = 0;
  if (zf == NULL) {
    printf("Error opening %s\n", zipfile_name.c_str());
    return ZIP_ERRNO;
  }
  printf("Creating %s\n", zipfile_name.c_str());
  vector<string> files;
  err = FindFilesToZip(absolute_dir, files);
  if (!err) {
    for (int i = 0; (i < files.size()) && (!err); ++i) {
      files[i].erase(0, absolute_dir.size());
      while (files[i][0] == '/')
        files[i].erase(0, 1);
      err = DoZipCurrentFile(zf, absolute_dir, files[i]);
    }
  }
  if (!err) {
    err = zipClose(zf, NULL);
    if (err != ZIP_OK)
      printf("Error in closing %s\n", zipfile_name.c_str());
  } else {
    zipClose(zf, NULL);
  }
  if (err) {
    string zip_fail_dir(zip_fail_path_ + '/' + dir_to_zip);
    if (rename(absolute_dir.c_str(), zip_fail_dir.c_str()) == -1)
      printf("Error moving the directory failed in zipping %s to "
          "zip_fail_path\n", dir_to_zip.c_str());
  }
  return err;
}

/** Achieve the command "mkdir -p newdir"
 * @return 0(success), -1 and others(failure)
 **/
int ZipManager::Makedir(const char* newdir) {
  char* buffer;
  char* p;
  int len = (int)strlen(newdir);
  if (len <= 0) return -1;

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
    return 0;
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
      return -1;
    }
    if (hold == 0)
      break;
    *p++ = hold;
  }
  free(buffer);
  return 0;
}

int ZipManager::Mymkdir(const char* dirname) {
  return mkdir(dirname, 0755);
}

/** Internal function for extracting the zip file specified by uf.
 * @param extract_dir  Directory to store the unzipped files.
 * @return 0(success), others(failure)
 **/
int ZipManager::DoExtract(unzFile uf, const string& extract_dir) {
  uLong i;
  unz_global_info64 gi;
  int err;

  err = unzGetGlobalInfo64(uf, &gi);
  if (err != UNZ_OK) {
    printf("Error %d with zipfile in unzGetGlobalInfo\n", err);
    return err;
  }

  for (i = 0; i < gi.number_entry; ++i) {
    err = DoExtractCurrentFile(uf, extract_dir);
    if (err != UNZ_OK)
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

/** Extract the current file in the zip file specified by uf.
 * @return 0(success), others(failure)
 **/
int ZipManager::DoExtractCurrentFile(unzFile uf, const string& extract_dir) {
  char filename_inzip[4096];
  char* write_filename;
  string fullname;
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

  fullname = extract_dir + '/' + string(filename_inzip);
  write_filename = new char[fullname.size() + 1];
  std::copy(fullname.begin(), fullname.end(), write_filename);
  write_filename[fullname.size()] = '\0';

  p = filename_withoutpath = write_filename;
  while (*p != '\0') {
    if ((*p == '/') || (*p == '\\'))
      filename_withoutpath = p + 1;
    ++p;
  }

  if (*filename_withoutpath == '\0') {
    printf("creating directory: %s\n", write_filename);
    Makedir(write_filename);
  } else {
    err = unzOpenCurrentFile(uf);
    if (err != UNZ_OK) {
      printf("error %d with zipfile in unzopencurrentfile\n", err);
    } else {
      fout = FOPEN_FUNC(write_filename, "wb");
      // some zipfile don't contain directory alone before file
      if (fout == NULL) {
        char c = *(filename_withoutpath - 1);
        *(filename_withoutpath - 1) = '\0';
        Makedir(write_filename);
        *(filename_withoutpath - 1) = c;
        fout = FOPEN_FUNC(write_filename, "wb");
      }
      if (fout == NULL) {
        printf("Error opening write file %s\n", write_filename);
        err = -1;
      } else {
        //printf("Extracting: %s\n", filename_inzip);
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
  delete write_filename;
  return err;
}

/** remove directory recursively
 * return 0(success), -1(failure), 1(path not exist)
 **/
int ZipManager::Removedir(const char* path) {
  DIR* dir = opendir(path);
  size_t path_len = strlen(path);
  int rc = 1;
  if (dir) {
    dirent* entry;
    rc = 0;
    while (!rc && (entry = readdir(dir))) {
      int sub_rc = -1;
      char* buf;
      size_t buf_size;
      // skip the "." and ".."
      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
        continue;
      buf_size = path_len + strlen(entry->d_name) + 2;
      buf = (char*)malloc(buf_size);
      if (buf) {
        struct stat statbuf;
        snprintf(buf, buf_size, "%s/%s", path, entry->d_name);
        if (!stat(buf, &statbuf)) {
          if (S_ISDIR(statbuf.st_mode))
            sub_rc = Removedir(buf);
          else
            sub_rc = unlink(buf);
        }
        free(buf);
      }
      rc = sub_rc;
    }
    closedir(dir);
  }
  if (!rc)
    rc = rmdir(path);
  return rc;
}

int ZipManager::IsLargeFile(const char* filename) {
  int large_file = 0;
  ZPOS64_T pos = 0;
  FILE* p_file = FOPEN_FUNC(filename, "rb");
  if (p_file) {
    int n = FSEEKO_FUNC(p_file, 0, SEEK_END);
    pos = FTELLO_FUNC(p_file);
    //printf("File: %s is %lld bytes\n", filename, pos);
    if (pos >= 0xffffffff)
      large_file = 1;
    fclose(p_file);
  }
  return large_file;
}

/** Find all files to zip into one zipfile.
 * @param dir The absolute path of the directory.
 * @param files The vector containing all the files under the top-level directory.
 * @return 0(success), -1(failure)
 **/
int ZipManager::FindFilesToZip(const string& dir, vector<string>& files) {
  int rc = 0;
  DIR* d = opendir(dir.c_str());
  if (d) {
    dirent* entry;
    while (!rc && (entry = readdir(d))) {
      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
        continue;
      int sub_rc = -1;
      struct stat statbuf;
      string subdir(dir + '/' + string(entry->d_name));
      if (!stat(subdir.c_str(), &statbuf)) {
        if (S_ISDIR(statbuf.st_mode)) {
          sub_rc = FindFilesToZip(subdir, files);
        } else {
          files.push_back(subdir);
          sub_rc = 0;
        }
      }
      rc = sub_rc;
    }
    closedir(d);
  } else {
    rc = -1;
  }

  return rc;
}

/** Zip a file into the zipfile.
 * @param dir_of_file The absolute path of the top-level directory where all
 *                    the files of the same target zipfile are.
 * @param filename_inzip The file name relative to the dir_of_file.
 * @return 0(success), others(failure)
 **/
int ZipManager::DoZipCurrentFile(zipFile zf, const string& dir_of_file,
    const string& filename_inzip) {
  int err = 0;
  int size_buf = WRITEBUFFERSIZE;
  void* buf = (void*)malloc(size_buf);
  if (buf == NULL) {
    printf("Error allocating memory\n");
    return ZIP_INTERNALERROR;
  }
  FILE* fin;
  int size_read;
  zip_fileinfo zi;
  string file_fullname(dir_of_file + '/' + filename_inzip);
  int zip64 = IsLargeFile(file_fullname.c_str());
  err = zipOpenNewFileInZip64(zf, filename_inzip.c_str(), &zi, NULL, 0, NULL, 0,
      NULL, 0, 0, zip64);
  if (err != ZIP_OK) {
    printf("Error in opening %s in zipfile\n", filename_inzip.c_str());
  } else {
    fin = FOPEN_FUNC(file_fullname.c_str(), "rb");
    if (fin == NULL) {
      err = ZIP_ERRNO;
      printf("Error in opening %s for reading\n", file_fullname.c_str());
    }
  }

  if (err == ZIP_OK) {
    do {
      size_read = (int)fread(buf, 1, size_buf, fin);
      if (size_read < size_buf) {
        if (feof(fin) == 0) {
          printf("Error in reading %s\n", file_fullname.c_str());
          err = ZIP_ERRNO;
          break;
        }
      }
      if (size_read > 0) {
        err = zipWriteInFileInZip(zf, buf, size_read);
        if (err < 0)
          printf("Error in writing %s in the zipfile\n", filename_inzip.c_str());
      }
    } while (err == ZIP_OK && size_read > 0);
  }
  free(buf);
  if (fin)
    fclose(fin);
  if (err < 0) {
    err = ZIP_ERRNO;
    zipCloseFileInZip(zf);
  } else {
    err = zipCloseFileInZip(zf);
    if (err != ZIP_OK)
      printf("Error in closing %s in the zipfile\n", filename_inzip.c_str());
  }

  return err;
}
