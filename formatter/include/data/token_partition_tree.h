#pragma once

#include <functional>

#include "unwrapped_line.h"

namespace format {

class TokenPartition {
 public:
  UnwrappedLine value;

  TokenPartition* addChild(UnwrappedLine line);

  void mergeWithPreviousSibling();

  void hoistOnlyChild();

  template <typename Func>
  void visitPreOrder(Func&& func) {
    func(*this);
    for (auto& child : children_)
      child->visitPreOrder(std::forward<Func>(func));
  }

  template <typename Func>
  void visitPostOrder(Func&& func) {
    for (auto& child : children_)
      child->visitPostOrder(std::forward<Func>(func));
    func(*this);
  }

  template <typename Func>
  void forEachLeaf(Func&& func) {
    if (isLeaf()) {
      func(value);
    } else {
      for (auto& child : children_)
        child->forEachLeaf(std::forward<Func>(func));
    }
  }

  template <typename Func>
  void forEachLeaf(Func&& func) const {
    if (isLeaf()) {
      func(value);
    } else {
      for (const auto& child : children_)
        child->forEachLeaf(std::forward<Func>(func));
    }
  }

  void collectLeaves(std::vector<UnwrappedLine*>& out);

  bool isLeaf() const noexcept { return children_.empty(); }
  bool isRoot() const noexcept { return parent_ == nullptr; }
  std::size_t childCount() const noexcept { return children_.size(); }
  TokenPartition* parent() const noexcept { return parent_; }

  const std::vector<std::unique_ptr<TokenPartition>>& children()
      const noexcept {
    return children_;
  }

 private:
  TokenPartition* parent_{nullptr};
  std::vector<std::unique_ptr<TokenPartition>> children_;
};

class TokenPartitionTree {
 public:
  explicit TokenPartitionTree(UnwrappedLine root_line);

  TokenPartition& root() noexcept { return *root_; }
  const TokenPartition& root() const noexcept { return *root_; }

  void collectLeaves(std::vector<UnwrappedLine*>& out);

  template <typename Func>
  void forEachLeaf(Func&& func) {
    root_->forEachLeaf(std::forward<Func>(func));
  };

 private:
  std::unique_ptr<TokenPartition> root_;
};

}  // namespace format
