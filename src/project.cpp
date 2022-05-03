#include "project.h"

#if TEST
#include <boost/test/unit_test.hpp>
#endif

#define BOOST_CHRONO_DONT_PROVIDES_DEPRECATED_IO_SINCE_V2_0_0 1
#include <algorithm>
#include <boost/algorithm/string/classification.hpp> // is_any_of
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/chrono/chrono_io.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/process.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <fstream>
#include <iostream>
#include <regex>

using namespace boost::process;
using namespace boost::chrono;
namespace fs = boost::filesystem;
using boost::format;
using boost::adaptors::filtered;
using boost::adaptors::sliced;
using boost::algorithm::join;
using std::string;
using std::vector;

const string worksJsonTail("/works.json");

void printCommand(const string &cmd, const ShellOut &out, std::ostream *os) {
  if (!os)
    return;

  *os << "@ " << cmd << ":" << std::endl;
  if (out.errorCode)
    *os << "  ? " << out.errorCode << ":" << std::endl;
  for (auto line : out.outs) {
    *os << "  o " << line << std::endl;
  }
  for (auto line : out.errs) {
    *os << "  e " << line << std::endl;
  }
}

vector<string> expandLineZero(const string &line) {
  vector<string> lines;
  for (auto it = line.begin(); it != line.end(); ++it) {
    auto found = std::find(it, line.end(), 0);
    lines.push_back(string(it, found));
    it = found;
  }
  return lines;
}

ShellOut run(const string &cmd, std::ostream *os = nullptr) {
  ipstream out_stream;
  ipstream err_stream;
  child c(cmd, std_out > out_stream, std_err > err_stream);

  vector<string> outs;
  vector<string> errs;
  string line;
  while (out_stream && std::getline(out_stream, line) && !line.empty())
    outs.push_back(line);

  while (err_stream && std::getline(err_stream, line) && !line.empty())
    errs.push_back(line);

  if (outs.size() == 1 && outs[0].find('\0') != string::npos)
    outs = expandLineZero(outs[0]);

  std::error_code ec;
  c.wait(ec);

  auto ret = ShellOut{ec, std::move(outs), std::move(errs)};
  printCommand(cmd, ret, os);
  return ret;
}

vector<ShellOut> runMany(vector<string> cmds) {
  vector<ShellOut> res;
  for (auto cmd : cmds) {
    res.push_back(run(cmd));
  }

  return res;
}

string dateStr() {
  std::stringstream formatted;
  formatted << time_fmt(timezone::local, "%y%m%d") << system_clock::now();
  return formatted.str();
}

string findWorksJson(const fs::path &curPath) {
  if (curPath.empty())
    return "";

  vector<string> dirParts;
  boost::algorithm::split(dirParts, curPath.string(), boost::is_any_of("/"));
  for (int i = dirParts.size(); i > 1; --i) {
    auto path = join(dirParts | sliced(0, i), "/") + worksJsonTail;
    if (fs::exists(path) && !fs::is_directory(path))
      return path;
  }

  return "";
}

WorkFolders getWorks(const fs::path &curPath) {
  auto jsonPath = findWorksJson(curPath);
  if (jsonPath.empty())
    return WorkFolders();

  std::ifstream f(jsonPath);
  const auto sz = fs::file_size(jsonPath);
  string jsonCode(sz, 0);
  f.read(jsonCode.data(), sz);

  auto works = readConfig(jsonCode);
  return works;
}

string getDirName(const fs::path &curPath) {
  vector<string> dirParts;
  boost::algorithm::split(dirParts, curPath.string(), boost::is_any_of("/"));
  if (!dirParts.size())
    return "";
  return *dirParts.rbegin();
}

WorkFolder *getCurrentWork(WorkFolders &works, const string &dirName) {
  for (int i = 0; i < works.size(); ++i) {
    WorkFolder *work = &works[i];
    if (work->path == dirName) {
      return work;
    }
  }

  return nullptr;
}

WorkEnv::WorkEnv(const char *cwd) {
  curWork = nullptr;
  curPath = fs::absolute(cwd ? cwd : fs::current_path()).string();
  jsonPath = findWorksJson(curPath);
  if (!jsonPath.size())
    return;
  rootPath = string(jsonPath.c_str(), jsonPath.size() - worksJsonTail.size());
  topicsPath = rootPath + "/Topics";
  dirName = getDirName(curPath);
  works = getWorks(jsonPath);
  curWork = getCurrentWork(works, dirName);
  inWork = curWork != nullptr;
  if (!inWork)
    inRef = curPath.starts_with(rootPath + "/Refs");
}

