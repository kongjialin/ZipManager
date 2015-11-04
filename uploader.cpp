#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "curl.h"

#include "uploader.h"

Uploader::Uploader(const string& sftp_url_with_pwd, const string& path,
                   const string& backup_path, int scan_sleep_time) :
    sftp_url_with_pwd_(sftp_url_with_pwd), path_(path),
    backup_path_(backup_path), scan_sleep_time_(scan_sleep_time) {
  curl_global_init(CURL_GLOBAL_ALL);
}

Uploader::~Uploader() {
  curl_global_cleanup();
}

/** Start scanning the zipfiles to upload and upload them.
  * It is an endless loop.
  **/
void Uploader::Start() {
  vector<string> file_to_upload;
  while (true) { // endless loop
    ScanZip(file_to_upload);
    if (file_to_upload.empty())
      sleep(scan_sleep_time_); // sleep for scan_sleep_time_ second
    while (!file_to_upload.empty()) {
      SFTPUpload(file_to_upload.back());
      file_to_upload.pop_back();
    }
  }
}

/** Upload a file to sftp server.
  * @param file_to_upload The file name relative to path_.
  * @return 0(success), others(failure);
  **/
int Uploader::SFTPUpload(const string& file_to_upload) {
  string remote_url(sftp_url_with_pwd_ + file_to_upload);
  string localfile_fullpath(path_ + '/' + file_to_upload);

  struct stat file_info;
  if (stat(localfile_fullpath.c_str(), &file_info)) {
    printf("Error opening '%s': %s\n", localfile_fullpath.c_str(), strerror(errno));
    return -1;
  }
  curl_off_t fsize = (curl_off_t)file_info.st_size;
  FILE* fin = fopen(localfile_fullpath.c_str(), "rb");
  if (fin == NULL) {
    printf("Error opening local file %s to upload\n", file_to_upload.c_str());
    return -1;
  }
  int err = 0;
  CURLcode rc;
  CURL* curl = curl_easy_init();
  if (curl) { // curl init success
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, fread);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, remote_url.c_str());
    curl_easy_setopt(curl, CURLOPT_READDATA, fin);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fsize);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_SFTP);
    rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
      printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(rc));
      err = -1;
    }
    curl_easy_cleanup(curl);
  } else { // curl init fail
    err = -1;
  }
  fclose(fin);
  if (!err) { // succeed in uploading the file
    // move the file zip file from path_ to backup_path_
    string backup(backup_path_ + '/' + file_to_upload);
    if (rename(localfile_fullpath.c_str(), backup.c_str()) == -1)
      printf("Error in moving the successfully uploaded file %s from path_"
          "to backup_path_: %s\n", localfile_fullpath.c_str(), strerror(errno));
  }
  return err;
}

/** Scan the zip files in path_, and push the files into a vector for uploading.
  * @param zipfiles[in, out] The vector of scanned zip files for uploading.
  * @return The count of scanned zip files.
  **/
int Uploader::ScanZip(vector<string>& zipfiles) {
  string zip(".zip");
  int count = 0;
  DIR* dir = opendir(path_.c_str());
  if (dir == NULL) { // opendir fail
    printf("Error open directory %s: %s\n", path_.c_str(), strerror(errno));
  } else { // opendir success
    dirent* entry;
    while ((entry = readdir(dir))) { // iterate all the files under the directory.
      string name(entry->d_name);
      if (std::equal(zip.rbegin(), zip.rend(), name.rbegin())) {
        // the file scanned is of type .zip
        zipfiles.push_back(name);
        ++count;
      }
    }
    closedir(dir);
  }
  return count;
}


