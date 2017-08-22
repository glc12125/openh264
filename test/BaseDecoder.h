#ifndef __BASEDECODER_H__
#define __BASEDECODER_H__

#include <thread>
#include <atomic>
#include "test_stdint.h"
#include <limits.h>
#include <istream>
#include "codec_api.h"
#include <DecoderListener.h>
#include "utils/BufferedData.h"
#include <OrderedImageReader.h>
#include <WorkerItem.h>
#include <YuvImageData.h>

class BaseDecoder : public RoboK::ImageReader::ImageDataListener {
 public:
  struct Plane {
    const uint8_t* data;
    int width;
    int height;
    int stride;
  };

  struct Frame {
    Plane y;
    Plane u;
    Plane v;
  };

  BaseDecoder();
  // Implementation of ImageDataListener::fisnihedReadingImageData
  virtual void fisnihedReadingImageData(const RoboK::YuvImageData& imageData, bool success);
  int32_t SetUp();
  void TearDown();
  void DecodeStream (std::istream& stream, DecoderListener::Callback* cbk);
  void imageDataFetcherMain();

  ISVCDecoder* decoder_;

 private:
  void DecodeFrame (const uint8_t* src, size_t sliceSize, DecoderListener::Callback* cbk);

  unsigned long lastTimestamp_;
  std::atomic_bool imageDataFetcherRunning_;
  std::atomic_int availableImageDataCount_;
  RoboK::OrderedImageReader orderedImageReader_;
  RoboK::WorkerThread<RoboK::OrderedImageReader> imageReaderThread_;

  /// The Thread that constantly tries to take ImageData out of the
  /// priority_queue
  std::thread imageDataFetcher_;

  BufferedData buf_;
  enum {
    OpenFile,
    Decoding,
    EndOfStream,
    End
  } decodeStatus_;
};

#endif //__BASEDECODER_H__
