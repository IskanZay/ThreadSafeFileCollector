#pragma once

#include "FileCollectorTypes.h"

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

class FileCollector
{
private:
	std::unordered_map<uint32_t, filecollector::FileData> m_files;

	filecollector::FileData* findState(uint32_t fileId) //невладеющий указатель указатель
	{
		auto it = m_files.find(fileId);
		if (it == m_files.end()) return nullptr;
		return &it->second;
	}

public:
	void CollectFile(uint32_t fileId, size_t fileSize)
	{
		auto it = m_files.find(fileId);
		if (it != m_files.end() && it->second.size == fileSize) return;

		filecollector::FileData fileData;
		fileData.size = fileSize;
		m_files[fileId] = std::move(fileData);
	}

	void OnNewChunk(uint32_t fileId, size_t pos, std::vector<uint8_t>&& chunk)
	{
		filecollector::FileData* fileData = findState(fileId);//невладеющий
		if (!fileData) throw std::invalid_argument("Error in (FileCollector::OnNewChunk, unknown fileId)");

		if (pos >= fileData->size || chunk.empty()) return;

		size_t n = std::min(chunk.size(), fileData->size - pos);
		size_t newEnd = pos + n;

		auto it = fileData->chunks.lower_bound(pos);
		if (it != fileData->chunks.begin()) 
		{
			auto prev = std::prev(it);
			size_t prevEnd = prev->first + prev->second.size();
			if (prevEnd >= pos) it = prev;
		}

		size_t mergeStart = pos;
		size_t mergeEnd = newEnd;

		auto scan = it;
		while (scan != fileData->chunks.end()) 
		{
			size_t sStart = scan->first;
			size_t sEnd = sStart + scan->second.size();
			if (sStart > newEnd) break;
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
		for (auto i = it; i != scan; ++i) 
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
			std::memcpy(merged.data() + (i->first - mergeStart), i->second.data(), i->second.size());
		}
		std::memcpy(merged.data() + (pos - mergeStart), chunk.data(), n);

		fileData->chunks.erase(it, scan);
		fileData->chunks.emplace(mergeStart, std::move(merged));

		fileData->received += addedBytes;
		if (fileData->received >= fileData->size) fileData->complete = true;
	}

	std::optional<std::vector<uint8_t>> GetFile(uint32_t fileId) 
	{
		filecollector::FileData* fileData = findState(fileId);//невладеющий
		if (!fileData) return std::nullopt;

		std::vector<uint8_t> out(fileData->size, 0);
		for (const auto& [p, data] : fileData->chunks) 
		{
			std::memcpy(out.data() + p, data.data(), data.size());
		}
		return out;
	}

	std::vector<filecollector::Piece> GetPieces(uint32_t fileId)
	{
		filecollector::FileData* fileData = findState(fileId);//невладеющий
		if (!fileData) return {};
		std::vector<filecollector::Piece> out;
		out.reserve(fileData->chunks.size());
		for (const auto& [p, data] : fileData->chunks)
		{
			out.push_back({ p, data });
		}
		return out;
	}

	std::optional<filecollector::FileData> GetFileInfo(uint32_t fileId)
	{
		filecollector::FileData* fileData = findState(fileId);//невладеющий
		if (!fileData) return std::nullopt;

		return *fileData;
	}

	void RemoveFile(uint32_t fileId)
	{
		m_files.erase(fileId);
	}
};

