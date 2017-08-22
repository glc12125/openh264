#ifndef __BUFFERINPUTSTREAM_H__
#define __BUFFERINPUTSTREAM_H__

#include <fstream>
#include "InputStream.h"

class BufferInputStream : public InputStream {
 public:
  BufferInputStream(const uint8_t* src, int size) : currentPos_(0), size_(size), src_(src) {}
  int read (void* ptr, size_t len) {
    if (src_ == NULL || ptr == NULL || currentPos_ >= size_) {
      return -1;
    }
    memcpy(ptr, src_ + currentPos_, sizeof(unsigned char) * len);
    currentPos_ += len;
    return (int) len;
  }
 private:
  unsigned long currentPos_;
  int size_;
  const uint8_t * src_;
};

#endif //__BUFFERINPUTSTREAM_H__
