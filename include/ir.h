#ifndef IR_H
#define IR_H

/**
 * 中间表示(IR)生成模块
 * 
 * 本模块负责将经过语义分析的AST转换为LLVM IR。
 * IR是编译器前端和后端之间的桥梁，便于优化和代码生成。
 * 本实现生成文本形式的LLVM IR，无需链接LLVM库。
 */

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <sstream>
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

namespace IRGen {
  namespace fs = std::filesystem;

  struct Value {
    std::string name;
    std::string type; // "i64", "i1", or "ptr"
    bool arrayAlloca = false; // true if the pointer comes from alloca [N x i64]
    size_t slots = 1; // total slots when arrayAlloca is true or aggregate pointer
    bool isLValuePtr = false; // true when the ptr represents an lvalue address (from & or reference)
  };

  struct TypeLayout {
    size_t slots = 1; // number of i64 slots occupied
    bool aggregate = false; // true for structs/arrays
    bool arrayLike = false; // true for arrays (including array fields)
  };

  struct FunctionCtx {
    std::string name;
    bool returnsVoid = false;
    bool aggregateReturn = false;
    TypeLayout retLayout;
    std::string retPtr;
    int tempId = 0;
    int labelId = 0;
    std::ostringstream body;
    std::vector<std::string> entryAllocas;
    std::string currentLabel;

    struct VarInfo {
      TypeRef type;
      TypeLayout layout;
      std::string ptr;
      bool arrayAlloca = false;
      bool isRefBinding = false; // true when the variable stores a reference (raw pointer)
      bool refIsRawSlot = false; // true if the reference pointer itself is stored in the alloca slot
    };

    std::unordered_map<std::string, VarInfo> vars;
    std::string breakLabel;
    std::string continueLabel;
    bool terminated = false;
  };

  //for debug, write to .ll
  fs::path deriveLlPath(const std::string &inputPath);
  std::string freshTemp(FunctionCtx &fn);
  std::string freshLabel(FunctionCtx &fn, const std::string &prefix);

  Value toI64(FunctionCtx &fn, const Value &v);
  void copySlots(FunctionCtx &fn, const Value &src, const Value &dst, size_t count);
  TypeRef stripRef(const TypeRef &t);
  Value ensureBool(FunctionCtx &fn, const Value &v);

  TypeLayout layoutOf(const TypeRef &t);
  bool isRefType(const TypeRef &t);
  bool needsByValue(const TypeRef &t);
  bool needsByRef(const TypeRef &t);

  Value emitExpr(FunctionCtx &fn, ExprAST *expr);
  void emitStmt(FunctionCtx &fn, StmtAST *stmt);

  // 全局变量声明
  extern std::unordered_map<std::string, size_t> g_declArity;
  extern std::unordered_set<std::string> g_definedFuncs;
  extern SemanticAnalyzer *g_analyzer;

  TypeRef exprType(ExprAST *expr);
  std::optional<int64_t> constInt(ExprAST *e);
  Value fallbackValue();
  Value emitNumber(int64_t v);
  Value emitBool(bool v);
  void emitBuiltinCToStderr();

  bool generate_ir(BlockStmtAST *program, SemanticAnalyzer &analyzer, const std::string &inputPath, bool emitLLVM);
}
#endif // IR_H
