#include <vector>
#include "zip_manager.h"

using std::vector;
using std::string;

int main() {
  ZipManager zm("/Users/kongjoy/proj/", "/Users/kongjoy/proj/dest/",
                "/Users/kongjoy/proj/inqueue/", "/Users/kongjoy/proj/unz_out/",
                "/Users/kongjoy/proj/unz_success/", "/Users/kongjoy/proj/unz_fail/",
                "/Users/kongjoy/proj/zip_fail/");
  vector<string> v;
  zm.ScanZip(v);
  for (int i = 0; i < v.size(); ++i)
    zm.Unzip(v[i]);
  zm.Zip("124-320000-320000-1445321494-00000-NENAME-A00_02214_1510201410");
  return 0;
}
