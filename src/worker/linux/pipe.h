#ifndef PIPE_H
#define PIPE_H

#include <string>

#include "../../errable.h"
#include "../../result.h"

// RAII wrapper for a Linux pipe created with pipe(2). We don't care about the actual data transmitted.
class Pipe : public SyncErrable
{
public:
  // Construct a new Pipe identified in Result<> errors with a specified name.
  Pipe(const std::string &name);

  // Deallocate and close() the underlying pipe file descriptor.
  ~Pipe();

  // Write a byte to the pipe to inform readers that data is available.
  Result<> signal();

  // Read and discard all data waiting on the pipe to prepare for a new signal.
  Result<> consume();

  // Access the file descriptor that should be polled for data.
  int get_read_fd() const { return read_fd; }

private:
  int read_fd;
  int write_fd;
};

#endif
