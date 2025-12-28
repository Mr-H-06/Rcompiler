#ifndef IR_H
#define IR_H

/**
 * 中间表示(IR)生成模块
 * 
 * 本模块负责将经过语义分析的AST转换为LLVM IR。
 * IR是编译器前端和后端之间的桥梁，便于优化和代码生成。
 * 本实现生成文本形式的LLVM IR，无需链接LLVM库。
 */

#include <string>
#include "ast.h"
#include "semantic.h"

/**
 * IR生成入口点
 * 
 * 将经过语义分析的程序AST转换为LLVM IR文本。
 * 如果遇到不支持的AST/类型或IO错误，将抛出异常。
 * 
 * @param program 程序的AST根节点
 * @param analyzer 语义分析器，包含类型信息和符号表
 * @param inputPath 输入文件路径，用于错误报告
 * @param emitLLVM 是否输出LLVM IR
 * @return 是否成功生成IR
 * @throws std::exception 当遇到不支持的AST/类型或IO错误时
 */
bool generate_ir(BlockStmtAST *program, SemanticAnalyzer &analyzer, const std::string &inputPath, bool emitLLVM);

#endif // IR_H
