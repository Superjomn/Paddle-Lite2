#include "cinn/core/isl_code_gen.h"
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <isl/ast.h>
#include <isl/ast_build.h>
#include <isl/constraint.h>
#include <isl/map.h>
#include <isl/set.h>
#include <isl/union_set.h>
#include <algorithm>
#include "cinn/ir/ir_printer.h"
#include "cinn/ir/ops_overload.h"

namespace cinn {

using namespace ir;

TEST(code_gen, IslAstExprToCinnExpr) {
  auto *ctx = isl_ctx_alloc();
  auto *context = isl_set_read_from_str(ctx, "{:}");
  auto *domain = isl_set_read_from_str(ctx, "[T,N]->{S[t,i]: 0 <= t < T and 1 <=i < N}");
  auto *transform = isl_map_read_from_str(ctx, "[T,N] -> {S[t,i] -> [t,i+t*10]}");
  auto *T = isl_union_map_from_map(isl_map_intersect_domain(transform, domain));

  auto build = isl::manage(isl_ast_build_from_context(context));

  // set iterators
  isl_id_list *iterators = isl_id_list_alloc(ctx, 2);
  isl_id *id = isl_id_alloc(ctx, "t", nullptr);
  iterators = isl_id_list_add(iterators, id);
  id = isl_id_alloc(ctx, "i", nullptr);
  iterators = isl_id_list_add(iterators, id);
  build = isl::manage(isl_ast_build_set_iterators(build.release(), iterators));

  auto ast = isl::manage(isl_ast_build_node_from_schedule_map(build.get(), T));

  isl::ast_expr iter = isl::manage(isl_ast_node_for_get_iterator(ast.get()));

  // ISL print C code
  isl_printer *p = isl_printer_to_str(ctx);
  isl_printer_set_output_format(p, 0);
  isl_printer_print_ast_node(p, ast.get());
  LOG(INFO) << "\n" << isl_ast_node_to_C_str(ast.get());

  ir::Expr expr;
  IslAstExprToCinnExpr(iter, &expr);

  LOG(INFO) << "Get node type " << static_cast<int>(expr.type());

  std::stringstream os;
  ir::IRPrinter printer(os);

  ir::Expr for_expr;
  IslAstNodeToCinnExpr(ast, &for_expr);

  printer.Print(for_expr);
  LOG(INFO) << "\n" << os.str();
}

TEST(code_gen, ExtractIslTransformedIndiceMap) {
  Var i("i", 0, 100);
  Var j("j", 0, 200);

  Expr A("A"), B("B");

  Stage s0 = B[i][j].Assign(A[i][j] + 1);

  isl_ctx *ctx = isl_ctx_alloc();
  isl::set context(ctx, "{:}");
  auto *build = isl_ast_build_from_context(context.copy());

  LOG(INFO) << "iterator_domain: " << s0.iterator_domain();
  isl::map transform(s0.ctx(), "[T,N] -> {S0[i,j] -> [i-1,j+1]}");
  isl::union_map T =
      isl::manage(isl_union_map_from_map(isl_map_intersect_domain(transform.copy(), s0.iterator_domain().copy())));
  LOG(INFO) << "T: " << T;

  isl_ast_build_set_at_each_domain(build, IslAstNodeInfoCollect, nullptr);

  auto *ast = isl_ast_build_node_from_schedule_map(build, T.copy());

  LOG(INFO) << "isl_ast_node_to_C: ";
  LOG(INFO) << "\n" << isl_ast_node_to_C_str(ast);
}

TEST(code_gen, FilterStagesByDomain) {
  Var i("i", 0, 100);
  Var j("j", 0, 120);
  Var k("k", 0, 30);

  Expr A("A"), B("B"), C("C");

  Stage s0 = A[i][j].Assign(B[i][j]);
  Stage s1 = C[k].Assign(C[k]);

  LOG(INFO) << "num_stages: " << Generator::Global().num_stages();
  LOG(INFO) << "s0.domain: " << s0.iterator_domain();
  LOG(INFO) << "s1.domain: " << s1.iterator_domain();
}

TEST(code_gen, ReplaceCinnIndiceWithIslTransformedIndices) {
  isl_ctx *ctx = Generator::Global().ctx().get();

  Var i("i", 0, 100);
  Var j("j", 0, 120);
  Var k("k", 0, 30);

  Expr A("A"), B("B"), C("C");

  Stage s0 = A[i][j].Assign(B[i][j] * 2 + C[i + 1][j - 2] - 1);
  LOG(INFO) << "S0.iterator_domain: " << s0.iterator_domain();

  isl::map t = s0.GetTransformedSchedule();
  CHECK(!t.is_null());
  LOG(INFO) << "transformed: " << t;

  isl::map t1(ctx, "{ " + s0.name() + "[i,j] -> [i+1, j-2] }");
  auto t2 = t1.intersect_domain(s0.iterator_domain());

  auto *T = isl_union_map_from_map(t2.release());

  // generate isl ast
  isl::set context(Generator::Global().ctx(), "{:}");
  auto *build = isl_ast_build_from_context(context.copy());
  isl_ast_build_set_at_each_domain(build, IslAstNodeInfoCollect, nullptr);
  auto ast = isl::manage(isl_ast_build_node_from_schedule_map(build, T));

  // Transform isl ast to CINN ast.
  Expr root;
  IslAstNodeToCinnExpr(ast, &root);

  LOG(INFO) << "isl generated CINN ast:\n" << Dump(root);
  auto expr = s0.GetIndiceTransformedExpr();
  LOG(INFO) << "transformed expr: " << Dump(expr);

  std::stringstream os;
  os << Dump(expr);

  ASSERT_EQ(os.str(),
            "A((c0 - 1),(c1 + 2)) = (((B((c0 - 1),(c1 + 2)) * 2) + C(((c0 - 1) + 1),((c1 + 2) - 2))) - 1);\n");

  ReplaceExprWithStage(root, s0.name(), s0.GetIndiceTransformedExpr());

  LOG(INFO) << "replace expr with stage: " << ir::Dump(root);
}

}  // namespace cinn