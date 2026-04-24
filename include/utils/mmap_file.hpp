#pragma once

#include <string_view>
#include <filesystem>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>
#include <system_error>

namespace utils {

    /**
     * @brief Simple RAII wrapper for memory-mapped files.
     */
    class MappedFile {
    public:
        MappedFile(const std::filesystem::path& path) {
            fd_ = open(path.c_str(), O_RDONLY);
            if (fd_ == -1) {
                return; // or throw
            }

            struct stat st;
            if (fstat(fd_, &st) == -1) {
                close(fd_);
                fd_ = -1;
                return;
            }

            size_ = static_cast<size_t>(st.st_size);
            if (size_ == 0) {
                return;
            }

            data_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
            if (data_ == MAP_FAILED) {
                close(fd_);
                fd_ = -1;
                data_ = nullptr;
                size_ = 0;
            }
        }

        ~MappedFile() {
            if (data_) {
                munmap(data_, size_);
            }
            if (fd_ != -1) {
                close(fd_);
            }
        }

        MappedFile(const MappedFile&) = delete;
        MappedFile& operator=(const MappedFile&) = delete;

        MappedFile(MappedFile&& other) noexcept 
            : fd_(other.fd_), data_(other.data_), size_(other.size_) {
            other.fd_ = -1;
            other.data_ = nullptr;
            other.size_ = 0;
        }

        MappedFile& operator=(MappedFile&& other) noexcept {
            if (this != &other) {
                if (data_) munmap(data_, size_);
                if (fd_ != -1) close(fd_);
                fd_ = other.fd_;
                data_ = other.data_;
                size_ = other.size_;
                other.fd_ = -1;
                other.data_ = nullptr;
                other.size_ = 0;
            }
            return *this;
        }

        bool is_open() const { return data_ != nullptr; }
        size_t size() const { return size_; }
        std::string_view view() const {
            return {static_cast<const char*>(data_), size_};
        }

    private:
        int fd_ = -1;
        void* data_ = nullptr;
        size_t size_ = 0;
    };

} // namespace utils
