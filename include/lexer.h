#ifndef LEXER_H
#define LEXER_H

/**
 * 词法分析器模块
 * 
 * 本模块负责将源代码文本转换为标记(Token)序列。
 * 词法分析是编译过程的第一步，将字符流转换为有意义的标记流，
 * 为后续的语法分析做准备。
 */

#include <regex>
#include <boost/regex.hpp>
#include "token.h"
#include <vector>
#include <string>

/**
 * 词法分析器类
 * 
 * 负责将源代码文本分解为一系列标记(Token)，
 * 支持Rust风格的语法，包括关键字、标识符、运算符、标点符号等。
 */
class Lexer {
public:
  /**
   * 构造词法分析器
   * @param src 源代码字符串
   */
  Lexer(const std::string &);

  /**
   * 获取下一个标记
   * @return 下一个标记
   */
  Token next_token();

  /**
   * 检查是否已到达源代码末尾
   * @return 如果到达末尾返回true，否则返回false
   */
  bool is_eof() const;

  /**
   * 将整个源代码标记化
   * @return 标记序列
   */
  std::vector<Token> tokenize_all();

  /**
   * 根据字符位置获取行号和列号
   * @param pos 字符位置
   * @return 行号和列号的pair
   */
  std::pair<int, int> getLineAndCol(int);

private:
  /**
   * 前进到下一个字符
   */
  void advance();

  /**
   * 尝试匹配正则表达式
   * @param re 正则表达式
   * @param src 源字符串
   * @param pos 当前位置
   * @param matched 匹配结果
   * @return 是否匹配成功
   */
  bool match(const boost::regex &, const std::string &, size_t &, std::string &);

  /**
   * 跳过空白字符
   */
  void skip_whitespace();

  /**
   * 跳过注释（支持单行和多行注释）
   */
  void skip_comment();

  std::string src_;  // 源代码字符串
  size_t pos_;       // 当前位置
  char currentChar;   // 当前字符
};
#endif //LEXER_H
