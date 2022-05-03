#include "workfolder.h"
#include <string>
#include <system_error>
#include <vector>

struct ShellOut {
  std::error_code errorCode;
  std::vector<std::string> outs;
  std::vector<std::string> errs;
};

extern ShellOut run(const std::vector<std::string> &cmd);

class WorkEnv {
public:
  WorkEnv(const char *cwd = nullptr);
  std::string str();

  WorkFolder *curWork;
  WorkFolders works;
  bool inRef;
  bool inWork;
  std::string curPath;
  std::string dirName;
  std::string jsonPath;
  std::string rootPath;
  std::string topicsPath;
};

extern std::string gmm();
extern std::string info();
extern std::string md();
extern std::string mdnew();
extern std::string mds();
