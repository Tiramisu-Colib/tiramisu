cmake_minimum_required(VERSION 3.16)
project(tiramisu)

enable_testing()

set(PROJECT_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_VERBOSE_MAKEFILE "ON")
set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

include(configure.cmake)

# Add CMake header files
set(HEADER_FILES
        include/tiramisu/block.h
        include/tiramisu/cuda_ast.h
        include/tiramisu/core.h
        include/tiramisu/debug.h
        include/tiramisu/utils.h
        include/tiramisu/type.h
        include/tiramisu/expr.h
        include/tiramisu/mpi_comm.h
        include/tiramisu/externs.h
        )
        
# Add autoscheduler headers if USE_AUTO_SCHEDULER is TRUE in configure.cmake
if (${USE_AUTO_SCHEDULER})
    set(HEADER_FILES ${HEADER_FILES}
        include/tiramisu/auto_scheduler/auto_scheduler.h
        include/tiramisu/auto_scheduler/utils.h
        include/tiramisu/auto_scheduler/optimization_info.h
        include/tiramisu/auto_scheduler/dnn_accesses.h
        include/tiramisu/auto_scheduler/ast.h
        include/tiramisu/auto_scheduler/evaluator.h
        include/tiramisu/auto_scheduler/schedules_generator.h
        include/tiramisu/auto_scheduler/search_method.h
        )
endif()

# Add CMake cpp files
set(OBJS expr block core codegen_halide codegen_c debug function utils codegen_halide_lowering codegen_from_halide mpi codegen_cuda externs)

# Add autoscheduler cpp files if USE_AUTO_SCHEDULER is TRUE in configure.cmake
if (${USE_AUTO_SCHEDULER})
    set(OBJS_AUTO_SCHEDULER auto_scheduler optimization_info dnn_accesses ast evaluator schedules_generator search_method)
endif()

### CMAKE FILE INTERNALS ###
if (${USE_GPU})
    find_package(CUDA REQUIRED)
    add_definitions(-DNVCC_PATH="${CUDA_TOOLKIT_ROOT_DIR}/bin/nvcc")
    add_library(cuda_wrapper STATIC "src/tiramisu_cuda_wrappers.cpp")
    target_link_libraries(cuda_wrapper ${CUDA_LIBRARIES} ${CUDA_CUBLAS_LIBRARIES})
    target_include_directories(cuda_wrapper PUBLIC ${CUDA_INCLUDE_DIRS})
    if (${USE_CUDNN})
        find_library(CUDNN_LIBRARIES cudnn PATHS ${CUDNN_LOCATION}/lib64 NO_DEFAULT_PATH)
        set(CUDNN_INCLUDE_DIRECTORY ${CUDNN_LOCATION}/include)
    endif()
endif()

if (${USE_MKL_WRAPPERS})
    add_library(mkl_wrapper STATIC "src/tiramisu_mkl_wrappers.cpp")
    target_include_directories(mkl_wrapper PUBLIC ${MKL_PREFIX}/include)
endif()

# Need to define FLEXNLP_LIBRARIES and FLEXNLP_INCLUDE_DIRS
#if (${USE_FLEXNLP})
    add_library(flexnlp_wrapper STATIC "src/tiramisu_flexnlp_wrappers.cpp")
    target_link_libraries(flexnlp_wrapper ${FLEXNLP_LIBRARIES})
    target_include_directories(flexnlp_wrapper PUBLIC ${FLEXNLP_INCLUDE_DIRS})
#endif()

FILE(STRINGS tests/test_list.txt TIRAMISU_TESTS_RAW)
FILE(STRINGS benchmarks/benchmark_list.txt TIRAMISU_BENCHMARKS_RAW)
FILE(STRINGS benchmarks/automatic_comm/dist_benchmark_list.txt TIRAMISU_DIST_BENCHMARKS_RAW)
FILE(STRINGS tutorials/developers_tutorial_list.txt TIRAMISU_DEVELOPERS_TUTORIALS_RAW)
FILE(STRINGS tutorials/users_tutorial_list.txt TIRAMISU_USERS_TUTORIALS_RAW)


