The files in this folder are organized as follows:

    General
        clean.sh : remove some useless files.
        compile_and_run_mkldnn.sh : compile MKL-DNN code and run it.
        configure.h: define some configuration constants.

    Tiramisu Wrapper
        end_to_end_resnet_tiramisu_wrapper.cpp: wrapper file that calls the blocks generated by Tiramisu.
        tiramisu_creation_macros.h : block creation macros for Tiramisu parts.

    Intel MKL-DNN
        end_to_end_resnet_wrapper.cpp : code that calls Intel MKL-DNN ResNet architecture.
        mkldnn_creation_macros.h : creation macros for MKL-DNN parts.

To run this benchmark:
    I. GENERATE THE TIRAMISU BLOCKS USING
            ./generate_tiramisu_blocks.sh

    II. GENERATE THE EXECUTABLE :
        At the directory build/benchmarks/DNN/end_to_end_architectures/End_To_End_ResNet/ execute
	         make

    wrapper_end_to_end_resnet_tiramisu executable will be created in the current directory.

    III. EXECUTE THE GENERATED EXECUTABLE
        To compare the result of tiramisu with MKL-DNN execute :
            ./compile_and_run_mkldnn.sh
        then
            ./wrapper_end_to_end_resnet_tiramisu

        Execution results can be found in the text files :
            mkl_result.txt (Intel MKL-DNN)
            tiramisu_result.txt (Tiramisu)

You can adjust the number of executions in the configure.h file (to get a more precise median executions time)
