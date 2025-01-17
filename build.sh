#!/bin/bash
set -ex

readonly workspace=$PWD

function check_style {
    export PATH=/usr/bin:$PATH
    #pre-commit install
    clang-format --version

    if ! pre-commit run -a ; then
        git diff
        exit 1
    fi
}

function cmake_ {
    cmake ..
}

function build {
    run_exe_test 1
    run_exe_test 2
    run_exe_test 3
    run_exe_test 4
    run_exe_test 5
    run_exe_test 6
    run_exe_test 7
    run_exe_test 8
    run_exe_test 9
    run_exe_test 10
    run_exe_test 12

    make -j8
}

function run_exe_test {
    export MALLOC_CHECK_=2
    local no=$1
    make exe_test$no -j8
    ctest -R "exe_test$no$"
}

function run_python_test {
    export cinn_so_prefix="$workspace/build/cinn/api/"
    python3 $workspace/cinn/api/python_api_test.py
}

function test_ {
    ctest
}

function CI {
    mkdir -p build
    cd build
    cmake_
    build
    test_
    # broken when ginac included
    # TODO(Superjomn) fix it
    #run_python_test
}

function main {
    # Parse command line.
    for i in "$@"; do
        case $i in
            check_style)
                check_style
                shift
                ;;
            cmake)
                cmake_
                shift
                ;;
            build)
                build
                shift
                ;;
            test)
                test_
                shift
                ;;
            ci)
                CI
                shift
                ;;
        esac
    done
}


main $@
