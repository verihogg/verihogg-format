#pragma once

#include <functional>

#include "unwrapped_line.h"

namespace format {

class TokenPartitionTree {
 public:
  TokenPartitionTree* addChild(UnwrappedLine line);

  void mergeWithPreviousSibling();

  void hoistOnlyChild();

  template <typename Func>
  void visitPreOrder(Func&& func) {
    func(*this);
    for (auto& child : children)
      child->visitPreOrder(std::forward<Func>(func));
  }

  template <typename Func>
  void visitPostOrder(Func&& func) {
    for (auto& child : children)
      child->visitPostOrder(std::forward<Func>(func));
    func(*this);
  }

  template <typename Func>
  void forEachLeaf(Func&& func) {
    if (isLeaf()) {
      func(value);
    } else {
      for (auto& child : children)
        child->forEachLeaf(std::forward<Func>(func));
    }
  }

  template <typename Func>
  void forEachLeaf(Func&& func) const {
    if (isLeaf()) {
      func(value);
    } else {
      for (const auto& child : children)
        child->forEachLeaf(std::forward<Func>(func));
    }
  }

  void collectLeaves(std::vector<UnwrappedLine*>& out);

  bool isLeaf() const noexcept { return children.empty(); }
  bool isRoot() const noexcept { return parent == nullptr; }
  std::size_t childCount() const noexcept { return children.size(); }
  TokenPartitionTree* parent() const noexcept { return parent; }

  const std::vector<std::unique_ptr<TokenPartition>>& children()
      const noexcept {
    return children;
  }

 private:
  TokenPartition* parent{nullptr};
  std::vector<std::unique_ptr<TokenPartition>> children;
  UnwrappedLine value;
};

}  // namespace format
