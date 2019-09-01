#include <tiramisu/tiramisu.h>
#include <tiramisu/utils.h>
#include "configure.h"

using namespace tiramisu;

Halide::Buffer<float> Hfilter(FOUT_BLOCKING, FIN_BLOCKING, K, K, FIN_NB_BLOCKS, FOUT_NB_BLOCKS);

void gen_random_dense_weights()
{
	srand(1);

	Halide::Buffer<float> Hinput(FIN_BLOCKING, N + 2, N + 2, FIN_NB_BLOCKS, BATCH_SIZE);

	// Initialize buffers
	for (int n = 0; n < BATCH_SIZE; ++n)
		for (int fin = 0; fin < FIn; ++fin)
			for (int y = 0; y < N + 2; ++y)
				for (int x = 0; x < N + 2; ++x)
					Hinput(fin%FIN_BLOCKING, x, y, fin/FIN_BLOCKING, n) = ((float) (rand()%256 - 128)) / 127.f;

	for (int fout = 0; fout < FOut; ++fout)
		for (int fin = 0; fin < FIn; ++fin)
			for (int k_y = 0; k_y < K; ++k_y)
				for (int k_x = 0; k_x < K; ++k_x)
					Hfilter(fout%FOUT_BLOCKING, fin%FIN_BLOCKING, k_x, k_y, fin/FIN_BLOCKING, fout/FOUT_BLOCKING) = ((float)(rand()%256 - 128)) / 127.f;
}

