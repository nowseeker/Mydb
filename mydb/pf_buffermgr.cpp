#include "pf.h"

using namespace std;
#define INVALID_SLOT  (-1)
#define MEMORY_FD -1

extern int PF_BUFFER_SIZE;
// ---------------------------------------------------------------------
// 构造函数：初始化缓冲池、哈希表和空闲链表。
// ---------------------------------------------------------------------
PF_BufferMgr::PF_BufferMgr(int _numPages) : hashTable(PF_HASH_TBL_SIZE)
{
    this->numPages = _numPages;
    pageSize = PF_PAGE_SIZE + sizeof(PF_PageHdr);

    bufTable = new PF_BufPageDesc[numPages];
    for (int i = 0; i < numPages; i++) {
        bufTable[i].pData = new char[pageSize];
        memset(bufTable[i].pData, 0, pageSize);
        bufTable[i].prev = i - 1;
        bufTable[i].next = i + 1;
    }
    bufTable[0].prev = bufTable[numPages - 1].next = INVALID_SLOT;
    free = 0;
    first = last = INVALID_SLOT;
}

// ---------------------------------------------------------------------
// 析构函数：释放所有页缓冲。
// ---------------------------------------------------------------------
PF_BufferMgr::~PF_BufferMgr()
{
    for (int i = 0; i < numPages; i++)
        delete[] bufTable[i].pData;
    delete[] bufTable;
}

// ---------------------------------------------------------------------
// GetPage：获取指定页。
// 若该页不在缓冲中，从文件读入；否则直接返回并增加 pinCount。
// ---------------------------------------------------------------------
RC PF_BufferMgr::GetPage(int fd, PageNum pageNum, char** ppBuffer, int bMultiplePins)
{
    RC rc;
    int slot;
    if ((rc = hashTable.Find(fd, pageNum, slot)) && rc != PF_HASHNOTFOUND)
        return rc;
    if (rc == PF_HASHNOTFOUND) {
        if ((rc = InternalAlloc(slot)))
            return rc;
        if ((rc = ReadPage(fd, pageNum, bufTable[slot].pData)) ||
            (rc = hashTable.Insert(fd, pageNum, slot)) ||
            (rc = InitPageDesc(fd, pageNum, slot))) {
            Unlink(slot);
            InsertFree(slot);
            return rc;
        }
    }
    else {
        if (!bMultiplePins && bufTable[slot].pinCount > 0)
            return PF_PAGEPINNED;
        bufTable[slot].pinCount++;
        if ((rc = Unlink(slot)) || (rc = LinkHead(slot)))
            return rc;
    }
    *ppBuffer = bufTable[slot].pData;
    return 0;
}

// ---------------------------------------------------------------------
// AllocatePage：在缓冲中分配新页。
// ---------------------------------------------------------------------
RC PF_BufferMgr::AllocatePage(int fd, PageNum pageNum, char** ppBuffer)
{
    RC rc;
    int slot;
    if (!(rc = hashTable.Find(fd, pageNum, slot)))
        return PF_PAGEINBUF;
    else if (rc != PF_HASHNOTFOUND)
        return rc;

    if ((rc = InternalAlloc(slot)))
        return rc;

    if ((rc = hashTable.Insert(fd, pageNum, slot)) ||
        (rc = InitPageDesc(fd, pageNum, slot))) {
        Unlink(slot);
        InsertFree(slot);
        return rc;
    }

    *ppBuffer = bufTable[slot].pData;
    return 0;
}

// ---------------------------------------------------------------------
// MarkDirty：将页标记为已修改。
// ---------------------------------------------------------------------
RC PF_BufferMgr::MarkDirty(int fd, PageNum pageNum)
{
    RC rc;
    int slot;
    if ((rc = hashTable.Find(fd, pageNum, slot))) {
        if (rc == PF_HASHNOTFOUND) return PF_PAGENOTINBUF;
        else return rc;
    }
    if (bufTable[slot].pinCount == 0)
        return PF_PAGEUNPINNED;
    bufTable[slot].bDirty = TRUE;
    if ((rc = Unlink(slot)) || (rc = LinkHead(slot)))
        return rc;
    return 0;
}

// ---------------------------------------------------------------------
// UnpinPage：减少 pinCount，若为0则可替换。
// ---------------------------------------------------------------------
RC PF_BufferMgr::UnpinPage(int fd, PageNum pageNum)
{
    RC rc;
    int slot;
    if ((rc = hashTable.Find(fd, pageNum, slot))) {
        if (rc == PF_HASHNOTFOUND) return PF_PAGENOTINBUF;
        else return rc;
    }
    if (bufTable[slot].pinCount == 0)
        return PF_PAGEUNPINNED;
    if (--bufTable[slot].pinCount == 0)
        if ((rc = Unlink(slot)) || (rc = LinkHead(slot))) return rc;
    return 0;
}

