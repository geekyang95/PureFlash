#include "s5errno.h"

#include <sstream>
#include <string>
#include <string.h>

std::string cpp_strerror(int err)
{
  char buf[128];

  if (err < 0)
    err = -err;
  std::ostringstream oss;
  oss << "(" << err << ") " << strerror_r(err, buf, sizeof(buf));

  return oss.str();
}
