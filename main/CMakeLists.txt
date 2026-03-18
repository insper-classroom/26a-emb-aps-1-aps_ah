add_executable(pico_emb main.c core1.c)

set_target_properties(pico_emb PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# pull in common dependencies
target_link_libraries(pico_emb 
        pico_stdlib
        pico_multicore
        hardware_dma
        hardware_pio
        hardware_timer
        hardware_clocks
        hardware_pwm)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(pico_emb)
