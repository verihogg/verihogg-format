#pragma once

#include <memory>
#include <vector>

#include "unwrapped_line.h"

namespace format {

template <typename Token>
class TokenPartitionTree {
 public:
  auto addChild(UnwrappedLine<Token> /*line*/) -> TokenPartitionTree* {
    throw std::runtime_error("TODO");
  }

  void mergeWithPreviousSibling() { throw std::runtime_error("TODO"); }

  void hoistOnlyChild() { throw std::runtime_error("TODO"); }

  template <typename Func>
  void visitPreOrder(Func&& func) {
    func(*this);
    for (auto& child : children_) {
      child->visitPreOrder(std::forward<Func>(func));
    }
  }

  template <typename Func>
  void visitPostOrder(Func&& func) {
    for (auto& child : children_) {
      child->visitPostOrder(std::forward<Func>(func));
    }
    func(*this);
  }

  [[nodiscard]] auto isLeaf() const noexcept -> bool {
    return children_.empty();
  }
  [[nodiscard]] auto isRoot() const noexcept -> bool {
    return this->parent_.lock() == nullptr;
  }
  [[nodiscard]] auto childCount() const noexcept -> std::size_t {
    return children_.size();
  }
  [[nodiscard]] auto parent() const noexcept
      -> std::weak_ptr<TokenPartitionTree> {
    return parent_;
  }

  [[nodiscard]] auto children() const noexcept
      -> std::span<const std::shared_ptr<TokenPartitionTree>> {
    return children_;
  }

  [[nodiscard]] auto toString() const -> std::string {
    throw std::runtime_error("TODO");
  }

 private:
  std::weak_ptr<TokenPartitionTree> parent_;
  std::vector<std::shared_ptr<TokenPartitionTree>> children_;
  UnwrappedLine<Token> value;
};

}  // namespace format
