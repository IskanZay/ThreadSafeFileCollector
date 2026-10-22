#include <gtest/gtest.h>
#include "FileCollector.h"

#include <thread>
#include <vector>

// Тест 1: неизвестный файл, пустой файл, один чанк
TEST(FileCollector, Basics)
{
	FileCollector fc;

	EXPECT_EQ(fc.GetFile(1), std::nullopt);
	EXPECT_TRUE(fc.GetPieces(1).empty());
	EXPECT_EQ(fc.GetFileInfo(1), nullptr);
	EXPECT_THROW(fc.OnNewChunk(1, 0, { 1 }), std::invalid_argument);

	fc.CollectFile(1, 10);
	auto empty = fc.GetFile(1);
	ASSERT_TRUE(empty);
	EXPECT_EQ(*empty, std::vector<uint8_t>(10, 0));

	fc.OnNewChunk(1, 0, { 1, 2, 3 });
	EXPECT_EQ(*fc.GetFile(1), (std::vector<uint8_t>{1, 2, 3, 0, 0, 0, 0, 0, 0, 0}));
}

// Тест 2: пересекающиеся чанки
TEST(FileCollector, TaskExampleOverlap)
{
	FileCollector fc;
	fc.CollectFile(1, 10);

	fc.OnNewChunk(1, 0, { 'A', 'B', 'C', 'D' });
	fc.OnNewChunk(1, 2, { 'C', 'D', 'E', 'F' }); 
	fc.OnNewChunk(1, 6, { 'G', 'H', 'I', 'J' }); 

	EXPECT_EQ(*fc.GetFile(1), (std::vector<uint8_t>{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'}));
	EXPECT_TRUE(fc.GetFileInfo(1)->complete);
}

// Тест 3: Дыры и дубликаты
TEST(FileCollector, GapsAndDuplicates)
{
	FileCollector fc;
	fc.CollectFile(1, 6);

	fc.OnNewChunk(1, 0, { 1, 2 });
	fc.OnNewChunk(1, 4, { 5, 6 });
	EXPECT_EQ(*fc.GetFile(1), (std::vector<uint8_t>{1, 2, 0, 0, 5, 6}));
	EXPECT_EQ(fc.GetFileInfo(1)->received, 4u);

	fc.OnNewChunk(1, 0, { 1, 2 });
	EXPECT_EQ(fc.GetFileInfo(1)->received, 4u);

	fc.OnNewChunk(1, 2, { 3, 4 });
	EXPECT_EQ(*fc.GetFile(1), (std::vector<uint8_t>{1, 2, 3, 4, 5, 6}));
	EXPECT_TRUE(fc.GetFileInfo(1)->complete);
}

// Тест 4: Обрезка чанка, выходит за пределы файла
TEST(FileCollector, Clipping)
{
	FileCollector fc;
	fc.CollectFile(1, 4);

	fc.OnNewChunk(1, 2, { 9, 9, 9, 9, 9 });

	EXPECT_EQ(*fc.GetFile(1), (std::vector<uint8_t>{0, 0, 9, 9}));
	EXPECT_EQ(fc.GetFileInfo(1)->received, 2u);
	EXPECT_FALSE(fc.GetFileInfo(1)->complete);
}

// Тест 5: Повторный CollectFile и RemoveFile
TEST(FileCollector, CollectAndRemove)
{
	FileCollector fc;
	fc.CollectFile(1, 10);
	fc.OnNewChunk(1, 0, { 1, 2, 3 });

	fc.CollectFile(1, 10);
	EXPECT_EQ(fc.GetFileInfo(1)->received, 3u);

	fc.CollectFile(1, 20);
	EXPECT_EQ(fc.GetFileInfo(1)->received, 0u);

	fc.RemoveFile(1);
	EXPECT_EQ(fc.GetFileInfo(1), nullptr);
}

// Тест 6 (Многопоточный): пишем в один файл из 4 потоков
TEST(FileCollector, MT_OneFile)
{
	FileCollector fc;
	const size_t SIZE = 1000;
	fc.CollectFile(1, SIZE);

	std::vector<std::thread> threads;
	for (int t = 0; t < 4; ++t)
	{
		threads.emplace_back([&, t]
			{
				for (size_t i = t * 250; i < (t + 1) * 250; ++i)
				{
					fc.OnNewChunk(1, i, { static_cast<uint8_t>(t) });
				}
			});
	}

	for (auto& th : threads) th.join();

	EXPECT_TRUE(fc.GetFileInfo(1)->complete);
	EXPECT_EQ(fc.GetFileInfo(1)->received, SIZE);
}

// Тест 7 (Многопоточный): разные файлы в разных потоках
TEST(FileCollector, MT_DifferentFiles)
{
	FileCollector fc;
	fc.CollectFile(1, 100);
	fc.CollectFile(2, 100);

	std::thread t1([&] { fc.OnNewChunk(1, 0, std::vector<uint8_t>(100, 1)); });
	std::thread t2([&] { fc.OnNewChunk(2, 0, std::vector<uint8_t>(100, 2)); });

	t1.join();
	t2.join();

	EXPECT_TRUE(fc.GetFileInfo(1)->complete);
	EXPECT_TRUE(fc.GetFileInfo(2)->complete);
}