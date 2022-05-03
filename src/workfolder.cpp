#include "workfolder.h"

#if TEST
#include <boost/test/unit_test.hpp>
#endif

#include <boost/json.hpp>

using namespace boost::json;

WorkFolder tag_invoke(value_to_tag<WorkFolder>, value const &jv) {
  object const &obj = jv.as_object();
  return WorkFolder(value_to<std::string>(obj.at("path")),
                    value_to<std::string>(obj.at("topic")),
                    value_to<std::string>(obj.at("issue")),
                    value_to<std::string>(obj.at("feature")));
}

WorkFolders readConfig(const std::string &jsonCode) {
  try {
    std::error_code ec;
    value jv = parse(jsonCode, ec);
    std::vector<WorkFolder> vc = value_to<std::vector<WorkFolder>>(jv);
    return vc;
  } catch (...) {
  }

  return WorkFolders();
}
