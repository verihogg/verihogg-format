#include <slang/diagnostics/Diagnostics.h>
#include <slang/parsing/Token.h>
#include <slang/text/SourceManager.h>
#include <slang/util/BumpAllocator.h>

#include <filesystem>
#include <string_view>
#include <vector>

class LexContext {
 public:
  auto lex_file(const std::filesystem::path& path)
      -> std::vector<slang::parsing::Token>;
  auto lex_string(std::string_view src) -> std::vector<slang::parsing::Token>;

  auto source_manager() -> slang::SourceManager& { return source_manager_; }
  auto diagnostics() const -> const slang::Diagnostics& { return diagnostics_; }

 private:
  slang::SourceManager source_manager_;
  slang::BumpAllocator alloc_;
  slang::Diagnostics diagnostics_;
};