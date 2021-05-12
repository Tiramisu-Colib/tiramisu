//  Copyright (c) 2019-2020 Christopher Taylor
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <memory>
#include <vector>
#include <algorithm>

#include <isl/set.h>
#include <isl/map.h>
#include <isl/union_set.h>
#include <isl/union_map.h>
#include <isl/ast_build.h>
#include <isl/schedule.h>
#include <isl/schedule_node.h>
#include <isl/space.h>
#include <isl/constraint.h>

#include <tiramisu/tiramisu.h>
#include <tiramisu/expr.h>

/*
 *
 *  tiramisu/include/tiramisu/macros.h:4:9: note: macro 'cast' defined here
 *  #define cast(TYPE, EXPRESSION) (tiramisu::expr(tiramisu::o_cast, TYPE, EXPRESSION))
 *
 *  undef b/c this macro conflicts with a method/function signature in pybind11
 *
 */
#undef cast

#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include "pytiramisu.hpp"

namespace py = pybind11;
using namespace tiramisu;

// wrapper types for isl_map and isl_set
//
class isl_map_t {
public:
    isl_map * value;

    isl_map_t() :
        value(nullptr) {
    }
};

class isl_set_t {
public:
    isl_set * value;

    isl_set_t() :
        value(nullptr) {
    } 
};

PYBIND11_MODULE(pytiramisu, m) {
    m.doc() = "pybind11 bindings for isl";

    // enums from `tiramisu/include/tiramisu/type.h`

    py::enum_<expr_t>(m, "expr_t")
        .value("e_val", e_val)
        .value("e_var", e_var)
        .value("e_sync", e_sync)
        .value("e_op", e_op)
        .value("e_none", e_none)
        .export_values();

    py::enum_<primitive_t>(m, "primitive_t")
        .value("p_uint8", p_uint8)
        .value("p_uint16", p_uint16)
        .value("p_uint32", p_uint32)
        .value("p_uint64", p_uint64)
        .value("p_int8", p_int8)
        .value("p_int16", p_int16)
        .value("p_int32", p_int32)
        .value("p_int64", p_int64)
        .value("p_float32", p_float32)
        .value("p_float64", p_float64)
        .value("p_boolean", p_boolean)
        .value("p_async", p_async)
        .value("p_wait_ptr", p_wait_ptr)
        .value("p_void_ptr", p_void_ptr)
        .value("pnone", p_none)
        .export_values();

    py::enum_<argument_t>(m, "argument_t")
        .value("a_input", a_input)
        .value("a_output", a_output)
        .value("a_temporary", a_temporary)
        .export_values();

    py::enum_<op_t>(m, "op_t")
        .value("o_minus", o_minus)
        .value("o_floor", o_floor)
        .value("o_sin", o_sin)
        .value("o_cos", o_cos)
        .value("o_tan", o_tan)
        .value("o_asin", o_asin)
        .value("o_acos", o_acos)
        .value("o_atan", o_atan)
        .value("o_sinh", o_sinh)
        .value("o_cosh", o_cosh)
        .value("o_tanh", o_tanh)
        .value("o_asinh", o_asinh)
        .value("o_acosh", o_acosh)
        .value("o_atanh", o_atanh)
        .value("o_abs", o_abs)
        .value("o_sqrt", o_sqrt)
        .value("o_expo", o_expo)
        .value("o_log", o_log)
        .value("o_ceil", o_ceil)
        .value("o_round", o_round)
        .value("o_trunc", o_trunc)
        .value("o_allocate", o_allocate)
        .value("o_free", o_free)
        .value("o_cast", o_cast)
        .value("o_address", o_address)
        .value("o_add", o_add)
        .value("o_sub", o_sub)
        .value("o_mul", o_mul)
        .value("o_div", o_div)
        .value("o_mod", o_mod)
        .value("o_logical_and", o_logical_and)
        .value("o_logical_or", o_logical_or)
        .value("o_logical_not", o_logical_not)
        .value("o_eq", o_eq)
        .value("o_ne", o_ne)
        .value("o_le", o_le)
        .value("o_lt", o_lt)
        .value("o_ge", o_ge)
        .value("o_gt", o_gt)
        .value("o_max", o_max)
        .value("o_min", o_min)
        .value("o_right_shift", o_right_shift)
        .value("o_left_shift", o_left_shift)
        .value("o_memcpy", o_memcpy)
        .value("o_select", o_select)
        .value("o_cond", o_cond)
        .value("o_lerp", o_lerp)
        .value("o_call", o_call)
        .value("o_access", o_access)
        .value("o_address_of", o_address_of)
        .value("o_lin_index", o_lin_index)
        .value("o_type", o_type)
        .value("o_dummy", o_dummy)
        .value("o_buffer", o_buffer)
        .value("o_none", o_none)
        .export_values();

    py::enum_<rank_t>(m, "rank_t")
        .value("r_sender", rank_t::r_sender)
        .value("r_receiver", rank_t::r_receiver)
        .export_values();

    py::class_<global>(m, "global")
        .def(py::init( []() {
            return tiramisu::global{};
        }))
        .def_static("generate_new_buffer_name", []() {
            return global::generate_new_buffer_name();
        })
        .def_static("get_implicit_function", []() {
            return global::get_implicit_function();
        }, py::return_value_policy::reference)
        .def_static("set_implicit_function", [](std::shared_ptr<function> fct) {
            global::set_implicit_function(fct.get());
        })
        .def_static("set_auto_data_mapping", [](bool v) {
            return global::set_auto_data_mapping(v);
        })
        .def_static("is_auto_data_mapping_set", []() {
            return global::is_auto_data_mapping_set();
        })
        .def_static("set_default_tiramisu_options", []() {
            return global::set_default_tiramisu_options();
        })
        .def_static("set_loop_iterator_type", [](primitive_t t) {
            return global::set_loop_iterator_type(t);
        });

    py::class_<function> func(m, "function");
        func.def(py::init<std::string>())
        .def("add_context_constraints", [](function &f,
            std::string new_context) {
            f.add_context_constraints(new_context);
        })
        .def("align_schedules", [](function &f) {
            f.align_schedules();
        })
        .def("allocate_and_map_buffers_automatically", [](function &f) {
            f.allocate_and_map_buffers_automatically();
        })
        .def("compute_bounds", [](function &f) {
            f.compute_bounds();
        })
        .def("dump", [](function &f, bool exhaustive) {
            f.dump(exhaustive);
        })
        .def("dump_dep_graph", [](function &f) {
            f.dump_dep_graph();
        })
        .def("dump_iteration_domain", [](function &f) {
            f.dump_iteration_domain();
        })
        .def("dump_schedule", [](function &f) {
            f.dump_schedule();
        })
        .def("dump_sched_graph", [](function &f) {
            f.dump_sched_graph();
        })
        .def("dump_time_processor_domain", [](function &f) {
            f.dump_time_processor_domain();
        })
        .def("dump_trimmed_time_processor_domain", [](function &f) {
            f.dump_trimmed_time_processor_domain();
        })
        .def("gen_c_code", [](function &f) {
            f.gen_c_code();
        })
        .def("gen_isl_ast", [](function &f) {
            f.gen_isl_ast();
        })
        .def("gen_time_space_domain", [](function &f) {
            f.gen_time_space_domain();
        })
        .def("set_arguments", [](function &f,
            std::vector<std::shared_ptr<buffer>> &buffer_vec) {

            std::vector<buffer *> buffer_vec_{};
            buffer_vec_.resize(buffer_vec.size());
            std::transform(buffer_vec.begin(), buffer_vec.end(), buffer_vec_.begin(), [](auto b) { return b.get(); });
            f.set_arguments(buffer_vec_);
        })
        .def("codegen", [](function &f,
            std::vector<std::shared_ptr<buffer>> &buffer_vec,
            std::string obj_filename,
            bool gen_cuda_stmt) {

            std::vector<buffer *> buffer_vec_{};
            buffer_vec_.resize(buffer_vec.size());
            std::transform(buffer_vec.begin(), buffer_vec.end(), buffer_vec_.begin(), [](auto b) { return b.get(); });
      
            f.codegen(buffer_vec_, obj_filename, gen_cuda_stmt);
        })
        .def("set_context_set", [](function &f, std::string context) {
            f.set_context_set(context);
        })
        .def("set_context_set", [](function &f, isl_set_t context) {
            f.set_context_set(context.value);
        });

    py::class_<expr>(m, "expr")
        .def(py::init())
        .def(py::init<primitive_t>())
        .def(py::init<op_t, primitive_t, expr>())
        .def(py::init<op_t, expr>())
        .def(py::init<op_t, std::string>())
        .def(py::init<op_t, expr, expr>())
        .def(py::init<op_t, expr, expr, expr>())
        .def(py::init<op_t, std::string, std::vector<expr>, primitive_t>())
        .def(py::init<uint8_t>())
        .def(py::init<int8_t>())
        .def(py::init<uint16_t>())
        .def(py::init<int16_t>())
        .def(py::init<uint32_t>())
        .def(py::init<int32_t>())
        .def(py::init<uint64_t>())
        .def(py::init<int64_t>())
        .def(py::init<float>())
        .def(py::init<double>())
        .def("copy", [](expr &e) { return e.copy(); })
        .def("get_uint8_value", [](expr &e) { return e.get_uint8_value(); })
        .def("get_int8_value", [](expr &e) { return e.get_int8_value(); })
        .def("get_uint16_value", [](expr &e) { return e.get_uint16_value(); })
        .def("get_int16_value", [](expr &e) { return e.get_int16_value(); })
        .def("get_uint32_value", [](expr &e) { return e.get_uint32_value(); })
        .def("get_int32_value", [](expr &e) { return e.get_int32_value(); })
        .def("get_uint64_value", [](expr &e) { return e.get_uint64_value(); })
        .def("get_int64_value", [](expr &e) { return e.get_int64_value(); })
        .def("get_float32_value", [](expr &e) { return e.get_float32_value(); })
        .def("get_float64_value", [](expr &e) { return e.get_float64_value(); })
        .def("get_int_val", [](expr &e) { return e.get_int_val(); })
        .def("get_double_val", [](expr &e) { return e.get_double_val(); })
        .def("get_operand", [](expr &e, int i) { return e.get_operand(i); })
        .def("get_n_arg", [](expr &e) { return e.get_n_arg(); })
        .def("get_expr_type", [](expr &e) { return e.get_expr_type(); })
        .def("get_data_type", [](expr &e) { return e.get_data_type(); })
        .def("get_name", [](expr &e) { return e.get_name(); })
        .def("set_name", [](expr &e, std::string name) { e.set_name(name); })
        .def("replace_op_in_expr", [](expr &e, std::string &to_replace, std::string &replace_with) { return e.replace_op_in_expr(to_replace, replace_with); })
        .def("get_op_type", [](expr &e) { return e.get_op_type(); })
        .def("get_access", [](expr &e) { return e.get_access(); })
        .def("get_arguments", [](expr &e) { return e.get_arguments(); })
        .def("get_n_dim_access", [](expr &e) { return e.get_n_dim_access(); })
        .def("is_defined", [](expr &e) { return e.is_defined(); })
        .def("is_equal", [](expr &e, expr e_) { return e.is_equal(e_); })
        .def("is_integer", [](expr &e) { return e.is_integer(); })
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self / py::self)
        .def(py::self * py::self)
        .def(py::self % py::self)
        .def(py::self >> py::self)
        .def(py::self << py::self)
