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
#include <sys/types.h>
#include <time.h>
#include <utime.h>

#include <algorithm>

#include "zip_manager.h"

#define WRITEBUFFERSIZE (9182)

ZipManager::ZipManager(const string& source_path, const string& dest_path,
                       const string& unz_out_path, const string& success_path,
                       const string& fail_path) :
    source_path_(source_path), dest_path_(dest_path),
    unz_out_path_(unz_out_path), success_path_(success_path),
    fail_path_(fail_path) {
}

ZipManager::~ZipManager() {
}

/** Scan the zip files in source_path, and if the zip file is not in processing
  * queue, push the name of the zip file (string, in the form of "xxx.zip")
  * into the prepare queue.
  * @param prepare[in, out] The queue preparing the zip file.
  * @param processing[in, out] The queue of zip files that are in processing.
  * @return The count of scanned zip files, put into prepare queue.
  **/
int ZipManager::ScanZip(ThreadQueue& prepare, ThreadQueue& processing) {
  string zip(".zip");
  int count = 0;
  DIR* dir = opendir(source_path_.c_str());
  if (dir == NULL) { // opendir fail
    printf("Error open directory %s: %s\n", source_path_.c_str(), strerror(errno));
  } else { // opendir success
    dirent* entry;
    while ((entry = readdir(dir))) { // iterate all the files under the directory.
      string name(entry->d_name);
      if (std::equal(zip.rbegin(), zip.rend(), name.rbegin())) {
        // the file scanned is of type .zip
        if (!processing.isExist(&name)) { // ensure the zip is not in processing
          prepare.Push(&name);
          ++count;
        }
      }
    }
    closedir(dir);
  }
  return count;
}

/** Unzip the zip file. If succeed, the unzipped directory is in unz_out_path.
  * If fail, the zip file is moved from source_path to fail_path.
  * @param zipfile[in] The file name to unzip (in the form "xxx.zip").
  * @return 0(success), others(failure)
  **/
int ZipManager::Unzip(string zipfile) {
  string zipfile_fullname = source_path_ +'/' + zipfile;
  int err = 0;
  zipfile.erase(zipfile.find(".zip"));
  unzFile uf = unzOpen64(zipfile_fullname.c_str());
  if (uf == NULL) { // fail in opening zipfile
    printf("Cannot open %s: File doesn't exist or is not valid\n",
           zipfile_fullname.c_str());
    err = -1;
  } else { // succeed in opening zipfile
    //printf("%s opened\n", zipfile_fullname.c_str());
    string extract_path(unz_out_path_ + '/' + zipfile);
    err = DoExtract(uf, extract_path);
    if (err) { // failed unzip
      // remove the unzipped directory having error from unz_out_path
      if (Removedir(extract_path.c_str()) == -1)
        printf("Error removing the failed unzipped directory %s from "
            "unz_out_path\n", extract_path.c_str());
    }
    unzClose(uf);
  }
  if (err) {
    // unzip fail, move the zip file from source_path to fail_path
    string fail_zip(fail_path_ + '/' + zipfile + ".zip");
    if (rename(zipfile_fullname.c_str(), fail_zip.c_str()) == -1 &&
        errno != ENOENT)
      printf("Error moving the failed-unzipped zip file %s from "
          "source_path to fail_path: %s\n", zipfile_fullname.c_str(),
          strerror(errno));
  }
  return err;
}

/** Zip all the files in dir_to_zip to one zip file. If fails, the 
  * dir_to_zip is deleted. If succeeds, the zip file is under dest_path,
  * the dir_to_zip is deleted from unz_out_path, and the corresponding
  * source zip file under source_path is moved to success_path as a backup.
  * @param dir_to_zip[in] The relative directory to the unz_out_path.
  *                       It should NOT end with '/'.
  * @param files[in] The vector containing the converted bcp and xml files.
           (files[files.size() - 1] is xml, and others are bcp files)
  * @return 0(success), others(failure)
  **/
