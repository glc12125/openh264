#ifndef __FILEOUTPUTSTREAM_H__
#define __FILEOUTPUTSTREAM_H__

#include <fstream>
#include "OutputStream.h"

class FileOutputStream : public OutputStream {
 public:
  bool Open (const char* fileName) {
    file_.open (fileName, std::ios_base::out | std::ios_base::binary);
    return file_.is_open();
  }

  int write (void* ptr, size_t len) {
    if (!file_.good()) {
      return -1;
    }
    size_t before = file_.tellp();
    if (file_.write(static_cast<char*> (ptr), len)) 
    {
      size_t after = file_.tellp();
      return (int)(after - before);
    } else {
      return 0; // something is wrong
    }
  }
 private:
  std::ofstream file_;
};

#endif //__FILEOUTPUTSTREAM_H__
