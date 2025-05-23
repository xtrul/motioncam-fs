#pragma once

#include <IVirtualFileSystem.h>
#include <memory>

namespace motioncam {

class Decoder;
class LRUCache;

class VirtualFileSystemImpl_MCRAW : public IVirtualFileSystem
{
public:
    VirtualFileSystemImpl_MCRAW(FileRenderOptions options, LRUCache& cache, const std::string& file);

    const std::vector<Entry>& listFiles(const std::string& filter = "") const override;
    std::optional<Entry> findEntry(const std::string& filter) const override;
    int readFile(const Entry& entry, FileRenderOptions options, const size_t pos, const size_t len, void* dst) const override;

    void updateOptions(FileRenderOptions options) override;

private:
    void init(FileRenderOptions options);

private:
    LRUCache& mCache;
    const std::string mSrcPath;
    const std::string mBaseName;
    size_t mTypicalDngSize;
    std::unique_ptr<Decoder> mDecoder;
    std::vector<Entry> mFiles;    
};

} // namespace motioncam
