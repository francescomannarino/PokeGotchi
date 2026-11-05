// String di Arduino ridotta a ciò che usano il firmware (console, protocollo SD,
// salvataggi) e gli overload di Arduino_GFX (getTextBounds/printCenterText).
#pragma once
#include <stdlib.h>
#include <string>

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))

class String {
public:
  String(const char *s = "") : s_(s ? s : "") {}
  String(const std::string &s) : s_(s) {}
  unsigned int length() const { return (unsigned int)s_.size(); }
  const char *c_str() const { return s_.c_str(); }

  bool operator==(const char *o) const { return s_ == (o ? o : ""); }
  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator!=(const char *o) const { return !(*this == o); }
  bool operator<(const String &o) const { return s_ < o.s_; }
  String &operator+=(const String &o) { s_ += o.s_; return *this; }
  String &operator+=(const char *o) { s_ += o ? o : ""; return *this; }
  String &operator+=(char c) { s_ += c; return *this; }
  friend String operator+(const String &a, const String &b) { return String(a.s_ + b.s_); }
  friend String operator+(const String &a, const char *b) { return String(a.s_ + (b ? b : "")); }
  friend String operator+(const char *a, const String &b) { return String((a ? a : "") + b.s_); }

  bool startsWith(const char *p) const { return s_.compare(0, strlen_(p), p) == 0; }
  bool endsWith(const char *p) const {
    size_t n = strlen_(p);
    return s_.size() >= n && s_.compare(s_.size() - n, n, p) == 0;
  }
  int indexOf(char c) const { return pos_(s_.find(c)); }
  int indexOf(const char *p) const { return pos_(s_.find(p)); }
  int lastIndexOf(char c) const { return pos_(s_.rfind(c)); }
  // come Arduino: gli indici fuori misura si accorciano, (a, b) con a > b si scambiano
  String substring(unsigned int a) const { return a >= s_.size() ? String() : String(s_.substr(a)); }
  String substring(unsigned int a, unsigned int b) const {
    if (a > b) { unsigned int t = a; a = b; b = t; }
    if (a >= s_.size()) return String();
    return String(s_.substr(a, b - a));
  }
  void trim() {
    size_t i = s_.find_first_not_of(" \t\r\n");
    if (i == std::string::npos) { s_.clear(); return; }
    s_ = s_.substr(i, s_.find_last_not_of(" \t\r\n") - i + 1);
  }
  long toInt() const { return atol(s_.c_str()); }

private:
  static size_t strlen_(const char *p) { return std::char_traits<char>::length(p); }
  static int pos_(size_t p) { return p == std::string::npos ? -1 : (int)p; }
  std::string s_;
};
