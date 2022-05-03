#include "project.h"

#if TEST
#define BOOST_TEST_MODULE Test KP
#include <boost/test/unit_test.hpp>
#endif

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <iostream>
#include <map>

namespace po = boost::program_options;
using std::string;
using std::vector;
using kpFunc = string (*)();

#if !TEST
int main(int argc, char *argv[]) {
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
        "  md    Topics/YYMMDD_TITLE/TITLE.md 存在しない場合もある"
        "  mdnew Topics/YYMMDD_TITLE/TITLE.md がない場合タイトルを出力"
        "  info  情報表示"
        "  mds   recommendation";
    return 0;
  }

  if (!vm.count("command"))
    return 1;

  const std::map<string, kpFunc> commands = {

      {"gmm", gmm}, {"info", info}, {"md", md}, {"mdnew", mdnew}, {"mds", mds},
  };

  auto command = vm["command"].as<string>();
  if (auto it = commands.find(command); it != commands.end()) {
    std::cout << it->second() << std::endl;
    return 0;
  }

  return 1;

#if 0
  // https://stackoverflow.com/questions/15541498/how-to-implement-subcommands-using-boost-program-options
  std::string cmd = vm["command"].as<std::string>();
  if (cmd == "ls") {
    // ls command has the following options:
    po::options_description ls_desc("ls options");
    ls_desc.add_options()               //
        ("hidden", "Show hidden files") //
        ("path", po::value<std::string>(), "Path to list");

    // Collect all the unrecognized options from the first pass. This will
    // include the (positional) command name, so we need to erase that.
    std::vector<std::string> opts =
        po::collect_unrecognized(parsed.options, po::include_positional);
    opts.erase(opts.begin());

    // Parse again...
    po::store(po::command_line_parser(opts).options(ls_desc).run(), vm);
  }
#endif
}
#endif
