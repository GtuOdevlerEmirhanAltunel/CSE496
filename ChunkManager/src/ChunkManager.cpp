#include <ChunkManager.hpp>

using namespace cse;

ChunkManager::ChunkManager() : chunkCount(0) {}

ChunkManager::ChunkManager(Identifier chunkCount) : chunkCount(chunkCount) {
  setChunkCount(chunkCount);
}

Identifier ChunkManager::createChunk() {
  for (Identifier i = 0; i < chunkCount; i++) {
    if (!m_chunkMap[i].used) {
      m_chunkMap[i].used = true;
      m_activeChunks[i].first = Chunk();
      return i;
    }
  }
  return -1;
}

void ChunkManager::deleteChunk(const Identifier &identifier) {
  m_chunkMap[identifier].used = false;
  m_activeChunks.erase(identifier);
}

Chunk &ChunkManager::getChunk(const Identifier &identifier) {
  if (m_activeChunks.find(identifier) == m_activeChunks.end()) {
    loadChunk(identifier);
  }
  return m_activeChunks[identifier].first;
}

const Chunk &ChunkManager::getChunk(const Identifier &identifier) const {
  if (m_activeChunks.find(identifier) == m_activeChunks.end()) {
    loadChunk(identifier);
  }
  return m_activeChunks.at(identifier).first;
}

void ChunkManager::writeChunk(const Identifier &identifier,
                              const Chunk &chunk) {
  m_activeChunks[identifier].first = chunk;
  m_activeChunks[identifier].second.modified = true;
}

void ChunkManager::flush() {
  for (auto &chunk : m_activeChunks) {
    if (chunk.second.second.modified) {
      saveChunk(chunk.first);
    }
  }
}

void ChunkManager::setChunkCount(Identifier chunkCount) {
  this->chunkCount = chunkCount;

  ChunkMapInfo info;
  info.used = false;

  m_chunkMap.resize(chunkCount, info);
}
