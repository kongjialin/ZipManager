#include <vector>
#include "zip_manager.h"
#include "ThreadQueue.h"
#include "uploader.h"

using std::vector;
using std::string;

int main() {
  //NOTE: the sftp_url_with_pwd MUST end with '/', which is not required for other parameters
  ZipManager zm("/Users/kongjoy/proj/source", "/Users/kongjoy/proj/dest",
                "/Users/kongjoy/proj/unz_out", "/Users/kongjoy/proj/success",
                "/Users/kongjoy/proj/fail");
  Uploader sftp_uploader("sftp://user:pwd@ip/upload/",
                         "/Users/kongjoy/proj/dest",
                         "/Users/kongjoy/proj/backup", 1);
  ThreadQueue prepare, processing;
  // Unzip usage
  int count = zm.ScanZip(prepare, processing);
  for (int i = 0; i < count; ++i)
    zm.Unzip(*prepare.Pop());
  // Zip usage
  vector<string> files;
  files.push_back("144-440000-1445321458-00000-JZ_SOURCE_0001-0.bcp");
  files.push_back("144-440000-1445321459-00000-JZ_SOURCE_0039-0.bcp");
  files.push_back("144-440000-1445321459-00000-JZ_SOURCE_0022-0.bcp");
  files.push_back("144-440000-1445321459-00000-JZ_SOURCE_0029-0.bcp");
  files.push_back("GAB_ZIP_INDEX.xml2");
	zm.Zip("124-320000-320000-1445321494-00000-NENAME-A00_02214_1510201410", files);
  // Upload usage
  sftp_uploader.Start();
  return 0;
}