macro(init_tags)
    set(is_gpu false)
    set(is_mpi false)
    set(NUM_MPI_RANKS 0)
endmacro()

macro(parse_tags tags)
    init_tags()
    foreach(tag ${tags})
        if (${tag} STREQUAL gpu)
            set(is_gpu true)
        elseif (${tag} STREQUAL mpi)
            set(is_mpi true)
        elseif (${tag} GREATER -1) # check if it is a number
            set(NUM_MPI_RANKS ${tag})
        else()
            message(WARNING "Tag ${tag} not recognized")
        endif()
    endforeach()
endmacro()

# transforms the list of the format (program_name/program_name[tag0,tag1,..,tagn]) into a list
# containing a list of "program_name property0 property1 property2 .. propertyn"
# the list can contain as many tags as necessary
# the number of properties is fixed and is in the order given below
# current tags are : gpu, mpi.
# current properties are : is_gpu(boolean), is_mpi(boolean)
function (parse_list raw_list list_name)
    set(result)
    foreach(element ${raw_list})
        set(tmp_result)
        init_tags()
        string(REGEX MATCH "^([a-zA-Z_0-9]+)(\\[([a-zA-Z0-9_]+(,[a-zA-Z0-9_]+)*)\\])?$" the_match "${element}")
        if(NOT ${CMAKE_MATCH_COUNT})
            message(WARNING "Could not parse: ${element}")
            continue()
        elseif(${CMAKE_MATCH_COUNT} GREATER 1)
            STRING(REPLACE "," ";" tags ${CMAKE_MATCH_3})
            parse_tags("${tags}")
        endif()
        set(name ${CMAKE_MATCH_1})
        # The list should always have four elements (name <USE_GPU> <USE_MPI> <NUM_MPI_RANKS>)
        list(APPEND tmp_result "${name}")
        # Check if we need to use the GPU
        if(${is_gpu})
            if(${USE_GPU})
                list(APPEND tmp_result "true")
            else()
                message(STATUS "skipping gpu tagged ${name} because USE_GPU is not set to true")
                continue()
            endif()
        else()
            list(APPEND tmp_result "false")
        endif()
        # Check if we need to use MPI
        if (${is_mpi})
            if (${USE_MPI})
                list(APPEND tmp_result "true")
                list(APPEND tmp_result "${NUM_MPI_RANKS}")
            else()
                message(STATUS "skipping mpi tagged ${name} because USE_MPI is not set to true")
                continue()
            endif()
        else ()
            list(APPEND tmp_result "false")
            list(APPEND tmp_result "0")
        endif()
        string(REPLACE ";" " " tmp_result "${tmp_result}")
        list(APPEND result "${tmp_result}")
    endforeach()
    set("${list_name}" "${result}" PARENT_SCOPE)
endfunction()


parse_list("${TIRAMISU_BENCHMARKS_RAW}" TIRAMISU_BENCHMARKS)
parse_list("${TIRAMISU_TESTS_RAW}" TIRAMISU_TESTS)
parse_list("${TIRAMISU_DEVELOPERS_TUTORIALS_RAW}" TIRAMISU_DEVELOPERS_TUTORIALS)
parse_list("${TIRAMISU_USERS_TUTORIALS_RAW}" TIRAMISU_USERS_TUTORIALS)
parse_list("${TIRAMISU_DIST_BENCHMARKS_RAW}" TIRAMISU_DIST_BENCHMARKS)


# Check that MPI variables are set correctly if we need it
if (${USE_MPI})
    if (MPI_BUILD_DIR STREQUAL "")
        message(FATAL_ERROR "USE_MPI==true but MPI_BUILD_DIR is empty.")
    elseif (MPI_NODES STREQUAL "")
        message(FATAL_ERROR "USE_MPI==true but MPI_NODES is empty.")
    endif()
endif()

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wno-sign-compare -fno-rtti")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
include_directories("include"
        "${ISL_INCLUDE_DIRECTORY}"
        "${HALIDE_SOURCE_DIRECTORY}/include" "${HALIDE_SOURCE_DIRECTORY}/tools"
        "build")

