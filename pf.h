#pragma once
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <memory>
#include <unordered_map>
#include "db.h"

using RC = int;
typedef int PageNum;

extern int g_nextFd;                               // 下一个文件ID
extern std::unordered_map<std::string, int> g_fileMap;   // fd -> 文件流映射
extern std::unordered_map<int, std::fstream*> g_fdToStream;
extern std::unordered_map<std::string, bool> g_fileExists;

// Helper function to get a unique fd for a file
static int GetNextFd(const char* fileName)
{
    std::string key(fileName);
    if (g_fileMap.find(key) != g_fileMap.end()) {
        return g_fileMap[key];
    }
    int fd = g_nextFd++;
    g_fileMap[key] = fd;
    return fd;
}

// Helper function to open file stream
static std::fstream* OpenFileStream(const char* fileName, std::ios_base::openmode mode)
{
    std::fstream* pf = new std::fstream(fileName, mode);
    return pf;
}

// Helper function to close and clean up file stream
static void CloseFileStream(int fd)
{
    if (g_fdToStream.find(fd) != g_fdToStream.end()) {
        std::fstream* pf = g_fdToStream[fd];
        if (pf && pf->is_open()) {
            pf->close();
        }
        delete pf;
        g_fdToStream.erase(fd);
    }
}


// ---------------------------------------------------------------------
// Basic Constants
// ---------------------------------------------------------------------

const int PF_PAGE_SIZE = 4096 - sizeof(int);   // Page size (bytes)
extern int PF_BUFFER_SIZE;     // Number of pages in the buffer
const int PF_HASH_TBL_SIZE = 30;     // Hash table size

const int PF_PAGE_LIST_END = -1;     // End of free page list
const int PF_PAGE_USED = -2;     // Page currently in use


// ---------------------------------------------------------------------
// Error codes and printing helper
// ---------------------------------------------------------------------

#define PF_PAGEPINNED      (START_PF_WARN + 0) // page pinned in buffer
#define PF_PAGENOTINBUF    (START_PF_WARN + 1) // page isn't pinned in buffer
#define PF_INVALIDPAGE     (START_PF_WARN + 2) // invalid page number
#define PF_FILEOPEN        (START_PF_WARN + 3) // file is open
#define PF_CLOSEDFILE      (START_PF_WARN + 4) // file is closed
#define PF_PAGEFREE        (START_PF_WARN + 5) // page already free
#define PF_PAGEUNPINNED    (START_PF_WARN + 6) // page already unpinned
#define PF_EOF             (START_PF_WARN + 7) // end of file
#define PF_TOOSMALL        (START_PF_WARN + 8) // Resize buffer too small
#define PF_LASTWARN        PF_TOOSMALL

#define PF_NOMEM           (START_PF_ERR - 0)  // no memory
#define PF_NOBUF           (START_PF_ERR - 1)  // no buffer space
#define PF_INCOMPLETEREAD  (START_PF_ERR - 2)  // incomplete read from file
#define PF_INCOMPLETEWRITE (START_PF_ERR - 3)  // incomplete write to file
#define PF_HDRREAD         (START_PF_ERR - 4)  // incomplete read of header
#define PF_HDRWRITE        (START_PF_ERR - 5)  // incomplete write to header

// Internal errors
#define PF_PAGEINBUF       (START_PF_ERR - 6) // new page already in buffer
#define PF_HASHNOTFOUND    (START_PF_ERR - 7) // hash table entry not found
#define PF_HASHPAGEEXIST   (START_PF_ERR - 8) // page already in hash table
#define PF_INVALIDNAME     (START_PF_ERR - 9) // invalid PC file name

#define PF_UNIX            (START_PF_ERR - 10) // Unix error
#define PF_LASTERROR       PF_UNIX

//
// HashEntry - Hash table bucket entries
//
struct PF_HashEntry {
    PF_HashEntry* next;   // next hash table element or NULL
    PF_HashEntry* prev;   // prev hash table element or NULL
    int          fd;      // file descriptor
    PageNum      pageNum; // page number
    int          slot;    // slot of this page in the buffer
};

//
// PF_HashTable - allow search, insertion, and deletion of hash table entries
//
class PF_HashTable {
public:
    PF_HashTable(int numBuckets);           // Constructor
    ~PF_HashTable();                         // Destructor
    RC  Find(int fd, PageNum pageNum, int& slot);
    // Set slot to the hash table
    // entry for fd and pageNum
    RC  Insert(int fd, PageNum pageNum, int slot);
    // Insert a hash table entry
    RC  Delete(int fd, PageNum pageNum);  // Delete a hash table entry

private:
    int Hash(int fd, PageNum pageNum) const
    {
        return ((fd + pageNum) % numBuckets);
    }   // Hash function
    int numBuckets;                               // Number of hash table buckets
    PF_HashEntry** hashTable;                     // Hash table
};



// ---------------------------------------------------------------------
// Page Header (stored at beginning of each page on disk)
// ---------------------------------------------------------------------
struct PF_PageHdr {
    int nextFree;   // Next free page number, or PF_PAGE_LIST_END / PF_PAGE_USED
};

const int PF_FILE_HDR_SIZE = PF_PAGE_SIZE + sizeof(PF_PageHdr);

// ---------------------------------------------------------------------
// File Header (stored in first page of each file)
// ---------------------------------------------------------------------
struct PF_FileHdr {
    int firstFree;   // Head of free-page linked list
    int numPages;    // Total number of pages in the file
};

// ---------------------------------------------------------------------
// Buffer Page Descriptor (for buffer pool management)
// ---------------------------------------------------------------------
struct PF_BufPageDesc {
    char       *pData;      // Pointer to page contents
    int        next;       // Next slot in used/free list
    int        prev;       // Previous slot
    int        bDirty;     // TRUE if page has been modified
    short int  pinCount;   // Number of pins (active users)
    PageNum    pageNum;    // Page number in file
    int        fd;         // File identifier
};


