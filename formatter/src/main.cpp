#include <slang/driver/Driver.h>

auto main(int argc, char** argv) -> int {
  slang::driver::Driver driver;
  driver.addStandardArgs();
  if (!driver.parseCommandLine(argc, argv) || !driver.parseAllSources() ||
      driver.syntaxTrees.empty()) {
    return 1;
  }
  return 0;
}
