#ifndef __BASEENCODER_H__
#define __BASEENCODER_H__

#include "codec_api.h"
#include "codec_app_def.h"
#include "utils/InputStream.h"
#include "sha1.h"
#include "test_stdint.h"

class BaseEncoder {
 public:
  struct Callback {
    virtual void onEncodeFrame (const SFrameBSInfo& frameInfo) = 0;
    virtual void finishedEncoding() = 0;
  };

  BaseEncoder();
  ~BaseEncoder();
  void EncodeFile (const char* fileName, SEncParamExt* pEncParamExt, Callback* cbk);
  void EncodeBuffer (const uint8_t* buffer, int size, SEncParamExt* pEncParamExt, Callback* cbk);
  void EncodeStream (InputStream* in,  SEncParamExt* pEncParamExt, Callback* cbk);

  ISVCEncoder* encoder_;
 private:
  SHA1Context d_ctx;
};

#endif //__BASEENCODER_H__
