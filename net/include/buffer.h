#pragma once

#include <vector>
#include <unistd.h>
#include <optional>
#include <string>

namespace net
{

    class Buffer
    {
    private:
        static const int kInitialSize = 1024; // 默认缓冲区大小
        static const int kCheapPrepend = 8;   // 前置默认预留空间大小

    public:
        Buffer();
        Buffer(const Buffer &other);
        Buffer(Buffer &&other);
        Buffer &operator=(const Buffer &other);
        Buffer &operator=(Buffer &&other);
        void swap(Buffer &other);

        const char *peek() const;
        size_t readableBytes() const;
        size_t writableBytes() const;

        void retrieve(size_t len);
        void retrieveAll();
        std::string retrieveAllAsString();
        std::string retrieveAsString(size_t len);

        void append(const void *data, size_t len);
        void ensureWritableBytes(size_t len);
        char *beginWrite();
        void hasWritten(size_t len);
        ssize_t readFd(int fd, int *saveErro);
        size_t prependableBytes();
        void prepend(const void *data, size_t len);

        std::optional<std::string> getline();

    private:
        char *begin();
        const char *begin() const;
        void makeSpace(size_t len);

    private:
        std::vector<char> _buffer;
        size_t _reader_idx;
        size_t _writer_idx;
    };

} // namespace net
