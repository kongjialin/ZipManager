#ifndef ZIP_MANAGER_H_
#define ZIP_MANAGER_H_

#include <string>
#include <vector>

#include "unzip.h"
#include "zip.h"

using std::string;
using std::vector;

class ZipManager {
  public:
    ZipManager(const string& source_path, const string& dest_path,
               const string& inqueue_path, const string& unz_out_path,
               const string& unz_success_path, const string& unz_fail_path,
               const string& zip_fail_path);
    ~ZipManager();
    int ScanZip(vector<string>& v);
    int Unzip(string zipfile);
    int Zip(const string& dir_to_zip);

  private:
    int Makedir(const char* newdir);
    int Mymkdir(const char* dirname);
    int DoExtract(unzFile uf, const string& extract_dir);
    int DoExtractCurrentFile(unzFile uf, const string& extract_dir);
    int Removedir(const char* path);
    int IsLargeFile(const char* filename);
    int FindFilesToZip(const string& dir, vector<string>& files);
    int DoZipCurrentFile(zipFile zf, const string& dir_of_file,
                         const string& filename_inzip);

    // Note: the paths should be absolute paths
    string source_path_;      // Path of zip files of system J.
    string dest_path_;        // Path of zip files of system W.
    string inqueue_path_;     // Temparary path of zip files that have been added
    // to the queue waiting for unzipping.
    string unz_out_path_;     // Root path of the extracted directories.
    string unz_success_path_; // Backup path of zip files having been unzipped successfully.
    string unz_fail_path_;    // Path of zip files failed in unzipping.
    string zip_fail_path_;    // Path of uncompressed directories failed in zipping.
};

#endif // ZIP_MANAGER_H_
