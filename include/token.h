#ifndef TOKEN_H
#define TOKEN_H

/**
 * 标记(Token)定义模块
 * 
 * 本模块定义了词法分析器产生的标记类型和结构。
 * 标记是词法分析的基本单位，代表源代码中的有意义的单元，
 * 如关键字、标识符、运算符等。
 */

#include <string>
#include <unordered_set>

/**
 * 语言关键字集合
 * 
 * 包含Rust风格语言的所有保留关键字，
 * 这些关键字有特殊的语法含义，不能用作标识符。
 */
static const std::unordered_set<std::string> keywords = {
  "as", "break", "const", "continue",
  "crate", "dyn", "else", "enum", "exit",
  "false",  "fn",  "for",  "if",
  "impl",  "in",  "let",  "loop",
  "match",  "mod",  "move",  "mut",
  "pub", "ref",  "return",  "self",
  "Self",  "static",  "struct",  "super",
  "trait",  "true",  "type",  "unsafe",
  "use",  "where",  "while"
};

/**
 * 标记类型枚举
 * 
 * 定义了所有可能的标记类型，用于分类和识别不同种类的标记。
 */
enum class TokenKind {
  /** 关键字 */
  Keyword,
  /** 标识符 */
  Identifier,
  /** 数字字面量 */
  Number,
  /** 浮点数字面量 */
  Float,
  /** 字符串字面量 */
  String,
  /** 运算符 */
  Operator,
  /** 比较运算符 */
  Comparison,
  /** 标点符号 */
  Punctuation,
  /** 文件末尾 */
  Eof,
  /** 未知/错误标记 */
  Unknown
};

/**
 * 标记类
 * 
 * 表示词法分析产生的标记，包含类型、文本内容和位置信息。
 * 标记是语法分析的基本输入单元。
 */
class Token {
public:
  /**
   * 构造标记
   * @param kind 标记类型
   * @param text 标记文本内容
   * @param pos 标记在源代码中的位置
   */
  Token(TokenKind, const std::string &, size_t);

  /**
   * 获取标记类型
   * @return 标记类型
   */
  TokenKind kind() const;
  
  /**
   * 获取标记文本内容
   * @return 标记文本内容
   */
  std::string text() const;

  /**
   * 获取标记在源代码中的位置
   * @return 标记位置
   */
  int position() const;

private:
  TokenKind kind_;   // 标记类型
  std::string text_; // 标记文本内容
  int pos_;          // 标记在源代码中的位置（调试用）
};
#endif //TOKEN_H