if (${USE_MPI})
    set(CMAKE_CXX_COMPILER "${MPI_BUILD_DIR}/bin/mpicxx")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWITH_MPI")
    link_directories(${MPI_BUILD_DIR}/lib)
    include_directories(${MPI_BUILD_DIR}/include)
else ()
    set(CMAKE_CXX_COMPILER "g++")
endif()


find_library(HalideLib Halide PATHS ${HALIDE_LIB_DIRECTORY} NO_DEFAULT_PATH)
find_library(ISLLib isl PATHS ${ISL_LIB_DIRECTORY} NO_DEFAULT_PATH)

# Require LLVM 5.0 or greater to keep in line with Halide
execute_process(COMMAND ${LLVM_CONFIG_BIN}/llvm-config --version OUTPUT_VARIABLE LLVM_VERSION)
string(STRIP "${LLVM_VERSION}" LLVM_VERSION)
if ("${LLVM_VERSION}" VERSION_LESS 5.0)
    message(FATAL_ERROR "tiramisu requires LLVM version >= 5.0")
endif()

execute_process(COMMAND ${LLVM_CONFIG_BIN}/llvm-config --ignore-libllvm --system-libs OUTPUT_VARIABLE LLVM_FLAGS)
string(STRIP ${LLVM_FLAGS} LLVM_FLAGS)

set(LINK_FLAGS "-ldl -lpthread ${LLVM_FLAGS}")

if(${USE_MPI})
    set(LINK_FLAGS "${LINK_FLAGS} -lmpi")
endif()

if(${USE_LIBPNG})
    execute_process(COMMAND libpng-config --I_opts OUTPUT_VARIABLE LIBPNG_INCLUDE_OPTIONS)
    string(STRIP ${LIBPNG_INCLUDE_OPTIONS} LIBPNG_INCLUDE_OPTIONS)
    add_compile_options("${LIBPNG_INCLUDE_OPTIONS}")
    execute_process(COMMAND libpng-config --ldflags OUTPUT_VARIABLE LIBPNG_FLAGS)
    string(STRIP ${LIBPNG_FLAGS} LIBPNG_FLAGS)
    set(LINK_FLAGS "${LINK_FLAGS} ${LIBPNG_FLAGS}")
endif()

if(${USE_LIBJPEG})
    set(LINK_FLAGS "${LINK_FLAGS} -ljpeg")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHALIDE_NO_JPEG")
endif()

if(${ENABLE_DEBUG})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_DEBUG -DDEBUG_LEVEL=${DEBUG_LEVEL}")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_DIR}/build)

set(T_CODE "")

foreach (obj ${OBJS})
    set(T_CODE ${T_CODE} src/tiramisu_${obj}.cpp)
endforeach ()

add_library(tiramisu SHARED ${T_CODE})

target_link_libraries(tiramisu ${HalideLib} ${ISLLib})
target_link_libraries(tiramisu ${LINK_FLAGS})

if (${USE_AUTO_SCHEDULER})
    set(AUTO_SCHEDULER_CODE "")
    foreach(obj ${OBJS_AUTO_SCHEDULER})
        set(AUTO_SCHEDULER_CODE ${AUTO_SCHEDULER_CODE} src/auto_scheduler/tiramisu_${obj}.cpp)
    endforeach()
    
    add_library(tiramisu_auto_scheduler SHARED ${AUTO_SCHEDULER_CODE})
    target_link_libraries(tiramisu_auto_scheduler tiramisu ${HalideLib} ${ISLLib})
    target_link_libraries(tiramisu_auto_scheduler ${LINK_FLAGS})
endif()

function(link_tags name)
    if (${is_gpu})
        if (${USE_GPU})
            target_link_libraries(${name} cuda_wrapper)
        endif()
    endif()
endfunction()

function(build_g name generator result)
    add_executable(${name} ${generator} ${HEADER_FILES})
    target_link_libraries(${name} tiramisu ${HalideLib} ${ISLLib})
    target_link_libraries(${name} ${LINK_FLAGS})
    link_tags(${name})
    if (NOT result STREQUAL "")
        set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${result}")
        add_custom_command(OUTPUT ${result} COMMAND ${name} WORKING_DIRECTORY ${PROJECT_DIR} DEPENDS ${name})
    endif()
