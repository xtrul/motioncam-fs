#include "macos/FuseFileSystemImpl_MacOS.h"
#include "VirtualFileSystemImpl_MCRAW.h"
#include "LRUCache.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <BS_thread_pool.hpp>
#include <fuse_t/fuse_t.h>

#include <QDir>

// Logging
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace fs = boost::filesystem;

namespace motioncam {

constexpr auto CACHE_SIZE = 1024 * 1024 * 1024; // 1 GB cache size

namespace {

bool isMacOsMetadataFile(const std::string& path)  {
    if (path == ".DS_Store"    ||
        path == "._"           ||
        path == ".Spotlight-"  ||
        path == ".fseventsd"   ||
        path == ".Trashes")
    {
        return true;
    }

    return false;
}

void setupLogging() {
    try {
        // Create a vector of sinks
        std::vector<spdlog::sink_ptr> sinks;

        // Regular console output
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

        // File sink
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/logfile.txt", true));

        // Create a logger with all sinks
        auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());

        // Set as default logger
        spdlog::set_default_logger(logger);

        // Set log level
#ifdef NDEBUG
        spdlog::set_level(spdlog::level::info);
#else
        spdlog::set_level(spdlog::level::debug);
#endif

        // Flush on info level messages
        spdlog::flush_on(spdlog::level::info);

    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

} // namespace

//

struct FuseContext {
    std::unique_ptr<VirtualFileSystemImpl_MCRAW> fs;
};

class Session {
public:
    Session(const std::string& srcFile, const std::string& dstPath, std::unique_ptr<VirtualFileSystemImpl_MCRAW> fs);
    ~Session();

private:
    void init(std::unique_ptr<VirtualFileSystemImpl_MCRAW> fs);

    void fuseMain(struct fuse_chan* ch, struct fuse* fuse);

    static void* fuseInit(struct fuse_conn_info* conn);
    static void fuseDestroy(void* privateData);
    static int fuseRelease(const char* path, struct fuse_file_info* fi);
    static int fuseGetattr(const char* path, struct stat* stbuf);
    static int fuseReaddir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi);
    static int fuseOpen(const char* path, struct fuse_file_info* fi);
    static int fuseRead(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);

private:
    std::string mSrcFile;
    std::string mDstPath;
    std::unique_ptr<std::thread> mThread;
    struct fuse_chan* mFuseCh;
    struct fuse* mFuse;
};


Session::Session(const std::string& srcFile, const std::string& dstPath, std::unique_ptr<VirtualFileSystemImpl_MCRAW> fs) :
    mSrcFile(srcFile),
    mDstPath(dstPath),
    mFuseCh(nullptr),
    mFuse(nullptr)
{
    init(std::move(fs));
}

Session::~Session() {
    fuse_unmount("/Users/mirsadm/workspace/tmp", mFuseCh);

    fuse_destroy(mFuse);

    if(mThread->joinable())
        mThread->join();
}

void Session::init(std::unique_ptr<VirtualFileSystemImpl_MCRAW> fs) {
    // FUSE operations structure
    struct fuse_operations ops = {};

    ops.init = fuseInit;
    ops.destroy = fuseDestroy;
    ops.release = fuseRelease;
    ops.getattr = fuseGetattr;
    ops.readdir = fuseReaddir;
    ops.open = fuseOpen;
    ops.read = fuseRead;

    struct fuse_args args = FUSE_ARGS_INIT(0, nullptr);

    // Read only
    fuse_opt_add_arg(&args, "-r");
    fuse_opt_add_arg(&args, "-o");
    fuse_opt_add_arg(&args, "nonamedattr");
    fuse_opt_add_arg(&args, "-o");
    fuse_opt_add_arg(&args, "nomtime");


    auto* context = new FuseContext();
    context->fs = std::move(fs);

    struct fuse_chan* ch = fuse_mount("/Users/mirsadm/workspace/tmp", &args);
    struct fuse* fuse = fuse_new(ch, &args, &ops, sizeof(ops), context);

    // Clean up
    fuse_opt_free_args(&args);

    if (fuse == nullptr) {
        free(context);
        throw std::runtime_error("Failed to create mount point");
    }

    mFuseCh = ch;
    mFuse = fuse;

    // Start fuse thread
    mThread = std::make_unique<std::thread>(&Session::fuseMain, this, ch, fuse);

}

void Session::fuseMain(struct fuse_chan* ch, struct fuse* fuse)
{
    int res = fuse_loop_mt(fuse);

    spdlog::info("Fuse has exited with code {}", res);

    // fuse_unmount("/Users/mirsadm/workspace/tmp", ch);

    // fuse_destroy(fuse);
}

FuseContext* fuseGetContext() {
    auto context = fuse_get_context();

    return reinterpret_cast<FuseContext*>(context->private_data);
}

void* Session::fuseInit(struct fuse_conn_info* conn) {
    auto context = fuse_get_context();

    return context->private_data;
}

void Session::fuseDestroy(void* privateData) {
}

int Session::fuseGetattr(const char* path, struct stat* stbuf) {
    spdlog::debug("fuse_get_attr(path: {})", path);

    memset(stbuf, 0, sizeof(struct stat));

    auto* context = fuseGetContext();
    std::string pathStr(path);

    if (isMacOsMetadataFile(path))
        return -ENOENT;

    // Root directory
    if (pathStr == "/") {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;

        return 0;
    }
    else {
        auto entry = context->fs->findEntry(pathStr);

        if(!entry.has_value())
            return -ENOENT;

        if(entry->type == EntryType::DIRECTORY_ENTRY) {
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
            stbuf->st_mtime = stbuf->st_ctime = time(NULL);
            stbuf->st_size = 4096;
        }
        else if(entry->type == EntryType::FILE_ENTRY) {
            stbuf->st_mode = S_IFREG | 0644;
            stbuf->st_nlink = 1;
            stbuf->st_size = entry->size;

            stbuf->st_mtime = stbuf->st_ctime = time(NULL);
            stbuf->st_uid = getuid();
            stbuf->st_gid = getgid();
        }

        return 0;
    }

    return -ENOENT;
}

int Session::fuseReaddir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    spdlog::debug("fuse_read_dir(path: {})", path);

