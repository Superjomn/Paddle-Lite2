#include "cinn/ir/ir_printer.h"
#include "cinn/core/function.h"
#include "cinn/core/stage.h"
#include "cinn/ir/ir.h"
#include "cinn/utils/logging.h"
#include "cinn/utils/macros.h"
#include "cinn/utils/string.h"
#include "ir_printer.h"

namespace cinn {
namespace ir {

std::string Dump(const ir::Expr &expr) {
  std::stringstream os;
  IRPrinter printer(os);
  printer.Print(expr);
  return os.str();
}

void IRPrinter::Visit(const Add *op) {
  os_ << "(";
  Print(op->a);
  os_ << " + ";
  Print(op->b);
  os_ << ")";
}
void IRPrinter::Visit(const Sub *op) {
  os_ << "(";
  Print(op->a);
  os_ << " - ";
  Print(op->b);
  os_ << ")";
}
void IRPrinter::Visit(const Mul *op) {
  os_ << "(";
  Print(op->a);
  os_ << " * ";
  Print(op->b);
  os_ << ")";
}
void IRPrinter::Visit(const Div *op) {
  os_ << "(";
  Print(op->a);
  os_ << " / ";
  Print(op->b);
  os_ << ")";
}
void IRPrinter::Visit(const IntImm *op) { os_ << op->val(); }
void IRPrinter::Visit(const FloatImm *op) { os_ << op->val(); }

void IRPrinter::Visit(const Expr *op) { IRVisitor::Visit(op); }

void IRPrinter::Print(Expr op) { Visit(&op); }
void IRPrinter::Print(Var op) { os_ << op.name(); }
void IRPrinter::Print(const std::string &x) { os_ << x; }

void IRPrinter::Visit(const Mod *op) {
  os_ << "(";
  Print(op->a);
  os_ << " % ";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const Minus *op) {
  os_ << "(";
  os_ << "-";
  Print(op->a);
  os_ << ")";
}

void IRPrinter::Visit(const Exp *op) {
  os_ << "exp(";
  Print(op->a);
  os_ << ")";
}

void IRPrinter::Visit(const Min *op) {
  os_ << "min(";
  Print(op->a);
  os_ << ",";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const Max *op) {
  os_ << "max(";
  Print(op->a);
  os_ << ",";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const NE *op) {
  os_ << "(";
  Print(op->a);
  os_ << " != ";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const EQ *op) {
  os_ << "(";
  Print(op->a);
  os_ << " == ";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const GT *op) {
  os_ << "(";
  Print(op->a);
  os_ << " > ";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const GE *op) {
  os_ << "(";
  Print(op->a);
  os_ << " >= ";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const LT *op) {
  os_ << "(";
  Print(op->a);
  os_ << " < ";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const LE *op) {
  os_ << "(";
  Print(op->a);
  os_ << " <= ";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const And *op) {
  os_ << "(";
  Print(op->a);
  os_ << " && ";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const Or *op) {
  os_ << "(";
  Print(op->a);
  os_ << " || ";
  Print(op->b);
  os_ << ")";
}

void IRPrinter::Visit(const Tensor *op) { os_ << op->name() << "<>"; }

void IRPrinter::Visit(const For *op) {
  os_ << "for(";
  Print(op->iterator);
  os_ << ", ";
  Print(op->iter_init);
  Print(", ");
  Print(op->iter_cond);
  Print(", ");
  Print(op->iter_inc);
  os_ << ") {\n";
  Print(op->body);
  PrintIndent();
  os_ << "}";
}

void IRPrinter::Visit(const IfThenElse *op) {
  PrintIndent();
  Print("if(");
  Print(op->condition);
  Print(")");
  Print(op->true_block);
  Print("else");
  Print(op->false_block);
}

void IRPrinter::Visit(const Block *op) {
  // PrintIndent();
  // os_ << "{\n";
  indent_size_++;
  for (auto expr : op->exprs) {
    PrintIndent();
    Print(expr);
    os_ << "\n";
  }
  indent_size_--;
  // PrintIndent();
  // os_ << "}\n";
}
void IRPrinter::Visit(const Constant *op) { IRVisitor::Visit(op); }
void IRPrinter::Visit(const Var *op) { Print(*op); }
void IRPrinter::Visit(const Reference *op) {
  CHECK_EQ(reference_braces.size(), 2UL);
  Print(op->target);
  os_ << reference_braces[0];
  for (int i = 0; i < op->iterators.size() - 1; i++) {
    Print(op->iterators[i]);
    os_ << ",";
  }
  if (op->iterators.size() >= 1) {
    Print(op->iterators.back());
  }
  os_ << reference_braces[1];
}

void IRPrinter::Visit(const Call *op) {
  os_ << op->caller;
  os_ << "(";
  for (int i = 0; i < op->arguments.size() - 1; i++) {
    Print(op->arguments[i]);
    os_ << ",";
  }
  if (op->arguments.size() > 1) {
    Print(op->arguments.back());
  }
  os_ << ")";
}

void IRPrinter::Visit(const Assign *op) {
  // PrintIndent();
  Print(op->a);
  os_ << " = ";
  Print(op->b);
  os_ << ";";
}

void IRPrinter::Visit(const Function *op) {
  LOG_INDENT("IRPrinter::Visit Function");
  CINN_DEBUG(3) << "print function " << op->name();

  // print func definition.
  std::vector<std::string> arguments;
  for (int i = 0; i < op->inputs().size(); i++) {
    auto &x = op->inputs()[i];
    CHECK(x.is_var() || x.is_tensor());
    if (x.is_var())
      arguments.push_back("Buffer& " + x.As<ir::Var>()->name());
    else
      arguments.push_back("Tensor& " + x.As<ir::Tensor>()->name());
  }
  for (int i = 0; i < op->outputs().size(); i++) {
    auto &x = op->outputs()[i];
    CHECK(x.is_var() || x.is_tensor());
    if (x.is_var())
      arguments.push_back("Buffer& " + x.As<ir::Var>()->name());
    else
      arguments.push_back("Tensor& " + x.As<ir::Tensor>()->name());
  }

  os_ << StringFormat("def %s (%s)", op->name().c_str(), Concat(arguments, ", ").c_str());

  // body print with indent
  PrintIndent();
  os_ << " {";
  indent_size_++;

  // print the buffer allocate.
  CINN_DEBUG(3) << "stage size: " << op->stages().size();

  Print(op->GetTransformedExpr());

  indent_size_--;

  PrintIndent();
  os_ << "}";
}

void IRPrinter::Visit(const Allocate *op) {
  PrintIndent();
  os_ << "Buffer " << op->buffer_name << "(";
  Print(op->size);
  os_ << ", ";
  Print(op->dtype);
  os_ << ");";
}

void IRPrinter::Print(primitive_t dtype) {
  switch (dtype) {
#define TYPE_CASE(type__)   \
  case primitive_t::type__: \
    os_ << #type__ "_t";    \
    break;

    PRIMITIVE_TYPE_FOR_EACH(TYPE_CASE)
#undef TYPE_CASE
  }
}

void IRPrinter::PrintIndent(int diff) { os_ << std::string(indent_block_ * (indent_size_ + diff), ' '); }

}  // namespace ir
}  // namespace cinn
