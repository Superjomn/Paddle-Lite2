#pragma once
/**
 * This file defines Network, the API to make model construction easier.
 */

#include <memory>
#include <set>
#include <string>
#include <vector>
#include "cinn/hlir/operator.h"
#include "cinn/hlir/program.h"
#include "cinn/hlir/session.h"

namespace cinn {
namespace hlir {

/**
 * Network is the high-level API.
 */
class Network {
 public:
  //! Variables of the operators' inputs and outputs.
  struct Var {
    Var() = default;
    explicit Var(const std::string& name) : name(name) {}

    //! Tell whether the variale is valid.
    operator bool() { return !name.empty(); }

    std::string name;
  };

  Network(const std::string& name, Session* session) : name_(name), session_(session) {}

  //! Declare an input placeholder.
  Var DeclInput(const std::string& name, primitive_t ptype, Shape shape);
  //! Declare an output placeholder.
  Network::Var DeclOutput(const std::string& name);
  //! Declare a weight for the model.
  template <typename T>
  Var DeclWeight(const std::string& name, primitive_t ptype, const Shape& shape, const std::vector<T>& data) {
    weight_names_.insert(name);

    Tensor* tensor = session_->NewTensor(name);
    auto buf = std::make_shared<Buffer>(name + "_buf", ptype);
    buf->Resize(shape.num_bytes(ptype));
    buf->SetData<T>(data.data());

    tensor->AttachBuffer(buf);
    tensor->set_is_weight();
    tensor->set_shape(shape);
    tensor->set_ptype(ptype);
    LOG(INFO) << name << " " << tensor->ptype();
    return Var(name);
  }

  /**
   * Add a Fully Connected layer.
   * @param x the input.
   * @param w the weight.
   * @param b the bias, leave empty is not valid.
   * @param w_transposed transpose w before matmul.
   * @return the output.
   */
  Network::Var AddFc(Network::Var x, Network::Var w, Network::Var b, bool w_transposed = false);

  /**
   * Add a MatMul operator.
   * @param x Name of the first input.
   * @param y Name of the second input.
   * @param out Name of the output.
   */
  Var AddMatMul(const Var& x, const Var& y, bool y_transposed = false);

  /**
   * Transpose a tensor.
   * @param x the input.
   * @param perm the permutated indexs.
   * @param call_once execute only once.
   * @return the transposed tensor.
   */
  Network::Var AddTranspose(const Network::Var& x, const std::vector<int>& perm, bool call_once = false);

  /**
   * Add a Tanh operator.
   * @param x Name of the input.
   * @param out Name of the output.
   */
  Var AddTanh(Var x);

  /**
   * Add a Sigmoid operator.
   * @param x Name of the input.
   * @param out Name of the output.
   */
  Var AddSigmoid(Var x);

  enum class ElementwiseOpKind {
    kAdd = 0,
    kSub,
    kMul,
    kDiv,
  };

  Var AddElementwiseAdd(Var x, Var y) { AddElementwise(ElementwiseOpKind::kAdd, x, y); }
  Var AddElementwiseSub(Var x, Var y) { AddElementwise(ElementwiseOpKind::kSub, x, y); }
  Var AddElementwiseMul(Var x, Var y) { AddElementwise(ElementwiseOpKind::kMul, x, y); }
  Var AddElementwiseDiv(Var x, Var y) { AddElementwise(ElementwiseOpKind::kDiv, x, y); }

  /**
   * Add a Reshape operator.
   * @param shape The new shape.
   * @param x Name of the input.
   * @param out Name of the output.
   */
  Var AddReshape(const std::vector<int>& shape, Var x);

  /**
   * Compile the network and generate a program.
   *
   * NOTE Once it compiled, the network is invalid for further modification.
   */
  Program Compile();

  //! Number of operators.
  size_t num_operators() { return operators_.size(); }

  const std::set<std::string>& input_names() const { return input_names_; }
  const std::set<std::string>& output_names() const { return output_names_; }
  const std::set<std::string>& weight_names() const { return weight_names_; }
  const std::set<std::string>& tmp_var_names() const { return tmp_var_names_; }

 private:
  /**
   * Add an Elementwise operator.
   * @param kind The kind of this elementwise operation.
   * @param x Name of the first input.
   * @param y Name of the second input.
   * @param out Name of the output.
   */
  Var AddElementwise(ElementwiseOpKind kind, Var x, Var y);

 private:
  /**
   * Allocate a temporary variable.
   * @param name name of the variable.
   * @return the corresponding tensor.
   */
  Tensor* DeclTmpVar(const std::string& name);

  //! Check whether this name is not duplicate.
  bool IsVarNameAvailable(const std::string& name) const;

  //! Tell whether this name belongs to an input.
  bool is_input(const std::string& name) const { return input_names_.count(name); }
  //! Tell whether this name belongs to an output.
  bool is_output(const std::string& name) const { return output_names_.count(name); }
  //! Tell whether this name belongs to a weight.
  bool is_weight(const std::string& name) const { return weight_names_.count(name); }
  //! Tell whether this name belongs to a temporary variable.
  bool is_tmp_var(const std::string& name) const { return tmp_var_names_.count(name); }

 private:
  //! Name of this network.
  std::string name_;
  //! Session used for all the operators.
  Session* session_{};
  //! Hold all the operator instances.
  std::vector<std::unique_ptr<Operator>> operators_;

  std::set<std::string> input_names_, output_names_;
  std::set<std::string> weight_names_;
  std::set<std::string> tmp_var_names_;
};

}  // namespace hlir
}  // namespace cinn