// ---------------------------------------------------------------------
// FlushPages：将某文件的所有页写回磁盘。
// ---------------------------------------------------------------------
RC PF_BufferMgr::FlushPages(int fd)
{
    RC rc, rcWarn = 0;
    int slot = first;
    while (slot != INVALID_SLOT) {
        int next = bufTable[slot].next;
        if (bufTable[slot].fd == fd) {
            if (bufTable[slot].pinCount)
                rcWarn = PF_PAGEPINNED;
            else {
                if (bufTable[slot].bDirty) {
                    if ((rc = WritePage(fd, bufTable[slot].pageNum, bufTable[slot].pData)))
                        return rc;
                    bufTable[slot].bDirty = FALSE;
                }
                if ((rc = hashTable.Delete(fd, bufTable[slot].pageNum)) ||
                    (rc = Unlink(slot)) ||
                    (rc = InsertFree(slot)))
                    return rc;
            }
        }
        slot = next;
    }
    return rcWarn;
}

// ---------------------------------------------------------------------
// ForcePages：强制写脏页但不移除。
// ---------------------------------------------------------------------
RC PF_BufferMgr::ForcePages(int fd, PageNum pageNum)
{
    RC rc;
    int slot = first;
    while (slot != INVALID_SLOT) {
        int next = bufTable[slot].next;
        if (bufTable[slot].fd == fd &&
            (pageNum == ALL_PAGES || bufTable[slot].pageNum == pageNum)) {
            if (bufTable[slot].bDirty) {
                if ((rc = WritePage(fd, bufTable[slot].pageNum, bufTable[slot].pData)))
                    return rc;
                bufTable[slot].bDirty = FALSE;
            }
        }
        slot = next;
    }
    return 0;
}

// ---------------------------------------------------------------------
// PrintBuffer：调试打印缓冲池内容。
// ---------------------------------------------------------------------
RC PF_BufferMgr::PrintBuffer()
{
    cout << "Buffer contains " << numPages << " pages of size " << pageSize << ".\n";
    cout << "Contents in order from most recently used to least recently used.\n";
    int slot = first, next;
    while (slot != INVALID_SLOT) {
        next = bufTable[slot].next;
        cout << slot << ": fd=" << bufTable[slot].fd
            << " pageNum=" << bufTable[slot].pageNum
            << " dirty=" << bufTable[slot].bDirty//<<"\n";
            << " pin=" << bufTable[slot].pinCount << "\n";
        slot = next;
    }
    return 0;
}

//------------------------------------------------------------------------------
// 清空缓冲区
//------------------------------------------------------------------------------
RC PF_BufferMgr::ClearBuffer()
{
    RC rc;
    int slot = first, next;
    while (slot != INVALID_SLOT) {
        next = bufTable[slot].next;
        if (bufTable[slot].pinCount == 0)
            if ((rc = hashTable.Delete(bufTable[slot].fd, bufTable[slot].pageNum)) ||
                (rc = Unlink(slot)) || (rc = InsertFree(slot)))
                return rc;
        slot = next;
    }
    return 0;
}

//------------------------------------------------------------------------------
// 重设缓冲区大小
//------------------------------------------------------------------------------
RC PF_BufferMgr::ResizeBuffer(int iNewSize)
{
    RC rc;
    ClearBuffer();

    PF_BufPageDesc* pNewBufTable = new PF_BufPageDesc[iNewSize];
    for (int i = 0; i < iNewSize; i++) {
        pNewBufTable[i].pData = new char[pageSize];
        memset(pNewBufTable[i].pData, 0, pageSize);
        pNewBufTable[i].prev = i - 1;
        pNewBufTable[i].next = i + 1;
    }
    pNewBufTable[0].prev = pNewBufTable[iNewSize - 1].next = INVALID_SLOT;

    int oldFirst = first;
    PF_BufPageDesc* pOldBufTable = bufTable;

    numPages = iNewSize;
    first = last = INVALID_SLOT;
    free = 0;
    bufTable = pNewBufTable;

    int slot = oldFirst, next, newSlot;
    while (slot != INVALID_SLOT) {
        next = pOldBufTable[slot].next;
        if ((rc = InternalAlloc(newSlot))) return rc;
        if ((rc = hashTable.Insert(pOldBufTable[slot].fd, pOldBufTable[slot].pageNum, newSlot)) ||
            (rc = InitPageDesc(pOldBufTable[slot].fd, pOldBufTable[slot].pageNum, newSlot)))
            return rc;
        Unlink(newSlot);
        InsertFree(newSlot);
        slot = next;
    }

    delete[] pOldBufTable;
    return 0;
}

