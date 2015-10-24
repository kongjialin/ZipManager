#ifndef ZIP_MANAGER_H_
#define ZIP_MANAGER_H_

#include "unzip.h"

class ZipManager {
 public:
  void test();
 private:
  int Makedir(char* newdir);
  int Mymkdir(const char* dirname);
  int DoExtract(unzFile uf);
  int DoExtractCurrentFile(unzFile uf);
  void ChangeFileDate(const char* filename, tm_unz tmu_date);
};

#endif // ZIP_MANAGER_H_
