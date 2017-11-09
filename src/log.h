#ifndef LOG_H
#define LOG_H

#include <memory>
#include <ostream>
#include <string>

class Logger
{
public:
  static Logger *current();

  static std::string to_file(const char *filename);

  static std::string to_stderr();

  static std::string to_stdout();

  static std::string disable();

  Logger() = default;

  virtual ~Logger() = default;

  virtual Logger *prefix(const char *file, int line) = 0;

  virtual std::ostream &stream() = 0;

  virtual std::string get_error() const { return ""; }

  Logger(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger &operator=(Logger &&) = delete;
};

std::string plural(long quantity, const std::string &singular_form, const std::string &plural_form);

std::string plural(long quantity, const std::string &singular_form);

#define LOGGER (Logger::current()->prefix(__FILE__, __LINE__)->stream())

#endif
