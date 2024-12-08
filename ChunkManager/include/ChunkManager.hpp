#ifndef CHUNKMANAGER_INCLUDE_CHUNKMANAGER
#define CHUNKMANAGER_INCLUDE_CHUNKMANAGER

#include <Chunk.hpp>
#include <unordered_map>
#include <vector>

namespace cse {

struct ActiveChunkMapInfo {
  ActiveChunkMapInfo() : modified(false) {}

  bool modified = false;
};

struct ChunkMapInfo {
  ChunkMapInfo() : used(false) {}

  bool used = false;
};

enum Modes {
  M_OVERRIDE = 1,
};

class ChunkManager {
public:
  ChunkManager();
  ChunkManager(Identifier chunkCount);
  ~ChunkManager() = default;

  Identifier createChunk();
  void deleteChunk(const Identifier &identifier);
  Chunk &getChunk(const Identifier &identifier);
  const Chunk &getChunk(const Identifier &identifier) const;
  void writeChunk(const Identifier &identifier, const Chunk &chunk);

  void flush();

protected:
  mutable std::unordered_map<Identifier, std::pair<Chunk, ActiveChunkMapInfo>>
      m_activeChunks;
  std::vector<ChunkMapInfo> m_chunkMap;
  Identifier chunkCount;

  virtual void saveChunk(const Identifier &identifier) = 0;
  virtual void loadChunk(const Identifier &identifier) const = 0;
  void setChunkCount(Identifier chunkCount);
};

} // namespace cse

#endif /* CHUNKMANAGER_INCLUDE_CHUNKMANAGER */
