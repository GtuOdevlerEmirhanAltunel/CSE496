#ifndef CHUNKMANAGER_INCLUDE_FILECHUNKMANAGER
#define CHUNKMANAGER_INCLUDE_FILECHUNKMANAGER

#include <ChunkManager.hpp>
#include <fstream>

namespace cse {

class FileChunkManager : public ChunkManager {

public:
  FileChunkManager(const std::string &filename, Identifier chunkCount);
  ~FileChunkManager();

private:
  mutable std::fstream m_file;
  void saveChunk(const Identifier &identifier) override;
  void loadChunk(const Identifier &identifier) const override;

  Identifier fileOffset(const Identifier &identifier) const;
};

} // namespace cse

#endif /* CHUNKMANAGER_INCLUDE_FILECHUNKMANAGER */