//------------------------------------------------------------------------------
// InsertFree / LinkHead / Unlink / InternalAlloc (逻辑不变)
//------------------------------------------------------------------------------
RC PF_BufferMgr::InsertFree(int slot)
{
    bufTable[slot].next = free;
    free = slot;
    return 0;
}

RC PF_BufferMgr::LinkHead(int slot)
{
    bufTable[slot].next = first;
    bufTable[slot].prev = INVALID_SLOT;
    if (first != INVALID_SLOT) bufTable[first].prev = slot;
    first = slot;
    if (last == INVALID_SLOT) last = first;
    return 0;
}

RC PF_BufferMgr::Unlink(int slot)
{
    if (first == slot) first = bufTable[slot].next;
    if (last == slot) last = bufTable[slot].prev;
    if (bufTable[slot].next != INVALID_SLOT)
        bufTable[bufTable[slot].next].prev = bufTable[slot].prev;
    if (bufTable[slot].prev != INVALID_SLOT)
        bufTable[bufTable[slot].prev].next = bufTable[slot].next;
    bufTable[slot].prev = bufTable[slot].next = INVALID_SLOT;
    return 0;
}

RC PF_BufferMgr::InternalAlloc(int& slot)
{
    RC rc;
    if (free != INVALID_SLOT) {
        slot = free;
        free = bufTable[slot].next;
    }
    else {
        for (slot = last; slot != INVALID_SLOT; slot = bufTable[slot].prev)
            if (bufTable[slot].pinCount <= 1) break;// 重大改动，test
        if (slot == INVALID_SLOT) return PF_NOBUF;
        if (bufTable[slot].bDirty) {
            if ((rc = WritePage(bufTable[slot].fd, bufTable[slot].pageNum, bufTable[slot].pData)))
                return rc;
            bufTable[slot].bDirty = FALSE;
        }
        if ((rc = hashTable.Delete(bufTable[slot].fd, bufTable[slot].pageNum)) || (rc = Unlink(slot)))
            return rc;
    }
    if ((rc = LinkHead(slot))) return rc;
    return 0;
}

//------------------------------------------------------------------------------
// ReadPage / WritePage 使用 fstream 替代低级IO
//------------------------------------------------------------------------------
RC PF_BufferMgr::ReadPage(int fd, PageNum pageNum, char* dest)
{
    if (g_fdToStream.find(fd) == g_fdToStream.end()) return PF_UNIX;
    std::fstream* fs = g_fdToStream[fd];
    if (!fs || !fs->is_open()) return PF_UNIX;

    long offset = pageNum * (long)pageSize + PF_FILE_HDR_SIZE;
    fs->seekg(offset, ios::beg);
    fs->read(dest, pageSize);
    if (!(*fs)) return PF_INCOMPLETEREAD;
    return 0;
}

RC PF_BufferMgr::WritePage(int fd, PageNum pageNum, char* source)
{
    if (g_fdToStream.find(fd) == g_fdToStream.end()) return PF_UNIX;
    std::fstream* fs = g_fdToStream[fd];
    if (!fs || !fs->is_open()) return PF_UNIX;

    long offset = pageNum * (long)pageSize + PF_FILE_HDR_SIZE;
    fs->seekp(offset, ios::beg);
    fs->write(source, pageSize);
    fs->flush();
    if (!(*fs)) return PF_INCOMPLETEWRITE;
    return 0;
}

//------------------------------------------------------------------------------
// 初始化页描述符
//------------------------------------------------------------------------------
RC PF_BufferMgr::InitPageDesc(int fd, PageNum pageNum, int slot)
{
    bufTable[slot].fd = fd;
    bufTable[slot].pageNum = pageNum;
    bufTable[slot].bDirty = FALSE;
    bufTable[slot].pinCount = 1;// 原为1
    return 0;
}

//------------------------------------------------------------------------------
// 内存页操作函数
//------------------------------------------------------------------------------

RC PF_BufferMgr::GetBlockSize(int& length) const
{
    length = pageSize;
    return OK_RC;
}

RC PF_BufferMgr::AllocateBlock(char*& buffer)
{
    RC rc = OK_RC;
    int slot;
    if ((rc = InternalAlloc(slot)) != OK_RC)
        return rc;
    PageNum pageNum = PageNum(reinterpret_cast<uintptr_t>(bufTable[slot].pData));
    if ((rc = hashTable.Insert(MEMORY_FD, pageNum, slot) != OK_RC) ||
        (rc = InitPageDesc(MEMORY_FD, pageNum, slot)) != OK_RC) {
        Unlink(slot);
        InsertFree(slot);
        return rc;
    }
    buffer = bufTable[slot].pData;
    return OK_RC;
}

RC PF_BufferMgr::DisposeBlock(char* buffer)
{
    return UnpinPage(MEMORY_FD, PageNum(reinterpret_cast<uintptr_t>(buffer)));
}