#ifndef LOG_H
#define LOG_H

#include <ostream>
#include <string>

class Logger
{
public:
  virtual ~Logger() {}

  static Logger *current();
  static void to_file(const char *filename);
  static void to_stderr();
  static void to_stdout();
  static void disable();

  virtual Logger *prefix(const char *file, int line) = 0;
  virtual std::ostream &stream() = 0;
};

std::string plural(long quantity, const std::string &singular_form, const std::string &plural_form);
std::string plural(long quantity, const std::string &singular_form);

#define LOGGER (Logger::current()->prefix(__FILE__, __LINE__)->stream())

#endif
