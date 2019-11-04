#include "cinn/hlir/graph.h"
#include <algorithm>
#include "cinn/backends/code_gen_c.h"
#include "cinn/hlir/graph_util.h"
#include "cinn/utils/logging.h"

namespace cinn {
namespace hlir {

static int node_count = 0;

void Graph::Build(const Program& program, const Session& session) {
  program_ = &program;
  session_ = &session;

  LOG_INDENT(0);
  for (auto& x : program.Inputs()) {
    CINN_DEBUG(2) << "add input " << x << " to program";
    NewTensorNode(x);
  }

  for (auto& op : program.ops()) {
    NewOpNode(op.get());
  }
}

std::string Graph::dot() const {
  Dot dot;
  CHECK(!nodes().empty());

  for (auto& x : vars_) {
    dot.AddNode(x.first, {});
  }

  for (auto& node : nodes()) {
    if (node->is_tensor()) {
      // dot.AddNode(node->name, {});

      for (auto& x : node->inlinks) {
        dot.AddEdge(x->name, node->name, {});
      }
    } else {  // op
      dot.AddNode(node->name, {});

      for (auto& x : node->inlinks) {
        LOG(INFO) << "var " << x->name;
        dot.AddEdge(x->name, node->name, {});
      }
    }
  }

  return dot.Build();
}

void Graph::NewOpNode(Operator* op) {
  nodes_.emplace_back(new Node);
  auto* op_node = nodes_.back().get();
  op_node->name = op->type() + std::to_string(node_count++);
  op_node->op = op;

  // link inputs
  for (auto& item : op->inputs()) {
    CHECK(vars_.count(item.second));
    auto* var_node = vars_[item.second];
    var_node->outlinks.push_back(op_node);
    op_node->inlinks.push_back(var_node);
    CINN_DEBUG(2) << "in " << var_node->name << " -> " << op_node->name;
  }

  // link outputs
  for (auto& item : op->outputs()) {
    NewTensorNode(item.second);
    auto* var_node = vars_[item.second];
    var_node->inlinks.push_back(op_node);
    op_node->outlinks.push_back(var_node);
    CINN_DEBUG(2) << "out " << op_node->name << " -> " << var_node->name;
  }
}

void Graph::NewTensorNode(const std::string& name) {
  CINN_DEBUG(2) << "new tensor " << name;
  CHECK(!vars_.count(name));
  nodes_.emplace_back(new Node);
  auto* var_node = nodes_.back().get();
  var_node->name = name;
  var_node->tensor = session_->GetTensor(name);

  vars_[name] = var_node;
}

std::set<const Node*> Graph::Inputs() const {
  std::set<const Node*> result;
  for (auto& node : nodes()) {
    if (node->is_tensor() && node->inlinks.empty()) {
      result.insert(node.get());
    }
  }
  return result;
}

std::set<const Node*> Graph::Outputs() const {
  std::set<const Node*> result;
  for (auto& node : nodes()) {
    if (node->is_tensor() && node->outlinks.empty()) {
      result.insert(node.get());
    }
  }
  return result;
}

std::set<Node*> Graph::Inputs() {
  std::set<Node*> result;
  for (auto& node : nodes()) {
    if (node->is_tensor() && node->inlinks.empty()) {
      result.insert(node.get());
    }
  }
  return result;
}

std::set<Node*> Graph::Outputs() {
  std::set<Node*> result;
  for (auto& node : nodes()) {
    if (node->is_tensor() && node->outlinks.empty()) {
      result.insert(node.get());
    }
  }
  return result;
}

Node* Graph::GetTensor(const std::string& name) {
  auto it = vars_.find(name);
  CHECK(it != vars_.end());
  return it->second;
}

void Graph::Compile() {
  LOG_INDENT(0);
  std::vector<Function> fns;
  fns.emplace_back(NameGenerator::Global().NewFuncionName());

  std::vector<ir::Expr> fn_inputs, fn_outputs;
  auto inputs = Inputs();
  auto outputs = Outputs();

  std::transform(
      inputs.begin(), inputs.end(), std::back_inserter(fn_inputs), [](Node* node) { return node->tensor->expr(); });
  std::transform(
      outputs.begin(), outputs.end(), std::back_inserter(fn_outputs), [](Node* node) { return node->tensor->expr(); });

  CINN_DEBUG(1) << "inputs.size " << fn_inputs.size();
  CINN_DEBUG(1) << "outputs.size " << fn_outputs.size();

  Node* last_node{};
  for (Node& node : GraphTraits::TS(*this)) {
    if (!node.is_tensor()) continue;

    for (const Stage& stage : node.tensor->stages()) {
      CINN_DEBUG(2) << "add stage: " << ir::Dump(stage.expr());
      fns.back().AddStage(stage);
    }

    if (node.outlinks.size() > 1) {
      fns.back().Inputs(fn_inputs);
      fns.back().Outputs(fn_outputs);
      fns.back().EndDefinition();
      fns.emplace_back(NameGenerator::Global().NewFuncionName());
      last_node = &node;
    }
  }

  fns.back().Inputs(fn_inputs);
  fns.back().Outputs(fn_outputs);
  fns.back().EndDefinition();

  std::vector<ir::Expr> exprs;
  std::transform(fns.begin(), fns.end(), std::back_inserter(exprs), [](const Function& x) { return x.ir_function(); });
  auto block = ir::Block::make(std::move(exprs));

  backends::CompileAsC(block, "1.h", "1.cc");
}

}  // namespace hlir
}  // namespace cinn
