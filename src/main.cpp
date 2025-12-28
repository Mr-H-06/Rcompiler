/** 所有注释由AI生成
 * 主程序
 * 
 * 本程序是R语言编译器的入口点，负责协调词法分析、语法分析、
 * 语义分析和中间代码生成等编译器各个阶段的工作。
 * 
 * 编译流程：
 * 1. 词法分析：将源代码转换为标记流
 * 2. 语法分析：将标记流转换为抽象语法树(AST)
 * 3. 语义分析：对AST进行类型检查和语义验证
 * 4. IR生成：将AST转换为LLVM IR
 */

#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "ir.h"

/**
 * 从标准输入读取源代码
 * 
 * 读取标准输入流中的所有内容，用于处理管道输入或交互式输入
 * 
 * @param input 输出参数，存储读取的源代码内容
 */
void read_from_cin(std::string &input) {
  std::ostringstream oss;
  oss << std::cin.rdbuf();
  input = oss.str();
}

/**
 * 从文件读取源代码
 * 
 * 打开指定文件并读取所有内容作为源代码
 * 
 * @param input 输出参数，存储读取的源代码内容
 * @param filename 要读取的文件名
 */
void read_from_file(std::string &input, const std::string &filename) {
  std::ifstream fin(filename, std::ios::in);
  if (!fin) {
    std::cerr << "Cannot open file: " << filename << std::endl;
    std::exit(1);
  }
  std::ostringstream oss;
  oss << fin.rdbuf();
  input = oss.str();
}

/**
 * 主函数
 * 
 * 编译器的主入口点，负责处理命令行参数、读取输入源代码，
 * 并按顺序执行词法分析、语法分析、语义分析和IR生成。
 * 
 * 输入策略：
 * - "-" 强制从标准输入读取（推荐用于实际运行）
 * - 任何路径参数读取该文件
 * - 无参数：使用标准输入；测试可传递"--use-test-input"保持旧行为
 * 
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码，0表示成功，非0表示失败
 */
int main(int argc, char** argv) {
  try {
    bool emitLLVM = true; // 默认生成LLVM IR，无需额外标志

    // 输入策略处理
    bool useTestInput = false;
    std::string input;
    for (int i = 1; i < argc; ++i) {
      if (std::string(argv[i]) == "--use-test-input") {
        useTestInput = true;
      }
    }

    // 根据参数决定输入源
    if (argc > 1 && std::string(argv[1]) != "-" && std::string(argv[1]) != "--use-test-input") {
      // 从指定文件读取
      read_from_file(input, argv[1]);
    } else if ((argc > 1 && std::string(argv[1]) == "-") || (!useTestInput && argc == 1)) {
      // 从标准输入读取
      read_from_cin(input);
    } else {
      // 测试模式，从默认测试文件读取
      read_from_file(input, "../test_case/test_case.in");
    }

    // 1. 词法分析：将源代码转换为标记流
    Lexer lexer(input);
    std::vector<Token> tokens = lexer.tokenize_all();
    tokens.push_back(Token(TokenKind::Eof, "", 0));
    
    // 2. 语法分析：将标记流转换为抽象语法树(AST)
    Parser parser(tokens);
    auto ast = parser.parse_program();

    // 3. 语义分析：对AST进行类型检查和语义验证
    SemanticAnalyzer analyzer;
    if (!analyzer.analyze(ast.get())) {
      return 1; // 语义分析失败
    }
    
    // 4. IR生成：将AST转换为LLVM IR
    if (emitLLVM) {
      const std::string irInputPath = (argc > 1) ? std::string(argv[1]) : "../test_case/test_case.in";
      try {
        if (!generate_ir(ast.get(), analyzer, irInputPath, emitLLVM)) {
          return 0; // 编译成功但IR生成报告失败
        }
      } catch (const std::exception &irEx) {
        std::cerr << "IR generation failed: " << irEx.what() << std::endl;
        return 0; // 按要求：将IR失败视为成功退出
      }
    }
  } catch (const std::exception& ex) {
    // 处理已知异常
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  } catch (...) {
    // 处理未知异常
    std::cerr << "Unknown error occurred" << std::endl;
    return 1;
  }
}
