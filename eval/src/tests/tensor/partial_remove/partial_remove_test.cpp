// Copyright Verizon Media. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/eval/eval/simple_value.h>
#include <vespa/eval/eval/test/tensor_model.hpp>
#include <vespa/eval/eval/value_codec.h>
#include <vespa/eval/tensor/cell_values.h>
#include <vespa/eval/tensor/default_tensor_engine.h>
#include <vespa/eval/tensor/partial_update.h>
#include <vespa/eval/tensor/sparse/sparse_tensor.h>
#include <vespa/eval/tensor/tensor.h>
#include <vespa/vespalib/util/stringfmt.h>
#include <vespa/vespalib/gtest/gtest.h>
#include <optional>

using namespace vespalib;
using namespace vespalib::eval;
using namespace vespalib::eval::test;

using vespalib::make_string_short::fmt;

std::vector<Layout> remove_layouts = {
    {x({"a"})},                           {x({"b"})},
    {x({"a","b"})},                       {x({"a","c"})},
    {x({"a","b"})},                       {x({"a","b"})},
    float_cells({x({"a","b"})}),          {x({"a","c"})},
    {x({"a","b"})},                       float_cells({x({"a","c"})}),
    float_cells({x({"a","b"})}),          float_cells({x({"a","c"})}),
    {x({"a","b","c"}),y({"d","e"})},      {x({"b","f"}),y({"d","g"})},             
    {x(3),y({"a","b"})},                  {y({"b","c"})}
};

TensorSpec::Address only_sparse(const TensorSpec::Address &input) {
    TensorSpec::Address output;
    for (const auto & kv : input) {
        if (kv.second.is_mapped()) {
            output.emplace(kv.first, kv.second);
        }
    }
    return output;
}

TensorSpec reference_remove(const TensorSpec &a, const TensorSpec &b) {
    TensorSpec result(a.type());
    auto end_iter = b.cells().end();
    for (const auto &cell: a.cells()) {
        auto iter = b.cells().find(only_sparse(cell.first));
        if (iter == end_iter) {
            result.add(cell.first, cell.second);
        }
    }
    return result;
}

Value::UP try_partial_remove(const TensorSpec &a, const TensorSpec &b) {
    const auto &factory = SimpleValueBuilderFactory::get();
    auto lhs = value_from_spec(a, factory);
    auto rhs = value_from_spec(b, factory);
    return tensor::TensorPartialUpdate::remove(*lhs, *rhs, factory);
}

TensorSpec perform_partial_remove(const TensorSpec &a, const TensorSpec &b) {
    auto up = try_partial_remove(a, b);
    EXPECT_TRUE(up);
    return spec_from_value(*up);
}

TensorSpec perform_old_remove(const TensorSpec &a, const TensorSpec &b) {
    const auto &engine = tensor::DefaultTensorEngine::ref();
    auto lhs = engine.from_spec(a);
    auto rhs = engine.from_spec(b);
    auto lhs_tensor = dynamic_cast<tensor::Tensor *>(lhs.get());
    EXPECT_TRUE(lhs_tensor);
    auto rhs_sparse = dynamic_cast<tensor::SparseTensor *>(rhs.get());
    EXPECT_TRUE(rhs_sparse);
    tensor::CellValues cell_values(*rhs_sparse);
    auto up = lhs_tensor->remove(cell_values);
    EXPECT_TRUE(up);
    return engine.to_spec(*up);
}


TEST(PartialRemoveTest, partial_remove_works_for_simple_values) {
    ASSERT_TRUE((remove_layouts.size() % 2) == 0);
    for (size_t i = 0; i < remove_layouts.size(); i += 2) {
        TensorSpec lhs = spec(remove_layouts[i], N());
        TensorSpec rhs = spec(remove_layouts[i + 1], Div16(N()));
        SCOPED_TRACE(fmt("\n===\nLHS: %s\nRHS: %s\n===\n", lhs.to_string().c_str(), rhs.to_string().c_str()));
        auto expect = reference_remove(lhs, rhs);
        auto actual = perform_partial_remove(lhs, rhs);
        EXPECT_EQ(actual, expect);
    }
}

TEST(PartialRemoveTest, partial_remove_works_like_old_remove) {
    ASSERT_TRUE((remove_layouts.size() % 2) == 0);
    for (size_t i = 0; i < remove_layouts.size(); i += 2) {
        TensorSpec lhs = spec(remove_layouts[i], N());
        TensorSpec rhs = spec(remove_layouts[i + 1], Div16(N()));
        SCOPED_TRACE(fmt("\n===\nLHS: %s\nRHS: %s\n===\n", lhs.to_string().c_str(), rhs.to_string().c_str()));
        auto expect = perform_old_remove(lhs, rhs);
        auto actual = perform_partial_remove(lhs, rhs);
        EXPECT_EQ(actual, expect);
    }
}

std::vector<Layout> bad_layouts = {
    {x(3)},                               {x(3)},
    {x(3),y({"a"})},                      {x(3)},
    {x(3),y({"a"})},                      {x(3),y({"a"})},
    {x({"a"})},                           {y({"a"})},
    {x({"a"})},                           {x({"a"}),y({"b"})}
};

TEST(PartialRemoveTest, partial_remove_returns_nullptr_on_invalid_inputs) {
    ASSERT_TRUE((bad_layouts.size() % 2) == 0);
    for (size_t i = 0; i < bad_layouts.size(); i += 2) {
        TensorSpec lhs = spec(bad_layouts[i], N());
        TensorSpec rhs = spec(bad_layouts[i + 1], Div16(N()));
        SCOPED_TRACE(fmt("\n===\nLHS: %s\nRHS: %s\n===\n", lhs.to_string().c_str(), rhs.to_string().c_str()));
        auto actual = try_partial_remove(lhs, rhs);
        auto expect = Value::UP();
        EXPECT_EQ(actual, expect);
    }
}

GTEST_MAIN_RUN_ALL_TESTS()