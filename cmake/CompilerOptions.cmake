# cmake/CompilerOptions.cmake
function(bettercpu_apply_compiler_options target_name)
    target_compile_options(${target_name} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endfunction()