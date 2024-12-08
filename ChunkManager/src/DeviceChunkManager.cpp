#include <DeviceChunkManager.hpp>
#include <iostream>

namespace cse {

DeviceChunkManager::DeviceChunkManager(const std::string &devicePath,
                                       Identifier chunkCount, uint32_t mode)
    : ChunkManager(), m_devicePath(devicePath) {
  m_deviceFd = open(m_devicePath.c_str(), O_RDWR);
  if (m_deviceFd == -1) {
    throw std::runtime_error("Failed to open device: " + m_devicePath);
  }

  if (!(mode & M_OVERRIDE)) {
    lseek(m_deviceFd, 0, SEEK_SET);
    ssize_t readBytes = read(m_deviceFd, &chunkCount, sizeof(chunkCount));
    setChunkCount(chunkCount);

    m_chunkMap.resize(chunkCount);
    readBytes =
        read(m_deviceFd, m_chunkMap.data(), chunkCount * sizeof(ChunkMapInfo));
    if (readBytes != static_cast<ssize_t>(chunkCount * sizeof(ChunkMapInfo))) {
      throw std::runtime_error("Failed to read chunk map from device.");
    }
  } else {
    setChunkCount(chunkCount);
    m_chunkMap.resize(chunkCount);
    ChunkMapInfo chunkMapInfo;
    std::fill(m_chunkMap.begin(), m_chunkMap.end(), chunkMapInfo);

    lseek(m_deviceFd, 0, SEEK_SET);
    write(m_deviceFd, &chunkCount, sizeof(chunkCount));
    write(m_deviceFd, m_chunkMap.data(), chunkCount * sizeof(ChunkMapInfo));

    // Preallocate storage
    lseek(m_deviceFd, fileOffset(chunkCount) - 1, SEEK_SET);
    write(m_deviceFd, "", 1);
  }
}

DeviceChunkManager::~DeviceChunkManager() {
  char buffer[16] = {0};
  Identifier infoSize = fileOffset(0);
  lseek(m_deviceFd, 0, SEEK_SET);
  for (Identifier i = 0; i < infoSize; i += 16) {
    write(m_deviceFd, buffer, 16);
  }
  lseek(m_deviceFd, -16, SEEK_CUR);
  for (Identifier i = 0; i < 16; i++) {
    buffer[i] = 0xff;
  }
  write(m_deviceFd, buffer, 16);
  lseek(m_deviceFd, 0, SEEK_SET);
  write(m_deviceFd, &chunkCount, sizeof(chunkCount));
  write(m_deviceFd, m_chunkMap.data(), chunkCount * sizeof(ChunkMapInfo));

  close(m_deviceFd);
}

void DeviceChunkManager::saveChunk(const Identifier &identifier) {
  std::cout << fileOffset(identifier) << std::endl;
  lseek(m_deviceFd, fileOffset(identifier), SEEK_SET);
  write(m_deviceFd, &m_activeChunks[identifier].first, sizeof(Chunk));
}

void DeviceChunkManager::loadChunk(const Identifier &identifier) const {
  lseek(m_deviceFd, fileOffset(identifier), SEEK_SET);
  Chunk chunk;
  read(m_deviceFd, &chunk, sizeof(Chunk));
  ActiveChunkMapInfo info;
  m_activeChunks[identifier] = {chunk, info};
}

Identifier DeviceChunkManager::fileOffset(const Identifier &identifier) const {
  Identifier fileInfoOffset =
      ((sizeof(Identifier) + m_chunkMap.size() * sizeof(ChunkMapInfo) + 16) /
           16 +
       1) *
      16;
  return fileInfoOffset + identifier * sizeof(Chunk);
}

} // namespace cse
