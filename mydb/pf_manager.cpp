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
// PF_Manager ����������
// ---------------------------------------------------------------------
PF_Manager::PF_Manager()
{
    // Ĭ�ϻ���ش�С�����û�����
    pBufferMgr = new PF_BufferMgr(PF_BUFFER_SIZE);

}

PF_Manager::~PF_Manager()
{
    delete pBufferMgr;
}

// ---------------------------------------------------------------------
// CreateFile
// ����һ���µķ�ҳ�ļ���д���ļ�ͷ
// ---------------------------------------------------------------------
RC PF_Manager::CreateFile(const char* fileName)
{
    // ����ļ��Ƿ����
    std::ifstream checkFile(fileName, ios::binary);
    if (checkFile.good()) {
        checkFile.close();
        return -1;// �ļ��Ѵ���
    }

    // �����ļ�����д�����ƣ���վ����ݣ�
    std::fstream* fs = new std::fstream(fileName, ios::in | ios::out | ios::binary | ios::trunc);
    if (!fs->is_open()) {
        delete fs;
        return PF_UNIX;
    }

    // ��ʼ���ļ�ͷ
    char hdrBuf[PF_FILE_HDR_SIZE];
    memset(hdrBuf, 0, PF_FILE_HDR_SIZE);
    PF_FileHdr* hdr = (PF_FileHdr*)hdrBuf;
    hdr->firstFree = PF_PAGE_LIST_END;
    hdr->numPages = 0;

	// д���ļ�ͷ
    fs->write(hdrBuf, PF_FILE_HDR_SIZE);
    if (!fs->good()) {
        fs->close();
        delete fs;
        std::remove(fileName);
        return PF_HDRWRITE;
    }
    fs->flush();

    // ���䲢ע�� fd
    int fd = GetNextFd(fileName);
    g_fdToStream[fd] = fs;
    g_fileMap[fileName] = fd;

    // �ر��ļ�����ֻ�����������ִ򿪣�
    fs->close();

    return 0;
}

// ---------------------------------------------------------------------
// DestroyFile
// ɾ��һ����ҳ�ļ�
// ---------------------------------------------------------------------
RC PF_Manager::DestroyFile(const char* fileName)
{
    // ��鲢�Ƴ��ļ�
    if (std::remove(fileName) != 0)
        return PF_UNIX;
    return 0;
}

// ---------------------------------------------------------------------
// OpenFile
// �򿪷�ҳ�ļ�������ȡ�ļ�ͷ��
// �ļ������PF_FileHandle����󶨵������������
// ---------------------------------------------------------------------
RC PF_Manager::OpenFile(const char* fileName, PF_FileHandle& fileHandle)
{
    int rc;

    if (fileHandle.bFileOpen)
        return PF_FILEOPEN;

    // ��ȡfd�����ļ���
    int fd = GetNextFd(fileName);
    std::fstream* fs = OpenFileStream(fileName, ios::in | ios::out | ios::binary);
    if (!fs || !fs->is_open())
        return PF_UNIX;

    g_fdToStream[fd] = fs;

    // ��ȡ�ļ�ͷ
    fs->seekg(0, ios::beg);
    fs->read(reinterpret_cast<char*>(&fileHandle.hdr), sizeof(PF_FileHdr));
    if (!fs->good()) {
        rc = PF_HDRREAD;
        CloseFileStream(fd);
        return rc;
    }

    // ��ʼ�����
    fileHandle.unixfd = fd;
    fileHandle.bHdrChanged = FALSE;
    fileHandle.pBufferMgr = pBufferMgr;
    fileHandle.bFileOpen = TRUE;

    return 0;
}

// ---------------------------------------------------------------------
// CloseFile
// �رշ�ҳ�ļ���ˢ�»������������ļ�ͷ��
// ---------------------------------------------------------------------
RC PF_Manager::CloseFile(PF_FileHandle& fileHandle)
{
    RC rc;

    if (!fileHandle.bFileOpen)
        return PF_CLOSEDFILE;

    // ˢ�»�����
    if ((rc = fileHandle.FlushPages()))
        return rc;

    // д���ļ�ͷ�����޸ģ�
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

    // �ر���
    CloseFileStream(fileHandle.unixfd);

    fileHandle.bFileOpen = FALSE;
    fileHandle.pBufferMgr = NULL;
    return 0;
}

// ---------------------------------------------------------------------
// ClearBuffer
// ���û���ؽӿڣ��������ҳ��
// ---------------------------------------------------------------------
RC PF_Manager::ClearBuffer()
{
    return pBufferMgr->ClearBuffer();
}

// ---------------------------------------------------------------------
// PrintBuffer
// ��ӡ��������ݣ������ã�
// ---------------------------------------------------------------------
RC PF_Manager::PrintBuffer()
{
    return pBufferMgr->PrintBuffer();
}

// ---------------------------------------------------------------------
// ResizeBuffer
// ��������ش�С
// ---------------------------------------------------------------------
RC PF_Manager::ResizeBuffer(int iNewSize)
{
    return pBufferMgr->ResizeBuffer(iNewSize);
}

// ---------------------------------------------------------------------
// �ڴ�����ӿڣ�ֱ��ת���������
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
