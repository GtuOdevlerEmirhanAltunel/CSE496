#include <FileChunkManager.hpp>

using namespace cse;

FileChunkManager::FileChunkManager(const std::string &filename,
                                   Identifier chunkCount)
    : ChunkManager() {
  m_file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
  if (m_file.is_open()) {
    m_file.read(reinterpret_cast<char *>(&chunkCount), sizeof(chunkCount));
    setChunkCount(chunkCount);
    m_file.read(reinterpret_cast<char *>(m_chunkMap.data()), m_chunkMap.size());
  } else {
    m_file.clear();
    m_file.open(filename, std::ios::in | std::ios::out | std::ios::binary |
                              std::ios::trunc);
    setChunkCount(chunkCount);
    m_file.seekp(fileOffset(m_chunkMap.size()) - 1);
    m_file.put('\0');
  }
}

FileChunkManager::~FileChunkManager() {
  m_file.seekp(0);
  Identifier chunkCount = m_chunkMap.size();
  m_file.write(reinterpret_cast<const char *>(&chunkCount), sizeof(chunkCount));
  m_file.write(reinterpret_cast<const char *>(m_chunkMap.data()),
               m_chunkMap.size() * sizeof(ChunkMapInfo));
  m_file.close();
}

void FileChunkManager::saveChunk(const Identifier &identifier) {
  m_file.seekp(fileOffset(identifier));
  m_file.write(reinterpret_cast<const char *>(&m_activeChunks[identifier]),
               sizeof(Chunk));
}

void FileChunkManager::loadChunk(const Identifier &identifier) const {
  m_file.seekg(fileOffset(identifier));
  Chunk chunk;
  m_file.read(reinterpret_cast<char *>(&chunk), sizeof(Chunk));
  m_activeChunks[identifier].first = chunk;
  m_activeChunks[identifier].second.modified = false;
}

Identifier FileChunkManager::fileOffset(const Identifier &identifier) const {
  Identifier fileInfoOffset =
      ((sizeof(Identifier) + m_chunkMap.size() * sizeof(ChunkMapInfo)) / 32 +
       1) *
      32;
  return fileInfoOffset + identifier * sizeof(Chunk);
}
