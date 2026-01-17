// src/ir_test.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <array>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// Execute command and capture output
std::pair<int, std::string> execute_command(const std::string& cmd, const std::string& input = "", int timeoutSeconds = 0) {
    std::array<char, 128> buffer;
    std::string result;

#ifdef _WIN32
    std::string temp_cmd = cmd + " > temp_output.txt 2>&1";
    if (!input.empty()) {
        std::ofstream temp_in("temp_input.txt");
        temp_in << input;
        temp_in.close();
        temp_cmd = "type temp_input.txt | " + temp_cmd;
    }

    int ret = system(temp_cmd.c_str());

    std::ifstream output_file("temp_output.txt");
    if (output_file.is_open()) {
        std::string line;
        while (std::getline(output_file, line)) {
            result += line + "\n";
        }
        output_file.close();
        std::remove("temp_output.txt");
    }

    if (!input.empty()) {
        std::remove("temp_input.txt");
    }

    return {ret, result};
#else
    std::string base_cmd = cmd;
    if (timeoutSeconds > 0) {
        base_cmd = "timeout -s KILL " + std::to_string(timeoutSeconds) + " " + cmd;
    }

    std::string full_cmd;
    if (!input.empty()) {
        std::ofstream temp_in("/tmp/temp_input.txt");
        temp_in << input;
        temp_in.close();
        full_cmd = "cat /tmp/temp_input.txt | " + base_cmd + " > /tmp/temp_output.txt 2>&1";
    } else {
        full_cmd = base_cmd + " > /tmp/temp_output.txt 2>&1";
    }

    int ret = system(full_cmd.c_str());

    std::ifstream output_file("/tmp/temp_output.txt");
    if (output_file.is_open()) {
        std::string line;
        while (std::getline(output_file, line)) {
            result += line + "\n";
        }
        output_file.close();
        std::remove("/tmp/temp_output.txt");
    }

    if (!input.empty()) {
        std::remove("/tmp/temp_input.txt");
    }

    return {ret, result};
#endif
}

