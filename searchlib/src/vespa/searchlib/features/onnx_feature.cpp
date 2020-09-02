// Copyright Verizon Media. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "onnx_feature.h"
#include <vespa/searchlib/fef/properties.h>
#include <vespa/searchlib/fef/featureexecutor.h>
#include <vespa/eval/tensor/dense/dense_tensor_view.h>
#include <vespa/eval/tensor/dense/mutable_dense_tensor_view.h>
#include <vespa/vespalib/util/stringfmt.h>
#include <vespa/vespalib/util/stash.h>

#include <vespa/log/log.h>
LOG_SETUP(".features.onnx_feature");

using search::fef::Blueprint;
using search::fef::FeatureExecutor;
using search::fef::FeatureType;
using search::fef::IIndexEnvironment;
using search::fef::IQueryEnvironment;
using search::fef::ParameterList;
using vespalib::Stash;
using vespalib::eval::ValueType;
using vespalib::make_string_short::fmt;
using vespalib::tensor::DenseTensorView;
using vespalib::tensor::MutableDenseTensorView;
using vespalib::tensor::Onnx;

namespace search::features {

/**
 * Feature executor that evaluates an onnx model
 */
class OnnxFeatureExecutor : public FeatureExecutor
{
private:
    Onnx::EvalContext _eval_context;
public:
    OnnxFeatureExecutor(const Onnx &model, const Onnx::WireInfo &wire_info)
        : _eval_context(model, wire_info) {}
    bool isPure() override { return true; }
    void handle_bind_outputs(vespalib::ArrayRef<fef::NumberOrObject>) override {
        for (size_t i = 0; i < _eval_context.num_results(); ++i) {
            outputs().set_object(i, _eval_context.get_result(i));
        }
    }
    void execute(uint32_t) override {
        for (size_t i = 0; i < _eval_context.num_params(); ++i) {
            _eval_context.bind_param(i, inputs().get_object(i).get());
        }
        _eval_context.eval();
    }
};

OnnxBlueprint::OnnxBlueprint()
    : Blueprint("onnxModel"),
      _model(nullptr),
      _wire_info()
{
}

OnnxBlueprint::~OnnxBlueprint() = default;

bool
OnnxBlueprint::setup(const IIndexEnvironment &env,
                     const ParameterList &params)
{
    auto optimize = (env.getFeatureMotivation() == env.FeatureMotivation::VERIFY_SETUP)
                    ? Onnx::Optimize::DISABLE
                    : Onnx::Optimize::ENABLE;
    auto file_name = env.getOnnxModelFullPath(params[0].getValue());
    if (!file_name.has_value()) {
        return fail("no model with name '%s' found", params[0].getValue().c_str());
    }
    try {
        _model = std::make_unique<Onnx>(file_name.value(), optimize);
    } catch (std::exception &ex) {
        return fail("model setup failed: %s", ex.what());
    }
    Onnx::WirePlanner planner;
    for (size_t i = 0; i < _model->inputs().size(); ++i) {
        const auto &model_input = _model->inputs()[i];
        if (auto maybe_input = defineInput(fmt("rankingExpression(\"%s\")", model_input.name.c_str()), AcceptInput::OBJECT)) {
            const FeatureType &feature_input = maybe_input.value();
            assert(feature_input.is_object());
            if (!planner.bind_input_type(feature_input.type(), model_input)) {
                return fail("incompatible type for input '%s': %s -> %s", model_input.name.c_str(),
                            feature_input.type().to_spec().c_str(), model_input.type_as_string().c_str());
            }
        }
    }
    for (size_t i = 0; i < _model->outputs().size(); ++i) {
        const auto &model_output = _model->outputs()[i];
        ValueType output_type = planner.make_output_type(model_output);
        if (output_type.is_error()) {
            return fail("unable to make compatible type for output '%s': %s -> error",
                        model_output.name.c_str(), model_output.type_as_string().c_str());
        }
        describeOutput(model_output.name, "output from onnx model", FeatureType::object(output_type));
    }
    _wire_info = planner.get_wire_info(*_model);
    return true;
}

FeatureExecutor &
OnnxBlueprint::createExecutor(const IQueryEnvironment &, Stash &stash) const
{
    assert(_model);
    return stash.create<OnnxFeatureExecutor>(*_model, _wire_info);
}

}