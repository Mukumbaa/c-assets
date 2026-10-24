if #arg < 2 then
    print(string.format("Usage: lua %s <file.c> <file.h> [file_output.h]", arg[0]))
    os.exit(1)
end

local c_filename = arg[1]
local h_filename = arg[2]

local h_basename = h_filename:match("([^/\\]+)$") or h_filename
local out_dir = "standalone_lib"
local out_filename = arg[3] or (out_dir .. "/" .. h_basename)

local function read_file(path)
    local f, err = io.open(path, "r")
    if not f then
        print("Error in opening the file '" .. path .. "': " .. tostring(err))
        os.exit(1)
    end
    local content = f:read("*a")
    f:close()
    return content
end

local h_content = read_file(h_filename)
local c_content = read_file(c_filename)

local lib_name = h_basename:gsub("%.h$", ""):gsub("[^%w_]", "_"):upper()

local header_guard    = lib_name .. "_H"
local impl_guard      = lib_name .. "_IMPLEMENTATION"
local impl_done_guard = lib_name .. "_IMPLEMENTED"
local def_macro       = lib_name .. "_DEF"

local include_pattern = "#%s*include%s*[\"<][^\">]*" .. h_basename:gsub("%.", "%%.") .. "[\">]%s*\r?\n?"
c_content = c_content:gsub(include_pattern, "")

local clean_h_content = h_content:gsub("#pragma once%s*\r?\n?", "")
clean_h_content = clean_h_content:gsub("^%s+", ""):gsub("%s+$", "")

clean_h_content = clean_h_content:gsub("extern%s+", def_macro .. " ")
c_content = c_content:gsub("extern%s+", def_macro .. " ")

local has_ifndef = clean_h_content:match("#ifndef%s+[%w_]+%s+#define%s+[%w_]+")
if not has_ifndef then
    clean_h_content = string.format(
[[#ifndef %s
#define %s

%s

#endif // %s]],
        header_guard, header_guard, clean_h_content, header_guard
    )
end

local api_macro_definition = string.format(
[[#ifndef %s
#ifdef __cplusplus
#define %s extern "C"
#else
#define %s extern
#endif
#endif]],
    def_macro, def_macro, def_macro
)

local result = string.format(
[[/* 
 * Single-header library
 * 
 * In EXACTLY ONE source file, define %s before including this file:
 * #define %s
 * #include "%s"
 */

%s

%s

#ifdef %s
#ifndef %s
#define %s

%s

#endif // %s
#endif // %s
]],
    impl_guard,
    impl_guard,
    h_basename,
    api_macro_definition,
    clean_h_content,
    impl_guard,
    impl_done_guard,
    impl_done_guard,
    c_content:gsub("^%s+", ""):gsub("%s+$", ""),
    impl_done_guard,
    impl_guard
)

local is_windows = package.config:sub(1,1) == '\\'
if is_windows then
    os.execute('if not exist "' .. out_dir .. '" mkdir "' .. out_dir .. '"')
else
    os.execute('mkdir -p "' .. out_dir .. '"')
end

local out_file, err = io.open(out_filename, "w")
if not out_file then
    print("Error creating the output file '" .. out_filename .. "': " .. tostring(err))
    os.exit(1)
end

out_file:write(result)
out_file:close()

print("Header-only created in: " .. out_filename)
