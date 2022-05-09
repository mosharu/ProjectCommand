#if TEST
#define BOOST_TEST_MODULE Test KP
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
#include <boost/json.hpp>
#include <boost/process.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>

namespace cr = boost::chrono;
namespace fs = boost::filesystem;
namespace js = boost::json;
namespace po = boost::program_options;
namespace pr = boost::process;
using boost::format;
using boost::adaptors::filtered;
using boost::adaptors::sliced;
using boost::algorithm::join;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

class WorkFolder;
using kpFunc = std::string (*)();
using WorkFolders = std::vector<WorkFolder>;

const string worksJsonTail("/works.json");

// shellコマンド実行結果
class Result {
public:
  Result(Result &&) = default;
  Result &operator=(Result &&) = default;

  Result(std::string cmd_, std::error_code errorCode_,
         std::vector<std::string> outs_, std::vector<std::string> errs_)
      : cmd(cmd_), errorCode(errorCode_), outs(outs_), errs(errs_) {}

  std::string cmd;
  std::error_code errorCode;
  std::vector<std::string> outs;
  std::vector<std::string> errs;

  void printCommand(std::ostream &os) {
    os << "@ " << cmd << ":" << endl;
    if (errorCode)
      os << "  ? " << errorCode << ":" << endl;
    for (auto line : outs) {
      os << "  . " << line << endl;
    }
    for (auto line : errs) {
      os << "  e " << line << endl;
    }
  }
};

// works.json の情報を保持する
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

