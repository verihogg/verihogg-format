#include <slang/diagnostics/Diagnostics.h>
#include <slang/parsing/Preprocessor.h>
#include <slang/parsing/Token.h>
#include <slang/text/SourceManager.h>
#include <slang/util/BumpAllocator.h>

#include <string_view>
#include <vector>

class LexContext {
 public:
  auto lex_file(std::string_view path) -> std::vector<slang::parsing::Token> {
    std::vector<slang::parsing::Token> tokens;

    auto buffer = source_manager_.readSource(path, /*library=*/nullptr);
    if (!buffer) {
      return tokens;
    }

    slang::Bag options;
    slang::parsing::Preprocessor pp(source_manager_, alloc_, diagnostics_,
                                    options);
    pp.pushSource(*buffer);

    while (true) {
      auto tok = pp.next();
      if (tok.kind == slang::parsing::TokenKind::EndOfFile) {
        break;
      }
      tokens.push_back(tok);
    }
    return tokens;
  }

  auto source_manager() -> slang::SourceManager& { return source_manager_; }
  auto diagnostics() const -> const slang::Diagnostics& { return diagnostics_; }

 private:
  slang::SourceManager source_manager_;
  slang::BumpAllocator alloc_;
  slang::Diagnostics diagnostics_;
};