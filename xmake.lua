add_rules("mode.debug", "mode.release")

includes("sysifus") -- pseudo-legal move generation library

target("tanathos")
    set_kind("binary")
    add_includedirs("include")
    add_files("src/*.cpp")
    set_warnings("all", "error")
    add_deps("sysifus")
    set_languages("c++20")

    if is_mode("debug") then
        set_symbols("debug")
    end

includes("test")