// works.json の情報を保持する
class WorkEnv {
public:
  WorkEnv(WorkEnv &&) = default;
  WorkEnv &operator=(WorkEnv &&) = default;

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

WorkFolder tag_invoke(js::value_to_tag<WorkFolder>, js::value const &jv) {
  js::object const &obj = jv.as_object();
  return WorkFolder(js::value_to<std::string>(obj.at("path")),
                    js::value_to<std::string>(obj.at("topic")),
                    js::value_to<std::string>(obj.at("issue")),
                    js::value_to<std::string>(obj.at("feature")));
}

// json ファイルを読み出す
WorkFolders readJson(const std::string &jsonCode) {
  try {
    std::error_code ec;
    js::value jv = js::parse(jsonCode, ec);
    std::vector<WorkFolder> vc = value_to<std::vector<WorkFolder>>(jv);
    return vc;
  } catch (...) {
  }

  return WorkFolders();
}

// 0を含む文字列を複数行に分解する
vector<string> expandLineZero(const string &line) {
  vector<string> lines;
  for (auto it = line.begin(); it != line.end(); ++it) {
    auto found = std::find(it, line.end(), 0);
    lines.push_back(string(it, found));
    it = found;
  }
  return lines;
}

// shell コマンドを実行する
Result run(const string &cmd) {
  pr::ipstream out_stream;
  pr::ipstream err_stream;
  pr::child c(cmd, pr::std_out > out_stream, pr::std_err > err_stream);

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

  return Result{cmd, ec, std::move(outs), std::move(errs)};
}

// shell コマンドを実行結果を文字列にする
Result runPrint(const string &cmd, std::ostream &os) {
  auto ret = run(cmd);
  ret.printCommand(os);
  return ret;
}

// カレントフォルダから上に向かって json ファイルを探す
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

// json ファイルを読み出す
WorkFolders getWorks(const fs::path &curPath) {
  auto jsonPath = findWorksJson(curPath);
  if (jsonPath.empty())
    return WorkFolders();

  std::ifstream f(jsonPath);
  const auto sz = fs::file_size(jsonPath);
  string jsonCode(sz, 0);
  f.read(jsonCode.data(), sz);

  auto works = readJson(jsonCode);
  return works;
}

// カレントフォルダのフォルダ名を返す
string getDirName(const fs::path &curPath) {
  vector<string> dirParts;
  boost::algorithm::split(dirParts, curPath.string(), boost::is_any_of("/"));
  if (!dirParts.size())
    return "";
  return *dirParts.rbegin();
}

// カレントフォルダに対応する works.json 情報を返す
WorkFolder *getCurrentWork(WorkFolders &works, const string &dirName) {
  for (int i = 0; i < works.size(); ++i) {
    WorkFolder *work = &works[i];
    if (work->path == dirName) {
      return work;
    }
  }

  return nullptr;
}

// ワークスペース情報
WorkEnv::WorkEnv(const char *cwd) {
  curWork = nullptr;
  curPath = fs::absolute(cwd ? cwd : fs::current_path()).string();
  jsonPath = findWorksJson(curPath);
  if (!jsonPath.size())
    return;
  rootPath = jsonPath.substr(0, jsonPath.size() - worksJsonTail.size());
  topicsPath = rootPath + "/Topics";
  dirName = getDirName(curPath);
  works = getWorks(jsonPath);
  curWork = getCurrentWork(works, dirName);
  inWork = curWork != nullptr;
  if (!inWork)
    inRef = curPath.rfind(rootPath + "/Refs", 0) == 0;
}

// ワークスペース情報の文字化
string WorkEnv::str() {
  std::stringstream ss;
  ss << "curPath: " << curPath << endl;
  ss << "dirName: " << dirName << endl;
  ss << "jsonPath: " << jsonPath << endl;
  ss << "rootPath: " << rootPath << endl;
  ss << "topicsPath: " << topicsPath << endl;
  ss << "inRef: " << inRef << endl;
  ss << "inWork: " << inWork << endl;
  ss << "curWork: " << curWork << endl;
  for (auto work : works | boost::adaptors::indexed(0)) {
    ss << "[" << work.index() << "]" << endl;
    ss << "  path: " << work.value().path << endl;
    ss << "  topic: " << work.value().topic << endl;
    ss << "  issue: " << work.value().issue << endl;
    ss << "  feature: " << work.value().feature << endl;
  }

  return ss.str();
}

// 年月日を6桁数値にする
string dateStr() {
  std::stringstream formatted;
  formatted << time_fmt(cr::timezone::local, "%y%m%d")
            << cr::system_clock::now();
  return formatted.str();
}

string updateBranchDate(const string &curBranch) {
  auto regNumber = std::regex(R"(^\d{6}_)");
  return std::regex_replace(curBranch, regNumber, dateStr() + "_");
}

// ブランチ名のナンバリング化
string nextBranchNumber(const string &curBranch) {
  if (curBranch.size() == 0)
    return "";

  auto regNumber = std::regex(R"(.*_(\d+)$)");
  std::smatch m;
  if (!std::regex_match(curBranch, m, regNumber))
    return updateBranchDate(curBranch + "_1");

  try {
    auto num = m[1].str();
    int i = std::stoi(num);
    int baseLen = curBranch.size() - num.size();
    string base(baseLen, 0);
    std::copy(curBranch.begin(), curBranch.begin() + (baseLen), base.begin());
    return updateBranchDate(base + (boost::format("%i") % (i + 1)).str());
  } catch (...) {
    return "";
  }
}

// git コマンド実行結果のエラーチェック
bool checkErrorMessage(const vector<string> &errs) {
  auto regErr = std::regex(R"(^error)", std::regex_constants::icase);
  auto regFatal = std::regex(R"(^fatal)", std::regex_constants::icase);
  for (auto err : errs)
    if (std::regex_search(err, regErr) || std::regex_search(err, regFatal))
      return true;

  return false;
}

// git merge main
string gmm() {
  std::stringstream msg;
  do {
    auto ret = runPrint("git status --ignore-submodules=all -z", msg);
    if (ret.errorCode || ret.outs.size() != 0)
      break;

    ret = runPrint("git branch --show-current", msg);
    if (ret.errorCode || ret.outs.size() != 1)
      break;
    auto curBranch = ret.outs[0];
    auto nextBranch = nextBranchNumber(curBranch);
    if (nextBranch.empty())
      break;

    ret = runPrint("git fetch", msg);
    if (ret.errorCode || ret.outs.size() != 0)
      break;

    ret = runPrint("git checkout -b " + nextBranch + " origin/main", msg);
    if (ret.errorCode || checkErrorMessage(ret.errs))
      break;

    ret = runPrint("git merge " + curBranch, msg);
    if (ret.errorCode || checkErrorMessage(ret.errs))
      break;

    ret = runPrint("git diff --name-only " + curBranch, msg);
    if (ret.errorCode || ret.outs.size() != 0 || checkErrorMessage(ret.errs))
      break;

    msg << "\n😄 😄 😄";
  } while (false);

  cerr << msg.str();

  return "";
}

// git branch feature/title
string gmf() {
  std::stringstream msg;
  do {
    WorkEnv workEnv;
    if (!workEnv.curWork) {
      cerr << "Not in work folder\n";
      break;
    }
    if (workEnv.curWork->feature.empty()) {
      cerr << "No feature name\n";
      break;
    }

    auto ret = runPrint("git status --ignore-submodules=all -z", msg);
    if (ret.errorCode || ret.outs.size() != 0)
      break;

    ret = runPrint("git branch --show-current", msg);
    if (ret.errorCode || ret.outs.size() != 1)
      break;
    auto curBranch = ret.outs[0];
    if (curBranch == workEnv.curWork->feature) {
      cerr << "In feature branch\n";
      break;
    }

    ret = runPrint("git fetch", msg);
    if (ret.errorCode || ret.outs.size() != 0)
      break;

    ret = runPrint("git branch", msg);
    if (ret.errorCode || ret.outs.size() == 0)
      break;

    auto reg = std::regex("^ +" + workEnv.curWork->feature + "$");
    bool featureExists =
        std::any_of(ret.outs.begin(), ret.outs.end(),
                    [&](string line) { return std::regex_search(line, reg); });

    if (featureExists) {
      auto newFeature = workEnv.curWork->feature;
      for (int i = 0; i < 100; ++i) {
        newFeature = nextBranchNumber(newFeature);
        auto reg = std::regex("^ +" + newFeature + "$");
        featureExists =
            std::any_of(ret.outs.begin(), ret.outs.end(), [&](string line) {
              return std::regex_search(line, reg);
            });
        if (!featureExists)
          break;
      }
      if (!featureExists) {
        ret = runPrint("git branch -m " + workEnv.curWork->feature + " " +
                           newFeature,
                       msg);
        if (ret.errorCode || checkErrorMessage(ret.errs))
          break;
      } else {
        break;
      }
    }

    ret = runPrint(
        "git checkout -b " + workEnv.curWork->feature + " origin/main", msg);
    if (ret.errorCode || checkErrorMessage(ret.errs))
      break;

    ret = runPrint("git merge --no-commit --squash " + curBranch, msg);
    if (ret.errorCode || checkErrorMessage(ret.errs))
      break;

    ret = runPrint("git diff --name-only " + curBranch, msg);
    if (ret.errorCode || ret.outs.size() != 0 || checkErrorMessage(ret.errs))
      break;

    msg << "\n😄 😄 😄 Check! : " << workEnv.curWork->issue << endl;
  } while (false);

  cerr << msg.str();

  return "";
}

// タイトル名からmarkdownファイルを探す
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

// カレントフォルダにあったmarkdownファイルパスを返す
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

// 関連のあるmarkdownファイルパスを返す
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

// 新しい関連するmarkdownファイルのパスを返す
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

// ワークスペース情報を返す
string info() { return WorkEnv().str(); }

int callCommand(int argc, char *argv[]) {
  po::options_description global("Global options");
  global.add_options()                                       //
      ("help", "produce help message")                       //
      ("command", po::value<string>(), "command to execute") //
      ("subargs", po::value<vector<string>>(), "Arguments for command");

  po::positional_options_description pos;
  pos.add("command", 1).add("subargs", -1);
  po::parsed_options parsed = po::command_line_parser(argc, argv)
                                  .options(global)
                                  .positional(pos)
                                  .allow_unregistered()
                                  .run();

  po::variables_map vm;
  po::store(parsed, vm);

  if (vm.count("help")) {
    std::cout << //
        "kp COMMAND"
        "COMMAND:"
        "  gmf   git branch feature"
        "  gmm   git merge main"
        "  info  情報表示"
        "  md    Topics/YYMMDD_TITLE/TITLE.md 存在しない場合もある"
        "  mdnew Topics/YYMMDD_TITLE/TITLE.md がない場合タイトルを出力"
        "  mds   recommendation";
    return 0;
  }

  if (!vm.count("command"))
    return 1;

  const std::map<string, kpFunc> commands = {
      {"gmf", gmf}, {"gmm", gmm},     {"info", info},
      {"md", md},   {"mdnew", mdnew}, {"mds", mds},
  };

  auto command = vm["command"].as<string>();
  if (auto it = commands.find(command); it != commands.end()) {
    std::cout << it->second() << endl;
    return 0;
  }

  return 1;
}

#if !TEST
int main(int argc, char *argv[]) { return callCommand(argc, argv); }
#else
BOOST_AUTO_TEST_CASE(run_gcc) {
  auto ret = run("gcc --version");
  BOOST_TEST(ret.outs.size());
}

BOOST_AUTO_TEST_CASE(parse_json) {
  string jsonCode = R"([
  {
    "path": "src",
    "topic": "Tidy1",
    "issue": "https://issue.com/issue101",
    "feature": "feature/branch1"
  },
  {
    "path": "src",
    "topic": "Tidy2",
    "issue": "https://issue.com/issue102",
    "feature": "feature/branch2"
  }
  ])";
  auto works = readJson(jsonCode);
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
  BOOST_CHECK_EQUAL(nextBranchNumber("220503").substr(6), "_1");
  BOOST_CHECK_EQUAL(nextBranchNumber("220503_Test").substr(6), "_Test_1");
  BOOST_CHECK_EQUAL(nextBranchNumber("220503_Test_1").substr(6), "_Test_2");
  BOOST_CHECK_EQUAL(nextBranchNumber("220503_Test_1000").substr(6),
                    "_Test_1001");
}

BOOST_AUTO_TEST_CASE(check_error) {
  BOOST_CHECK_EQUAL(checkErrorMessage({"OK"}), false);
  BOOST_CHECK_EQUAL(checkErrorMessage({"error"}), true);
  BOOST_CHECK_EQUAL(checkErrorMessage({"ERROR"}), true);
  BOOST_CHECK_EQUAL(checkErrorMessage({" error:"}), false);
  BOOST_CHECK_EQUAL(checkErrorMessage({"fatal"}), true);
  BOOST_CHECK_EQUAL(checkErrorMessage({"FATAL"}), true);
  BOOST_CHECK_EQUAL(checkErrorMessage({" fatal:"}), false);
}
#endif
