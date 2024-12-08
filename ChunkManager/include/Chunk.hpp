#ifndef CHUNKMANAGER_INCLUDE_CHUNK
#define CHUNKMANAGER_INCLUDE_CHUNK

#include <cstdint>

namespace cse {

using Index = uint16_t;
using Identifier = uint32_t;

struct ChunkIdentifier {
  Identifier managerID = 0;
  Identifier chunkIndex = 0;
};

struct ChunkInfo {
  ChunkIdentifier nextChunk;
};
constexpr Index CHUNK_DATA_SIZE = 32 - sizeof(ChunkInfo);

struct Chunk {
  ChunkInfo info;
  char data[CHUNK_DATA_SIZE] = {0};
};

} // namespace cse

#endif /* CHUNKMANAGER_INCLUDE_CHUNK */
