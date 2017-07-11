#include <iostream>
#include <fstream>
#include <cstdarg>
#include <uv.h>

#include "log.h"

using std::cerr;
using std::endl;
using std::ios_base;
using std::ofstream;

class NullLogger : public Logger {
public:
  virtual Logger& prefix(const char *file, int line) override
  {
    return *this;
  }

  virtual Logger& operator<<(const char *message) override
  {
    return *this;
  }
};

class FileLogger : public Logger {
public:
  FileLogger(const char *filename) :
    logStream{filename, std::ios::out | std::ios::app}
  {
    prefix(__FILE__, __LINE__);
    logStream << "FileLogger opened." << endl;
  }

  virtual Logger& prefix(const char *file, int line) override
  {
    logStream << "[" << file << ":" << line << "] ";
    return *this;
  }

  virtual Logger& operator<<(const char *message) override
  {
    logStream << message;
    return *this;
  }

private:
  ofstream logStream;
};

static uv_key_t current_logger_key;
static const NullLogger theNullLogger;
static uv_once_t make_key_once = UV_ONCE_INIT;

static void make_key()
{
  uv_key_create(&current_logger_key);
}

Logger* Logger::current()
{
  uv_once(&make_key_once, &make_key);

  Logger* logger = (Logger*) uv_key_get(&current_logger_key);

  if (logger == nullptr) {
    uv_key_set(&current_logger_key, (void*) &theNullLogger);
  }

  return logger;
}

static void replaceLogger(const Logger *newLogger)
{
  Logger *prior = Logger::current();
  if (prior != &theNullLogger) {
    delete prior;
  }

  uv_key_set(&current_logger_key, (void*) newLogger);
}

void Logger::toFile(const char *filename)
{
  replaceLogger(new FileLogger(filename));
}

void Logger::disable()
{
  replaceLogger(&theNullLogger);
}
