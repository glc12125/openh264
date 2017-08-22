#ifndef __BUFFEREDDATA_H__
#define __BUFFEREDDATA_H__

#include <stddef.h>
#include <stdlib.h>
#include "../test_stdint.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include <stdexcept>

class BufferedData {
 public:
  BufferedData() : data_ (NULL), capacity_ (0), length_ (0) {}

  ~BufferedData() {
    free (data_);
  }

  BufferedData(const BufferedData& other)
  {
    if(capacity_ != other.capacity_)
    {
        initiateData(other.capacity_);
    }
    length_ = other.length_;
    std::copy(other.data_, other.data_ + length_, data_);
  }

  BufferedData& operator=(const BufferedData& other)
  {
    // check for self-assignment
    if(&other == this)
        return *this;
    // reuse storage when possible
    if(capacity_ != other.capacity_)
    {
        initiateData(other.capacity_);
    }
    length_ = other.length_;
    std::copy(other.data_, other.data_ + length_, data_);
    return *this;
  }

  bool PushBack (uint8_t c) {
    if (!EnsureCapacity (length_ + 1)) {
      return false;
    }
    data_[length_++] = c;
    return true;
  }

  bool PushBack (const uint8_t* data, size_t len) {
    if (!EnsureCapacity (length_ + len)) {
      return false;
    }
    std::memcpy (data_ + length_, data, len);
    length_ += len;
    return true;
  }

  size_t PopFront (uint8_t* ptr, size_t len) {
    len = std::min (length_, len);
    std::memcpy (ptr, data_, len);
    std::memmove (data_, data_ + len, length_ - len);
    SetLength (length_ - len);
    return len;
  }

  void PopBack(size_t len) {
    std::memmove (data_, data_, length_ - len);
    SetLength (length_ - len);
  }

  void Clear() {
    length_ = 0;
  }

  void SetLength (size_t newLen) {
    if (EnsureCapacity (newLen)) {
      length_ = newLen;
    } else {
      std::cerr << "unable to alloc memory in SetLength()";
      exit(-1);
    }
  }

  size_t Length() const {
    return length_;
  }

  uint8_t* data() {
    return data_;
  }

 private:
  void initiateData(size_t capacity) {
      uint8_t* data = static_cast<uint8_t*> (realloc (data_, capacity));

      if (!data)
        throw std::runtime_error("Cannot allocate memory for data buffer\n");
      data_ = data;
      capacity_ = capacity;
  }

  bool EnsureCapacity (size_t capacity) {
    if (capacity > capacity_) {
      size_t newsize = capacity * 2;
      initiateData(newsize);
      return true;
    }
    return true;
  }

  uint8_t* data_;
  size_t capacity_;
  size_t length_;
};

#endif //__BUFFEREDDATA_H__