string WorkEnv::str() {
  std::stringstream ss;
  ss << "curPath: " << curPath << std::endl;
  ss << "dirName: " << dirName << std::endl;
  ss << "jsonPath: " << jsonPath << std::endl;
  ss << "rootPath: " << rootPath << std::endl;
  ss << "topicsPath: " << topicsPath << std::endl;
  ss << "inRef: " << inRef << std::endl;
  ss << "inWork: " << inWork << std::endl;
  ss << "curWork: " << curWork << std::endl;
  for (auto work : works | boost::adaptors::indexed(0)) {
    ss << "[" << work.index() << "]" << std::endl;
    ss << "  path: " << work.value().path << std::endl;
    ss << "  topic: " << work.value().topic << std::endl;
    ss << "  issue: " << work.value().issue << std::endl;
    ss << "  feature: " << work.value().feature << std::endl;
  }

  return ss.str();
}

string getMdPath(const WorkEnv &workEnv, const string &title) {
  if (workEnv.topicsPath.empty())
    return "";
  auto cmd = format("find %1% -name *%2%.md") % workEnv.topicsPath % title;
  auto ret = run(cmd.str());
  if (ret.errorCode || ret.outs.size() == 0) {
    auto found =
        (format("%1%/%2%_%3%/%3%.md") % workEnv.topicsPath % dateStr() % title);
    return fs::relative(found.str()).string();
  }
  boost::sort(ret.outs);
  return fs::relative(*ret.outs.rbegin()).string();
}

string nextBranchNumber(const string &curBranch) {
  if (curBranch.size() == 0)
    return "";

  auto regNumber = std::regex(R"(.*_(\d+)$)");
  std::smatch m;
  if (!std::regex_match(curBranch, m, regNumber))
    return curBranch + "_1";

  try {
    auto num = m[1].str();
    int i = std::stoi(num);
    int baseLen = curBranch.size() - num.size();
    string base(baseLen, 0);
    std::copy(curBranch.begin(), curBranch.begin() + (baseLen), base.begin());
    return base + (boost::format("%i") % (i + 1)).str();
  } catch (...) {
    return "";
  }
}

bool checkErrorMessage(const vector<string> &errs) {
  auto regErr = std::regex(R"(^error)", std::regex_constants::icase);
  auto regFatal = std::regex(R"(^fatal)", std::regex_constants::icase);
  for (auto err : errs)
    if (std::regex_search(err, regErr) || std::regex_search(err, regFatal))
      return true;

  return false;
}

string gmm() {
  std::stringstream msg;
  do {
    auto ret = run("git status --ignore-submodules=all -z", &msg);
    if (ret.errorCode || ret.outs.size() != 0)
      break;

    ret = run("git branch --show-current", &msg);
    if (ret.errorCode || ret.outs.size() != 1)
      break;
    auto curBranch = ret.outs[0];
    auto nextBranch = nextBranchNumber(curBranch);
    if (nextBranch.empty())
      break;

    ret = run("git fetch", &msg);
    if (ret.errorCode || ret.outs.size() != 0)
      break;

    ret = run("git checkout -b " + nextBranch + " origin/main", &msg);
    if (ret.errorCode || checkErrorMessage(ret.errs))
      break;

    ret = run("git merge " + curBranch, &msg);
    if (ret.errorCode || checkErrorMessage(ret.errs))
      break;

    ret = run("git diff --name-only " + curBranch, &msg);
    if (ret.errorCode || ret.outs.size() != 0 || checkErrorMessage(ret.errs))
      break;

    msg << "\n😄 😄 😄";
  } while (false);

  std::cerr << msg.str();

  return "";
}

string md() {
  WorkEnv workEnv;
  if (workEnv.topicsPath.empty())
    return "";
  if (workEnv.curWork)
    return getMdPath(workEnv, workEnv.curWork->topic);
  if (workEnv.inRef)
    return getMdPath(workEnv, workEnv.dirName);
  return "";
}

