#ifndef UPLOADER_H_
#define UPLOADER_H_

#include <string>
#include <vector>

using std::string;
using std::vector;

class Uploader {
 public:
  Uploader(const string& sftp_url_with_pwd, const string& path,
           const string& back_up_path, int scan_sleep_time);
  ~Uploader();
  void Start();
  int SFTPUpload(const string& file_to_upload);
  int ScanZip(vector<string>& zipfiles);
 private:
  string sftp_url_with_pwd_; // sftp url with password (MUST end with '/')
  string path_;              // Path of zip files to upload
  string backup_path_;       // Path for backuping the successfully uploaded zip files 
  int scan_sleep_time_;      // Sleep time in seconds when nothing to upload
};

#endif // UPLOADER_H_