//        .def(py::self && py::self)
//        .def(py::self || py::self)
        .def(-py::self)
        .def(!py::self)
        .def(py::self == py::self)
	.def(py::self < py::self)
        .def(py::self <= py::self)
	.def(py::self > py::self)
        .def(py::self >= py::self)
        .def("set_access", [](expr &e, std::vector<expr> vec) { e.set_access(vec); })
        .def("set_access_dimension", [](expr &e, int i, expr acc) { e.set_access_dimension(i, acc); })
        .def("set_arguments", [](expr &e, std::vector<expr> vec) { e.set_arguments(vec); })
        .def("dump", [](expr &e, bool exhaust) { e.dump(exhaust); })
        .def("is_constant", [](expr &e) { return e.is_constant(); })
        .def("is_unbounded", [](expr &e) { return e.is_unbounded(); })
        .def("simplify", [](expr &e) { return e.simplify(); })
        .def("__repr__", [](expr &e) { return e.to_str(); })
        .def("substitute", [](expr &e, std::vector<std::pair<var, expr>> substitutions) { return e.substitute(substitutions); })
        .def("substitute_access", [](expr &e, std::string orig, std::string sub) { return e.substitute_access(orig, sub); });
        //.def("apply_to_operands", [](expr &e, std::function<expr () { return e.substitute_access(orig, sub); })

    m.def("uint8_expr", [](uint8_t val) {
            return expr{static_cast<uint8_t>(val)};
        });

    m.def("int8_expr", [](int8_t val) {
            return expr{static_cast<int8_t>(val)};
        });

    m.def("uint16_expr", [](uint16_t val) {
            return expr{static_cast<uint16_t>(val)};
        });

    m.def("int16_expr", [](int16_t val) {
            return expr{static_cast<int16_t>(val)};
        });

    m.def("uint32_expr", [](uint32_t val) {
            return expr{static_cast<uint32_t>(val)};
        });

    m.def("int32_expr", [](int32_t val) {
            return expr{static_cast<int32_t>(val)};
        });

    m.def("uint64_expr", [](uint64_t val) {
            return expr{static_cast<uint64_t>(val)};
        });

    m.def("int64_expr", [](int64_t val) {
            return expr{static_cast<int64_t>(val)};
        });

    m.def("float_expr", [](float val) {
            return expr{static_cast<float>(val)};
        });

    m.def("double_expr", [](double val) {
            return expr{static_cast<double>(val)};
        });


    py::class_<var>(m, "var")
        .def(py::init<primitive_t, std::string>())
        .def(py::init<std::string>())
        .def(py::init<std::string, expr, expr>())
        .def(py::init<>())
        .def("get_upper", [](var &v) { return v.get_upper(); })
        .def("get_lower", [](var &v) { return v.get_lower(); });

    py::class_<tiramisu::computation> comp(m, "computation");
        comp.def(py::init([](std::string n, expr e, bool b, primitive_t t, std::shared_ptr<function> f) {
            return computation(n, e, b, t, f.get());
        }))
        .def(py::init<std::string, std::vector<var>, expr, bool>())
        .def(py::init<std::vector<var>, expr>())
        .def(py::init<std::string, std::vector<var>, expr>())
        .def(py::init<std::vector<var>, expr, bool>()) 
        .def(py::init<std::string, std::vector<var>, primitive_t>())
        .def(py::init<std::vector<var>, primitive_t>())
        .def("is_send", [](computation &c) { return c.is_send(); })
        .def("is_recv", [](computation &c) { return c.is_recv(); })
        .def("is_send_recv", [](computation &c) { return c.is_send_recv(); })
        .def("is_wait", [](computation &c) { return c.is_wait(); })
        .def("add_associated_let_stmt", [](computation &c,
            std::string access_name,
            expr e) {
                return c.add_associated_let_stmt(access_name, e);
        })
        .def("unschedule_this_computation", [](computation &c) {
                c.unschedule_this_computation();
        })
        .def("add_definitions", [](computation &c,
            std::string iteration_domain_str,
            expr e,
            bool schedule_this_computation,
            primitive_t t,
            std::shared_ptr<function> & fct) {
                c.add_definitions(iteration_domain_str, e, schedule_this_computation, t, fct.get());
        })
        .def("add_predicate", [](computation &c,
             expr e) {
                c.add_predicate(e);
        })
        .def("after", [](computation &c,
             computation & comp,
             var iterator) {
                c.after(comp, iterator);
        })
        .def("after", [](computation &c,
             computation & comp,
             int level) {
                c.after(comp, level);
        })
        .def("after_low_level", [](computation &c,
             computation & comp,
             int level) {
                c.after_low_level(comp, level);
        })