endfunction()

function(build_halide_g name generator result)
    add_executable(${name} ${generator})
    target_link_libraries(${name} ${HalideLib})
    target_link_libraries(${name} ${LINK_FLAGS})
    if (NOT result STREQUAL "")
        set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${result})
        add_custom_command(OUTPUT ${result} COMMAND ${name} WORKING_DIRECTORY ${PROJECT_DIR} DEPENDS ${NAME})
    endif()
endfunction()

function(build_w name objs wrapper header)
    add_executable(${name} ${wrapper} ${objs} ${header})
    target_link_libraries(${name} tiramisu ${HalideLib} ${ISLLib})
    target_link_libraries(${name} ${LINK_FLAGS})
    link_tags(${name})
endfunction()

macro(set_obj inp_obj)
    if (${is_gpu})
        set(obj "${inp_obj};${inp_obj}_gpu.o;${inp_obj}_cpu.o")
    else()
        set(obj "${inp_obj}")
    endif()
endmacro()

function(parse_descriptor descriptor)
    string(REPLACE " " ";" descriptor ${descriptor})
    list(GET descriptor 0 id)
    set(id ${id} PARENT_SCOPE)
    list(GET descriptor 1 is_gpu)
    set(is_gpu ${is_gpu} PARENT_SCOPE)
    list(GET descriptor 2 is_mpi)
    set(is_mpi ${is_mpi} PARENT_SCOPE)
    list(GET descriptor 3 NUM_MPI_RANKS)
    set(NUM_MPI_RANKS ${NUM_MPI_RANKS} PARENT_SCOPE)
endfunction()

if (APPLE)
    set(LIB_SUF dylib)
else ()
    set(LIB_SUF so)
endif ()

function(new_test descriptor)
    parse_descriptor(${descriptor})
    set(generator_target test_${id}_fct_generator)
    set_obj(${PROJECT_DIR}/build/generated_fct_test_${id}.o)
    set(test_name test_${id})
    build_g(${generator_target} tests/test_${id}.cpp "${obj}")
    build_w(${test_name} "${obj}" tests/wrapper_test_${id}.cpp tests/wrapper_test_${id}.h)
    add_test(NAME ${id}_build COMMAND "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target ${test_name})
    if (NOT ${is_mpi})
        add_test(NAME ${id} COMMAND ${test_name} WORKING_DIRECTORY ${PROJECT_DIR})
    elseif (${USE_MPI}) # This is an MPI test (sanity check that we want to use MPI though)
        # configure the options so files are copied on the fly as necessary.
        add_test(NAME ${id} COMMAND ${MPI_BUILD_DIR}/bin/mpirun -x LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH}:/tmp/ -np ${NUM_MPI_RANKS} -host ${MPI_NODES} --map-by node --oversubscribe --wdir /tmp/ --preload-files ${PROJECT_DIR}/build/libtiramisu.${LIB_SUF},${PROJECT_DIR}/3rdParty/isl/.libs/libisl.${LIB_SUF} --preload-binary ${PROJECT_DIR}/build/${test_name} WORKING_DIRECTORY ${PROJECT_DIR})
    endif()
    set_tests_properties(${id} PROPERTIES DEPENDS ${id}_build)
endfunction()

