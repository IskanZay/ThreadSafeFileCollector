#pragma once
#include <vector>
#include <map>
#include <mutex>

namespace filecollector 
{
	struct Piece
	{
		size_t pos = 0;
		std::vector<uint8_t> data;
	};

	struct FileData
	{
		size_t size = 0;
		size_t received = 0;
		std::map<size_t, std::vector<uint8_t>> chunks;
		bool complete = false;
	};
}