int ZipManager::Zip(const string& dir_to_zip, const vector<string>& files) {
  string absolute_dir(unz_out_path_ + '/' + dir_to_zip);
  string zipfile_name(dest_path_ + '/' + dir_to_zip + "-ing");
  zipFile zf = zipOpen64(zipfile_name.c_str(), 0);
  int err = 0;
  if (zf == NULL) {
    printf("Error creating %s\n", zipfile_name.c_str());
    err = ZIP_ERRNO;
  } else { // creat (open) zip file in dest_path sucessfully
    //printf("Creating %s\n", zipfile_name.c_str());
    // pick out the entity directory
    string entity_dir(absolute_dir + '/');
    bool entity_dir_exist = false;
    vector<string> entity_files;
    DIR* d = opendir(absolute_dir.c_str());
    if (d) { // succeed in opening dir_to_zip
      dirent* entry;
      while ((entry = readdir(d))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
          continue;
        struct stat statbuf;
        string sub(absolute_dir + '/' + string(entry->d_name));
        if (!stat(sub.c_str(), &statbuf)) {
          if (S_ISDIR(statbuf.st_mode)) {
            entity_dir += string(entry->d_name);
            entity_dir_exist = true;
            break;
          }
        }
      }
      if (entity_dir_exist) {
        err = FindFilesToZip(entity_dir, entity_files);
      } else { // entity directory doesn't exist
        printf("Entity dirctory doesn't exist!\n");
        err = -1;
      }
    } else { // error in opening dir_to_zip
      printf("Error in opening directory%s\n", absolute_dir.c_str());
      err = -1;
    }
    if (!err) { // succeed in finding all the entity files to zip
      // zip all entity files
      for (int i = 0; (i < entity_files.size()) && (!err); ++i) {
        entity_files[i].erase(0, absolute_dir.size());
        while (entity_files[i][0] == '/')
          entity_files[i].erase(0, 1);
        err = DoZipCurrentFile(zf, absolute_dir, entity_files[i], NULL);
      }
      // zip all bcp files
      int file_count = files.size();
      for (int i = 0; (i < file_count - 1) && (!err); ++i) {
        err = DoZipCurrentFile(zf, absolute_dir, files[i], NULL);
      }
      // zip xml file
      string xml(files[file_count - 1]);
      xml.pop_back(); // becasuse the original xml file is .xml2
      if (!err)
        err = DoZipCurrentFile(zf, absolute_dir, files[file_count - 1], &xml);
    }
    if (!err) { // zip all the files successfully
      err = zipClose(zf, NULL); // NULL or 0?
      if (err != ZIP_OK)
        printf("Error in closing %s\n", zipfile_name.c_str());
    } else {
    // error in finding all the files or zipping a certain file
      zipClose(zf, NULL); // NULL or 0?
    }
  }
  if (err) { // any error happens in the whole zip procedure
  // delete the incomplete zip file having error from dest_path
    if (unlink(zipfile_name.c_str()) && errno != ENOENT)
      printf("Error deleting the failed zip file %s in dest_path: %s\n",
          zipfile_name.c_str(), strerror(errno));
  } else { // succeed in the whole zip procedure
  // move the source zip file from source_path to success_path as a backup
    string source_zip(source_path_ + '/' + dir_to_zip + ".zip");
    string success_zip(success_path_ + '/' + dir_to_zip + ".zip");
    if (rename(source_zip.c_str(), success_zip.c_str()) == -1)
      printf("Error moving the successful zip file %s from "
          "source_path to success_path: %s\n", zipfile_name.c_str(),
          strerror(errno));
  // rename the generated zip file from -ing to .zip
    string finished(dest_path_ + '/' + dir_to_zip + ".zip");
    if (rename(zipfile_name.c_str(), finished.c_str()) == -1)
      printf("Error renaming the successful zip file from "
          "%s to %s: %s\n", zipfile_name.c_str(), finished.c_str(),
          strerror(errno));
  }
  // delete the corresponding direcotry under unz_out_path
  if (Removedir(absolute_dir.c_str()) == -1)
    printf("Error in deleting the directory %s after zipping(succeed or fail)"
        "from unz_out_path\n", absolute_dir.c_str());

  return err;
}

