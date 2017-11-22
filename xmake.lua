set_modes("debug")

-- the debug mode
if is_mode("debug") then

    -- enable the debug symbols
    set_symbols("debug")

    -- disable optimization
    set_optimize("none")
end

-- the release mode
if is_mode("release") then

    -- set the symbols visibility: hidden
    set_symbols("hidden")

    -- enable fastest optimization
    set_optimize("fastest")

    -- strip all symbols
    set_strip("all")
end

-- add target
target("my_server")
    -- include
    add_includedirs("$(projectdir)/src/3rd/libuuid-1.0.3")
    add_includedirs("$(projectdir)/src/lua-5.3.4")
    add_includedirs("$(projectdir)/src/lualib-src")
    add_includedirs("$(projectdir)/src/c-src")
    add_includedirs("$(projectdir)/src/c-src/aev")

    -- set kind
    set_kind("binary")

    -- add files
    add_files("src/**.c")

    --add_defines("WIN32")