    filler(buf, ".", nullptr, 0);
    filler(buf, "..", nullptr, 0);

    auto* context = fuseGetContext();
    std::string pathStr(path);

    if (pathStr == "/") {
        pathStr = "";
    }
    else {
        pathStr = pathStr.substr(1); // Remove leading slash
    }

    auto files = context->fs->listFiles(pathStr);
    for(auto& entry : files) {
        filler(buf, entry.getFullPath().c_str(), nullptr, 0);
    }

    return 0;
}

int Session::fuseOpen(const char* path, struct fuse_file_info* fi) {
    spdlog::debug("open(fuse_path: {})", path);

    auto* context = fuseGetContext();
    std::string pathStr(path);

    auto entry = context->fs->findEntry(pathStr);

    if(!entry.has_value())
        return -ENOENT;

    // Only allow read access
    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
}

int Session::fuseRead(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    spdlog::debug("fuse_read(path: {}, size: {}, offset: {})", path, size, offset);

    auto* context = fuseGetContext();
    std::string pathStr(path);

    auto entry = context->fs->findEntry(pathStr);

    if(!entry.has_value())
        return -ENOENT;

    return context->fs->readFile(
        entry.value(),
        FileRenderOptions::RENDER_OPT_NONE,
        offset,
        size,
        buf,
        [](auto a, auto b) {},
        false
        );
}

int Session::fuseRelease(const char* path, struct fuse_file_info* fi) {
    return 0;
}

//

FuseFileSystemImpl_MacOs::FuseFileSystemImpl_MacOs() :
    mNextMountId(0),
    mCache(std::make_unique<LRUCache>(CACHE_SIZE))
{
    setupLogging();
}

FuseFileSystemImpl_MacOs::~FuseFileSystemImpl_MacOs() {
}

MountId FuseFileSystemImpl_MacOs::mount(
    FileRenderOptions options, int draftScale, const std::string& srcFile, const std::string& dstPath)
{
    fs::path srcPath(srcFile);
    std::string extension = srcPath.extension().string();

    spdlog::debug("Mounting file {} to {}", srcFile, dstPath);

    QDir dst;

    if(!dst.exists()) {
        spdlog::info("Creating path {}, dstPath");

        if(!dst.mkpath(dstPath.c_str())) {
            spdlog::error("Could not create path {}", dstPath);
            return InvalidMountId;
        }
    }

    if(boost::iequals(extension, ".mcraw")) {
        auto mountId = mNextMountId++;

        void* stack_addr = nullptr;
        size_t stack_size = 0;

        try {
            auto fs = std::make_unique<VirtualFileSystemImpl_MCRAW>(*mCache, options, draftScale, srcFile);
            auto session = std::make_unique<Session>(srcFile, dstPath, std::move(fs));

            if(!session) {
                spdlog::error("Failed to mount {} to {}", srcFile, dstPath);
                return InvalidMountId;
            }

            mMountedFiles[mountId] = std::move(session);
        }
        catch(std::runtime_error& e) {
            spdlog::error("Failed to mount {} to {} (error: {})", srcFile, dstPath, e.what());

            return InvalidMountId;
        }

        return mountId;
    }

    spdlog::error("Failed to mount {} to {}", srcFile, dstPath);

    return InvalidMountId;
}

void FuseFileSystemImpl_MacOs::unmount(MountId mountId) {
    auto it = mMountedFiles.find(mountId);
    if(it != mMountedFiles.end()) {
        mMountedFiles.erase(it);
    }
}

void FuseFileSystemImpl_MacOs::updateOptions(MountId mountId, FileRenderOptions options, int draftScale) {
}

} // namespace motioncam