/** Implement the command "mkdir -p newdir"
  * @param newdir[in] The directory to make(in the form of "/xx/xx/xx")
  * @return 0(success), -1 and others(failure)
  **/
int ZipManager::Makedir(const char* newdir) {
  int len = (int)strlen(newdir);
  if (len <= 0) return -1;

  char* buffer = (char*)malloc(len + 1);
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

  char* p = buffer + 1;
  while (1) {
    while (*p && *p != '\\' && *p != '/')
      ++p;
    char hold = *p;
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

/** Encapsulate the system call -- mkdir **/
int ZipManager::Mymkdir(const char* dirname) {
  return mkdir(dirname, 0755);
}

/** Internal function for extracting the zip file specified by uf.
  * @param uf[in] A pointer to the zip file to unzip.
  * @param extract_dir[in]  Directory to store the unzipped files of
  *                         the same zip file.
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
  // unzip all the files of a zip file
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

/** Extract the current one file in the zip file specified by uf.
  * @param uf[in] The pointer to the zip file to unzip.
  * @param extract_dir[in] The dirctory to store the unzipped file of uf.
  * @return 0(success), others(failure)
  **/
int ZipManager::DoExtractCurrentFile(unzFile uf, const string& extract_dir) {
  char filename_inzip[4096];
  int err = UNZ_OK;
  unz_file_info64 file_info;
  err = unzGetCurrentFileInfo64(uf, &file_info, filename_inzip,
      sizeof(filename_inzip), NULL, 0, NULL, 0);
  if (err != UNZ_OK) {
    printf("Error %d with zipfile in unzGetCurrentFileInfo\n", err);
    return err;
  }

  uInt size_buf = WRITEBUFFERSIZE;
  void* buf = (void*)malloc(size_buf);
  if (buf == NULL) {
    printf("Error allocating memory\n");
    return UNZ_INTERNALERROR;
  }

  string fullname = extract_dir + '/' + string(filename_inzip);
  char* write_filename = new char[fullname.size() + 1];
  std::copy(fullname.begin(), fullname.end(), write_filename);
  write_filename[fullname.size()] = '\0';

  char* p = write_filename;
  char* filename_withoutpath = write_filename;
  while (*p != '\0') {
    if ((*p == '/') || (*p == '\\'))
      filename_withoutpath = p + 1;
    ++p;
  }

  if (*filename_withoutpath == '\0') {
    // the current is only an empty directory
    printf("creating directory: %s\n", write_filename);
    Makedir(write_filename);
  } else { // the current is a file
    err = unzOpenCurrentFile(uf);
    if (err != UNZ_OK) {
      printf("error %d with zipfile in unzOpenCurrentFile\n", err);
    } else { // unzOpenCurrentFile success
      FILE* fout = FOPEN_FUNC(write_filename, "wb");
      if (fout == NULL) {
      // The parent directories of the file may do not exist, 
      // create them first.
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
        // read the content in the file of the zipfile and write them to the
        // output file under extracted_path
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

      if (err == UNZ_OK) { // the current file is unzipped successfully
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

/** Remove (delete) directory recursively.
  * @param path[in] The directory to remove.
  * return 0(success), -1(failure), 1(path not exist)
  **/
int ZipManager::Removedir(const char* path) {
  DIR* dir = opendir(path);
  size_t path_len = strlen(path);
  int rc = 1;
  if (dir) {
    dirent* entry;
    rc = 0;
    // recursively delete all the files.
    while (!rc && (entry = readdir(dir))) {
      int sub_rc = -1;
      // skip the "." and ".."
      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
        continue;
      size_t buf_size = path_len + strlen(entry->d_name) + 2;
      char* buf = (char*)malloc(buf_size);
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
  } else if (errno != ENOENT) {
    rc = -1;
  }
  if (!rc)
    rc = rmdir(path);
  return rc;
}

/** Check whether a file is a large file(greater than (2^32 - 1) bytes)
  * @param filename The file name to check.
  * @return 0(not large file), 1(large file)
  **/
int ZipManager::IsLargeFile(const char* filename) {
  int large_file = 0;
  FILE* p_file = FOPEN_FUNC(filename, "rb");
  if (p_file) {
    int n = FSEEKO_FUNC(p_file, 0, SEEK_END);
    ZPOS64_T pos = FTELLO_FUNC(p_file);
    //printf("File: %s is %lld bytes\n", filename, pos);
    if (pos >= 0xffffffff)
      large_file = 1;
    fclose(p_file);
  }
  return large_file;
}

/** Find all files under a directory.
 * @param dir[in] The absolute path of the directory.
 * @param files[in, out] The vector containing all the files under the top-level directory.
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
  * @param zf The pointer to the target zipfile.
  * @param dir_of_file The absolute path of the top-level directory where all
  *                    the files of the same target zipfile are.
  * @param origin_filename The real file name relative to the dir_of_file.
  * @param filename_inzip The pointer to the filename in the zip.
  *        NULL means the filename in the zip is the same as the origin_filename.
  * @return 0(success), others(failure)
  **/
int ZipManager::DoZipCurrentFile(zipFile zf, const string& dir_of_file,
    const string& origin_filename, const string* filename_inzip) {
  if (filename_inzip == NULL)
    filename_inzip = &origin_filename;
  int size_buf = WRITEBUFFERSIZE;
  void* buf = (void*)malloc(size_buf);
  if (buf == NULL) {
    printf("Error allocating memory\n");
    return ZIP_INTERNALERROR;
  }
  FILE* fin;
  int size_read;
  zip_fileinfo zi;
  zi.internal_fa = 0; // IMPORTANT!
  zi.external_fa = 0; // IMPORTANT!
  zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour =
    zi.tmz_date.tm_mday = zi.tmz_date.tm_mon = zi.tmz_date.tm_year = 0;
  zi.dosDate = 0;
  string file_fullname(dir_of_file + '/' + origin_filename);
  FileTime(file_fullname, &zi.tmz_date);
  int zip64 = IsLargeFile(file_fullname.c_str());
  int err = zipOpenNewFileInZip64(zf, filename_inzip->c_str(), &zi, NULL, 0, NULL, 0,
      NULL, 0, 0, zip64);
  if (err != ZIP_OK) {
    printf("Error in opening %s in zipfile\n", filename_inzip->c_str());
  } else {
    fin = FOPEN_FUNC(file_fullname.c_str(), "rb");
    if (fin == NULL) {
      err = ZIP_ERRNO;
      printf("Error in opening %s for reading\n", file_fullname.c_str());
    }
  }

  if (err == ZIP_OK) {
    // The source file and the goal file are all opened successfully
    // Read the content from the source file and write them to goal file in zip
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
          printf("Error in writing %s in the zipfile\n", filename_inzip->c_str());
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
      printf("Error in closing %s in the zipfile\n", filename_inzip->c_str());
  }

  return err;
}

/** Set the file time in the zip same as the time of the original file.
  * @param f Abosulute name with path of the file to get info on.
  * @param tm_zip Pointer to the struct of access, modific and creation times.
  * @return 0(success), 1(failure)
  **/
uLong ZipManager::FileTime(const string& f, tm_zip* tmzip) {
  int ret = 1;
  struct stat s;
  time_t tm_t = 0;
  if (f != "-") {
    if (!stat(f.c_str(), &s)) {
      tm_t = s.st_mtime;
      ret = 0;
    }
  }
  struct tm* filedate = localtime(&tm_t);
  tmzip->tm_sec = filedate->tm_sec;
  tmzip->tm_min = filedate->tm_min;
  tmzip->tm_hour = filedate->tm_hour;
  tmzip->tm_mday = filedate->tm_mday;
  tmzip->tm_mon = filedate->tm_mon;
  tmzip->tm_year = filedate->tm_year;

  return ret;
}

