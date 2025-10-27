#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include "pf.h"


using namespace std;
int PF_BUFFER_SIZE=40;

int g_nextFd = 1;
std::unordered_map<std::string, int> g_fileMap;
std::unordered_map<int, std::fstream*> g_fdToStream;
std::unordered_map<std::string, bool> g_fileExists;

// ---------------------------------------------------------------------
// PF_Manager 构造与析构
// ---------------------------------------------------------------------
PF_Manager::PF_Manager()
{
    // 默认缓冲池大小可由用户配置
    pBufferMgr = new PF_BufferMgr(PF_BUFFER_SIZE);

}

PF_Manager::~PF_Manager()
{
    delete pBufferMgr;
}

// ---------------------------------------------------------------------
// CreateFile
// 创建一个新的分页文件，写入文件头
// ---------------------------------------------------------------------
RC PF_Manager::CreateFile(const char* fileName)
{
    // 检查文件是否存在
    std::ifstream checkFile(fileName, ios::binary);
    if (checkFile.good()) {
        checkFile.close();
        return -1;// 文件已存在
    }

    // 创建文件流（写二进制，清空旧内容）
    std::fstream* fs = new std::fstream(fileName, ios::in | ios::out | ios::binary | ios::trunc);
    if (!fs->is_open()) {
        delete fs;
        return PF_UNIX;
    }

    // 初始化文件头
    char hdrBuf[PF_FILE_HDR_SIZE];
    memset(hdrBuf, 0, PF_FILE_HDR_SIZE);
    PF_FileHdr* hdr = (PF_FileHdr*)hdrBuf;
    hdr->firstFree = PF_PAGE_LIST_END;
    hdr->numPages = 0;

	// 写入文件头
    fs->write(hdrBuf, PF_FILE_HDR_SIZE);
    if (!fs->good()) {
        fs->close();
        delete fs;
        std::remove(fileName);
        return PF_HDRWRITE;
    }
    fs->flush();

    // 分配并注册 fd
    int fd = GetNextFd(fileName);
    g_fdToStream[fd] = fs;
    g_fileMap[fileName] = fd;

    // 关闭文件流（只创建，不保持打开）
    fs->close();

    return 0;
}

// ---------------------------------------------------------------------
// DestroyFile
// 删除一个分页文件
// ---------------------------------------------------------------------
RC PF_Manager::DestroyFile(const char* fileName)
{
    // 检查并移除文件
    if (std::remove(fileName) != 0)
        return PF_UNIX;
    return 0;
}

// ---------------------------------------------------------------------
// OpenFile
// 打开分页文件，并读取文件头。
// 文件句柄（PF_FileHandle）会绑定到缓冲管理器。
// ---------------------------------------------------------------------
RC PF_Manager::OpenFile(const char* fileName, PF_FileHandle& fileHandle)
{
    int rc;

    if (fileHandle.bFileOpen)
        return PF_FILEOPEN;

    // 获取fd并打开文件流
    int fd = GetNextFd(fileName);
    std::fstream* fs = OpenFileStream(fileName, ios::in | ios::out | ios::binary);
    if (!fs || !fs->is_open())
        return PF_UNIX;

    g_fdToStream[fd] = fs;

    // 读取文件头
    fs->seekg(0, ios::beg);
    fs->read(reinterpret_cast<char*>(&fileHandle.hdr), sizeof(PF_FileHdr));
    if (!fs->good()) {
        rc = PF_HDRREAD;
        CloseFileStream(fd);
        return rc;
    }

    // 初始化句柄
    fileHandle.unixfd = fd;
    fileHandle.bHdrChanged = FALSE;
    fileHandle.pBufferMgr = pBufferMgr;
    fileHandle.bFileOpen = TRUE;

    return 0;
}

// ---------------------------------------------------------------------
// CloseFile
// 关闭分页文件，刷新缓冲区并更新文件头。
// ---------------------------------------------------------------------
RC PF_Manager::CloseFile(PF_FileHandle& fileHandle)
{
    RC rc;

    if (!fileHandle.bFileOpen)
        return PF_CLOSEDFILE;

    // 刷新缓冲区
    if ((rc = fileHandle.FlushPages()))
        return rc;

    // 写回文件头（若修改）
    if (fileHandle.bHdrChanged) {
        auto it = g_fdToStream.find(fileHandle.unixfd);
        if (it == g_fdToStream.end() || !it->second || !it->second->is_open())
            return PF_UNIX;
        std::fstream* fs = it->second;
        fs->seekp(0, ios::beg);
        fs->write(reinterpret_cast<char*>(&fileHandle.hdr), sizeof(PF_FileHdr));
        fs->flush();
        if (!fs->good())
            return PF_HDRWRITE;
    }

    // 关闭流
    CloseFileStream(fileHandle.unixfd);

    fileHandle.bFileOpen = FALSE;
    fileHandle.pBufferMgr = NULL;
    return 0;
}

// ---------------------------------------------------------------------
// ClearBuffer
// 调用缓冲池接口：清空所有页。
// ---------------------------------------------------------------------
RC PF_Manager::ClearBuffer()
{
    return pBufferMgr->ClearBuffer();
}

// ---------------------------------------------------------------------
// PrintBuffer
// 打印缓冲池内容（调试用）
// ---------------------------------------------------------------------
RC PF_Manager::PrintBuffer()
{
    return pBufferMgr->PrintBuffer();
}

// ---------------------------------------------------------------------
// ResizeBuffer
// 调整缓冲池大小
// ---------------------------------------------------------------------
RC PF_Manager::ResizeBuffer(int iNewSize)
{
    return pBufferMgr->ResizeBuffer(iNewSize);
}

// ---------------------------------------------------------------------
// 内存块管理接口：直接转发到缓冲池
// ---------------------------------------------------------------------
RC PF_Manager::GetBlockSize(int& length) const
{
    return pBufferMgr->GetBlockSize(length);
}

RC PF_Manager::AllocateBlock(char*& buffer)
{
    return pBufferMgr->AllocateBlock(buffer);
}

RC PF_Manager::DisposeBlock(char* buffer)
{
    return pBufferMgr->DisposeBlock(buffer);
}
