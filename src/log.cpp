#include <fstream>
#include <iostream>
#include <string>
#include <uv.h>

#include "log.h"

#include <iomanip>

using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::ofstream;
using std::ostream;
using std::setw;
using std::string;
using std::to_string;

class NullLogger : public Logger
{
public:
  NullLogger()
  {
    //
  }

  virtual Logger *prefix(const char *file, int line) override { return this; }

  virtual ostream &stream() override { return unopened; }

private:
  ofstream unopened;
};

class FileLogger : public Logger
{
public:
  FileLogger(const char *filename) : log_stream{filename, std::ios::out | std::ios::app}
  {
    prefix(__FILE__, __LINE__);
    log_stream << "FileLogger opened." << endl;
  }

  virtual Logger *prefix(const char *file, int line) override
  {
    log_stream << "[" << setw(15) << file << ":" << setw(3) << dec << line << "] ";
    return this;
  }

  virtual ostream &stream() override { return log_stream; }

private:
  ofstream log_stream;
};

class StderrLogger : public Logger
{
public:
  StderrLogger()
  {
    prefix(__FILE__, __LINE__);
    cerr << "StderrLogger opened." << endl;
  }

  virtual Logger *prefix(const char *file, int line) override
  {
    cerr << "[" << setw(15) << file << ":" << setw(3) << dec << line << "] ";
    return this;
  }

  virtual ostream &stream() override { return cerr; }
};

class StdoutLogger : public Logger
{
public:
  StdoutLogger()
  {
    prefix(__FILE__, __LINE__);
    cout << "StdoutLogger opened." << endl;
  }

  virtual Logger *prefix(const char *file, int line) override
  {
    cout << "[" << setw(15) << file << ":" << setw(3) << dec << line << "] ";
    return this;
  }

  virtual ostream &stream() override { return cout; }
};

static uv_key_t current_logger_key;
static NullLogger the_null_logger;
static uv_once_t make_key_once = UV_ONCE_INIT;

static void make_key()
{
  uv_key_create(&current_logger_key);
}

Logger *Logger::current()
{
  uv_once(&make_key_once, &make_key);

  Logger *logger = (Logger *) uv_key_get(&current_logger_key);

  if (logger == nullptr) {
    uv_key_set(&current_logger_key, (void *) &the_null_logger);
    logger = &the_null_logger;
  }

  return logger;
}

static void replace_logger(const Logger *new_logger)
{
  Logger *prior = Logger::current();
  if (prior != &the_null_logger) {
    delete prior;
  }

  uv_key_set(&current_logger_key, (void *) new_logger);
}

void Logger::to_file(const char *filename)
{
  replace_logger(new FileLogger(filename));
}

void Logger::to_stderr()
{
  replace_logger(new StderrLogger());
}

void Logger::to_stdout()
{
  replace_logger(new StdoutLogger());
}

void Logger::disable()
{
  replace_logger(&the_null_logger);
}

string plural(long quantity, const string &singular_form, const string &plural_form)
{
  string result("");
  result += to_string(quantity);
  result += " ";

  if (quantity == 1) {
    result += singular_form;
  } else {
    result += plural_form;
  }

  return result;
}

string plural(long quantity, const string &singular_form)
{
  string plural_form(singular_form + "s");
  return plural(quantity, singular_form, plural_form);
}
