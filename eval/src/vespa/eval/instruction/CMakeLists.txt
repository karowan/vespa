# Copyright Verizon Media. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

vespa_add_library(eval_instruction OBJECT
    SOURCES
    add_trivial_dimension_optimizer.cpp
    dense_cell_range_function.cpp
    dense_dot_product_function.cpp
    dense_lambda_peek_function.cpp
    dense_lambda_peek_optimizer.cpp
    dense_matmul_function.cpp
    dense_multi_matmul_function.cpp
    dense_simple_expand_function.cpp
    dense_single_reduce_function.cpp
    dense_tensor_create_function.cpp
    dense_tensor_peek_function.cpp
    dense_xw_product_function.cpp
    fast_rename_optimizer.cpp
    generic_concat.cpp
    generic_create.cpp
    generic_join.cpp
    generic_lambda.cpp
    generic_map.cpp
    generic_merge.cpp
    generic_peek.cpp
    generic_reduce.cpp
    generic_rename.cpp
    index_lookup_table.cpp
    join_with_number_function.cpp
    mixed_inner_product_function.cpp
    mixed_map_function.cpp
    mixed_simple_join_function.cpp
    pow_as_map_optimizer.cpp
    remove_trivial_dimension_optimizer.cpp
    replace_type_function.cpp
    sparse_dot_product_function.cpp
    sum_max_dot_product_function.cpp
    vector_from_doubles_function.cpp
)
