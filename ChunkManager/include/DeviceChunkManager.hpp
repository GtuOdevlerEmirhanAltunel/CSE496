#ifndef CHUNKMANAGER_INCLUDE_DEVICECHUNKMANAGER
#define CHUNKMANAGER_INCLUDE_DEVICECHUNKMANAGER

#include <ChunkManager.hpp>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace cse {

class DeviceChunkManager : public ChunkManager {
public:
  DeviceChunkManager(const std::string &devicePath, Identifier chunkCount,
                     uint32_t mode = 0);
  ~DeviceChunkManager();

private:
  int m_deviceFd; // File descriptor for the device
  const std::string m_devicePath;

  void saveChunk(const Identifier &identifier) override;
  void loadChunk(const Identifier &identifier) const override;

  Identifier fileOffset(const Identifier &identifier) const;
};

} // namespace cse

#endif /* CHUNKMANAGER_INCLUDE_DEVICECHUNKMANAGER */
