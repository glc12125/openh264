#include "codec_api.h"

#ifndef __DECODERLISTENER_H__
#define __DECODERLISTENER_H__

namespace DecoderListener {

struct Callback {
   virtual void onDecodeFrame (uint8_t** data, SBufferInfo * sDstBufInfo, unsigned long timestamp) = 0;
   virtual void onFinishedDecodingStream(bool success) = 0;
};

} // End of namespace DecoderListener

#endif // __DECODERLISTENER_H__