build_g(test_global tests/test_global.cpp "")
add_test(NAME global_build COMMAND "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target test_global)
add_test(NAME global COMMAND test_global WORKING_DIRECTORY ${PROJECT_DIR})
set_tests_properties(global PROPERTIES DEPENDS global_build)
foreach(t ${TIRAMISU_TESTS})
    new_test(${t})
endforeach()

add_custom_target(benchmarks)

function(new_benchmark descriptor)
    parse_descriptor(${descriptor})
    set(tiramisu_generator_target bench_tiramisu_${id}_generator)
    set(halide_generator_target   bench_halide_${id}_generator)
    set_obj("${PROJECT_DIR}/build/generated_fct_${id}.o")
    set(generated_obj_halide   ${PROJECT_DIR}/build/generated_fct_${id}_ref.o)
    set(bench_name bench_${id})
    build_g(${tiramisu_generator_target} benchmarks/halide/${id}_tiramisu.cpp "${obj}")
    build_halide_g(${halide_generator_target} benchmarks/halide/${id}_ref.cpp ${generated_obj_halide})
    build_w(${bench_name} "${obj};${generated_obj_halide}" benchmarks/halide/wrapper_${id}.cpp benchmarks/halide/wrapper_${id}.h)
    if (NOT ${is_mpi})
        add_custom_target(run_benchmark_${id} COMMAND ${bench_name} WORKING_DIRECTORY ${PROJECT_DIR})
        add_custom_command(TARGET benchmarks COMMAND ${bench_name} WORKING_DIRECTORY ${PROJECT_DIR})
    elseif (${USE_MPI})
        # configure the options so files are copied on the fly as necessary.
        add_custom_target(run_benchmark_${id} COMMAND ${MPI_BUILD_DIR}/bin/mpirun -x LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH}:/tmp/ -np ${NUM_MPI_RANKS} -host ${MPI_NODES} --map-by node --oversubscribe -wdir ${PROJECT_DIR} --preload-files ${PROJECT_DIR}/build/libtiramisu.${LIB_SUF},${PROJECT_DIR}/3rdParty/isl/.libs/libisl.${LIB_SUF} --preload-binary ${PROJECT_DIR}/build/${bench_name} WORKING_DIRECTORY ${PROJECT_DIR})
    endif()
    add_dependencies(run_benchmark_${id} ${bench_name})
endfunction()

foreach(b ${TIRAMISU_BENCHMARKS})
    new_benchmark(${b})
endforeach()


add_custom_target(dist_benchmarks)

function(new_dist_benchmark descriptor)
    parse_descriptor(${descriptor})
    set(tiramisu_generator_target dist_bench_tiramisu_${id}_generator)
    set(ref_generator_target   dist_bench_ref_${id}_generator)
    set_obj("${PROJECT_DIR}/build/generated_fct_${id}.o")
    set(generated_obj_ref   ${PROJECT_DIR}/build/generated_fct_${id}_ref.o)
    set(bench_name bench_${id})
    build_g(${tiramisu_generator_target} benchmarks/automatic_comm/${id}_tiramisu.cpp "${obj}")
    build_g(${ref_generator_target} benchmarks/automatic_comm/${id}_ref.cpp ${generated_obj_ref})
    build_w(${bench_name} "${obj};${generated_obj_ref}" benchmarks/automatic_comm/wrapper_${id}.cpp benchmarks/automatic_comm/wrapper_${id}.h)
    if (NOT ${is_mpi})
        add_custom_target(run_dist_benchmark_${id} COMMAND ${bench_name} WORKING_DIRECTORY ${PROJECT_DIR})
        add_custom_command(TARGET dist_benchmarks COMMAND ${bench_name} WORKING_DIRECTORY ${PROJECT_DIR})
    elseif (${USE_MPI})
        # configure the options so files are copied on the fly as necessary.
        add_custom_target(run_dist_benchmark_${id} COMMAND ${MPI_BUILD_DIR}/bin/mpirun -x LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH}:/tmp/ -np ${NUM_MPI_RANKS} -host ${MPI_NODES} --map-by node --oversubscribe -wdir ${PROJECT_DIR} --preload-files ${PROJECT_DIR}/build/libtiramisu.${LIB_SUF},${PROJECT_DIR}/3rdParty/isl/.libs/libisl.${LIB_SUF} --preload-binary ${PROJECT_DIR}/build/${bench_name} WORKING_DIRECTORY ${PROJECT_DIR})
    endif()
    add_dependencies(run_dist_benchmark_${id} ${bench_name})
endfunction()

foreach(b ${TIRAMISU_DIST_BENCHMARKS})
    new_dist_benchmark(${b})
endforeach()

add_custom_target(doc DEPENDS ${PROJECT_DIR}/utils/doc_generation/Doxyfile)
add_custom_command(TARGET doc COMMAND doxygen utils/doc_generation/Doxyfile WORKING_DIRECTORY ${PROJECT_DIR})

