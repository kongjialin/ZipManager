#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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
  CURL* curl = NULL;
  while (true) { // endless loop
    ScanZip(file_to_upload);
    int sleep_count = 0;
    while (file_to_upload.empty()) { // no files to upload
      sleep(scan_sleep_time_);       // sleep for scan_sleep_time_ second
      if (++sleep_count == 10)  {  // no file to upload for a long time, stop the ftp connection
        if (curl) {                // if the connection exists
          curl_easy_cleanup(curl); // stop it
          curl = NULL;
        }
      }
      ScanZip(file_to_upload);
    }
    if (!curl)
      curl = InitCurl();
    while (!curl) { // curl handle is not initated
      printf("Failed in initiating curl handle\n");
      sleep(2); // sleep 2s
      curl = InitCurl();
    }
    off_t total_size_kb = 0;
    off_t file_size = 0;
    time_t start_time = time(NULL);
    while (!file_to_upload.empty()) { // uploading the scanned files
      int err = SFTPUpload(file_to_upload.back(), curl, file_size);
      if (err == -1) { // fail due to curl error, destroy the failed curl handle
        curl_easy_cleanup(curl);
        curl = NULL;
        file_to_upload.clear();
        break;
      }
      file_to_upload.pop_back();
      if (err == 0) // upload successfully
        total_size_kb += file_size / 1024;
    }
    time_t end_time = time(NULL);
    time_t interval = end_time - start_time;
    if (interval > 0) {
      long long mbps = (total_size_kb / 1024) / (end_time - start_time);
      printf("Uploading rate: %lld MB/s\n", mbps);
    }
  }
}

/** Upload a file to sftp server.
  * @param file_to_upload The file name relative to path_.
  * @param curl The curl handle used for uploading.
  * @return 0(success), -999(local file error), -1(curl failure);
  **/
int Uploader::SFTPUpload(const string& file_to_upload, CURL* curl, off_t& size) {
  string remote_url(sftp_url_with_pwd_ + file_to_upload);
  string localfile_fullpath(path_ + '/' + file_to_upload);

  struct stat file_info;
  if (stat(localfile_fullpath.c_str(), &file_info)) {
    printf("Error opening '%s': %s\n", localfile_fullpath.c_str(), strerror(errno));
    return -999;
  }
  size = file_info.st_size;
  curl_off_t fsize = (curl_off_t)file_info.st_size;
  FILE* fin = fopen(localfile_fullpath.c_str(), "rb");
  if (fin == NULL) {
    printf("Error opening local file %s to upload\n", file_to_upload.c_str());
    return -999;
  }
  int err = 0;
  curl_easy_setopt(curl, CURLOPT_URL, remote_url.c_str());
  curl_easy_setopt(curl, CURLOPT_READDATA, fin);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fsize);
  CURLcode rc = curl_easy_perform(curl);
  if (rc != CURLE_OK) {
    printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(rc));
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
  * Scan at most 100 files each time.
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
    while ((entry = readdir(dir)) && count < 100) { // iterate all the files under the directory.
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

/** Init a curl handle(long connection) for uploading many files one by one.
  * @return The curl handle.
  **/
CURL* Uploader::InitCurl() {
	CURL* curl = curl_easy_init();
	if (curl) { // curl init success
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, fread);
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_SFTP);
	}
  return curl;
}
