#include <errno.h>
#include <fcntl.h>
#include <string>
#include <utility>
#include <unistd.h>

#include "../../errable.h"
#include "../../helper/linux/helper.h"
#include "../../result.h"
#include "pipe.h"

using std::move;
using std::string;

const char WAKE = '!';

Pipe::Pipe(string &&name) : SyncErrable(move(name)), read_fd{0}, write_fd{0}
{
  int fds[2] = {0, 0};
  int err = pipe(fds);
  if (err == -1) {
    Errable::report_error<>(errno_result<>("Unable to open pipe"));
    return;
  }

  read_fd = fds[0];
  err = fcntl(read_fd, F_SETFL, O_NONBLOCK);
  if (err == -1) {
    Errable::report_error<>(errno_result<>("Unable to set read fd to non-blocking mode"));
    return;
  }
  err = fcntl(read_fd, F_SETFD, FD_CLOEXEC);
  if (err == -1) {
    Errable::report_error<>(errno_result<>("Unable to change read fd to close-on-exec mode"));
    return;
  }

  write_fd = fds[1];
  err = fcntl(write_fd, F_SETFL, O_NONBLOCK);
  if (err == -1) {
    Errable::report_error<>(errno_result<>("Unable to set write fd to non-blocking mode"));
    return;
  }
  err = fcntl(write_fd, F_SETFD, FD_CLOEXEC);
  if (err == -1) {
    Errable::report_error<>(errno_result<>("Unable to change write fd to close-on-exec mode"));
    return;
  }
}

Pipe::~Pipe()
{
  close(read_fd);
  close(write_fd);
}

Result<> Pipe::signal()
{
  if (!is_healthy()) return Errable::health_err_result<>();

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
  if (!is_healthy()) return Errable::health_err_result<>();

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
