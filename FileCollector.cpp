#include "FileCollector.h"

#include <algorithm>
#include <unordered_map>
#include <mutex>

class FileCollector::FileCollectorImpl
{
private:
    mutable std::mutex m_map_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<filecollector::FileData>> m_files;

    std::shared_ptr<filecollector::FileData> findState(uint32_t fileId) 
    {
        std::lock_guard<std::mutex> lck(m_map_mutex);
        auto it = m_files.find(fileId);
        if (it == m_files.end()) return nullptr;
        return it->second;
    }

public:
    void CollectFile(uint32_t fileId, size_t fileSize)
    {
        std::lock_guard<std::mutex> lck(m_map_mutex);
        auto it = m_files.find(fileId);
        if (it != m_files.end() && it->second->size == fileSize) return;

        auto fileData = std::make_shared<filecollector::FileData>();
        fileData->size = fileSize;
        m_files[fileId] = std::move(fileData);
    }

    void OnNewChunk(uint32_t fileId, size_t pos, std::vector<uint8_t>&& chunk)
    {
        std::shared_ptr<filecollector::FileData> fileData = findState(fileId);
        if (!fileData)
            throw std::invalid_argument("Error in (FileCollector::OnNewChunk, unknown fileId)");

        if (pos >= fileData->size || chunk.empty()) return;


        std::lock_guard<std::mutex> lock(fileData->mtx);
        size_t n = std::min(chunk.size(), fileData->size - pos);
        size_t newEnd = pos + n;

        auto it = fileData->chunks.lower_bound(pos);// >= pos право
        if (it != fileData->chunks.begin())// случай если что-то левее 
        {
            auto prev = std::prev(it);
            size_t prevEnd = prev->first + prev->second.size();
            if (prevEnd >= pos) it = prev; //если есть, то обновляем на левый
        }

        size_t mergeStart = pos;
        size_t mergeEnd = newEnd;

        auto scan = it; 
        while (scan != fileData->chunks.end()) //сканирование вправо
        {
            size_t sStart = scan->first;
            size_t sEnd = sStart + scan->second.size();
            if (sStart > newEnd) break; //если справа разрыв
            if (sEnd < pos)
            {
                scan++;
                continue;
            }
            mergeStart = std::min(mergeStart, sStart);
            mergeEnd = std::max(mergeEnd, sEnd);
            scan++;
        }

        size_t covered = 0;
        for (auto i = it; i != scan; i++)
        {
            size_t sStart = i->first;
            size_t sEnd = sStart + i->second.size();
            size_t lo = std::max(sStart, pos);
            size_t hi = std::min(sEnd, newEnd);
            if (lo < hi) covered += (hi - lo);
        }
        size_t addedBytes = n - covered;

        std::vector<uint8_t> merged(mergeEnd - mergeStart, 0);
        for (auto i = it; i != scan; i++)
        {
            std::memcpy(merged.data() + (i->first - mergeStart), i->second.data(), i->second.size()); //старые чанки
        }
        std::memcpy(merged.data() + (pos - mergeStart), chunk.data(), n); //наложение нового чанка 

        fileData->chunks.erase(it, scan);
        fileData->chunks.emplace(mergeStart, std::move(merged));

        fileData->received += addedBytes;
        if (fileData->received >= fileData->size) fileData->complete = true;
    }

    std::optional<std::vector<uint8_t>> GetFile(uint32_t fileId)
    {
        auto fileData = findState(fileId);
        if (!fileData) return std::nullopt;

        std::lock_guard<std::mutex> lock(fileData->mtx);

        std::vector<uint8_t> out(fileData->size, 0);
        for (const auto& [p, data] : fileData->chunks)
        {
            std::memcpy(out.data() + p, data.data(), data.size());
        }
        return out;
    }

    std::vector<filecollector::Piece> GetPieces(uint32_t fileId)
    {
        auto fileData = findState(fileId);
        if (!fileData) return {};

        std::lock_guard<std::mutex> lock(fileData->mtx);

        std::vector<filecollector::Piece> out;
        out.reserve(fileData->chunks.size());
        for (const auto& [p, data] : fileData->chunks)
        {
            out.push_back({ p, data });
        }
        return out;
    }

    std::shared_ptr<const filecollector::FileData> GetFileInfo(uint32_t fileId)
    {
        auto fileData = findState(fileId);
        if (!fileData) return nullptr;

        return std::static_pointer_cast<const filecollector::FileData>(fileData);
    }

    void RemoveFile(uint32_t fileId)
    {
        std::lock_guard<std::mutex> lock(m_map_mutex);
        m_files.erase(fileId);
    }
};

FileCollector::FileCollector() : m_impl(std::make_unique<FileCollectorImpl>()) {}

FileCollector::~FileCollector() = default;

FileCollector::FileCollector(FileCollector&&) noexcept = default;
FileCollector& FileCollector::operator=(FileCollector&&) noexcept = default;

void FileCollector::CollectFile(uint32_t fileId, size_t fileSize)
{
    m_impl->CollectFile(fileId, fileSize);
}

void FileCollector::OnNewChunk(uint32_t fileId, size_t pos, std::vector<uint8_t>&& chunk)
{
    m_impl->OnNewChunk(fileId, pos, std::move(chunk));
}

std::optional<std::vector<uint8_t>> FileCollector::GetFile(uint32_t fileId)
{
    return m_impl->GetFile(fileId);
}

std::vector<filecollector::Piece> FileCollector::GetPieces(uint32_t fileId)
{
    return m_impl->GetPieces(fileId);
}

std::shared_ptr<const filecollector::FileData> FileCollector::GetFileInfo(uint32_t fileId)
{
    return m_impl->GetFileInfo(fileId);
}

void FileCollector::RemoveFile(uint32_t fileId)
{
    m_impl->RemoveFile(fileId);
}