// ---------------------------------------------------------------------
// PF_BufferMgr: manages buffer pool pages (in-memory page cache)
// ---------------------------------------------------------------------
class PF_BufferMgr {
public:
    PF_BufferMgr(int numPages);

    ~PF_BufferMgr();

    // Retrieve page from buffer (or load from file)
    RC GetPage(int fd, PageNum pageNum, char** ppBuffer, int bMultiplePins = TRUE);
    // Allocate new page slot in buffer
    RC AllocatePage(int fd, PageNum pageNum, char** ppBuffer);
    // Mark page as dirty
    RC MarkDirty(int fd, PageNum pageNum);
    // Unpin page
    RC UnpinPage(int fd, PageNum pageNum);
    // Flush all pages for a given file
    RC FlushPages(int fd);
    // Force a page to disk without removing from buffer
    RC ForcePages(int fd, PageNum pageNum);

    // Clear all entries
    RC ClearBuffer();
    // Display buffer contents (debug)
    RC PrintBuffer();
    // Resize buffer pool
    RC ResizeBuffer(int iNewSize);

    // Memory block management (raw buffer usage)
    RC GetBlockSize(int &length) const;
    RC AllocateBlock(char *&buffer);
    RC DisposeBlock(char *buffer);

private:
    RC InsertFree(int slot);
    RC LinkHead(int slot);
    RC Unlink(int slot);
    RC InternalAlloc(int& slot);
    RC ReadPage(int fd, PageNum pageNum, char* dest);
    RC WritePage(int fd, PageNum pageNum, char* source);
    RC InitPageDesc(int fd, PageNum pageNum, int slot);

    PF_BufPageDesc *bufTable;// info on buffer pages
    PF_HashTable   hashTable;// Hash table object
    int numPages;
    int pageSize;
    int first;  // MRU
    int last;   // LRU
    int free;   // head of free list
};

// ---------------------------------------------------------------------
// PF_PageHandle: represents a single in-memory page
// ---------------------------------------------------------------------
class PF_PageHandle {
    friend class PF_FileHandle;
public:
    PF_PageHandle();
    ~PF_PageHandle();
    PF_PageHandle(const PF_PageHandle& pageHandle);
    PF_PageHandle& operator=(const PF_PageHandle& pageHandle);
    RC GetData(char*& pData) const;           // Set pData to point to the page contents
    RC GetPageNum(PageNum& pageNum) const;       // Return the page number
private:
    int  pageNum;                                  // page number
    char* pPageData;                               // pointer to page data
};

// ---------------------------------------------------------------------
// PF_FileHandle: logical handle to an opened file
// ---------------------------------------------------------------------
class PF_FileHandle {
    friend class PF_Manager;
public:
    PF_FileHandle();
    ~PF_FileHandle();
    PF_FileHandle(const PF_FileHandle& fileHandle);
    PF_FileHandle& operator=(const PF_FileHandle& fileHandle);
    // Get the first page
    RC GetFirstPage(PF_PageHandle& pageHandle) const;
    // Get the next page after current
    RC GetNextPage(PageNum current, PF_PageHandle& pageHandle) const;
    // Get a specific page
    RC GetThisPage(PageNum pageNum, PF_PageHandle& pageHandle) const;
    // Get the last page
    RC GetLastPage(PF_PageHandle& pageHandle) const;
    // Get the prev page after current
    RC GetPrevPage(PageNum current, PF_PageHandle& pageHandle) const;

    RC AllocatePage(PF_PageHandle& pageHandle);    // Allocate a new page
    RC DisposePage(PageNum pageNum);              // Dispose of a page
    RC MarkDirty(PageNum pageNum) const;        // Mark page as dirty
    RC UnpinPage(PageNum pageNum) const;        // Unpin the page

    // Flush pages from buffer pool.  Will write dirty pages to disk.
    RC FlushPages() const;

    // Force a page or pages to disk (but do not remove from the buffer pool)
    RC ForcePages(PageNum pageNum = ALL_PAGES) const;

private:
    // IsValidPageNum will return TRUE if page number is valid and FALSE
    // otherwise
    int IsValidPageNum(PageNum pageNum) const;

    PF_BufferMgr* pBufferMgr;                      // pointer to buffer manager
    PF_FileHdr hdr;                                // file header
    int bFileOpen;                                 // file open flag
    int bHdrChanged;                               // dirty flag for file hdr
    int unixfd;                                    // OS file descriptor
};

// ---------------------------------------------------------------------
// PF_Manager: manages creation, deletion, and opening of files
// ---------------------------------------------------------------------
class PF_Manager {
public:
    PF_Manager();                              // Constructor
    ~PF_Manager();                              // Destructor
    RC CreateFile(const char* fileName);       // Create a new file
    RC DestroyFile(const char* fileName);       // Delete a file
    // Open and close file methods
    RC OpenFile(const char* fileName, PF_FileHandle& fileHandle);
    RC CloseFile(PF_FileHandle& fileHandle);
    // Three methods that manipulate the buffer manager.
    RC ClearBuffer();
    RC PrintBuffer();
    RC ResizeBuffer(int iNewSize);

    // Return the size of the block that can be allocated.
    RC GetBlockSize(int& length) const;

    // Allocate a memory chunk that lives in buffer manager
    RC AllocateBlock(char*& buffer);
    // Dispose of a memory chunk managed by the buffer manager.
    RC DisposeBlock(char* buffer);

private:
    PF_BufferMgr* pBufferMgr;// page-buffer manager
};


//
// Print-error function and PF return code defines
//
//void PF_PrintError(RC rc);