// Read entire file
std::string read_file_content(const fs::path& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Run IR test: compile -> llc -> clang -> run and compare output.
bool run_ir_test(const fs::path& test_file, const fs::path& compiler_path, const std::string &ref_builtin) {
    std::string base_name = test_file.stem().string();

    fs::path in_file = test_file.parent_path() / (base_name + ".in");
    fs::path out_file = test_file.parent_path() / (base_name + ".out");
    fs::path ir_file = test_file.parent_path() / (base_name + ".ll");
    fs::path exe_file = test_file.parent_path() / (base_name +
#ifdef _WIN32
        ".exe"
#else
        ""
#endif
    );

    std::cout << "Running test: " << base_name << std::endl;

    fs::path asm_file = test_file.parent_path() / (base_name + ".s");

    // 1. Compile to LLVM IR
    std::cout << "  Compiling to LLVM IR..." << std::endl;
    auto quote = [](const fs::path &p) {
        const std::string s = p.string();
        if (s.find(' ') != std::string::npos || s.find('"') != std::string::npos) {
            return std::string("\"") + s + "\"";
        }
        return s;
    };

    fs::path builtin_file = test_file.parent_path() / (base_name + "_builtin.c");

    std::string compile_cmd = quote(compiler_path) + " " + quote(test_file) + " --emit-llvm";
    auto [compile_ret, compile_out] = execute_command(compile_cmd);

    if (compile_ret != 0) {
        std::cerr << "[ir_test] compile_ret=" << compile_ret << " output:\n" << compile_out << std::endl;
        throw std::runtime_error("Compilation failed or unsupported IR feature: " + compile_out);
    }

    // Split combined output into IR (stdout) and builtin.c (stderr) by marker
    const std::string marker = "typedef unsigned long size_t;";
    std::string ir_text = compile_out;
    std::string builtin_text;
    size_t pos = compile_out.find(marker);
    if (pos != std::string::npos) {
        ir_text = compile_out.substr(0, pos);
        builtin_text = compile_out.substr(pos);
    }

    bool used_compiler_builtin = !builtin_text.empty();
    bool used_ref_builtin = false;
    bool used_stub_builtin = false;

    // For host-side testing we override the module triple/datalayout to x86_64
    // so llc/clang assemble locally produced code instead of riscv.
#if defined(_WIN32)
    const std::string host_triple = "x86_64-pc-windows-msvc";
    const std::string host_datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128";
#else
    const std::string host_triple = "x86_64-pc-linux-gnu";
    const std::string host_datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128";
#endif
    auto replace_once = [](std::string &s, const std::string &from, const std::string &to) {
        size_t p = s.find(from);
        if (p != std::string::npos) {
            s.replace(p, from.size(), to);
        }
    };
    replace_once(ir_text, "target triple = \"riscv64-unknown-elf\"", "target triple = \"" + host_triple + "\"");
    replace_once(ir_text, "target datalayout = \"e-m:e-p:64:64-i64:64-i128:128-n64-S128\"", "target datalayout = \"" + host_datalayout + "\"");

    // For host testing, replace inline-asm RISC-V builtins with portable stubs to satisfy clang on x86
    const char *kHostBuiltin =
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "long printInt(long x){printf(\"%ld\", x);return x;}\n"
        "long printlnInt(long x){printf(\"%ld\\n\", x);return x;}\n"
        "long printlnStr(const char *s){printf(\"%s\\n\", s ? s : \"\");return 0;}\n"
        "long getInt(void){long v=0;if(scanf(\"%ld\", &v)!=1)v=0;return v;}\n"
        "__attribute__((noreturn)) void exit_rt(long code){exit((int)code);}\n";
    // Prefer compiler-emitted builtin when available; otherwise, use reference IR-1 builtin if present.
    // For host runs, swap out RISC-V inline asm with a portable stub to keep clang happy.
    const std::string riscv_marker = ".word 0x00000073";
    if (builtin_text.empty() && !ref_builtin.empty()) {
        builtin_text = ref_builtin;
        used_ref_builtin = true;
    }

    if (!builtin_text.empty() && builtin_text.find(riscv_marker) != std::string::npos) {
        if (!ref_builtin.empty()) {
            builtin_text = ref_builtin;
            used_ref_builtin = true;
            used_compiler_builtin = false;
        } else {
            builtin_text = kHostBuiltin;
            used_stub_builtin = true;
            used_compiler_builtin = false;
        }
    }

    if (builtin_text.empty()) {
        builtin_text = kHostBuiltin;
        used_stub_builtin = true;
        used_compiler_builtin = false;
    }

    {
        std::ofstream irf(ir_file, std::ios::trunc);
        if (!irf.is_open()) {
            throw std::runtime_error("Cannot write IR file: " + ir_file.string());
        }
        irf << ir_text;
    }
    {
        std::ofstream bf(builtin_file, std::ios::trunc);
        if (!bf.is_open()) {
            throw std::runtime_error("Cannot write builtin file: " + builtin_file.string());
        }
        bf << builtin_text;
    }

    std::cout << "  Builtin source: "
              << (used_compiler_builtin ? "compiler output" : used_ref_builtin ? "IR-1/builtin" : "host stub")
              << std::endl;

    // 2. Compile LLVM IR to executable
    std::cout << "  Compiling IR to executable..." << std::endl;
    std::string llc_cmd;
#ifdef _WIN32
    llc_cmd = std::string("llc -mtriple=x86_64-pc-windows-msvc -o ") + quote(asm_file) + " " + quote(ir_file);
#else
    llc_cmd = std::string("llc -mtriple=x86_64-pc-linux-gnu -o ") + quote(asm_file) + " " + quote(ir_file);
#endif
    auto [llc_ret, llc_err] = execute_command(llc_cmd);

    if (llc_ret != 0) {
        throw std::runtime_error("llc failed (likely unsupported IR): " + llc_err);
    }

    asm_file = test_file.parent_path() / (base_name + ".s");

    // 4. Assemble and link (include builtin.c)
    std::cout << "  Assembling and linking..." << std::endl;
#ifdef _WIN32
    std::string clang_cmd = std::string("clang ") + quote(asm_file) + " " + quote(builtin_file) + " -o " + quote(exe_file);
#else
    std::string clang_cmd = std::string("clang -no-pie ") + quote(asm_file) + " " + quote(builtin_file) + " -o " + quote(exe_file);
#endif
    auto [clang_ret, clang_err] = execute_command(clang_cmd);

    if (clang_ret != 0) {
        throw std::runtime_error("clang failed (likely unsupported IR): " + clang_err);
    }

    // 4. Run program and capture output
    std::cout << "  Running program..." << std::endl;
    std::string input_data = "";
    if (fs::exists(in_file)) {
        input_data = read_file_content(in_file);
    }

    constexpr int kRunTimeoutSeconds = 8;
    std::string run_cmd = quote(exe_file);
    auto [run_ret, run_output] = execute_command(run_cmd, input_data, kRunTimeoutSeconds);

    // 5. Compare output
    if (fs::exists(out_file)) {
        std::string expected_output = read_file_content(out_file);
        auto trim_newlines = [](std::string s) {
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            return s;
        };
        std::string expected_norm = trim_newlines(expected_output);
        std::string got_norm = trim_newlines(run_output);
        auto strip_all_newlines = [](std::string s) {
            s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
            s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
            return s;
        };

        if (expected_output == run_output || expected_norm == got_norm ||
            strip_all_newlines(expected_norm) == strip_all_newlines(got_norm)) {
            std::cout << "  \u2713 Test passed" << std::endl;
            return true;
        } else {
            std::cout << "  \u2717 Test failed" << std::endl;
            std::cout << "  Expected:\n" << expected_output << std::endl;
            std::cout << "  Got:\n" << run_output << std::endl;
            return false;
        }
    } else {
        std::cout << "  Warning: No .out file to compare against" << std::endl;
        std::cout << "  Output:\n" << run_output << std::endl;
        return true;
    }
}

int main(int argc, char* argv[]) {
    fs::path exe_dir = fs::absolute(argv[0]).parent_path();

    std::vector<std::string> filters;
    for (int i = 1; i < argc; ++i) {
        if (argv[i]) filters.emplace_back(argv[i]);
    }
    if (filters.empty()) {
        filters = {"comprehensive1", "comprehensive19", "comprehensive26"};
    }

    auto should_run = [&](const fs::path &p) {
        if (filters.empty()) return true;
        std::string s = p.string();
        for (const auto &f : filters) {
            if (s.find(f) != std::string::npos) return true;
        }
        return false;
    };

    std::vector<fs::path> compiler_candidates = {
        exe_dir / "compiler",
        exe_dir / "code",
        exe_dir / "build" / "compiler",
        exe_dir / "build" / "code",
        exe_dir.parent_path() / "build" / "compiler",
        exe_dir.parent_path() / "build" / "code",
        fs::current_path() / "build" / "compiler",
        fs::current_path() / "build" / "code",
        fs::current_path() / "compiler",
        fs::current_path() / "code"
    };
    fs::path compiler_path;
    for (const auto &c : compiler_candidates) {
        std::error_code ec;
        if (fs::exists(c, ec)) {
            compiler_path = c;
            break;
        }
    }

    if (compiler_path.empty()) {
        std::cerr << "Cannot find compiler binary (tried ./compiler, ./build/compiler, ../build/compiler)" << std::endl;
        return 1;
    }

    std::cout << "Using compiler: " << compiler_path << std::endl;

    auto collect_tests = [&](const fs::path &root, std::vector<fs::path> &out) {
        std::error_code ec;
        if (!fs::exists(root, ec)) return;
        for (const auto &entry : fs::recursive_directory_iterator(root, ec)) {
            if (entry.is_regular_file(ec) && entry.path().extension() == ".rx") {
                out.push_back(entry.path().lexically_normal());
            }
        }
    };

    std::vector<fs::path> test_files;
    std::vector<fs::path> root_candidates;
    for (const std::string &name : {"IR-1/src"}) {
        root_candidates.push_back(fs::current_path() / "test_case" / name);
        root_candidates.push_back(exe_dir / "test_case" / name);
        root_candidates.push_back(exe_dir.parent_path() / "test_case" / name);
    }
    for (const auto &root : root_candidates) {
        collect_tests(root, test_files);
    }

    // Load reference builtin from IR-1/builtin if present (used when compiler emits RISC-V asm builtin)
    std::string ref_builtin;
    for (const auto &root : root_candidates) {
        fs::path candidate = root.parent_path() / "builtin" / "builtin.c";
        std::error_code ec;
        if (!fs::exists(candidate, ec)) continue;
        ref_builtin = read_file_content(candidate);
        if (!ref_builtin.empty()) {
            std::cout << "Loaded reference builtin: " << candidate << std::endl;
            break;
        }
    }

    std::sort(test_files.begin(), test_files.end(), [](const fs::path &a, const fs::path &b) {
        return a.string() < b.string();
    });
    test_files.erase(std::unique(test_files.begin(), test_files.end(), [](const fs::path &a, const fs::path &b) {
        return a.string() == b.string();
    }), test_files.end());

    if (test_files.empty()) {
        std::cout << "No .rx test files found under test_case/IR-1/src (checked relative to cwd and exe dir)" << std::endl;
        return 0;
    }

    std::cout << "Found " << test_files.size() << " test files" << std::endl;

    bool all_passed = true;
    for (const auto &test_file : test_files) {
        if (!should_run(test_file)) continue;
        try {
            if (!run_ir_test(test_file, compiler_path, ref_builtin)) {
                all_passed = false;
            }
        } catch (const std::exception &e) {
            std::cerr << "  Exception: " << e.what() << std::endl;
            all_passed = false;
        }
    }

    return all_passed ? 0 : 1;
}

//test脚本由copilot生成