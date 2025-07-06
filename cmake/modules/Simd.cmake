include(CheckCXXCompilerFlag)

macro(check_and_add_simd_flag target flag_def flag_name compiler_flag)
    check_cxx_compiler_flag("${compiler_flag}" SUPPORTS_${flag_def})
    if(SUPPORTS_${flag_def})
        # Teste de runtime para verificar se o processador realmente suporta
        string(REPLACE "-" "_" TEST_NAME "${flag_name}")
        string(REPLACE "." "_" TEST_NAME "${TEST_NAME}")
        string(TOUPPER "${TEST_NAME}" TEST_NAME)

        try_run(${TEST_NAME}_RUN_RESULT ${TEST_NAME}_COMPILE_RESULT
                ${CMAKE_BINARY_DIR}/temp
                ${CMAKE_SOURCE_DIR}/cmake/test_simd.cpp
                COMPILE_DEFINITIONS "${compiler_flag}" "-DTEST_${TEST_NAME}"
                RUN_OUTPUT_VARIABLE ${TEST_NAME}_OUTPUT
        )

        if(${TEST_NAME}_COMPILE_RESULT AND ${TEST_NAME}_RUN_RESULT EQUAL 0)
            target_compile_options(${target} PRIVATE ${compiler_flag})
            target_compile_definitions(${target} PRIVATE ${flag_def})
            set(ACTIVATED_SIMD_FLAGS "${ACTIVATED_SIMD_FLAGS}${flag_name}, ")
            message(STATUS "Activated SIMD: ${flag_name}")
        else()
            message(STATUS "SIMD ${flag_name} not supported at runtime, skipping")
        endif()
    else()
        message(STATUS "SIMD ${flag_name} not supported by compiler, skipping")
    endif()
endmacro()

function(configure_simd_flags target)
    set(ACTIVATED_SIMD_FLAGS "")

    if(MSVC)
        check_and_add_simd_flag(${target} "__SSE__" "SSE" "/arch:SSE")
        check_and_add_simd_flag(${target} "__SSE2__" "SSE2" "/arch:SSE2")
        check_and_add_simd_flag(${target} "__SSE4_1__" "SSE4.1" "/arch:SSE4.1")
        check_and_add_simd_flag(${target} "__SSE4_2__" "SSE4.2" "/arch:SSE4.2")
        check_and_add_simd_flag(${target} "__AVX__" "AVX" "/arch:AVX")
        check_and_add_simd_flag(${target} "__AVX2__" "AVX2" "/arch:AVX2")
        check_and_add_simd_flag(${target} "__BMI__" "BMI" "/arch:BMI")
        check_and_add_simd_flag(${target} "__AVX512F__" "AVX-512" "/arch:AVX512")
    else()
        check_and_add_simd_flag(${target} "__SSE__" "SSE" "-msse")
        check_and_add_simd_flag(${target} "__SSE2__" "SSE2" "-msse2")
        check_and_add_simd_flag(${target} "__SSE4_1__" "SSE4.1" "-msse4.1")
        check_and_add_simd_flag(${target} "__SSE4_2__" "SSE4.2" "-msse4.2")
        check_and_add_simd_flag(${target} "__AVX__" "AVX" "-mavx")
        check_and_add_simd_flag(${target} "__AVX2__" "AVX2" "-mavx2")
        check_and_add_simd_flag(${target} "__BMI__" "BMI" "-mbmi")
        check_and_add_simd_flag(${target} "__AVX512F__" "AVX-512" "-mavx512f")
    endif()

    string(REGEX REPLACE ", $" "" ACTIVATED_SIMD_FLAGS "${ACTIVATED_SIMD_FLAGS}")

    if(NOT ACTIVATED_SIMD_FLAGS)
        message(STATUS "No SIMD flags were activated for target ${target}")
    else()
        message(STATUS "Activated SIMD flags for ${target}: ${ACTIVATED_SIMD_FLAGS}")
    endif()
endfunction()