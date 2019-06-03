#include <tiramisu/tiramisu.h>
#include "configure.h"

using namespace tiramisu;

int main()
{
    init("densenet_block");

    // -------------------------------------------------------
    // Layer I
    // -------------------------------------------------------

    // Input tensor parameters
    // N : parameters[0] (width and height of the input tensor)
    // BATCH_SIZE : parameters[1]
    var i("i", 0, 2);
    input parameters("parameters", {i}, p_int32);

    constant C_N("C_N", parameters(0));
    constant C_BATCH_SIZE("C_BATCH_SIZE", parameters(1));
    constant C_NB_ELEMENTS("C_NB_ELEMENTS", parameters(0) * parameters(0) * parameters(1));

    // Input tensor computation
    // n : batch dimension
    // z : features dimension
    // y : height dimension
    // x : width dimension
    var x("x", 0, C_N), y("y", 0, C_N), z("z", 0, expr(4*GR)), n("n", 0, C_BATCH_SIZE);
    input c_input("c_input", {n, z, y, x}, p_float64);

    // Batch normalization computation
    input bn_scale("bn_scale", {z}, p_float64);
    input bn_shift("bn_shift", {z}, p_float64);

    // Compute the sum over the features dimension (z)
	computation input_sum_init("input_sum_init", {z}, cast(p_float64, expr(0)));
    computation input_sum("input_sum", {n, z, y, x}, input_sum_init(z) + c_input(n, z, y, x));

    // Compute the sum of squares over the features dimension (z)
	computation input_sum_squares_init("input_sum_squares_init", {z}, cast(p_float64, expr(0)));
    computation input_sum_squares("input_sum_squares", {n, z, y, x}, input_sum_squares_init(z) + c_input(n, z, y, x) * c_input(n, z, y, x));

    computation input_mean("input_mean", {z}, input_sum(C_BATCH_SIZE - 1, z, C_N - 1, C_N - 1) / cast(p_float64, C_NB_ELEMENTS));
    computation input_sd("input_sd", {z}, expr(o_sqrt, input_sum_squares(C_BATCH_SIZE - 1, z, C_N - 1, C_N - 1) / cast(p_float64, C_NB_ELEMENTS) - input_mean(z) * input_mean(z) + EPSILON));
    computation bn("bn", {n, z, y, x}, bn_scale(z) * ((c_input(n, z, y, x) - input_mean(z)) / input_sd(z)) + bn_shift(z));

    // ReLU computation
    computation relu("relu", {n, z, y, x}, expr(o_max, cast(p_float64, 0), bn(n, z, y, x)));

    // Apply padding to ReLU
    var y_pad("y_pad", 0, C_N + 2), x_pad("x_pad", 0, C_N + 2);
    computation relu_padded("relu_padded", {n, z, y_pad, x_pad}, p_float64, false);

    var pad_iter("pad_iter", 0, C_N + 2);
    computation pad_top("pad_top", {n, z, pad_iter}, cast(p_float64, expr(0)));
    computation pad_bottom("pad_bottom", {n, z, pad_iter}, cast(p_float64, expr(0)));
    computation pad_left("pad_left", {n, z, pad_iter}, cast(p_float64, expr(0)));
    computation pad_right("pad_right", {n, z, pad_iter}, cast(p_float64, expr(0)));

    // Convolution computation
    var k_x("k_x", 0, K_X), k_y("k_y", 0, K_Y), fin("fin", 0, 4*GR), fout("fout", 0, GR);
    input conv_filter("conv_filter", {fout, fin, k_y, k_x}, p_float64);
    input conv_bias("conv_bias", {fout}, p_float64);

    var fh("fh", 0, K_Y), fw("fw", 0, K_X), fk("fk", 0, 4*GR);
    computation init_output("init_output", {n, fout, y, x}, cast(p_float64, expr(0)));
    computation apply_filter("apply_filter", {n, fout, y, x, fh, fw, fk}, init_output(n, fout, y, x) + relu_padded(n, fk, y + fh, x + fw)*conv_filter(fout, fk, fh, fw));

    computation conv("conv", {n, fout, y, x}, conv_bias(fout) + apply_filter(n, fout, y, x, K_Y - 1, K_X - 1, 4*GR - 1));

    // -------------------------------------------------------
    // Layer II
    // -------------------------------------------------------
    input_sum_squares_init.after(input_sum_init, z);
    
 	input_sum.after(input_sum_squares_init, computation::root);
    input_sum_squares.after(input_sum, x);

    input_mean.after(input_sum, computation::root);
    input_sd.after(input_mean, z);

    bn.after(input_sd, computation::root);
    relu.after(bn, x);

    pad_top.after(relu, computation::root);
    pad_bottom.after(pad_top, pad_iter);
    pad_left.after(pad_bottom, pad_iter);
    pad_right.after(pad_left, pad_iter);

    init_output.after(pad_right, n);
    apply_filter.after(init_output, x);
    conv.after(apply_filter, x);

    // -------------------------------------------------------
    // Layer III
    // -------------------------------------------------------
    buffer params_buf("params_buf", {2}, p_int32, a_input);
    buffer input_buf("input_buf", {C_BATCH_SIZE, 4*GR, C_N, C_N}, p_float64, a_input);
    buffer bn_scale_buf("bn_scale_buf", {4*GR}, p_float64, a_input);
    buffer bn_shift_buf("bn_shift_buf", {4*GR}, p_float64, a_input);
    buffer conv_filter_buf("conv_filter_buf", {GR, 4*GR, K_Y, K_X}, p_float64, a_input);
    buffer conv_bias_buf("conv_bias_buf", {GR}, p_float64, a_input);

    buffer input_mean_buf("input_mean_buf", {4*GR}, p_float64, a_temporary);
    buffer input_sd_buf("input_sd_buf", {4*GR}, p_float64, a_temporary);

    // We use this buffer as a temporary buffer to store BN and ReLU results
    // This buffer is padded (its width and height are C_N + 2) so as to prepare it for convolution
    buffer workspace_buf("workspace_buf", {C_BATCH_SIZE, 4*GR, C_N + 2, C_N + 2}, p_float64, a_temporary);

    buffer output_buf("output_buf", {C_BATCH_SIZE, GR, C_N, C_N}, p_float64, a_output);

    parameters.store_in(&params_buf);
    c_input.store_in(&input_buf);
    bn_scale.store_in(&bn_scale_buf);
    bn_shift.store_in(&bn_shift_buf);
    conv_filter.store_in(&conv_filter_buf);
    conv_bias.store_in(&conv_bias_buf);

	input_sum_init.store_in(&input_mean_buf);
    input_sum.store_in(&input_mean_buf, {z});
    input_mean.store_in(&input_mean_buf);

	input_sum_squares_init.store_in(&input_sd_buf);
    input_sum_squares.store_in(&input_sd_buf, {z});
    input_sd.store_in(&input_sd_buf);

    // We shift the BN and ReLU computations, so as to avoid to
    // compute on the padding region of workspace_buf.
    bn.store_in(&workspace_buf, {n, z, y + 1, x + 1});
    relu.store_in(&workspace_buf, {n, z, y + 1, x + 1});
    relu_padded.store_in(&workspace_buf);

    pad_top.store_in(&workspace_buf, {n, z, pad_iter, 0});
    pad_bottom.store_in(&workspace_buf, {n, z, pad_iter, C_N + 1});
    pad_left.store_in(&workspace_buf, {n, z, 0, pad_iter});
    pad_right.store_in(&workspace_buf, {n, z, C_N + 1, pad_iter});

    init_output.store_in(&output_buf);
    apply_filter.store_in(&output_buf, {n, fout, y, x});
    conv.store_in(&output_buf);

    // -------------------------------------------------------
    // Code Generation
    // -------------------------------------------------------
    codegen({&params_buf, &input_buf, &bn_scale_buf, &bn_shift_buf, &conv_filter_buf, &conv_bias_buf, &output_buf}, "densenet_block_tiramisu.o");

    return 0;
}