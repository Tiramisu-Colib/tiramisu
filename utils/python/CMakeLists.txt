#  Copyright (c) 2019-2020 Christopher Taylor
#
#  Distributed under the Boost Software License, Version 1.0. (See accompanying
#  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
cmake_minimum_required(VERSION 3.17.0)
project(pytiramisu)
find_package(pybind11)

pybind11_add_module(pytiramisu src/pytiramisu.cpp)
