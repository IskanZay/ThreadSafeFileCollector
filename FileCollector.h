    #pragma once

    #include "FileCollectorTypes.h"

    #include <vector>
    #include <memory>
    #include <optional>

    class FileCollector
    {
    private:
        class FileCollectorImpl;
        std::unique_ptr<FileCollectorImpl> m_impl;

    public:
        FileCollector();
        ~FileCollector();

        FileCollector(FileCollector&&) noexcept;
        FileCollector& operator=(FileCollector&&) noexcept;

        FileCollector(const FileCollector&) = delete;
        FileCollector& operator=(const FileCollector&) = delete;

        void CollectFile(uint32_t fileId, size_t fileSize);
        void OnNewChunk(uint32_t fileId, size_t pos, std::vector<uint8_t>&& chunk);

        std::optional<std::vector<uint8_t>> GetFile(uint32_t fileId);
        std::vector<filecollector::Piece> GetPieces(uint32_t fileId);
        std::shared_ptr<const filecollector::FileData> GetFileInfo(uint32_t fileId);

        void RemoveFile(uint32_t fileId);
    };