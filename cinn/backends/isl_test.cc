#include <glog/logging.h>
#include <gtest/gtest.h>
#include <isl/constraint.h>
#include <isl/map.h>
#include <isl/set.h>

std::string isl_to_str(isl_set *x) { return isl_set_to_str(x); }
std::string isl_to_str(isl_map *x) { return isl_map_to_str(x); }
std::string isl_to_str(isl_space *x) { return isl_space_to_str(x); }

struct param_pack_1 {
  int in_dim;
  int out_constant;
};

isl_map *isl_map_add_dim_and_eq_constraint(isl_map *map, int dim_pos, int constant) {
  CHECK(map);
  CHECK_GE(dim_pos, 0);
  CHECK_LE(dim_pos, isl_map_dim(map, isl_dim_out));

  map = isl_map_insert_dims(map, isl_dim_out, dim_pos, 1);
  map = isl_map_set_tuple_name(map, isl_dim_out, isl_map_get_tuple_name(map, isl_dim_in));

  isl_space *sp = isl_map_get_space(map);
  isl_local_space *lsp = isl_local_space_from_space(isl_space_copy(sp));
  isl_constraint *cst = isl_constraint_alloc_equality(lsp);
  cst = isl_constraint_set_coefficient_si(cst, isl_dim_out, dim_pos, 1);
  cst = isl_constraint_set_constant_si(cst, (-1) * constant);
  map = isl_map_add_constraint(map, cst);

  return map;
}

/**
 * Take a basic map as input, go through all of its constraints,
 * identifies the constraint of the static dimension param_pack_1.in_dim
 * (passed in user) and replace the value of param_pack_1.out_constant if
 * the static dimension is bigger than that value.
 */
isl_stat extract_static_dim_value_from_bmap(__isl_take isl_basic_map *bmap, void *user) {
  struct param_pack_1 *data = (struct param_pack_1 *)user;

  isl_constraint_list *list = isl_basic_map_get_constraint_list(bmap);
  int n_constraints = isl_constraint_list_n_constraint(list);

  for (int i = 0; i < n_constraints; i++) {
    isl_constraint *cst = isl_constraint_list_get_constraint(list, i);
    isl_val *val = isl_constraint_get_coefficient_val(cst, isl_dim_out, data->in_dim);
    if (isl_val_is_one(val))  // i.e., the coefficient of the dimension data->in_dim is 1
    {
      isl_val *val2 = isl_constraint_get_constant_val(cst);
      int const_val = (-1) * isl_val_get_num_si(val2);
      data->out_constant = const_val;
    }
  }

  return isl_stat_ok;
}

// if multiple const values exist, choose the maximal value among them because we
// want to use this value to know by how much we shift the computations back.
// so we need to figure out the maximal const value and use it to shift the iterations
// backward so that that iteration runs before the consumer.
isl_stat extract_constant_value_from_bset(__isl_take isl_basic_set *bset, void *user) {
  struct param_pack_1 *data = (struct param_pack_1 *)user;

  isl_constraint_list *list = isl_basic_set_get_constraint_list(bset);
  int n_constraints = isl_constraint_list_n_constraint(list);

  for (int i = 0; i < n_constraints; i++) {
    isl_constraint *cst = isl_constraint_list_get_constraint(list, i);
    if (isl_constraint_is_equality(cst) && isl_constraint_involves_dims(cst, isl_dim_set, data->in_dim, 1)) {
      isl_val *val = isl_constraint_get_coefficient_val(cst, isl_dim_out, data->in_dim);
      assert(isl_val_is_one(val));
      // assert that the coefficients of all the other dimension spaces are zero.

      isl_val *val2 = isl_constraint_get_constant_val(cst);
      int const_val = (-1) * isl_val_get_num_si(val2);
      data->out_constant = std::max(data->out_constant, const_val);
    }
  }

  return isl_stat_ok;
}

/**
 * Return the value of the static dimension.
 *
 * For example, if we have a map M = {S0[i,j]->[0,0,i,1,j,2]; S0[i,j]->[1,0,i,1,j,3]}
 * and call isl_map_get_static_dim(M, 5, 1), it will return 3.
 */
int isl_map_get_static_dim(isl_map *map, int dim_pos) {
  assert(map != NULL);
  assert(dim_pos >= 0);
  assert(dim_pos <= (signed int)isl_map_dim(map, isl_dim_out));

  struct param_pack_1 *data = (struct param_pack_1 *)malloc(sizeof(struct param_pack_1));
  data->out_constant = 0;
  data->in_dim = dim_pos;

  isl_map_foreach_basic_map(isl_map_copy(map), &extract_static_dim_value_from_bmap, data);

  return data->out_constant;
}

int isl_set_get_const_dim(isl_set *set, int dim_pos) {
  assert(set != NULL);
  assert(dim_pos >= 0);
  assert(dim_pos <= (signed int)isl_set_dim(set, isl_dim_out));

  struct param_pack_1 *data = (struct param_pack_1 *)malloc(sizeof(struct param_pack_1));
  data->out_constant = 0;
  data->in_dim = dim_pos;

  isl_set_foreach_basic_set(isl_set_copy(set), &extract_constant_value_from_bset, data);

  return data->out_constant;
}

