#ifndef __OUTPUTSTREAM_H__
#define __OUTPUTSTREAM_H__

#include <cstddef>

struct OutputStream {
  virtual int write (void* ptr, size_t len) = 0;
};

#endif //__OUTPUTSTREAM_H__
