#include <MemoryChunkManager.hpp>

using namespace cse;

MemoryChunkManager::MemoryChunkManager(Identifier chunkCount)
    : ChunkManager(chunkCount) {
  m_chunks.resize(chunkCount);
}

void MemoryChunkManager::saveChunk(const Identifier &identifier) {
  m_chunks[identifier] = m_activeChunks[identifier].first;
  m_activeChunks[identifier].second.modified = false;
}

void MemoryChunkManager::loadChunk(const Identifier &identifier) const {
  m_activeChunks[identifier].first = m_chunks[identifier];
}
