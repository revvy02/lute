add_library(Lute.Stream STATIC)

target_sources(Lute.Stream PRIVATE
    include/lute/stream.h
    src/stream.cpp
)

target_compile_features(Lute.Stream PUBLIC cxx_std_17)
target_include_directories(Lute.Stream PUBLIC "include" ${LIBUV_INCLUDE_DIR})
target_link_libraries(Lute.Stream PRIVATE Lute.Common Lute.Runtime Luau.VM uv_a)
target_compile_options(Lute.Stream PRIVATE ${LUTE_OPTIONS})
