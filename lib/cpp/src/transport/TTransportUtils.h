// Copyright (c) 2006- Facebook
// Distributed under the Thrift Software License
//
// See accompanying file LICENSE or visit the Thrift site at:
// http://developers.facebook.com/thrift/

#ifndef _THRIFT_TRANSPORT_TTRANSPORTUTILS_H_
#define _THRIFT_TRANSPORT_TTRANSPORTUTILS_H_ 1

#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <transport/TTransport.h>
// Include the buffered transports that used to be defined here.
#include <transport/TBufferTransports.h>
#include <transport/TFileTransport.h>

namespace apache { namespace thrift { namespace transport {

/**
 * The null transport is a dummy transport that doesn't actually do anything.
 * It's sort of an analogy to /dev/null, you can never read anything from it
 * and it will let you write anything you want to it, though it won't actually
 * go anywhere.
 *
 * @author Mark Slee <mcslee@facebook.com>
 */
class TNullTransport : public TTransport {
 public:
  TNullTransport() {}

  ~TNullTransport() {}

  bool isOpen() {
    return true;
  }

  void open() {}

  void write(const uint8_t* /* buf */, uint32_t /* len */) {
    return;
  }

};


/**
 * TPipedTransport. This transport allows piping of a request from one
 * transport to another either when readEnd() or writeEnd(). The typical
 * use case for this is to log a request or a reply to disk.
 * The underlying buffer expands to a keep a copy of the entire
 * request/response.
 *
 * @author Aditya Agarwal <aditya@facebook.com>
 */
class TPipedTransport : virtual public TTransport {
 public:
  TPipedTransport(boost::shared_ptr<TTransport> srcTrans,
                  boost::shared_ptr<TTransport> dstTrans) :
    srcTrans_(srcTrans),
    dstTrans_(dstTrans),
    rBufSize_(512), rPos_(0), rLen_(0),
    wBufSize_(512), wLen_(0) {

    // default is to to pipe the request when readEnd() is called
    pipeOnRead_ = true;
    pipeOnWrite_ = false;

    rBuf_ = (uint8_t*) std::malloc(sizeof(uint8_t) * rBufSize_);
    wBuf_ = (uint8_t*) std::malloc(sizeof(uint8_t) * wBufSize_);
  }

  TPipedTransport(boost::shared_ptr<TTransport> srcTrans,
                  boost::shared_ptr<TTransport> dstTrans,
                  uint32_t sz) :
    srcTrans_(srcTrans),
    dstTrans_(dstTrans),
    rBufSize_(512), rPos_(0), rLen_(0),
    wBufSize_(sz), wLen_(0) {

    rBuf_ = (uint8_t*) std::malloc(sizeof(uint8_t) * rBufSize_);
    wBuf_ = (uint8_t*) std::malloc(sizeof(uint8_t) * wBufSize_);
  }

  ~TPipedTransport() {
    std::free(rBuf_);
    std::free(wBuf_);
  }

  bool isOpen() {
    return srcTrans_->isOpen();
  }

  bool peek() {
    if (rPos_ >= rLen_) {
      // Double the size of the underlying buffer if it is full
      if (rLen_ == rBufSize_) {
        rBufSize_ *=2;
        rBuf_ = (uint8_t *)std::realloc(rBuf_, sizeof(uint8_t) * rBufSize_);
      }

      // try to fill up the buffer
      rLen_ += srcTrans_->read(rBuf_+rPos_, rBufSize_ - rPos_);
    }
    return (rLen_ > rPos_);
  }


  void open() {
    srcTrans_->open();
  }

  void close() {
    srcTrans_->close();
  }

  void setPipeOnRead(bool pipeVal) {
    pipeOnRead_ = pipeVal;
  }

  void setPipeOnWrite(bool pipeVal) {
    pipeOnWrite_ = pipeVal;
  }

  uint32_t read(uint8_t* buf, uint32_t len);

  void readEnd() {

    if (pipeOnRead_) {
      dstTrans_->write(rBuf_, rPos_);
      dstTrans_->flush();
    }

    srcTrans_->readEnd();

    // If requests are being pipelined, copy down our read-ahead data,
    // then reset our state.
    int read_ahead = rLen_ - rPos_;
    memcpy(rBuf_, rBuf_ + rPos_, read_ahead);
    rPos_ = 0;
    rLen_ = read_ahead;
  }

  void write(const uint8_t* buf, uint32_t len);

