#ifndef LOG_H
#define LOG_H

#include <cstdarg>

#define OPERATOR(type) virtual Logger& operator<<(type arg) = 0

class Logger {
public:
  static Logger* current();
  static void toFile(const char *filename);
  static void disable();

  virtual ~Logger() {};

  virtual Logger& prefix(const char *file, int line) = 0;
  virtual Logger& operator<<(const char *message) = 0;
};

#define LOGGER (Logger::current()->prefix())

#endif
