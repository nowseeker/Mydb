#pragma once
#include "pf.h"
#include "catalog.h"

//
// PageNum: uniquely identifies a page in a file
//
typedef int PageNum;

//
// SlotNum: uniquely identifies a record in a page
//
typedef int SlotNum;


//---------------------------------------------
// RID：记录标识符（页号 + 槽号）
//---------------------------------------------
class RID {
public:
    static const PageNum NULL_PAGE = -1;
    static const SlotNum NULL_SLOT = -1;
    RID():page(NULL_PAGE), slot(NULL_SLOT) {
    }     // Default constructor
    RID(PageNum pageNum, SlotNum slotNum):page(pageNum), slot(slotNum) {
    }
    ~RID() {}

    RC GetPageNum(PageNum& pageNum) const {
        pageNum = page; return 0;
    }
    RC GetSlotNum(SlotNum& slotNum) const {
        slotNum = slot; return 0;
    }
    PageNum Page() const {
        return page;
    }
    SlotNum Slot() const {
        return slot;
    }
    bool operator==(const RID& rhs) const
    {
        PageNum p; SlotNum s;
        rhs.GetPageNum(p);
        rhs.GetSlotNum(s);
        return (p == page && s == slot);
    }
private:
    PageNum page;
    SlotNum slot;
};

//---------------------------------------------
// RM_FileHeader：表文件头信息（存在第0页）
//---------------------------------------------
struct RM_FileHeader {
    int firstFree;     // 首个空闲页
    int numPages;      // 总页数
    int recordCount;   // 文件中总记录数
};

// 页面头部结构
struct RM_PageHdr {
    int nextFree;        // 指向下一个有空闲空间的页面
    int recordCount;          // 当前页面中的记录数
    int freeSpaceOffset;     // 空闲空间起始位置
    int freeSpace;           // 剩余可用空间大小
};

// 槽位结构
struct RM_SlotEntry {
    int offset;            // 记录在页面中的偏移量(从页面起始位置
    int length;           // 记录的总长度
    unsigned char flags;  // 改为flags，支持多种状态
    // flags位定义：
    // bit 0: isValid (0=deleted, 1=valid)
    // bit 1-7: 保留
};

// 记录头部结构
struct RM_RecordHeader {

    unsigned char flags;        // 标志位
    // bit 0: hasNull
    // bit 1: hasVarLen
    // bit 2-7: 保留
    unsigned char varLenAttrCount; // 变长属性数量
    // nullBitmap 动态存储在这里，大小为 attrCount/8字节

};

// 变长属性描述符
struct VarLenAttrDesc {
    int offset;             // 变长属性内容的偏移量（相对记录起始位置
    int length;            // 变长属性的实际长度
};

// -------------------------------------------------------------
// RM_Record：封装记录数据与 RID（用于上层接口显示）
// -------------------------------------------------------------
class RM_Record {
public:
    RM_Record();
    ~RM_Record();

    RC GetData(char*& pData) const;   // 获取记录数据指针
    RC GetRid(RID& rid) const;        // 获取记录标识

    RC Set(const char* pData, int size, RID rid);

private:
    char* pData;     // 指向记录内容（含 RecordHeader）
    int size;        // 记录长度
    RID rid;         // 记录标识
    bool isValid;    // 是否有效
};

// -------------------------------------------------------------
// RM_FileHandle：操作单个表文件的类
// -------------------------------------------------------------
class RM_FileHandle {
    friend class RM_Manager;
public:
    RM_FileHandle();
    ~RM_FileHandle();

    // 与 PF 层文件绑定
    RC SetPFFileHandle(PF_FileHandle* pfHandle);

    // 设置 / 获取文件头缓存
    void SetFileHeader(const RM_FileHeader& hdr);
    RM_FileHeader GetFileHeader() const;

    // 设置 / 获取关系表元信息
    void SetRelInfo(const RelCatEntry& relInfo);
    const RelCatEntry& GetRelInfo() const;

    // 插入记录（无需返回 RID）
    RC InsertRec(const char* recordBuf, int recordLen);

    // 删除记录（通过 RID 定位）
    RC DeleteRec(const RID& rid);

    // 修改记录（先删后插）
    RC UpdateRec(const RID& rid, const char* newData, int newLen);

    // 获取记录（仅供查看）
    RC GetRec(const RID& rid, RM_Record& rec);

    // 写回文件头（第 0 页）
    RC FlushFileHeader();

    // 将文件中所有脏页刷新到磁盘
    RC ForcePages();

    PF_FileHandle* GetPFFileHandle() const { return pfFileHandle; }
    bool IsHeaderChanged() const { return headerChanged; }

    RC CompactFile(); // 将表文件按页紧凑（清除已删除记录）


private:
    // 内部辅助函数 -----------------------------------------
    RC FindFreePage(PageNum& pageNum, PF_PageHandle& pageHandle);
    RC InitNewPage(PF_PageHandle& pageHandle, PageNum pageNum);
    RC InsertIntoPage(PF_PageHandle& pageHandle, const char* recordBuf, int recordLen);

    void WritePageHeader(char* pData, const RM_PageHdr& hdr);
    void ReadPageHeader(const char* pData, RM_PageHdr& hdr);

    RC UpdateFileHeaderOnInsert(PageNum pageNum, const RM_PageHdr& pageHdr);

private:
    PF_FileHandle* pfFileHandle;  // PF 层文件句柄
    RM_FileHeader fileHdr;        // 文件头缓存
    RelCatEntry relInfo;          // 表的元信息（来自 catalog）
    bool headerChanged;           // 文件头是否被修改
};