/**
 * Set the value \p val for the output dimension \p dim_pos of \p map.
 *
 * Example
 *
 * Assuming the map M = {S[i,j]->[i0,i1,i2]}
 *
 * M = isl_map_set_const_dim(M, 0, 0);
 *
 * Would create the constraint i0=0 and add it to the map.
 * The resulting map is
 *
 * M = {S[i,j]->[i0,i1,i2]: i0=0}
 *
 */
isl_map *isl_map_set_const_dim(isl_map *map, int dim_pos, int val) {
  assert(map != NULL);
  assert(dim_pos >= 0);
  assert(dim_pos <= (signed int)isl_map_dim(map, isl_dim_out));

  isl_map *identity = isl_set_identity(isl_map_range(isl_map_copy(map)));
  // We need to create a universe of the map (i.e., an unconstrained map)
  // because isl_set_identity() create an identity transformation and
  // inserts the constraints that were in the original set.  We don't
  // want to have those constraints.  We want to have a universe map, i.e.,
  // a map without any constraint.
  identity = isl_map_universe(isl_map_get_space(identity));

  isl_space *sp = isl_map_get_space(identity);
  isl_local_space *lsp = isl_local_space_from_space(isl_space_copy(sp));

  // This loops goes through the output dimensions of the map one by one
  // and adds a constraint for each dimension. IF the dimension is dim_pos
  // it add a constraint of equality to val
  // Otherwise it adds a constraint that keeps the original value, i.e.,
  // (output dimension = input dimension)
  // Example
  //  Assuming that dim_pos = 0, val = 10 and the universe map is
  //  {S[i0,i1]->S[j0,j1]}, this loop produces
  //  {S[i0,i1]->S[j0,j1]: j0=0 and j1=i1}
  //  i.e.,
  //  {S[i0,i1]->S[0,i1]}
  for (int i = 0; i < isl_map_dim(identity, isl_dim_out); i++)
    if (i == dim_pos) {
      isl_constraint *cst = isl_constraint_alloc_equality(isl_local_space_copy(lsp));
      cst = isl_constraint_set_coefficient_si(cst, isl_dim_out, dim_pos, 1);
      cst = isl_constraint_set_constant_si(cst, (-1) * (val));
      identity = isl_map_add_constraint(identity, cst);
    } else {
      isl_constraint *cst2 = isl_constraint_alloc_equality(isl_local_space_copy(lsp));
      cst2 = isl_constraint_set_coefficient_si(cst2, isl_dim_in, i, 1);
      cst2 = isl_constraint_set_coefficient_si(cst2, isl_dim_out, i, -1);
      identity = isl_map_add_constraint(identity, cst2);
    }

  map = isl_map_apply_range(map, identity);

  return map;
}

TEST(isl, basic) {
  // Create a new context
  isl_ctx *context = isl_ctx_alloc();
  isl_set *iter_domain = isl_set_read_from_str(context, "[N] -> { A[i,j, z] : 0<=i<100 and 0<=j<100 and 0<=z<=10}");
  LOG(INFO) << "iter_domain " << isl_to_str(iter_domain);
  isl_space *iter_domain_space = isl_set_get_space(iter_domain);
  LOG(INFO) << "isl_space " << isl_to_str(iter_domain_space);
  LOG(INFO) << "space tuple name: " << isl_space_get_tuple_name(iter_domain_space, isl_dim_type::isl_dim_set);
  LOG(INFO) << "num of dims " << isl_set_dim(iter_domain, isl_dim_type::isl_dim_set);
  LOG(INFO) << "isl had dim name " << isl_set_has_dim_name(iter_domain, isl_dim_type::isl_dim_set, 0);
  LOG(INFO) << "isl had dim name " << isl_set_has_dim_name(iter_domain, isl_dim_type::isl_dim_set, 1);

  auto *sp = isl_space_map_from_set(iter_domain_space);
  isl_map *sched = isl_map_identity(sp);
  LOG(INFO) << "sp " << isl_to_str(sp);
  LOG(INFO) << "map " << isl_to_str(sched);
  sched = isl_map_coalesce(sched);
  LOG(INFO) << "map " << isl_to_str(sched);
  sched = isl_map_intersect_domain(sched, isl_set_copy(iter_domain));
  LOG(INFO) << "map intersect " << isl_to_str(sched);

  // Add Beta dimensions.
  for (int i = 0; i < isl_space_dim(iter_domain_space, isl_dim_out) + 1; i++) {
    sched = isl_map_add_dim_and_eq_constraint(sched, 2 * i, 0);
    LOG(INFO) << "map " << i << " " << isl_to_str(sched);
  }

  // Add the duplication dimension.
  sched = isl_map_add_dim_and_eq_constraint(sched, 0, 0);
  LOG(INFO) << "map " << isl_to_str(sched);
}