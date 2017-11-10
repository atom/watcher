#include <cerrno>
#include <string>
#include <unistd.h>
#include <utility>

#include "../../errable.h"
#include "../../helper/linux/helper.h"
#include "../../result.h"
#include "pipe.h"

using std::move;
using std::string;

const char WAKE = '!';

Pipe::Pipe() : read_fd{0}, write_fd{0}
{
  int fds[2] = {0, 0};
  int err = pipe2(fds, O_CLOEXEC | O_NONBLOCK);
  if (err == -1) {
    report_if_error<>(errno_result<>("Unable to open pipe"));
    freeze();
    return;
  }

  read_fd = fds[0];
  write_fd = fds[1];
  freeze();
}

Pipe::~Pipe()
{
  close(read_fd);
  close(write_fd);
}

Result<> Pipe::signal()
{
  ssize_t result = write(write_fd, &WAKE, sizeof(char));
  if (result == -1) {
    int write_errno = errno;

    if (write_errno == EAGAIN || write_errno == EWOULDBLOCK) {
      // If the kernel buffer is full, that means there's already a pending signal.
      return ok_result();
    }

    return errno_result<>("Unable to write a byte to the pipe", write_errno);
  }
  if (result == 0) {
    return error_result("No bytes written to pipe");
  }

  return ok_result();
}

Result<> Pipe::consume()
{
  const size_t BUFSIZE = 256;
  char buf[BUFSIZE];
  ssize_t result = 0;

  do {
    result = read(read_fd, &buf, BUFSIZE);
  } while (result > 0);

  if (result < 0) {
    int read_errno = errno;

    if (read_errno == EAGAIN || read_errno == EWOULDBLOCK) {
      // Nothing left to read.
      return ok_result();
    }

    return errno_result<>("Unable to read from pipe", read_errno);
  }

  return ok_result();
}