string mds() {
  vector<string> mdList;
  WorkEnv workEnv;
  if (workEnv.topicsPath.empty())
    return "";
  if (workEnv.curWork) {
    if (auto md = getMdPath(workEnv, workEnv.curWork->topic); fs::exists(md))
      return md;
    else
      return "";
  }
  if (!workEnv.inRef)
    return "";

  mdList.push_back(getMdPath(workEnv, workEnv.dirName));
  for (auto work : workEnv.works) {
    mdList.push_back(getMdPath(workEnv, work.topic));
  }

  auto f = [](string p) { return fs::exists(p); };
  return join(mdList | filtered(f), "\n");
}

string mdnew() {
  WorkEnv workEnv;
  if (workEnv.topicsPath.empty())
    return "";

  if (workEnv.curWork) {
    auto mdFile = getMdPath(workEnv, workEnv.curWork->topic);
    if (!fs::exists(mdFile))
      return workEnv.curWork->topic;
  }

  if (workEnv.inRef) {
    auto mdFile = getMdPath(workEnv, workEnv.dirName);
    if (!fs::exists(mdFile))
      return workEnv.dirName;
  }

  return "";
}

string info() { return WorkEnv().str(); }

#if TEST
BOOST_AUTO_TEST_CASE(run_gcc) {
  auto ret = run("gcc --version");
  BOOST_TEST(ret.outs.size());
}

BOOST_AUTO_TEST_CASE(parse_json) {
  string jsonCode = R"([
  {
    "path": "fire",
    "topic": "Tidy1",
    "issue": "https://issue.com/issue101",
    "feature": "feature/branch1"
  },
  {
    "path": "fire2",
    "topic": "Tidy2",
    "issue": "https://issue.com/issue102",
    "feature": "feature/branch2"
  }
  ])";
  auto works = readConfig(jsonCode);
  BOOST_TEST(works.size());
}

BOOST_AUTO_TEST_CASE(call_dateStr) {
  auto date = dateStr();
  BOOST_TEST(!date.empty());
}

BOOST_AUTO_TEST_CASE(call_findWorksJson) {
  auto path = findWorksJson(fs::absolute("Refs/Test/test").string());
  BOOST_TEST(!path.empty());
}

BOOST_AUTO_TEST_CASE(call_getWorks) {
  auto works = getWorks(fs::absolute("Refs/Test/test").string());
  BOOST_TEST(works.size() > 0);
}

BOOST_AUTO_TEST_CASE(call_WorkEnv) {
  WorkEnv workEnv;
  BOOST_TEST(!workEnv.jsonPath.empty());
  WorkEnv workEnv2("src");
  BOOST_TEST(!workEnv2.jsonPath.empty());
  WorkEnv workEnv3("/usr");
  BOOST_TEST(workEnv3.jsonPath.empty());
  WorkEnv workEnv4("Refs/boost_1_79_0");
  BOOST_TEST(!workEnv4.jsonPath.empty());
}

BOOST_AUTO_TEST_CASE(call_getMdPath) {
  WorkEnv workEnv("Refs/Test/test");
  auto md = getMdPath(workEnv, "Double");
  BOOST_CHECK_EQUAL(md, "Topics/220102_Double/Double.md");

  md = getMdPath(workEnv, "None");
  BOOST_TEST(!md.empty());
}

BOOST_AUTO_TEST_CASE(call_nextBranch) {
  BOOST_CHECK_EQUAL(nextBranchNumber(""), "");
  BOOST_CHECK_EQUAL(nextBranchNumber("220503"), "220503_1");
  BOOST_CHECK_EQUAL(nextBranchNumber("220503_Test"), "220503_Test_1");
  BOOST_CHECK_EQUAL(nextBranchNumber("220503_Test_1"), "220503_Test_2");
  BOOST_CHECK_EQUAL(nextBranchNumber("220503_Test_1000"), "220503_Test_1001");
}

BOOST_AUTO_TEST_CASE(check_error) {
  auto regErr = std::regex(R"(^error)", std::regex_constants::icase);
  BOOST_CHECK_EQUAL(std::regex_search("error: ", regErr), true);
  BOOST_CHECK_EQUAL(std::regex_search(" error: ", regErr), false);
  BOOST_CHECK_EQUAL(std::regex_search("eRror: ", regErr), true);
}
#endif
