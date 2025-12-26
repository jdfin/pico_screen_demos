
add_compile_options(-Wall -Wextra -Werror)

add_executable(screen_test
    screen_test.cpp
)

pico_enable_stdio_uart(screen_test 0)
pico_enable_stdio_usb(screen_test 1)

target_link_libraries(screen_test PRIVATE
    pico_stdlib
    pico_stdio_usb
    framebuffer
    touchscreen
    misc
)

pico_add_extra_outputs(screen_test)
