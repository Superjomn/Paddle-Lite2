#pragma once
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "cinn/core/function.h"
#include "cinn/hlir/operator.h"
#include "cinn/hlir/program.h"
#include "cinn/hlir/tensor.h"
#include "cinn/utils/any.h"
#include "cinn/utils/dot.h"

namespace cinn {
namespace hlir {

/**
 * Node the node in a Graph, it contains an Operator and input and output links.
 */
struct Node {
  std::list<Node*> inlinks;
  std::list<Node*> outlinks;
  Operator* op{};
  Tensor* tensor{};
  std::string name;

  bool is_op() const { return op; }
  bool is_tensor() const { return tensor; }

  /**
   * is middle endpoint.
   * Tell whether this node is a end point of the expression. We make a node end point if it
   * - has more than one consumers.
   * - is the start point of the graph.
   */
  bool IsMidEndPoint() const { return outlinks.size() >= 2UL || HasCallOnceOpAsInput(); }

  bool is_call_once_node() { return op && op->is_call_once(); }

 private:
  bool HasCallOnceOpAsInput() const {
    if (!tensor) return false;
    for (auto* op_node : inlinks) {
      if (op_node->is_call_once_node()) return true;
    }
    return false;
  }
};

static int dot_node_count = 0;

struct ArgumentRegistry {
 private:
  std::map<std::string, void*> fields;

  std::vector<Function> functions_;

 public:
  void SetFunctions(const std::vector<::cinn::Function>& funcs) { functions_ = funcs; }
  std::vector<::cinn::Function>& functions() { return functions_; }
};

/**
 * Graph helps to transpose a Program to an SSA graph.
 */
class Graph {
 public:
  /**
   * Build a Graph from a Program.
   * @param program
   */
  void Build(const Program& program, const Session& session);

  /**
   * Compile a graph to lower compiler.
   * @param finalize_function Call EndDefinition in Function, if true, the further modification on Stages is not
   * allowed.
   */
  void Compile(bool finalize_function = true);
  Expr CompileExpr(std::vector<Function>* fns);

  /**
   * Get all the nodes.
   */
  const std::vector<std::unique_ptr<Node>>& nodes() const { return nodes_; }

  /*
   * DOT visualization codes.
   */
  std::string dot() const;

  /**
   * Create a new op node.
   * @param op
   */
  void NewOpNode(Operator* op);

  /**
   * Create a new tensor with a specific name.
   * @param name
   */
  void NewTensorNode(const std::string& name);

  /**
   * Get a tensor with a specific name.
   * @param name
   * @return
   */
  Node* GetTensor(const std::string& name);

  /**
   * Get the input nodes of the graph.
   * @return
   */
  std::set<const Node*> Inputs() const;
  /**
   * Get the output nodes of the graph.
   * @return
   */
  std::set<const Node*> Outputs() const;
  /**
   * Get the input nodes of the graph.
   * @return
   */
  std::set<Node*> Inputs();
  /**
   * Get the output nodes of the graph.
   * @return
   */
  std::set<Node*> Outputs();

  ArgumentRegistry& arguments() { return arguments_; }

  /**
   * Partition the graph and generate functions.
   */
  std::vector<Function> PartitionFunctions();

 protected:
  /**
   * Allocate buffers for temporary variables, the buffer's ptype, name and size are same from tensor.
   * NOTE Should call after all the operators are compiled(after that each tensor's meta data is refreshed).
   */
  void AllocateBuffersForTempVars();

 private:
  std::map<std::string, Node*> vars_;
  std::vector<std::unique_ptr<Node>> nodes_;
  const Program* program_;
  const Session* session_;
  ArgumentRegistry arguments_;
};

/**
 * Compile a graph to a library.
 * @param graph
 * @param file
 */
void Compile(Graph* graph, const std::string& file);

}  // namespace hlir
}  // namespace cinn
