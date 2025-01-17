#include "cinn/hlir/tensor.h"
#include <algorithm>
#include <numeric>
#include "cinn/ir/ir.h"

namespace cinn {
namespace hlir {

ir::Expr Tensor::Elem() const {
  CHECK(!shape().empty());
  CHECK_EQ(iterators().size(), shape().size());
  auto node = ir::Reference::make(expr(), iterators());
  node.InferenceIteratorDomain();
  return node;
}

const std::vector<ir::Expr> &Tensor::iterators() const {
  CHECK(!shape_.empty());
  if (iterators_.empty()) {
    for (int i = 0; i < shape().size(); i++) {
      iterators_.emplace_back(ir::Var());
      CHECK(iterators_.back().valid());
    }
  } else {
    CHECK_EQ(shape().size(), iterators_.size());
  }
  return iterators_;
}

std::vector<ir::Expr> &Tensor::iterators() {
  CHECK(!shape_.empty());
  if (iterators_.empty()) {
    for (int i = 0; i < shape().size(); i++) {
      iterators_.emplace_back(ir::Var());
    }
  } else {
    CHECK_EQ(shape().size(), iterators_.size());
  }
  return iterators_;
}

ir::Expr &Tensor::expr() { return expr_; }

const ir::Expr &Tensor::expr() const { return expr_; }

void Tensor::InitExpr() {
  if (!expr_.valid()) {
    CHECK(!shape().empty()) << "should set shape first";
    std::vector<ir::Constant> ir_shape;
    for (int v : shape().data) {
      ir_shape.emplace_back(v);
    }
    ir_inner_name_ = name_.empty() ? GlobalContext().name_generator().NewNamed("tensor") : name_;
    expr_ = ir::Expr(ir_shape, primitive_t::float32, ir_inner_name());
  }
}

void Tensor::set_shape(const Shape &x) {
  CHECK(shape_.empty()) << "duplicate set shape";
  shape_ = x;
  InitExpr();
}

std::string Tensor::__repr__() const {
  std::vector<std::string> shape_str;
  CHECK(!shape().empty()) << " tensor " << ir_inner_name() << " empty";
  std::transform(
      shape().data.begin(), shape().data.end(), std::back_inserter(shape_str), [](int v) { return std::to_string(v); });
  return StringFormat("Tensor %s->%s [%s]", name().c_str(), ir_inner_name().c_str(), Concat(shape_str, ",").c_str());
}

const std::string &Tensor::ir_inner_name() const {
  CHECK(!ir_inner_name_.empty());
  return ir_inner_name_;
}

void Tensor::AttachBuffer() {
  CHECK_NE(ptype(), primitive_t::unk);
  CHECK(!name().empty());

  buffer_ = std::make_shared<Buffer>(name(), ptype());
  buffer_->Resize(shape().num_bytes(ptype()));
}

int Shape::num_bytes(primitive_t ptype) const {
  int bytes = primitive_bytes(ptype);
  CHECK_GT(bytes, 0);
  return num_elements() * bytes;
}

int Shape::num_elements() const {
  return std::accumulate(data.begin(), data.end(), 1, [](int x, int y) { return x * y; });
}
}  // namespace hlir
}  // namespace cinn