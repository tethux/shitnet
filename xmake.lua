set_project("shitnet")
set_version("0.1.0")
set_languages("c++23")
set_toolchains("clang")

add_rules("mode.debug", "mode.release")

set_warnings("all", "extra")
add_cxxflags("-Wpedantic", "-Wconversion", "-Wshadow", { tools = { "gcc", "clang" } })

target("shitnet")
set_kind("static")
add_files("modules/*.cppm", { public = true })
add_files("src/*.cpp")
add_includedirs("include", { public = true })
add_headerfiles("include/(shitnet/*.h)")

target("shitnet-tests")
set_kind("binary")
set_default(false)
add_deps("shitnet")
add_files("tests/*.cpp")

target("shitnet-playground")
set_kind("binary")
set_default(false)
add_deps("shitnet")
add_files("playground/*.cpp")
