#include "buffer.h"

#include <sys/uio.h>
#include <assert.h>
#include <cstring>

namespace net
{

    char *Buffer::begin() { return &_buffer[0]; }
    const char *Buffer::begin() const { return &_buffer[0]; }
    void Buffer::makeSpace(size_t len)
    {
        if ((prependableBytes() + writableBytes()) < len + kCheapPrepend)
        {
            _buffer.resize(_writer_idx + len);
        }
        else
        {
            assert(kCheapPrepend < _reader_idx);
            size_t datasize = readableBytes();
            std::copy(peek(), peek() + datasize, begin() + kCheapPrepend);
            _reader_idx = kCheapPrepend;
            _writer_idx = _reader_idx + datasize;
            assert(datasize == readableBytes());
        }
    }

    Buffer::Buffer() : _buffer(kInitialSize + kCheapPrepend),
                       _reader_idx(kCheapPrepend),
                       _writer_idx(kCheapPrepend) {}
    Buffer::Buffer(const Buffer &other) : _buffer(other._buffer),
                                          _reader_idx(other._reader_idx),
                                          _writer_idx(other._writer_idx) {}
    Buffer::Buffer(Buffer &&other)
    {
        Buffer tmp;
        tmp.swap(other);
        tmp.swap(*this);
    }
    Buffer &Buffer::operator=(const Buffer &other)
    {
        Buffer tmp(other);
        tmp.swap(*this);
        return *this;
    }
    Buffer &Buffer::operator=(Buffer &&other)
    {
        Buffer tmp(std::move(other));
        tmp.swap(*this);
        return *this;
    }
    void Buffer::swap(Buffer &other)
    {
        _buffer.swap(other._buffer);
        std::swap(_reader_idx, other._reader_idx);
        std::swap(_writer_idx, other._writer_idx);
    }

    const char *Buffer::peek() const { return begin() + _reader_idx; }
    size_t Buffer::readableBytes() const { return _writer_idx - _reader_idx; }
    size_t Buffer::writableBytes() const { return _buffer.size() - _writer_idx; }
    size_t Buffer::prependableBytes() { return _reader_idx; }

    void Buffer::retrieve(size_t len)
    {
        assert(len <= readableBytes());
        _reader_idx += len;
    }
    void Buffer::retrieveAll()
    {
        _reader_idx = kCheapPrepend;
        _writer_idx = kCheapPrepend;
    }
    std::string Buffer::retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }
    std::string Buffer::retrieveAsString(size_t len)
    {
        assert(len <= readableBytes());
        std::string retval;
        retval.assign(peek(), len);
        retrieve(len);
        return retval;
    }

    void Buffer::append(const void *data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(static_cast<const char *>(data), static_cast<const char *>(data) + len, beginWrite());
        hasWritten(len);
    }
    void Buffer::ensureWritableBytes(size_t len)
    {
        if (len > writableBytes())
        {
            makeSpace(len);
        }
    }
    char *Buffer::beginWrite() { return begin() + _writer_idx; }
    void Buffer::hasWritten(size_t len)
    {
        assert(len <= writableBytes());
        _writer_idx += len;
    }
    ssize_t Buffer::readFd(int fd, int *saveErro)
    {
        size_t writableSize = writableBytes();
        char buff[65536]; // 64k
        struct iovec vecs[2];
        vecs[0].iov_base = beginWrite();
        vecs[0].iov_len = writableSize;
        vecs[1].iov_base = buff;
        vecs[1].iov_len = 65536;
        int cnt = writableSize >= 65536 ? 1 : 2;
        ssize_t ret = ::readv(fd, vecs, cnt);
        if (ret < 0)
        {
            *saveErro = errno;
        }
        else if (ret <= writableSize)
        {
            hasWritten(ret);
        }
        else
        {
            hasWritten(writableSize);
            append(buff, ret - writableSize);
        }
        return ret;
    }
    void Buffer::prepend(const void *data, size_t len)
    {
        assert(len <= prependableBytes());
        _reader_idx -= len;
        std::copy(static_cast<const char *>(data), static_cast<const char *>(data) + len, begin()+_reader_idx);
    }
    std::optional<std::string> Buffer::getline()
    {
        void *pos = memchr((void *)peek(), '\n', readableBytes());
        if (pos == nullptr)
        {
            return std::nullopt;
        }
        size_t linelen = (char *)pos - peek() + 1;
        return retrieveAsString(linelen);
    }

} // namespace net