  void writeEnd() {
    if (pipeOnWrite_) {
      dstTrans_->write(wBuf_, wLen_);
      dstTrans_->flush();
    }
  }

  void flush();

  boost::shared_ptr<TTransport> getTargetTransport() {
    return dstTrans_;
  }

 protected:
  boost::shared_ptr<TTransport> srcTrans_;
  boost::shared_ptr<TTransport> dstTrans_;

  uint8_t* rBuf_;
  uint32_t rBufSize_;
  uint32_t rPos_;
  uint32_t rLen_;

  uint8_t* wBuf_;
  uint32_t wBufSize_;
  uint32_t wLen_;

  bool pipeOnRead_;
  bool pipeOnWrite_;
};


/**
 * Wraps a transport into a pipedTransport instance.
 *
 * @author Aditya Agarwal <aditya@facebook.com>
 */
class TPipedTransportFactory : public TTransportFactory {
 public:
  TPipedTransportFactory() {}
  TPipedTransportFactory(boost::shared_ptr<TTransport> dstTrans) {
    initializeTargetTransport(dstTrans);
  }
  virtual ~TPipedTransportFactory() {}

  /**
   * Wraps the base transport into a piped transport.
   */
  virtual boost::shared_ptr<TTransport> getTransport(boost::shared_ptr<TTransport> srcTrans) {
    return boost::shared_ptr<TTransport>(new TPipedTransport(srcTrans, dstTrans_));
  }

  virtual void initializeTargetTransport(boost::shared_ptr<TTransport> dstTrans) {
    if (dstTrans_.get() == NULL) {
      dstTrans_ = dstTrans;
    } else {
      throw TException("Target transport already initialized");
    }
  }

 protected:
  boost::shared_ptr<TTransport> dstTrans_;
};

/**
 * TPipedFileTransport. This is just like a TTransport, except that
 * it is a templatized class, so that clients who rely on a specific
 * TTransport can still access the original transport.
 *
 * @author James Wang <jwang@facebook.com>
 */
class TPipedFileReaderTransport : public TPipedTransport,
                                  public TFileReaderTransport {
 public:
  TPipedFileReaderTransport(boost::shared_ptr<TFileReaderTransport> srcTrans, boost::shared_ptr<TTransport> dstTrans);

  ~TPipedFileReaderTransport();

  // TTransport functions
  bool isOpen();
  bool peek();
  void open();
  void close();
  uint32_t read(uint8_t* buf, uint32_t len);
  uint32_t readAll(uint8_t* buf, uint32_t len);
  void readEnd();
  void write(const uint8_t* buf, uint32_t len);
  void writeEnd();
  void flush();

  // TFileReaderTransport functions
  int32_t getReadTimeout();
  void setReadTimeout(int32_t readTimeout);
  uint32_t getNumChunks();
  uint32_t getCurChunk();
  void seekToChunk(int32_t chunk);
  void seekToEnd();

 protected:
  // shouldn't be used
  TPipedFileReaderTransport();
  boost::shared_ptr<TFileReaderTransport> srcTrans_;
};

/**
 * Creates a TPipedFileReaderTransport from a filepath and a destination transport
 *
 * @author James Wang <jwang@facebook.com>
 */
class TPipedFileReaderTransportFactory : public TPipedTransportFactory {
 public:
  TPipedFileReaderTransportFactory() {}
  TPipedFileReaderTransportFactory(boost::shared_ptr<TTransport> dstTrans)
    : TPipedTransportFactory(dstTrans)
  {}
  virtual ~TPipedFileReaderTransportFactory() {}

  boost::shared_ptr<TTransport> getTransport(boost::shared_ptr<TTransport> srcTrans) {
    boost::shared_ptr<TFileReaderTransport> pFileReaderTransport = boost::dynamic_pointer_cast<TFileReaderTransport>(srcTrans);
    if (pFileReaderTransport.get() != NULL) {
      return getFileReaderTransport(pFileReaderTransport);
    } else {
      return boost::shared_ptr<TTransport>();
    }
  }

  boost::shared_ptr<TFileReaderTransport> getFileReaderTransport(boost::shared_ptr<TFileReaderTransport> srcTrans) {
    return boost::shared_ptr<TFileReaderTransport>(new TPipedFileReaderTransport(srcTrans, dstTrans_));
  }
};

}}} // apache::thrift::transport

#endif // #ifndef _THRIFT_TRANSPORT_TTRANSPORTUTILS_H_