add_custom_target(tutorials)

function(new_developers_tutorial descriptor)
    parse_descriptor(${descriptor})
    set(generator_target developers_tutorial_${id}_fct_generator)
    set_obj("${PROJECT_DIR}/build/generated_fct_developers_tutorial_${id}.o")
    set(tutorial_name tutorial_developers_${id})
    build_g(${generator_target} tutorials/developers/tutorial_${id}/tutorial_${id}.cpp "${obj}")
    build_w(${tutorial_name} "${obj}" tutorials/developers/tutorial_${id}/wrapper_tutorial_${id}.cpp tutorials/developers/tutorial_${id}/wrapper_tutorial_${id}.h)
    if (NOT ${is_mpi})
        add_custom_target(run_developers_tutorial_${id} COMMAND ${tutorial_name} WORKING_DIRECTORY ${PROJECT_DIR})
        add_custom_command(TARGET tutorials COMMAND ${tutorial_name} WORKING_DIRECTORY ${PROJECT_DIR})
    elseif (${USE_MPI}) # This is an MPI test (sanity check that we want to use MPI though)
        # configure the options so files are copied on the fly as necessary.
        add_custom_target(run_developers_tutorial_${id} COMMAND ${MPI_BUILD_DIR}/bin/mpirun -x LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH}:/tmp/ -np ${NUM_MPI_RANKS} -host ${MPI_NODES} --map-by node --oversubscribe --wdir /tmp/ --preload-files ${PROJECT_DIR}/build/libtiramisu.${LIB_SUF},${PROJECT_DIR}/3rdParty/isl/.libs/libisl.${LIB_SUF} --preload-binary ${PROJECT_DIR}/build/${tutorial_name} WORKING_DIRECTORY ${PROJECT_DIR})
    endif()
    add_dependencies(run_developers_tutorial_${id} ${tutorial_name})
endfunction()

function(new_users_tutorial descriptor)
    parse_descriptor(${descriptor})
    set(generator_target users_tutorial_${id}_fct_generator)
    set_obj("${PROJECT_DIR}/build/generated_fct_users_tutorial_${id}.o")
    set(tutorial_name tutorial_users_${id})
    build_g(${generator_target} tutorials/users/tutorial_${id}/tutorial_${id}.cpp "${obj}")
    build_w(${tutorial_name} "${obj}" tutorials/users/tutorial_${id}/wrapper_tutorial_${id}.cpp tutorials/users/tutorial_${id}/wrapper_tutorial_${id}.h)
    if (NOT ${is_mpi})
        add_custom_target(run_users_tutorial_${id} COMMAND ${tutorial_name} WORKING_DIRECTORY ${PROJECT_DIR})
        add_custom_command(TARGET tutorials COMMAND ${tutorial_name} WORKING_DIRECTORY ${PROJECT_DIR})
    elseif (${USE_MPI}) # This is an MPI test (sanity check that we want to use MPI though)
        # configure the options so files are copied on the fly as necessary.
        add_custom_target(run_users_tutorial_${id} COMMAND ${MPI_BUILD_DIR}/bin/mpirun -x LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH}:/tmp/ -np ${NUM_MPI_RANKS} -host ${MPI_NODES} --map-by node --oversubscribe --wdir /tmp/ --preload-files ${PROJECT_DIR}/build/libtiramisu.${LIB_SUF},${PROJECT_DIR}/3rdParty/isl/.libs/libisl.${LIB_SUF} --preload-binary ${PROJECT_DIR}/build/${tutorial_name} WORKING_DIRECTORY ${PROJECT_DIR})
    endif()
    add_dependencies(run_users_tutorial_${id} ${tutorial_name})
endfunction()


foreach(t ${TIRAMISU_DEVELOPERS_TUTORIALS})
    new_developers_tutorial(${t})
endforeach()

foreach(t ${TIRAMISU_USERS_TUTORIALS})
    new_users_tutorial(${t})
endforeach()

# Individual benchmarks are moved to benchmarks/CMakeLists.txt to reduce clutter
add_subdirectory(benchmarks)
