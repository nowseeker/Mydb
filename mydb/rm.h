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
// RID����¼��ʶ����ҳ�� + �ۺţ�
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
// RM_FileHeader�����ļ�ͷ��Ϣ�����ڵ�0ҳ��
//---------------------------------------------
struct RM_FileHeader {
    int firstFree;     // �׸�����ҳ
    int numPages;      // ��ҳ��
    int recordCount;   // �ļ����ܼ�¼��
};

// ҳ��ͷ���ṹ
struct RM_PageHdr {
    int nextFree;        // ָ����һ���п��пռ��ҳ��
    int recordCount;          // ��ǰҳ���еļ�¼��
    int freeSpaceOffset;     // ���пռ���ʼλ��
    int freeSpace;           // ʣ����ÿռ��С
};

// ��λ�ṹ
struct RM_SlotEntry {
    int offset;            // ��¼��ҳ���е�ƫ����(��ҳ����ʼλ��
    int length;           // ��¼���ܳ���
    unsigned char flags;  // ��Ϊflags��֧�ֶ���״̬
    // flagsλ���壺
    // bit 0: isValid (0=deleted, 1=valid)
    // bit 1-7: ����
};

// ��¼ͷ���ṹ
struct RM_RecordHeader {

    unsigned char flags;        // ��־λ
    // bit 0: hasNull
    // bit 1: hasVarLen
    // bit 2-7: ����
    unsigned char varLenAttrCount; // �䳤��������
    // nullBitmap ��̬�洢�������СΪ attrCount/8�ֽ�

};

// �䳤����������
struct VarLenAttrDesc {
    int offset;             // �䳤�������ݵ�ƫ��������Լ�¼��ʼλ��
    int length;            // �䳤���Ե�ʵ�ʳ���
};

// -------------------------------------------------------------
// RM_Record����װ��¼������ RID�������ϲ�ӿ���ʾ��
// -------------------------------------------------------------
class RM_Record {
public:
    RM_Record();
    ~RM_Record();

    RC GetData(char*& pData) const;   // ��ȡ��¼����ָ��
    RC GetRid(RID& rid) const;        // ��ȡ��¼��ʶ

    RC Set(const char* pData, int size, RID rid);

private:
    char* pData;     // ָ���¼���ݣ��� RecordHeader��
    int size;        // ��¼����
    RID rid;         // ��¼��ʶ
    bool isValid;    // �Ƿ���Ч
};

// -------------------------------------------------------------
// RM_FileHandle�������������ļ�����
// -------------------------------------------------------------
class RM_FileHandle {
    friend class RM_Manager;
public:
    RM_FileHandle();
    ~RM_FileHandle();

    // �� PF ���ļ���
    RC SetPFFileHandle(PF_FileHandle* pfHandle);

    // ���� / ��ȡ�ļ�ͷ����
    void SetFileHeader(const RM_FileHeader& hdr);
    RM_FileHeader GetFileHeader() const;

    // ���� / ��ȡ��ϵ��Ԫ��Ϣ
    void SetRelInfo(const RelCatEntry& relInfo);
    const RelCatEntry& GetRelInfo() const;

    // �����¼�����践�� RID��
    RC InsertRec(const char* recordBuf, int recordLen);

    // ɾ����¼��ͨ�� RID ��λ��
    RC DeleteRec(const RID& rid);

    // �޸ļ�¼����ɾ��壩
    RC UpdateRec(const RID& rid, const char* newData, int newLen);

    // ��ȡ��¼�������鿴��
    RC GetRec(const RID& rid, RM_Record& rec);

    // д���ļ�ͷ���� 0 ҳ��
    RC FlushFileHeader();

    // ���ļ���������ҳˢ�µ�����
    RC ForcePages();

    PF_FileHandle* GetPFFileHandle() const { return pfFileHandle; }
    bool IsHeaderChanged() const { return headerChanged; }

    RC CompactFile(); // �����ļ���ҳ���գ������ɾ����¼��


private:
    // �ڲ��������� -----------------------------------------
    RC FindFreePage(PageNum& pageNum, PF_PageHandle& pageHandle);
    RC InitNewPage(PF_PageHandle& pageHandle, PageNum pageNum);
    RC InsertIntoPage(PF_PageHandle& pageHandle, const char* recordBuf, int recordLen);

    void WritePageHeader(char* pData, const RM_PageHdr& hdr);
    void ReadPageHeader(const char* pData, RM_PageHdr& hdr);

    RC UpdateFileHeaderOnInsert(PageNum pageNum, const RM_PageHdr& pageHdr);

private:
    PF_FileHandle* pfFileHandle;  // PF ���ļ����
    RM_FileHeader fileHdr;        // �ļ�ͷ����
    RelCatEntry relInfo;          // ���Ԫ��Ϣ������ catalog��
    bool headerChanged;           // �ļ�ͷ�Ƿ��޸�
};


//---------------------------------------------
// RM_Manager��������ļ��Ĵ������
//---------------------------------------------
class RM_Manager {
public:
    RM_Manager(PF_Manager& pfm);
    ~RM_Manager();

    // �����ļ�����
    RC CreateFile(const char* fileName);

    // ɾ���ļ�����
    RC DestroyFile(const char* fileName);

    // ���ļ�
    RC OpenFile(const char* fileName, RM_FileHandle& fileHandle);

    // �ر��ļ�
    RC CloseFile(RM_FileHandle& fileHandle);

private:
    PF_Manager& pfManager;   // ���õײ� PF ������
};


//#define RM_PAGE_LIST_END     -1  // end of list of free pages
//#define RM_PAGE_FULLY_USED   -2  // page is fully used with no free slots
//
//struct RM_PageHeader {
//    int nextFree;         // ָ����һ���п��пռ��ҳ��
//    int recordCount;      // ��ǰҳ���еļ�¼��
//    int freeSpaceOffset;  // ���пռ���ʼλ�ã���ҳ����ʼ���㣩
//    int freeSpace;        // ʣ����ÿռ��С���ֽڣ�
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