//---------------------------------------------
// RM_Manager：负责表文件的创建与打开
//---------------------------------------------
class RM_Manager {
public:
    RM_Manager(PF_Manager& pfm);
    ~RM_Manager();

    // 创建文件（表）
    RC CreateFile(const char* fileName);

    // 删除文件（表）
    RC DestroyFile(const char* fileName);

    // 打开文件
    RC OpenFile(const char* fileName, RM_FileHandle& fileHandle);

    // 关闭文件
    RC CloseFile(RM_FileHandle& fileHandle);

private:
    PF_Manager& pfManager;   // 引用底层 PF 管理器
};


//#define RM_PAGE_LIST_END     -1  // end of list of free pages
//#define RM_PAGE_FULLY_USED   -2  // page is fully used with no free slots
//
//struct RM_PageHeader {
//    int nextFree;         // 指向下一个有空闲空间的页面
//    int recordCount;      // 当前页面中的记录数
//    int freeSpaceOffset;  // 空闲空间起始位置（从页面起始计算）
//    int freeSpace;        // 剩余可用空间大小（字节）
//
//    RM_PageHeader() : nextFree(RM_PAGE_LIST_END), recordCount(0),
//        freeSpaceOffset(PF_PAGE_SIZE), freeSpace(0) {
//    }
//
//    int size() const { return sizeof(RM_PageHeader); }
//
//    void to_buf(char* buf) const {
//        memcpy(buf, this, sizeof(RM_PageHeader));
//    }
//
//    void from_buf(const char* buf) {
//        memcpy(this, buf, sizeof(RM_PageHeader));
//    }
//};


//class RM_Record {
//    friend class RM_RecordTest;
//public:
//    RM_Record();
//    ~RM_Record();
//
//    // Return the data corresponding to the record.
//    RC GetData(char*& pData) const;
//
//    // Sets data in the record (supports variable length)
//    RC Set(char* pData, int size, RID id);
//
//    // Return the RID associated with the record
//    RC GetRid(RID& rid) const;
//
//    // Get record size
//    int GetRecordSize() const { return recordSize; }
//
//private:
//    int recordSize;
//    char* data;
//    RID rid;
//};


//class RM_FileHandle {
//    friend class RM_FileHandleTest;
//    friend class RM_FileScan;
//    friend class RM_Manager;
//public:
//    RM_FileHandle();
//    RC Open(PF_FileHandle*, int maxRecordSize);
//    RC SetHdr(RM_FileHeader h) { hdr = h; return 0; }
//    ~RM_FileHandle();
//
//    // Given a RID, return the record
//    RC GetRec(const RID& rid, RM_Record& rec) const;
//
//    // Insert a new record (variable length)
//    RC InsertRec(const char* pData, int length, RID& rid);
//
//    // Delete a record
//    RC DeleteRec(const RID& rid);
//
//    // Update a record
//    RC UpdateRec(const RM_Record& rec);
//
//    // Forces a page from the buffer pool to disk
//    RC ForcePages(PageNum pageNum = ALL_PAGES);
//
//    RC GetPF_FileHandle(PF_FileHandle&) const;
//    bool hdrChanged() const { return bHdrChanged; }
//    int maxRecordSize() const { return hdr.maxRecordSize; }
//    int GetNumPages() const { return hdr.numPages; }
//
//    RC IsValid() const;
//
//private:
//    bool IsValidPageNum(const PageNum pageNum) const;
//    bool IsValidRID(const RID rid) const;
//
//    // Return next free page or allocate one as needed
//    RC GetNextFreePage(PageNum& pageNum, int recordLength);
//
//    // Find a page with enough free space
//    RC FindPageWithSpace(PageNum& pageNum, int recordLength);
//
//    // Get/Set page header
//    RC GetPageHeader(PF_PageHandle ph, RM_PageHeader& pHdr) const;
//    RC SetPageHeader(PF_PageHandle ph, const RM_PageHeader& pHdr);
//
//    // Slot operations
//    RC GetSlotEntry(PF_PageHandle ph, SlotNum s, RM_SlotEntry& slot) const;
//    RC SetSlotEntry(PF_PageHandle ph, SlotNum s, const RM_SlotEntry& slot);
//    RC GetSlotPointer(PF_PageHandle ph, SlotNum s, char*& pData) const;
//
//    // File header operations
//    RC GetFileHeader(PF_PageHandle ph);
//    RC SetFileHeader(PF_PageHandle ph) const;
//
//    // Calculate available space for records in a page
//    int GetAvailableSpaceInPage() const;
//
//    PF_FileHandle* pfHandle;   // pointer to opened PF_FileHandle
//    RM_FileHeader hdr;            // file header
//    bool bFileOpen;            // file open flag
//    bool bHdrChanged;          // dirty flag for file hdr
//};

//
// RM_FileScan: condition-based scan of records in the file
//
//class RM_FileScan {
//public:
//    RM_FileScan();
//    ~RM_FileScan();
//
//    RC OpenScan(const RM_FileHandle& fileHandle,
//        AttrType   attrType,
//        int        attrLength,
//        int        attrOffset,
//        CompOp     compOp,
//        void* value,
//        ClientHint pinHint = NO_HINT);
//    RC GetNextRec(RM_Record& rec);
//    RC CloseScan();
//    bool IsOpen() const { return (bOpen && prmh != NULL && pred != NULL); }
//    void resetState() { current = RID(1, -1); }
//    RC GotoPage(PageNum p);
//
//private:
//    Predicate* pred;
//    RM_FileHandle* prmh;
//    RID current;
//    bool bOpen;
//};
