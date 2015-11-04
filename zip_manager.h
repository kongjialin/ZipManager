#ifndef ZIP_MANAGER_H_
#define ZIP_MANAGER_H_

#include <string>
#include <vector>

#include "unzip.h"
#include "zip.h"

#include "ThreadQueue.h"

using std::string;
using std::vector;

class ZipManager {
  public:
    ZipManager(const string& source_path, const string& dest_path,
        const string& unz_out_path, const string& success_path,
        const string& fail_path);
    ~ZipManager();
    int ScanZip(ThreadQueue& prepare, ThreadQueue& processing);
    int Unzip(string zipfile);
    int Zip(const string& dir_to_zip, const vector<string>& files);

  private:
    int Makedir(const char* newdir);
    int Mymkdir(const char* dirname);
    int DoExtract(unzFile uf, const string& extract_dir);
    int DoExtractCurrentFile(unzFile uf, const string& extract_dir);
    int Removedir(const char* path);
    int IsLargeFile(const char* filename);
    int FindFilesToZip(const string& dir, vector<string>& files);
    int DoZipCurrentFile(zipFile zf, const string& dir_of_file,
        const string& origin_filename, const string* filename_inzip);
    uLong FileTime(const string& f, tm_zip* tmzip);

    // Note: the paths should be absolute paths
    string source_path_;      // Path of zip files of system J.
    string dest_path_;        // Path of zip files of system W.
    string unz_out_path_;     // Path of the extracted directories.
    string success_path_;     // Backup path of zip files having been unzipped successfully.
    string fail_path_;        // Path of zip files failed in unzipping.
};

#endif // ZIP_MANAGER_H_