/*        .def("after_low_level", [](computation &c,
             computation & comp,
             std::vector<int> levels) {
                c.after_low_level(comp, levels);
        })
*/
        .def("allocate_and_map_buffer_automatically", [](computation &c,
             argument_t type) {
                c.allocate_and_map_buffer_automatically(type);
        })
        .def("apply_transformation_on_schedule", [](computation &c,
            std::string map_str) {
                c.apply_transformation_on_schedule(map_str);
        })
        .def("before", [](computation &c,
            computation &consumer,
            var L) {
                c.before(consumer, L);
        })
        .def("between", [](computation &c,
            computation &before_comp,
            var before_l,
            computation &after_comp,
            var after_l) {
                c.between(before_comp, before_l, after_comp, after_l);
        })
        .def("between", [](computation &c,
            computation &before_comp,
            int before_l,
            computation &after_comp,
            int after_l) {
                c.between(before_comp, before_l, after_comp, after_l);
        })
        .def("store_in", [](computation &c,
            buffer & buff) {
                c.store_in( std::addressof(buff) );
        })
        .def("store_in", [](computation &c,
            buffer & buff,
            std::vector<expr> iterators) {
                c.store_in( std::addressof(buff), iterators);
        })
        .def("store_in", [](computation &c,
            std::vector<expr> mapping,
            std::vector<expr> sizes) {
                c.store_in(mapping, sizes);
        })
        .def("cache_shared", [](computation &c,
            computation & inp,
            const var & level,
            std::vector<int> buffer_shape,
            std::vector<expr> copy_offsets,
            bool pad_buffer) {
                return c.cache_shared(inp, level, buffer_shape, copy_offsets, pad_buffer);
        }, py::return_value_policy::reference)
        .def("cache_shared", [](computation &c,
            computation & inp,
            const var & level,
            std::vector<int> buffer_shape,
            std::vector<expr> copy_offsets) {
                return c.cache_shared(inp, level, buffer_shape, copy_offsets);
        }, py::return_value_policy::reference)
        .def("compute_at", [](computation &c,
            computation &consumer,
            var L) {
                c.compute_at(consumer, L);
        })
        .def("compute_at", [](computation &c,
            computation &consumer,
            int L) {
                c.compute_at(consumer, L);
        })
        .def("compute_maximal_AST_depth", [](computation &c) {
            return c.compute_maximal_AST_depth();
        })
        .def("dump_iteration_domain", [](computation &c) {
            c.dump_iteration_domain();
        })
        .def("dump_schedule", [](computation &c) {
            c.dump_schedule();
        })
        .def("dump", [](computation &c) {
            c.dump();
        })
        .def("fuse_after", [](computation &c,
            var lev, computation & comp) {
            c.after(comp, lev);
        })
        .def("gen_time_space_domain", [](computation &c) {
            c.gen_time_space_domain();
        })
        .def("drop_rank_iter", [](computation &c,
            var level) {
                c.drop_rank_iter(level);
        })
        .def("get_buffer", [](computation &c) {
                return c.get_buffer();
        }, py::return_value_policy::reference)
        .def("get_data_type", [](computation &c) {
                return c.get_data_type();
        })
        .def("get_expr", [](computation &c) {
                return c.get_expr();
        })
        .def("get_iteration_domain", [](computation &c) {
                isl_set_t ret{};
                ret.value = c.get_iteration_domain();
                return ret;
        })
        .def("get_last_update", [](computation &c) {
                return c.get_last_update();
        })
        .def("get_loop_level_number_from_dimension_name", [](computation &c,
            std::string dim_name) {
                return c.get_loop_level_number_from_dimension_name(dim_name);
        })
        .def("get_name", [](computation &c) {
                return c.get_name();
        })
        .def("get_predecessor", [](computation &c) {
                return c.get_predecessor();
        }, py::return_value_policy::reference)
        .def("get_successor", [](computation &c) {
                return c.get_successor();
        }, py::return_value_policy::reference)
        .def("get_update", [](computation &c, int index) {
                return c.get_update(index);
        })
        .def("get_schedule", [](computation &c) {
                isl_map_t ret{};
                ret.value = c.get_schedule();
                return ret;
        })
        .def("gpu_tile", [](computation &c,
            var L0, var L1,
            int sizeX, int sizeY) {
                c.gpu_tile(L0, L1, sizeX, sizeY);
        })
        .def("gpu_tile", [](computation &c,
            var L0, var L1,
            int sizeX, int sizeY,
            var L0_outer, var L1_outer,
            var L0_inner, var L1_inner) {
                c.gpu_tile(L0, L1, sizeX, sizeY, L0_outer, L1_outer, L0_inner, L1_inner);
        })
        .def("gpu_tile", [](computation &c,
            var L0, var L1, var L2,
            int sizeX, int sizeY, int sizeZ) {
                c.gpu_tile(L0, L1, L2, sizeX, sizeY, sizeZ);
        })
        .def("gpu_tile", [](computation &c,
            var L0, var L1, var L2,
            int sizeX, int sizeY, int sizeZ,
            var L0_outer, var L1_outer, var L2_outer,
            var L0_inner, var L1_inner, var L2_inner) {
                c.gpu_tile(L0, L1, L2, sizeX, sizeY, sizeZ, L0_outer, L1_outer, L2_outer, L0_inner, L1_inner, L2_inner);
        })
        .def("get_automatically_allocated_buffer", [](computation &c) {
                return c.get_automatically_allocated_buffer();
        }, py::return_value_policy::reference)
        .def("interchange", [](computation &c,
            var L0, var L1) {
                c.interchange(L0, L1);
        })
        .def("interchange", [](computation &c,
            int L0, int L1) {
                c.interchange(L0, L1);
        })
        .def("mark_as_let_statement", [](computation &c) {
                c.mark_as_let_statement();
        })
        .def("mark_as_library_call", [](computation &c) {
                c.mark_as_library_call();
        })
        .def("parallelize", [](computation &c,
            var L) {
                c.parallelize(L);
        })
        .def("set_access", [](computation &c,
            std::string access_str) {
                c.set_access(access_str);
        })
        .def("set_access", [](computation &c,
            isl_map_t &access) {
                c.set_access(access.value);
        })
        .def("set_wait_access", [](computation &c,
            std::string access_str) {
                c.set_wait_access(access_str);
        })
        .def("set_wait_access", [](computation &c,
            isl_map_t &access) {
                c.set_wait_access(access.value);
        })
        .def("set_expression", [](computation &c,
            expr e) {
                c.set_expression(e);
        })
        .def("set_inline", [](computation &c,
            bool is_inline) {
                c.set_inline(is_inline);
        })
        .def("set_inline", [](computation &c) {
                c.set_inline(true);
        })
        .def("is_inline_computation", [](computation &c) {
                c.is_inline_computation();
        })
        .def("set_low_level_schedule", [](computation &c,
            isl_map_t map) {
                c.set_low_level_schedule(map.value);
        })
        .def("set_low_level_schedule", [](computation &c,
            std::string map_str) {
                c.set_low_level_schedule(map_str);
        })
        .def("shift", [](computation &c,
            var L0, int n) {
                c.shift(L0, n);
        })
        .def("skew", [](computation &c,
            var i, var j, int f, var ni, var nj) {
                c.skew(i, j, f, ni, nj);
        })
        .def("skew", [](computation &c,
            var i, var j, var k, int factor, var ni, var nj, var nk) {
                c.skew(i, j, k, factor, ni, nj, nk);
        })
        .def("skew", [](computation &c,
            var i, var j, var k, var l, int factor, var ni, var nj, var nk, var nl) {
                c.skew(i, j, k, l, factor, ni, nj, nk, nl);
        })
        .def("skew", [](computation &c,
            var i, var j, int f) {
                c.skew(i, j, f);
        })
        .def("skew", [](computation &c,
            var i, var j, var k, int f) {
                c.skew(i, j, k, f);
        })
        .def("skew", [](computation &c,
            var i, var j, var k, var l, int f) {
                c.skew(i, j, k, l, f);
        })
        .def("skew", [](computation &c,
            int i, int j, int f) {
                c.skew(i, j, f);
        })
        .def("skew", [](computation &c,
            int i, int j, int k, int f) {
                c.skew(i, j, k, f);
        })
        .def("skew", [](computation &c,
            int i, int j, int k, int l, int f) {
                c.skew(i, j, k, l, f);
        })
        .def("split", [](computation &c,
            var L0, int sizeX) {
                c.split(L0, sizeX);
        })
        .def("split", [](computation &c,
            var L0, int sizeX, var L0_outer, var L0_inner) {
                c.split(L0, sizeX, L0_outer, L0_inner);
        })
        .def("split", [](computation &c,
            int L0, int sizeX) {
                c.split(L0, sizeX);
        })
        .def("storage_fold", [](computation &c,
            var dim, int f) {
                c.storage_fold(dim, f);
        })
        .def("tag_gpu_level", [](computation &c,
            var L0, var L1) {
                c.tag_gpu_level(L0, L1);
        })
        .def("tag_gpu_level", [](computation &c,
            var L0, var L1, var L2, var L3) {
                c.tag_gpu_level(L0, L1, L2, L3);
        })
        .def("tag_gpu_level", [](computation &c,
            var L0, var L1, var L2, var L3, var L4, var L5) {
                c.tag_gpu_level(L0, L1, L2, L3, L4, L5);
        })
        .def("tag_parallel_level", [](computation &c,
            var L) {
                c.tag_parallel_level(L);
        })
        .def("tag_parallel_level", [](computation &c,
            int L) {
                c.tag_parallel_level(L);
        })
        .def("tag_vector_level", [](computation &c,
            var L,
            int len) {
                c.tag_vector_level(L, len);
        })
        .def("tag_vector_level", [](computation &c,
            int L,
            int len) {
                c.tag_vector_level(L, len);
        })
        .def("tag_distribute_level", [](computation &c,
            var L) {
                c.tag_distribute_level(L);
        })
        .def("tag_distribute_level", [](computation &c,
            int L) {
                c.tag_distribute_level(L);
        })
        .def("tag_unroll_level", [](computation &c,
            var L) {
                c.tag_unroll_level(L);
        })
        .def("tag_unroll_level", [](computation &c,
            int L) {
                c.tag_unroll_level(L);
        })
        .def("tag_unroll_level", [](computation &c,
            var L, int F) {
                c.tag_unroll_level(L, F);
        })
        .def("tag_unroll_level", [](computation &c,
            int L, int F) {
                c.tag_unroll_level(L, F);
        })
        .def("then", [](computation &c,
            computation & next, var L) {
                return c.then(next, L);
        })
        .def("then", [](computation &c,
            computation & next, int L) {
                return c.then(next, L);
        })
        .def("tile", [](computation &c,
            var L0, var L1,
            int sizeX, int sizeY) {
                c.tile(L0, L1, sizeX, sizeY);
        })
        .def("tile", [](computation &c,
            int L0, int L1,
            int sizeX, int sizeY) {
                c.tile(L0, L1, sizeX, sizeY);
        })
        .def("tile", [](computation &c,
            var L0, var L1,
            int sizeX, int sizeY,
            var L0_outer, var L1_outer,
            var L0_inner, var L1_inner) {
                c.tile(L0, L1, sizeX, sizeY, L0_outer, L1_outer, L0_inner, L1_inner);
        })
        .def("tile", [](computation &c,
            var L0, var L1, var L2,
            int sizeX, int sizeY, int sizeZ) {
                c.tile(L0, L1, L2, sizeX, sizeY, sizeZ);
        })
        .def("tile", [](computation &c,
            int L0, int L1, int L2,
            int sizeX, int sizeY, int sizeZ) {
                c.tile(L0, L1, L2, sizeX, sizeY, sizeZ);
        })
        .def("tile", [](computation &c,
            var L0, var L1, var L2,
            int sizeX, int sizeY, int sizeZ,
            var L0_outer, var L1_outer, var L2_outer,
            var L0_inner, var L1_inner, var L2_inner) {
                c.tile(L0, L1, L2, sizeX, sizeY, sizeZ, L0_outer, L1_outer, L2_outer, L0_inner, L1_inner, L2_inner);
        })
        .def("unroll", [](computation &c,
            var L, int fac) {
                c.unroll(L, fac);
        })
        .def("unroll", [](computation &c,
            var L, int fac, var L_outer, var L_inner) {
                c.unroll(L, fac, L_outer, L_inner);
        })
        .def("vectorize", [](computation &c,
            var L, int fac) {
                c.vectorize(L, fac);
        })
        .def("vectorize", [](computation &c,
            var L, int fac, var L_outer, var L_inner) {
                c.vectorize(L, fac, L_outer, L_inner);
        })
        .def("gen_communication", [](computation &c) { c.gen_communication(); })
        //.def("gen_communication", [](computation &c, var L) { c.gen_communication(L); })
        .def("__call__", [](computation &c, var i) {
            return c(i);
        })
        .def("__call__", [](computation &c, var i, var j) {
            return c(i, j);
        })
        .def("__call__", [](computation &c, var i, var j, var k) {
            return c(i, j, k);
        })
        .def("__call__", [](computation &c, var i, var j, var k, var l) {
            return c(i, j, k, l);
        })
        .def("__call__", [](input & i, std::vector<expr> & args) {
            return i(args);
        })
        .def("__call__", [](computation &c) { c(); })
        .def_static("create_xfer", [](computation &c,
            std::string send_iter_domain, std::string recv_iter_domain,
            expr send_dest, expr recv_src, xfer_prop send_prop, xfer_prop recv_prop,
            expr send_expr, std::shared_ptr<function> & fct) {
                return c.create_xfer(send_iter_domain, recv_iter_domain,
                    send_dest, recv_src, send_prop, recv_prop, send_expr, fct.get());
        })
        .def_static("create_xfer", [](computation &c,
            std::string iter_domain, xfer_prop prop, expr exper, std::shared_ptr<function> & fct) {
                return c.create_xfer(iter_domain, prop, exper, fct.get());
        });

    py::class_<generator>(m, "generator")
        .def(py::init<generator>());
        //.def_static("update_producer_expr_name", [](generator &g,
        //        std::shared_ptr<tiramisu::computation> comp, std::string name_to_replace, std::string replace_with) {
        //    g.update_producer_expr_name(comp.get(), name_to_replace, replace_with);
        //});

    py::class_<buffer>(m, "buffer")
        .def(py::init([](std::string s, std::vector<expr> & e, primitive_t t, argument_t a, std::shared_ptr<function> &f) {
             return buffer(s, e, t, a, f.get());
        }))
        .def(py::init<std::string, std::vector<expr> &, primitive_t, argument_t>())
        .def("allocate_at", [](buffer &b,
                computation & C,
                var level) {
            return b.allocate_at(C, level);
        }, py::return_value_policy::reference)
        .def("allocate_at", [](buffer &b,
                computation & C,
                int level) {
            return b.allocate_at(C, level);
        }, py::return_value_policy::reference)
        .def("dump", [](buffer &b,
                bool exhaustive) {
            b.dump(exhaustive);
        })
        .def("get_argument_type", [](buffer &b) {
            return b.get_argument_type();
        })
        .def("get_location", [](buffer &b) {
            return b.get_location();
        })
        .def("get_name", [](buffer &b) {
            return b.get_name();
        })
        .def("get_n_dims", [](buffer &b) {
            return b.get_n_dims();
        })
        .def("get_elements_type", [](buffer &b) {
            return b.get_elements_type();
        })
        .def("get_dim_sizes", [](buffer &b) {
            return b.get_dim_sizes();
        })
        .def("set_auto_allocate", [](buffer &b,
                bool auto_allocation) {
            return b.set_auto_allocate(auto_allocation);
        })
        .def("set_automatic_gpu_copy", [](buffer &b,
                bool auto_gpu_copy) {
            return b.set_automatic_gpu_copy(auto_gpu_copy);
        })
        .def("has_constant_extents", [](buffer &b) {
            return b.has_constant_extents();
        })
        .def("is_allocated", [](buffer &b) {
            return b.is_allocated();
        })
        .def("mark_as_allocated", [](buffer &b) {
            b.mark_as_allocated();
        })
        .def("tag_gpu_global", [](buffer &b) {
            b.tag_gpu_global();
        })
        .def("tag_gpu_register", [](buffer &b) {
            b.tag_gpu_register();
        })
        .def("tag_gpu_shared", [](buffer &b) {
            b.tag_gpu_shared();
        })
        .def("tag_gpu_local", [](buffer &b) {
            b.tag_gpu_local();
        })
        .def("tag_gpu_constant", [](buffer &b) {
            b.tag_gpu_constant();
        });

    py::class_<constant>(m, "constant", comp)
        .def(py::init([](std::string n, expr & e, primitive_t t, bool b, std::shared_ptr<computation> & c, int l, std::shared_ptr<function> & f) {
            return constant(n, e, t, b, c.get(), l, f.get());
        }))
        .def(py::init([](std::string n, expr & e, primitive_t p, bool b, std::shared_ptr<computation> & c, int l) {
            return constant(n, e, p, b, c.get(), l);
        }))
        .def(py::init([] (std::string n, expr & e, primitive_t p, std::shared_ptr<function> & f) {
            return constant(n, e, p, f.get());
        }))
        .def(py::init<std::string, expr &, primitive_t>())
        .def(py::init<std::string, expr &>())
        .def("get_computation_with_whom_this_is_computed", [](constant &c) {
                std::shared_ptr<tiramisu::computation> cptr{nullptr};
                cptr.reset(c.get_computation_with_whom_this_is_computed());
                return cptr;
        })
        .def("dump", [](constant &c,
            bool exhaustive) {
                c.dump(exhaustive);
        })
        .def("__call__", [](constant &c) {
                return c();
        });

    py::class_<tiramisu::input>(m, "input", func)
        .def(py::init<std::string, std::vector<var> &, primitive_t>())
        .def(py::init<std::vector<var> &, primitive_t>())
        .def(py::init<std::string, std::vector<std::string> &, std::vector<expr> &, primitive_t>())
