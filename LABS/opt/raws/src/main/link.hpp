#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <sys/types.h>
#include <unistd.h>

namespace pqtr
{

    // Opaque handle to I/O source
        using Handle = void *;

        // Read function: returns bytes read (0 = EOF, <0 = error)
        using ReadFn = ssize_t (*)(Handle h, uint8_t *buf, size_t len);

        // Seek function: returns true on success
        using SeekFn = bool (*)(Handle h, size_t offset);

        // Size function: returns total size (-1 if unknown/streaming)
        using SizeFn = ssize_t (*)(Handle h);

        // Link dispatcher between host I/O and pipe
        struct Link
        {
            Handle handle; // Opaque I/O handle (FILE*, fd, buffer, etc.)
            ReadFn read;   // Required: read bytes
            SeekFn seek;   // Optional: seek to offset (nullptr if not seekable)
            SizeFn size;   // Optional: get total size (nullptr if unknown)
        };

        // Helper functions
        inline ssize_t read(Link &link, uint8_t *buf, size_t len)
        {
            return link.read(link.handle, buf, len);
        }

        inline bool seek(Link &link, size_t offset)
        {
            return link.seek ? link.seek(link.handle, offset) : false;
        }

        inline ssize_t size(Link &link)
        {
            return link.size ? link.size(link.handle) : -1;
        }

        inline bool is_seekable(const Link &link)
        {
            return link.seek != nullptr;
        }

        inline bool has_known_size(const Link &link)
        {
            return link.size != nullptr;
        }

        // Factory functions for common link types
        namespace factory
        {

            // File-based link
            // Note: Caller owns FILE* lifetime - must keep it open while Link is in use
            inline Link file(void *file_handle)
            {
                Link l;
                l.handle = file_handle;

                l.read = [](Handle h, uint8_t *buf, size_t len) -> ssize_t
                {
                    return fread(buf, 1, len, static_cast<FILE *>(h));
                };

                l.seek = [](Handle h, size_t offset) -> bool
                {
                    return fseek(static_cast<FILE *>(h), offset, SEEK_SET) == 0;
                };

                l.size = [](Handle h) -> ssize_t
                {
                    FILE *fp = static_cast<FILE *>(h);
                    long pos = ftell(fp);
                    fseek(fp, 0, SEEK_END);
                    long sz = ftell(fp);
                    fseek(fp, pos, SEEK_SET);
                    return sz;
                };

                return l;
            }

            // Memory buffer context for link
            struct MemBuf
            {
                const uint8_t *data;
                size_t size;
                size_t pos;

                MemBuf(const uint8_t *d, size_t s) : data(d), size(s), pos(0) {}
            };

            // Memory buffer link
            // Note: Caller owns MemBuf lifetime - must keep it alive while Link is in use
            inline Link memory(MemBuf *buf)
            {
                Link l;
                l.handle = buf;

                l.read = [](Handle h, uint8_t *dst, size_t len) -> ssize_t
                {
                    auto *mb = static_cast<MemBuf *>(h);
                    size_t available = mb->size - mb->pos;
                    size_t to_read = len < available ? len : available;
                    memcpy(dst, mb->data + mb->pos, to_read);
                    mb->pos += to_read;
                    return to_read;
                };

                l.seek = [](Handle h, size_t offset) -> bool
                {
                    auto *mb = static_cast<MemBuf *>(h);
                    if (offset > mb->size)
                        return false;
                    mb->pos = offset;
                    return true;
                };

                l.size = [](Handle h) -> ssize_t
                {
                    return static_cast<MemBuf *>(h)->size;
                };

                return l;
            }

            // Socket/fd link (non-seekable, streaming)
            // Note: Caller owns fd lifetime - must keep it open while Link is in use
            inline Link socket(int *fd_ptr)
            {
                Link l;
                l.handle = fd_ptr;

                l.read = [](Handle h, uint8_t *buf, size_t len) -> ssize_t
                {
                    int fd = *static_cast<int *>(h);
                    return ::read(fd, buf, len);
                };

                l.seek = nullptr; // Sockets not seekable
                l.size = nullptr; // Unknown size for streams

                return l;
            }

    } // namespace factory

} // namespace pqtr
