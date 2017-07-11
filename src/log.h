#ifndef LOG_H
#define LOG_H

#include <ostream>

class Logger {
public:
  virtual ~Logger() {}

  static Logger* current();
  static void toFile(const char *filename);
  static void disable();

  virtual Logger* prefix(const char *file, int line) = 0;
  virtual std::ostream& stream() = 0;
};

#define LOGGER (Logger::current()->prefix(__FILE__, __LINE__)->stream())

#endif