/*
        .def(py::init( [](
            std::string name, std::vector<var> & iterator_variables, primitive_t t) {
            return input{name, iterator_variables, t};
        }))
        .def(py::init( [](
            std::vector<var> & iterator_variables, primitive_t t) {
            return input{iterator_variables, t};
        }))
        .def(py::init( [](
            std::string name, std::vector<std::string> & dimension_names, std::vector<expr> & dimension_sizes, primitive_t t) {
            return input{name, dimension_names, dimension_sizes, t};
        }))
*/
        .def("is_send", [](input &c) { return c.is_send(); })
        .def("is_recv", [](input &c) { return c.is_recv(); })
        .def("is_send_recv", [](input &c) { return c.is_send_recv(); })
        .def("is_wait", [](input &c) { return c.is_wait(); })
        .def("add_associated_let_stmt", [](input &c,
            std::string access_name,
            expr e) {
                return c.add_associated_let_stmt(access_name, e);
        })
        .def("unschedule_this_computation", [](input &c) {
                c.unschedule_this_computation();
        })
        .def("add_definitions", [](input &c,
            std::string iteration_domain_str,
            expr e,
            bool schedule_this_computation,
            primitive_t t,
            std::shared_ptr<function> & fct) {
                c.add_definitions(iteration_domain_str, e, schedule_this_computation, t, fct.get());
        })
        .def("add_predicate", [](input &c,
             expr e) {
                c.add_predicate(e);
        })
        .def("after", [](input &c,
             computation & comp,
             var iterator) {
                c.after(comp, iterator);
        })
        .def("after", [](input &c,
             computation & comp,
             int level) {
                c.after(comp, level);
        })
        .def("after", [](input &c,
             input & comp,
             var iterator) {
                c.after(comp, iterator);
        })
        .def("after", [](input &c,
             input & comp,
             int level) {
                c.after(comp, level);
        })
        .def("after_low_level", [](input &c,
             computation & comp,
             int level) {
                c.after_low_level(comp, level);
        })
        .def("after_low_level", [](input &c,
             input & comp,
             int level) {
                c.after_low_level(comp, level);
        })
