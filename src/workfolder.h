#include <string>
#include <vector>

class WorkFolder {
public:
  WorkFolder(const std::string &path_, const std::string &topic_,
             const std::string &issue_, const std::string &feature_)
      : path(path_), topic(topic_), issue(issue_), feature(feature_) {}

  std::string path;
  std::string topic;
  std::string issue;
  std::string feature;
};

using WorkFolders = std::vector<WorkFolder>;
extern WorkFolders readConfig(const std::string &jsonCode);
