#ifndef PIPE_H
#define PIPE_H

#include <string>

#include "../../result.h"
#include "../../errable.h"

class Pipe : public SyncErrable {
public:
  Pipe(const std::string &name);
  ~Pipe();

  Result<> signal();

  Result<> consume();

  int get_read_fd() const { return read_fd; }

private:
  int read_fd;
  int write_fd;
};

#endif