int main(int argc, char **argv)
{
    init("conv_tiramisu");

    gen_random_dense_weights();

    // -------------------------------------------------------
    // Layer I
    // -------------------------------------------------------
    var x("x", 0, N), y("y", 0, N), n("n", 0, BATCH_SIZE);
    var k_x("k_x", 0, K), k_y("k_y", 0, K);

    var fin_b("fin_b", 0, FIN_NB_BLOCKS), ffin("ffin", 0, FIN_BLOCKING);
    var fout_b("fout_b", 0, FOUT_NB_BLOCKS), ffout("ffout", 0, FOUT_BLOCKING);
    
    var x_pad("x_pad", 0, N + 2), y_pad("y_pad", 0, N + 2);

    input c_input("c_input", {n, fin_b, y_pad, x_pad, ffin}, p_float32);
    input filter("filter", {fout_b, fin_b, k_y, k_x, ffin, ffout}, p_float32);
    input bias("bias", {fout_b, ffout}, p_float32);

    computation conv_init("conv_init", {n, y, fout_b, x, ffout}, bias(fout_b, ffout));
    view conv_out("conv_out", {n, y, fout_b, x, ffout}, p_float32);
    
    // x_bound is used to have the width dimension divisible by X_BLOCKING
    // in the conv computation.
    var x_bound("x_bound", 0, X_BOUND);
    var x_conclude("x_conclude", X_BOUND, N);

    // Compute convolution from 0 to x_bound
    /*computation conv(
        "conv",
        {n, y, fout_b, fin_b, x_bound, k_y, k_x, ffin, ffout},
        conv_out(n, y, fout_b, x_bound, ffout) + filter(fout_b, fin_b, k_y, k_x, ffin, ffout) * c_input(n, fin_b, y + k_y, x_bound + k_x, ffin)
    );*/

    // Compute unrolled convolution from 0 to x_bound
    computation *unrolled_conv[FOut];
    for (int f0 = 0; f0 < FOUT_NB_BLOCKS; f0++)
      for (int f1 = 0; f1 < FOUT_BLOCKING; f1++)
	{
	    expr e = expr((float) 0);

	    for (int in0 = 0; in0 < FIN_NB_BLOCKS; in0++)
		for (int in1 = 0; in1 < FIN_BLOCKING; in1++)
		    for (int ky = 0; ky < K; ky++)
			for (int kx = 0; kx < K; kx++)
			    if (Hfilter(f1, in1, kx, ky, in0, f0) != 0)
				e = e + expr((float) Hfilter(f1, in1, kx, ky, in0, f0)) * c_input(n, in0, y + ky, x_bound + kx, in1);

	    computation *conv =
			new computation(
			    "conv_" + std::to_string(f0) + "_" + std::to_string(f1),
			    {n, y, x_bound},
				conv_out(n, y, f0, x_bound, f1) + e);

	    conv->get_buffer()->set_auto_allocate(false);
	    unrolled_conv[f0*FOUT_BLOCKING+f1] = conv;
	}

    // Compute convolution from x_bound to N
    computation conv_conclude(
        "conv_conclude",
        {n, y, fout_b, fin_b, k_y, k_x, ffin, ffout, x_conclude},
        conv_out(n, y, fout_b, x_conclude, ffout) + filter(fout_b, fin_b, k_y, k_x, ffin, ffout) * c_input(n, fin_b, y + k_y, x_conclude + k_x, ffin)
    );

    // -------------------------------------------------------
    // Layer II
    // -------------------------------------------------------

    // schedule for conv computation

    // We introduce those two computations to do register blocking
    computation reg_load(
        "reg_load",
        {n, y, fout_b, x_bound, ffout},
        conv_init(n, y, fout_b, x_bound, ffout)
    );

    view conv_orig("conv_orig", {n, y, fout_b, x_bound, ffout}, p_float32);

    computation reg_store(
        "reg_store",
        {n, y, fout_b, x_bound, ffout},
        conv_orig(n, y, fout_b, x_bound, ffout)
    );

    // Split over dimension x
    var x_b, xx;
    reg_load.split(x_bound, X_BLOCKING, x_b, xx);
    for (int f = 0; f < FOut; f++)
	unrolled_conv[f]->split(x_bound, X_BLOCKING, x_b, xx);
    reg_store.split(x_bound, X_BLOCKING, x_b, xx);

    // Interchange
    reg_load.interchange(xx, ffout);
/*    for (int f = 0; f < FOut; f++)
	for (int in = 0; in < FIN_NB_BLOCKS; in++)
	    unrolled_conv[f][in]->interchange(xx, ffout);*/
    reg_store.interchange(xx, ffout);

    // Split over dimension fout_b
    var fout_b_up, fout_b_low;
    
    if (FOUT_B_SPLIT_FACTOR*FOUT_BLOCKING != FOut) {
	exit(0);
//        conv_init.split(fout_b, FOUT_B_SPLIT_FACTOR, fout_b_up, fout_b_low);
//        conv.split(fout_b, FOUT_B_SPLIT_FACTOR, fout_b_up, fout_b_low);
        
        reg_load.split(fout_b, FOUT_B_SPLIT_FACTOR, fout_b_up, fout_b_low);
        reg_store.split(fout_b, FOUT_B_SPLIT_FACTOR, fout_b_up, fout_b_low);
    }
    else {
        fout_b_up = y;
        fout_b_low = fout_b;
    }

//    reg_load.interchange(fout_b_low, fin_b);
    reg_load.interchange(fout_b_low, x_b);
    for (int f = 0; f < FOut; f++)
	{
//	    unrolled_conv[f][in]->interchange(fout_b_low, fin_b);
//	    unrolled_conv[f][in]->interchange(fout_b_low, x_b);
	}
//    reg_store.interchange(fout_b_low, fin_b);
    reg_store.interchange(fout_b_low, x_b);

    // Vectorize and unroll
    for (int f = 0; f < FOut; f++)
	{
	    unrolled_conv[f]->tag_vector_level(3, 3);
//	    unrolled_conv[f]->tag_unroll_level(4); //xx);
//	    unrolled_conv[f][in]->tag_vector_level(5, FOUT_BLOCKING); //ffout, FOUT_BLOCKING);
//	    unrolled_conv[f][in]->tag_unroll_level(6); //fout_b_low);
	}
    reg_load.vectorize(ffout, FOUT_BLOCKING);
    reg_store.vectorize(ffout, FOUT_BLOCKING);
    reg_load.tag_unroll_level(xx);
    reg_load.tag_unroll_level(fout_b_low);
    reg_store.tag_unroll_level(xx);
    reg_store.tag_unroll_level(fout_b_low);

    // schedule for conv_conclude
    // This schedule is the same as conv computation
    computation reg_load_conclude(
        "reg_load_conclude",
        {n, y, fout_b, fin_b, ffout, x_conclude},
        conv_init(n, y, fout_b, x_conclude, ffout)
    );

    computation reg_store_conclude(
        "reg_store_conclude",
        {n, y, fout_b, fin_b, ffout, x_conclude},
        conv_conclude(n, y, fout_b, fin_b, 0, 0, 0, ffout, x_conclude)
    );
    
    // Split over dimension fout_b
    if (FOUT_B_SPLIT_FACTOR*FOUT_BLOCKING != FOut) {
        conv_conclude.split(fout_b, FOUT_B_SPLIT_FACTOR, fout_b_up, fout_b_low);
        reg_load_conclude.split(fout_b, FOUT_B_SPLIT_FACTOR, fout_b_up, fout_b_low);
        reg_store_conclude.split(fout_b, FOUT_B_SPLIT_FACTOR, fout_b_up, fout_b_low);
    }
    else {
        fout_b_up = y;
        fout_b_low = fout_b;
    }

    conv_conclude.interchange(fout_b_low, fin_b);
    conv_conclude.interchange(fout_b_low, k_y);
    conv_conclude.interchange(fout_b_low, k_x);
    conv_conclude.interchange(fout_b_low, ffin);

    reg_load_conclude.interchange(fout_b_low, fin_b);
    reg_store_conclude.interchange(fout_b_low, fin_b);

    // Vectorize and unroll
    reg_load_conclude.vectorize(ffout, FOUT_BLOCKING);
    conv_conclude.vectorize(ffout, FOUT_BLOCKING);
    reg_store_conclude.vectorize(ffout, FOUT_BLOCKING);

    conv_conclude.tag_unroll_level(x_conclude);
    conv_conclude.tag_unroll_level(fout_b_low);

    reg_load_conclude.tag_unroll_level(x_conclude);
    reg_load_conclude.tag_unroll_level(fout_b_low);

    reg_store_conclude.tag_unroll_level(x_conclude);
    reg_store_conclude.tag_unroll_level(fout_b_low);

    // Parallelize and order
//    unrolled_conv[0][0]->tag_parallel_level(y);
//    unrolled_conv[0][0]->tag_parallel_level(n);

    conv_init.then(reg_load, fout_b_up);

    computation *previous_comp = &reg_load;

    for (int f = 0; f < FOut; f++)
	{
	    previous_comp->then(*unrolled_conv[f], 2);//4);
	    previous_comp = unrolled_conv[f];
	}

    previous_comp
	    ->then(reg_store, 2) //x_b)
             .then(reg_load_conclude, 1) //fout_b_up)
             .then(conv_conclude, fin_b)
             .then(reg_store_conclude, fin_b);

    // -------------------------------------------------------
    // Layer III
    // -------------------------------------------------------
    buffer conv_buf("conv_buf", {BATCH_SIZE, FOUT_NB_BLOCKS, N, N, FOUT_BLOCKING}, p_float32, a_output);
    
    // This is where intermediate results of convolution will be stored.
    // We rely on the compiler to detect that this buffer can be mapped to CPU registers.
    buffer reg_buf("reg_buf", {FOUT_B_SPLIT_FACTOR, X_BLOCKING, FOUT_BLOCKING}, p_float32, a_temporary);

    conv_init.store_in(&conv_buf, {n, fout_b, y, x, ffout});
    conv_out.store_in(&reg_buf, {fout_b%FOUT_B_SPLIT_FACTOR, x%X_BLOCKING, ffout});

    reg_load.store_in(&reg_buf, {fout_b%FOUT_B_SPLIT_FACTOR, x_bound%X_BLOCKING, ffout});
    //for (int f = 0; f < FOut; f++)
//	    unrolled_conv[f][in]->store_in(&reg_buf, {fout_b%FOUT_B_SPLIT_FACTOR, x_bound%X_BLOCKING, ffout});
    for (int f0 = 0; f0 < FOUT_NB_BLOCKS; f0++)
      for (int f1 = 0; f1 < FOUT_BLOCKING; f1++)
	   unrolled_conv[f0*FOUT_BLOCKING+f1]->store_in(&reg_buf, {f0%FOUT_B_SPLIT_FACTOR, x_bound%X_BLOCKING, f1});
    conv_orig.store_in(&reg_buf, {fout_b%FOUT_B_SPLIT_FACTOR, x_bound%X_BLOCKING, ffout});
    reg_store.store_in(&conv_buf, {n, fout_b, y, x_bound, ffout});

    reg_load_conclude.store_in(&reg_buf, {fout_b%FOUT_B_SPLIT_FACTOR, x_conclude%X_BLOCKING, ffout});
    conv_conclude.store_in(&reg_buf, {fout_b%FOUT_B_SPLIT_FACTOR, x_conclude%X_BLOCKING, ffout});
    reg_store_conclude.store_in(&conv_buf, {n, fout_b, y, x_conclude, ffout});

    // -------------------------------------------------------
    // Code Generation
    // -------------------------------------------------------
    tiramisu::codegen({
        c_input.get_buffer(), 
        filter.get_buffer(), 
        bias.get_buffer(), 
        &conv_buf
    },"generated_conv_layer.o");

    return 0;
}
