// Copyright Verizon Media. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include "dense_tensor_view.h"
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <vespa/vespalib/stllike/string.h>
#include <vespa/eval/eval/value_type.h>
#include <vector>
#include <map>

namespace vespalib::eval { struct Value; }

namespace vespalib::tensor {

/**
 * Wrapper around an ONNX model handeled by onnxruntime.
 *
 * Create an Onnx object that will load your model and extract
 * information about inputs and outputs. Use an Onnx::WirePlanner to
 * bind vespa value types to each of the onnx model inputs. Ask the
 * wire planner about the vespa value types corresponding to each of
 * the model outputs for external wiring. Use the wire planner to make
 * a WireInfo object which is a simple struct indicating the concrete
 * onnx and vespa types to be used when converting inputs and
 * outputs. Create an Onnx::EvalContex based on the model and the wire
 * plan. Bind actual vespa values to the model inputs, invoke eval and
 * inspect the results. See the unit test (tests/tensor/onnx_wrapper)
 * for some examples.
 **/
class Onnx {
public:
    // model optimization
    enum class Optimize { ENABLE, DISABLE };

    // the size of a dimension
    struct DimSize {
        size_t value;
        vespalib::string name;
        DimSize() : value(0), name() {}
        DimSize(size_t size) : value(size), name() {}
        DimSize(const vespalib::string &symbol) : value(0), name(symbol) {}
        bool is_known() const { return (value > 0); }
        bool is_symbolic() const { return !name.empty(); }
        vespalib::string as_string() const;
    };

    // information about a single input or output tensor
    struct TensorInfo {
        enum class ElementType { FLOAT, DOUBLE, UNKNOWN };
        vespalib::string name;
        std::vector<DimSize> dimensions;
        ElementType elements;
        vespalib::string type_as_string() const;
        ~TensorInfo();
    };

    // how the model should be wired with inputs/outputs
    struct WireInfo {
        std::vector<std::vector<int64_t>> input_sizes;
        std::vector<eval::ValueType> output_types;
        WireInfo() : input_sizes(), output_types() {}
    };

    // planning how we should wire the model based on input types
    class WirePlanner {
    private:
        std::map<vespalib::string,size_t> _symbolic_sizes;
        std::map<std::pair<vespalib::string,size_t>,size_t> _unknown_sizes;
    public:
        WirePlanner() : _symbolic_sizes(), _unknown_sizes() {}
        ~WirePlanner();
        bool bind_input_type(const eval::ValueType &vespa_in, const TensorInfo &onnx_in);
        eval::ValueType make_output_type(const TensorInfo &onnx_out) const;
        WireInfo get_wire_info(const Onnx &model) const;
    };

    // evaluation context; use one per thread and keep model/wire_info alive
    // all parameter values are expected to be bound per evaluation
    // output values are pre-allocated and will not change
    class EvalContext {
    private:
        static Ort::AllocatorWithDefaultOptions _alloc;

        const Onnx                      &_model;
        const WireInfo                  &_wire_info;
        Ort::MemoryInfo                  _cpu_memory;
        std::vector<Ort::Value>          _param_values;
        std::vector<Ort::Value>          _result_values;
        std::vector<DenseTensorView>     _result_views;

    public:
        EvalContext(const Onnx &model, const WireInfo &wire_info);
        ~EvalContext();
        size_t num_params() const { return _param_values.size(); }
        size_t num_results() const { return _result_values.size(); }
        void bind_param(size_t i, const eval::Value &param);
        void eval();
        const eval::Value &get_result(size_t i) const;
    };

private:
    // common stuff shared between model sessions
    class Shared {
    private:
        Ort::Env _env;
        Shared();
    public:
        static Shared &get();
        Ort::Env &env() { return _env; }
    };

    Shared                   &_shared;
    Ort::SessionOptions       _options;
    Ort::Session              _session;
    std::vector<TensorInfo>   _inputs;
    std::vector<TensorInfo>   _outputs;
    std::vector<const char *> _input_name_refs;
    std::vector<const char *> _output_name_refs;

    void extract_meta_data();

public:
    Onnx(const vespalib::string &model_file, Optimize optimize);
    ~Onnx();
    const std::vector<TensorInfo> &inputs() const { return _inputs; }
    const std::vector<TensorInfo> &outputs() const { return _outputs; }
};

}