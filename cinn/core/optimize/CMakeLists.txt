cc_library(pass SRCS pass.cc DEPS ir)
cc_library(pass_registry SRCS pass_registry.cc DEPS pass)
cc_library(passes SRCS indices_to_absolute_offset_pass.cc
        simplify_pass.cc
        increment_assign_compose_pass.cc
        fold_reference_indices_pass.cc
        nested_block_clean_pass.cc
        vectorize_pass.cc
        display_program_pass.cc
        call_once_pass.cc
        temp_variable_fold_pass.cc
        vectorize_utils.cc
        unroll_pass.cc
        unroll_utils.cc
        fold_variable_utils.cc
        DEPS pass pass_registry ir)

cc_library(optimizer SRCS optimizer.cc DEPS passes pass_registry)

cc_test(test_optimizer SRCS optimizer_test.cc DEPS ir optimizer)
cc_test(test_call_once_pass SRCS call_once_pass_test.cc DEPS optimizer)
cc_test(test_vectorize_utils SRCS vectorize_utils_test.cc DEPS optimizer)