/*        .def("after_low_level", [](computation &c,
             computation & comp,
             std::vector<int> levels) {
                c.after_low_level(comp, levels);
        })
*/
        .def("allocate_and_map_buffer_automatically", [](input &c,
             argument_t type) {
                c.allocate_and_map_buffer_automatically(type);
        })
        .def("apply_transformation_on_schedule", [](input &c,
            std::string map_str) {
                c.apply_transformation_on_schedule(map_str);
        })
        .def("before", [](input &c,
            computation &consumer,
            var L) {
                c.before(consumer, L);
        })
        .def("between", [](input &c,
            computation &before_comp,
            var before_l,
            computation &after_comp,
            var after_l) {
                c.between(before_comp, before_l, after_comp, after_l);
        })
        .def("between", [](input &c,
            computation &before_comp,
            int before_l,
            computation &after_comp,
            int after_l) {
                c.between(before_comp, before_l, after_comp, after_l);
        })
        .def("store_in", [](input &c,
            buffer & buff) {
                c.store_in( std::addressof(buff) );
        })
        .def("store_in", [](input &c,
            buffer & buff,
            std::vector<expr> iterators) {
                c.store_in( std::addressof(buff), iterators);
        })
        .def("store_in", [](input &c,
            std::vector<expr> mapping,
            std::vector<expr> sizes) {
                c.store_in(mapping, sizes);
        })
        .def("cache_shared", [](input &c,
            computation & inp,
            const var & level,
            std::vector<int> buffer_shape,
            std::vector<expr> copy_offsets,
            bool pad_buffer) {
                return c.cache_shared(inp, level, buffer_shape, copy_offsets, pad_buffer);
        }, py::return_value_policy::reference)
        .def("cache_shared", [](input &c,
            computation & inp,
            const var & level,
            std::vector<int> buffer_shape,
            std::vector<expr> copy_offsets) {
                return c.cache_shared(inp, level, buffer_shape, copy_offsets);
        }, py::return_value_policy::reference)
        .def("compute_at", [](input &c,
            computation &consumer,
            var L) {
                c.compute_at(consumer, L);
        })
        .def("compute_at", [](input &c,
            computation &consumer,
            int L) {
                c.compute_at(consumer, L);
        })
        .def("compute_maximal_AST_depth", [](input &c) {
            return c.compute_maximal_AST_depth();
        })
        .def("dump_iteration_domain", [](input &c) {
            c.dump_iteration_domain();
        })
        .def("dump_schedule", [](input &c) {
            c.dump_schedule();
        })
        .def("dump", [](input &c) {
            c.dump();
        })
        .def("fuse_after", [](input &c,
            var lev, computation & comp) {
            c.after(comp, lev);
        })
        .def("gen_time_space_domain", [](input &c) {
            c.gen_time_space_domain();
        })
        .def("drop_rank_iter", [](input &c,
            var level) {
                c.drop_rank_iter(level);
        })
        .def("get_buffer", [](input &c) {
                return c.get_buffer();
        }, py::return_value_policy::reference)
        .def("get_data_type", [](input &c) {
                return c.get_data_type();
        })
        .def("get_expr", [](input &c) {
                return c.get_expr();
        })
        .def("get_iteration_domain", [](input &c) {
                isl_set_t ret{};
                ret.value = c.get_iteration_domain();
                return ret;
        })
        .def("get_last_update", [](input &c) {
                return c.get_last_update();
        })
        .def("get_loop_level_number_from_dimension_name", [](input &c,
            std::string dim_name) {
                return c.get_loop_level_number_from_dimension_name(dim_name);
        })
        .def("get_name", [](input &c) {
                return c.get_name();
        })
        .def("get_predecessor", [](input &c) {
                return c.get_predecessor();
        }, py::return_value_policy::reference)
        .def("get_successor", [](input &c) {
                return c.get_successor();
        }, py::return_value_policy::reference)
        .def("get_update", [](input &c, int index) {
                return c.get_update(index);
        })
        .def("get_schedule", [](input &c) {
                isl_map_t ret{};
                ret.value = c.get_schedule();
                return ret;
        })
        .def("gpu_tile", [](input &c,
            var L0, var L1,
            int sizeX, int sizeY) {
                c.gpu_tile(L0, L1, sizeX, sizeY);
        })
        .def("gpu_tile", [](input &c,
            var L0, var L1,
            int sizeX, int sizeY,
            var L0_outer, var L1_outer,
            var L0_inner, var L1_inner) {
                c.gpu_tile(L0, L1, sizeX, sizeY, L0_outer, L1_outer, L0_inner, L1_inner);
        })
        .def("gpu_tile", [](input &c,
            var L0, var L1, var L2,
            int sizeX, int sizeY, int sizeZ) {
                c.gpu_tile(L0, L1, L2, sizeX, sizeY, sizeZ);
        })
        .def("gpu_tile", [](input &c,
            var L0, var L1, var L2,
            int sizeX, int sizeY, int sizeZ,
            var L0_outer, var L1_outer, var L2_outer,
            var L0_inner, var L1_inner, var L2_inner) {
                c.gpu_tile(L0, L1, L2, sizeX, sizeY, sizeZ, L0_outer, L1_outer, L2_outer, L0_inner, L1_inner, L2_inner);
        })
        .def("get_automatically_allocated_buffer", [](input &c) {
                return c.get_automatically_allocated_buffer();
        }, py::return_value_policy::reference)
        .def("interchange", [](input &c,
            var L0, var L1) {
                c.interchange(L0, L1);
        })
        .def("interchange", [](input &c,
            int L0, int L1) {
                c.interchange(L0, L1);
        })
        .def("mark_as_let_statement", [](input &c) {
                c.mark_as_let_statement();
        })
        .def("mark_as_library_call", [](input &c) {
                c.mark_as_library_call();
        })
        .def("parallelize", [](input &c,
            var L) {
                c.parallelize(L);
        })
        .def("set_access", [](input &c,
            std::string access_str) {
                c.set_access(access_str);
        })
        .def("set_access", [](input &c,
            isl_map_t &access) {
                c.set_access(access.value);
        })
        .def("set_wait_access", [](input &c,
            std::string access_str) {
                c.set_wait_access(access_str);
        })
        .def("set_wait_access", [](input &c,
            isl_map_t &access) {
                c.set_wait_access(access.value);
        })
        .def("set_expression", [](input &c,
            expr e) {
                c.set_expression(e);
        })
        .def("set_inline", [](input &c,
            bool is_inline) {
                c.set_inline(is_inline);
        })
        .def("set_inline", [](input &c) {
                c.set_inline(true);
        })
        .def("is_inline_computation", [](input &c) {
                c.is_inline_computation();
        })
        .def("set_low_level_schedule", [](input &c,
            isl_map_t map) {
                c.set_low_level_schedule(map.value);
        })
        .def("set_low_level_schedule", [](input &c,
            std::string map_str) {
                c.set_low_level_schedule(map_str);
        })
        .def("shift", [](input &c,
            var L0, int n) {
                c.shift(L0, n);
        })
        .def("skew", [](input &c,
            var i, var j, int f, var ni, var nj) {
                c.skew(i, j, f, ni, nj);
        })
        .def("skew", [](input &c,
            var i, var j, var k, int factor, var ni, var nj, var nk) {
                c.skew(i, j, k, factor, ni, nj, nk);
        })
        .def("skew", [](input &c,
            var i, var j, var k, var l, int factor, var ni, var nj, var nk, var nl) {
                c.skew(i, j, k, l, factor, ni, nj, nk, nl);
        })
        .def("skew", [](input &c,
            var i, var j, int f) {
                c.skew(i, j, f);
        })
        .def("skew", [](input &c,
            var i, var j, var k, int f) {
                c.skew(i, j, k, f);
        })
        .def("skew", [](input &c,
            var i, var j, var k, var l, int f) {
                c.skew(i, j, k, l, f);
        })
        .def("skew", [](input &c,
            int i, int j, int f) {
                c.skew(i, j, f);
        })
        .def("skew", [](input &c,
            int i, int j, int k, int f) {
                c.skew(i, j, k, f);
        })
        .def("skew", [](input &c,
            int i, int j, int k, int l, int f) {
                c.skew(i, j, k, l, f);
        })
        .def("split", [](input &c,
            var L0, int sizeX) {
                c.split(L0, sizeX);
        })
        .def("split", [](input &c,
            var L0, int sizeX, var L0_outer, var L0_inner) {
                c.split(L0, sizeX, L0_outer, L0_inner);
        })
        .def("split", [](input &c,
            int L0, int sizeX) {
                c.split(L0, sizeX);
        })
        .def("storage_fold", [](input &c,
            var dim, int f) {
                c.storage_fold(dim, f);
        })
        .def("tag_gpu_level", [](input &c,
            var L0, var L1) {
                c.tag_gpu_level(L0, L1);
        })
        .def("tag_gpu_level", [](input &c,
            var L0, var L1, var L2, var L3) {
                c.tag_gpu_level(L0, L1, L2, L3);
        })
        .def("tag_gpu_level", [](input &c,
            var L0, var L1, var L2, var L3, var L4, var L5) {
                c.tag_gpu_level(L0, L1, L2, L3, L4, L5);
        })
        .def("tag_parallel_level", [](input &c,
            var L) {
                c.tag_parallel_level(L);
        })
        .def("tag_parallel_level", [](input &c,
            int L) {
                c.tag_parallel_level(L);
        })
        .def("tag_vector_level", [](input &c,
            var L,
            int len) {
                c.tag_vector_level(L, len);
        })
        .def("tag_vector_level", [](input &c,
            int L,
            int len) {
                c.tag_vector_level(L, len);
        })
        .def("tag_distribute_level", [](input &c,
            var L) {
                c.tag_distribute_level(L);
        })
        .def("tag_distribute_level", [](input &c,
            int L) {
                c.tag_distribute_level(L);
        })
        .def("tag_unroll_level", [](input &c,
            var L) {
                c.tag_unroll_level(L);
        })
        .def("tag_unroll_level", [](input &c,
            int L) {
                c.tag_unroll_level(L);
        })
        .def("tag_unroll_level", [](input &c,
            var L, int F) {
                c.tag_unroll_level(L, F);
        })
        .def("tag_unroll_level", [](input &c,
            int L, int F) {
                c.tag_unroll_level(L, F);
        })
        .def("then", [](input &c,
            computation & next, var L) {
                return c.then(next, L);
        })
        .def("then", [](input &c,
            computation & next, int L) {
                return c.then(next, L);
        })
        .def("tile", [](input &c,
            var L0, var L1,
            int sizeX, int sizeY) {
                c.tile(L0, L1, sizeX, sizeY);
        })
        .def("tile", [](input &c,
            int L0, int L1,
            int sizeX, int sizeY) {
                c.tile(L0, L1, sizeX, sizeY);
        })
        .def("tile", [](input &c,
            var L0, var L1,
            int sizeX, int sizeY,
            var L0_outer, var L1_outer,
            var L0_inner, var L1_inner) {
                c.tile(L0, L1, sizeX, sizeY, L0_outer, L1_outer, L0_inner, L1_inner);
        })
        .def("tile", [](input &c,
            var L0, var L1, var L2,
            int sizeX, int sizeY, int sizeZ) {
                c.tile(L0, L1, L2, sizeX, sizeY, sizeZ);
        })
        .def("tile", [](input &c,
            int L0, int L1, int L2,
            int sizeX, int sizeY, int sizeZ) {
                c.tile(L0, L1, L2, sizeX, sizeY, sizeZ);
        })
        .def("tile", [](input &c,
            var L0, var L1, var L2,
            int sizeX, int sizeY, int sizeZ,
            var L0_outer, var L1_outer, var L2_outer,
            var L0_inner, var L1_inner, var L2_inner) {
                c.tile(L0, L1, L2, sizeX, sizeY, sizeZ, L0_outer, L1_outer, L2_outer, L0_inner, L1_inner, L2_inner);
        })
        .def("unroll", [](input &c,
            var L, int fac) {
                c.unroll(L, fac);
        })
        .def("unroll", [](input &c,
            var L, int fac, var L_outer, var L_inner) {
                c.unroll(L, fac, L_outer, L_inner);
        })
        .def("vectorize", [](input &c,
            var L, int fac) {
                c.vectorize(L, fac);
        })
        .def("vectorize", [](input &c,
            var L, int fac, var L_outer, var L_inner) {
                c.vectorize(L, fac, L_outer, L_inner);
        })
        .def("gen_communication", [](input &c) { c.gen_communication(); })
        //.def("gen_communication", [](computation &c, var L) { c.gen_communication(L); })
        .def_static("create_xfer", [](input &c,
            std::string send_iter_domain, std::string recv_iter_domain,
            expr send_dest, expr recv_src, xfer_prop send_prop, xfer_prop recv_prop,
            expr send_expr, std::shared_ptr<function> & fct) {
                return c.create_xfer(send_iter_domain, recv_iter_domain,
                    send_dest, recv_src, send_prop, recv_prop, send_expr, fct.get());
        })
        .def_static("create_xfer", [](input &c,
            std::string iter_domain, xfer_prop prop, expr exper, std::shared_ptr<function> & fct) {
                return c.create_xfer(iter_domain, prop, exper, fct.get());
        })
        .def("__call__", [](input &c, var i) {
            return c(i);
        })
        .def("__call__", [](input &c, var i, var j) {
            return c(i, j);
        })
        .def("__call__", [](input &c, var i, var j, var k) {
            return c(i, j, k);
        })
        .def("__call__", [](input &c, var i, var j, var k, var l) {
            return c(i, j, k, l);
        })
        .def("__call__", [](input & i, std::vector<expr> & args) {
            return i(args);
        })
        .def("__call__", [](input &c) { c(); });

    py::class_<Input>(m, "Input")
        .def(py::init<std::string, std::vector<expr> &, primitive_t>())
        .def("iterators_from_size_expressions", [](Input &i,
            std::vector<expr> & sizes) {
            return i.iterators_from_size_expressions(sizes);
        });


    py::class_<isl_set_t>(m, "isl_set")
        .def(py::init<>());

    py::class_<isl_map_t>(m, "isl_map")
        .def(py::init<>());

    // init
    //
    m.def("init", []() { tiramisu::init(); });
    m.def("init", [](std::string name) { tiramisu::init(name); });
    m.def("cast", [](primitive_t tT, expr & e) {
        return cast(tT, e);
    });

    m.def("cast", [](primitive_t tT, var & v) {
        return cast(tT, v);
    });

    m.def("value_cast", [](primitive_t tT, std::int8_t val) {
        return value_cast<std::int8_t>(tT, val);
    });

    m.def("value_cast", [](primitive_t tT, std::uint8_t val) {
        return value_cast<std::uint8_t>(tT, val);
    });

    m.def("value_cast", [](primitive_t tT, std::int16_t val) {
        return value_cast<std::int16_t>(tT, val);
    });

    m.def("value_cast", [](primitive_t tT, std::uint16_t val) {
        return value_cast<std::uint16_t>(tT, val);
    });

    m.def("value_cast", [](primitive_t tT, std::int32_t val) {
        return value_cast<std::int32_t>(tT, val);
    });

    m.def("value_cast", [](primitive_t tT, std::uint32_t val) {
        return value_cast<std::uint32_t>(tT, val);
    });

    m.def("value_cast", [](primitive_t tT, std::int64_t val) {
        return value_cast<std::int64_t>(tT, val);
    });

    m.def("value_cast", [](primitive_t tT, std::uint64_t val) {
        return value_cast<std::uint64_t>(tT, val);
    });

    m.def("value_cast", [](primitive_t tT, float val) {
        return value_cast<float>(tT, val);
    });

    m.def("value_cast", [](primitive_t tT, double val) {
        return value_cast<double>(tT, val);
    });

    // codegen
    //
    m.def("codegen", [](std::vector< buffer > &arguments, std::string obj_filename) {
       std::vector<buffer *> bufs;
       bufs.reserve(arguments.size());
       std::transform(arguments.begin(), arguments.end(), bufs.begin(), [](auto arg) { return std::move(&arg); });
       tiramisu::codegen(bufs, obj_filename, false);
    });

    m.def("codegen", [](std::vector< buffer > &arguments, std::string obj_filename, bool gen_cuda_stmt) {
       std::vector<buffer *> bufs;
       bufs.reserve(arguments.size());
       std::transform(arguments.begin(), arguments.end(), bufs.begin(), [](auto arg) { return std::move(&arg); });
       tiramisu::codegen(bufs, obj_filename, gen_cuda_stmt);
    });

} // end pyisl module
