#ifndef CHUNKMANAGER_INCLUDE_MEMORYCHUNKMANAGER
#define CHUNKMANAGER_INCLUDE_MEMORYCHUNKMANAGER

#include <ChunkManager.hpp>
#include <vector>

namespace cse {

class MemoryChunkManager : public ChunkManager {
public:
  MemoryChunkManager(Identifier chunkCount);
  ~MemoryChunkManager() = default;

private:
  std::vector<Chunk> m_chunks;

  void saveChunk(const Identifier &identifier) override;
  void loadChunk(const Identifier &identifier) const override;
};

} // namespace cse

#endif /* CHUNKMANAGER_INCLUDE_MEMORYCHUNKMANAGER */
