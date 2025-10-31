#include "rm.h"
using namespace std;

// -------------------------------------------------------------
// ���� / ����
// -------------------------------------------------------------
RM_FileHandle::RM_FileHandle()
    : pfFileHandle(nullptr), headerChanged(false) {
    memset(&fileHdr, 0, sizeof(RM_FileHeader));
    memset(&relInfo, 0, sizeof(RelCatEntry));
}

RM_FileHandle::~RM_FileHandle() {}

// -------------------------------------------------------------
// Set / Get �ӿ�
// -------------------------------------------------------------
RC RM_FileHandle::SetPFFileHandle(PF_FileHandle* pfHandle) {
    pfFileHandle = pfHandle;
    return 0;
}

void RM_FileHandle::SetFileHeader(const RM_FileHeader& hdr) {
    fileHdr = hdr;
    headerChanged = false;
}

RM_FileHeader RM_FileHandle::GetFileHeader() const {
    return fileHdr;
}

void RM_FileHandle::SetRelInfo(const RelCatEntry& info) {
    relInfo = info;
}

const RelCatEntry& RM_FileHandle::GetRelInfo() const {
    return relInfo;
}

// -------------------------------------------------------------
// InsertRec()
// -------------------------------------------------------------
// �ҵ��ɲ���Ŀ���ҳ��FindFreePage��
// ���޿���ҳ�������ҳ��InitNewPage��
// �����¼��InsertIntoPage��
// �����ļ�ͷ��Ϣ
// -------------------------------------------------------------
RC RM_FileHandle::InsertRec(const char* recordBuf, int recordLen) {
    if (!pfFileHandle || !recordBuf || recordLen <= 0)
        return -1;

    RC rc;
    PF_PageHandle pageHandle;
    PageNum pageNum;

    // === 1. �ҿ���ҳ ===
    rc = FindFreePage(pageNum, pageHandle);
    if (rc != 0) {
        // ���޿���ҳ���������ҳ
        rc = pfFileHandle->AllocatePage(pageHandle);
        if (rc != 0) return rc;

        pageHandle.GetPageNum(pageNum);

        rc = InitNewPage(pageHandle, pageNum);
        if (rc != 0) {
            pfFileHandle->UnpinPage(pageNum);// newadd
            return rc;
        }
    }

    // === 2. �����¼ ===
    rc = InsertIntoPage(pageHandle, recordBuf, recordLen);
    if (rc != 0) {
        // ��ǰҳ�Ų��£�������ҳ
        pfFileHandle->UnpinPage(pageNum);// newadd
        PF_PageHandle newPage;
        RC newRc = pfFileHandle->AllocatePage(newPage);
        if (newRc != 0) return newRc;

        PageNum newPageNum;
        newPage.GetPageNum(newPageNum);
        InitNewPage(newPage, newPageNum);
        rc = InsertIntoPage(newPage, recordBuf, recordLen);

        if (rc != 0) {
            pfFileHandle->UnpinPage(newPageNum);
            return rc;
        }

        pfFileHandle->MarkDirty(newPageNum);
        pfFileHandle->UnpinPage(newPageNum);

        fileHdr.numPages++;
        fileHdr.recordCount++;
        headerChanged = true;
        return 0;
    }

    // === 3. �ɹ����������ļ�ͷ ===
    fileHdr.recordCount++;
    headerChanged = true;

    // д��ҳ
    pfFileHandle->MarkDirty(pageNum);
    pfFileHandle->UnpinPage(pageNum);

    return 0;
}

// -------------------------------------------------------------
// DeleteRec()
// -------------------------------------------------------------
RC RM_FileHandle::DeleteRec(const RID& rid) {
    PageNum pageNum = rid.Page();
    SlotNum slotNum = rid.Slot();

    PF_PageHandle pageHandle;
    RC rc = pfFileHandle->GetThisPage(pageNum, pageHandle);
    if (rc != 0) return rc;

    char* pData = nullptr;
    pageHandle.GetData(pData);

    RM_PageHdr pageHdr;
    ReadPageHeader(pData, pageHdr);

    if (slotNum < 0 || slotNum >= pageHdr.recordCount) {
        pfFileHandle->UnpinPage(pageNum);
        return -1;
    }

    RM_SlotEntry* slot =
        (RM_SlotEntry*)(pData + sizeof(RM_PageHdr) + slotNum * sizeof(RM_SlotEntry));

    if (!(slot->flags & 0x1)) {
        pfFileHandle->UnpinPage(pageNum);
        return -1; // ��ɾ��
    }

    // === �߼�ɾ�� ===
    slot->flags &= ~0x1;

    // �����ղۻ��¼���������ɾ��
    WritePageHeader(pData, pageHdr);
    pfFileHandle->MarkDirty(pageNum);

    fileHdr.recordCount--;
    headerChanged = true;

    pfFileHandle->UnpinPage(pageNum);
    return 0;
}

// -------------------------------------------------------------
// UpdateRec() = Delete + Insert
// -------------------------------------------------------------
RC RM_FileHandle::UpdateRec(const RID& rid, const char* newData, int newLen) {
    RC rc = DeleteRec(rid);
    if (rc != 0) return rc;
    return InsertRec(newData, newLen);
}

// -------------------------------------------------------------
// GetRec()
// -------------------------------------------------------------
RC RM_FileHandle::GetRec(const RID& rid, RM_Record& rec) {
    if (!pfFileHandle) return -1;

    PageNum pageNum = rid.Page();
    SlotNum slotNum = rid.Slot();

    PF_PageHandle pageHandle;
    RC rc = pfFileHandle->GetThisPage(pageNum, pageHandle);
    if (rc != 0) return rc;

    char* pData;
    pageHandle.GetData(pData);

    RM_PageHdr pageHdr;
    ReadPageHeader(pData, pageHdr);

    if (slotNum < 0 || slotNum >= pageHdr.recordCount) {
        pfFileHandle->UnpinPage(pageNum);
        return -1;
    }

    RM_SlotEntry* slot =
        (RM_SlotEntry*)(pData + sizeof(RM_PageHdr) + slotNum * sizeof(RM_SlotEntry));
    if (!(slot->flags & 0x1)) {
        pfFileHandle->UnpinPage(pageNum);
        return -1;
    }

    char* recordPtr = pData + slot->offset;
    rec.Set(recordPtr, slot->length, rid);

    pfFileHandle->UnpinPage(pageNum);
    return 0;
}

// -------------------------------------------------------------
// FlushFileHeader()
// -------------------------------------------------------------
RC RM_FileHandle::FlushFileHeader() {
    if (!pfFileHandle) return -1;

    PF_PageHandle pageHandle;
    RC rc = pfFileHandle->GetThisPage(0, pageHandle);
    if (rc != 0) return rc;

    char* pData;
    pageHandle.GetData(pData);

    memcpy(pData, &fileHdr, sizeof(RM_FileHeader));

    pfFileHandle->MarkDirty(0);
    pfFileHandle->UnpinPage(0);
    headerChanged = false;

    return 0;
}

// -------------------------------------------------------------
// ForcePages()
// -------------------------------------------------------------
RC RM_FileHandle::ForcePages() {
    if (!pfFileHandle) return -1;
    return pfFileHandle->ForcePages();
}

// -------------------------------------------------------------
// FindFreePage()
// -------------------------------------------------------------
RC RM_FileHandle::FindFreePage(PageNum& pageNum, PF_PageHandle& pageHandle) {
    // �� firstFree ��Ч���ļ�����ҳ���򷵻ش���
    if (fileHdr.firstFree < 0 || fileHdr.numPages == 0)
        return -1;

    RC rc = pfFileHandle->GetThisPage(fileHdr.firstFree, pageHandle);
    if (rc != 0) return rc;

    pageNum = fileHdr.firstFree;

    // ����ҳ�Ƿ����пռ�
    char* pData;
    pageHandle.GetData(pData);
    RM_PageHdr hdr;
    ReadPageHeader(pData, hdr);

    int minNeeded = sizeof(RM_SlotEntry) + relInfo.fixedRecordSize;
    if (hdr.freeSpace < minNeeded) {
        // ҳ�������������Ƴ�
        fileHdr.firstFree = hdr.nextFree;
        headerChanged = true;
        pfFileHandle->UnpinPage(pageNum);
        return -1;
    }

    return 0;
}

// -------------------------------------------------------------
// InitNewPage()
// -------------------------------------------------------------
RC RM_FileHandle::InitNewPage(PF_PageHandle& pageHandle, PageNum pageNum) {
    char* pData;
    RC rc=pageHandle.GetData(pData);

    if (rc != 0 || !pData) {// newadd
        pfFileHandle->UnpinPage(pageNum);   // �����޷����������ͷ�ҳ
        return -1;
    }

    RM_PageHdr pageHdr;
    pageHdr.nextFree = -1;
    pageHdr.recordCount = 0;
    pageHdr.freeSpaceOffset = sizeof(RM_PageHdr);
    pageHdr.freeSpace = PF_PAGE_SIZE - sizeof(RM_PageHdr);

    WritePageHeader(pData, pageHdr);
    pfFileHandle->MarkDirty(pageNum);

    // �����ļ�ͷ
    fileHdr.numPages++;
    fileHdr.firstFree = pageNum;
    headerChanged = true;

    return 0;
}

// -------------------------------------------------------------
// InsertIntoPage()
// -------------------------------------------------------------
// �Ľ��棺ɾ���󲻻Ḳ�Ǿɼ�¼
// -------------------------------------------------------------
RC RM_FileHandle::InsertIntoPage(PF_PageHandle& pageHandle, const char* recordBuf, int recordLen) {
    char* pData = nullptr;
    pageHandle.GetData(pData);
    if (!pData) return -1;

    PageNum pageNum;
    pageHandle.GetPageNum(pageNum);

    RM_PageHdr pageHdr;
    ReadPageHeader(pData, pageHdr);

    int needed = sizeof(RM_SlotEntry) + recordLen;
    if (pageHdr.freeSpace < needed)
        return -1; // �ռ䲻��

    // === ���㷨������ɨ���Ŀ¼���ҵ���ǰ��¼������ ===
    int recordMinOffset = PF_PAGE_SIZE;
    for (int i = 0; i < pageHdr.recordCount; i++) {
        RM_SlotEntry* s = (RM_SlotEntry*)(pData + sizeof(RM_PageHdr) + i * sizeof(RM_SlotEntry));
        if (s->flags & 0x01) {
            recordMinOffset = std::min(recordMinOffset, s->offset);
        }
    }

    if (recordMinOffset == PF_PAGE_SIZE)
        recordMinOffset = PF_PAGE_SIZE; // ҳ��Ϊ�գ�ֱ�Ӵ�ĩβд

    int recordOffset = recordMinOffset - recordLen;
    if (recordOffset < (int)(sizeof(RM_PageHdr) + pageHdr.recordCount * sizeof(RM_SlotEntry))) {
        // ��¼���������ͻ
        return -1;
    }

    // === д���¼���� ===
    memcpy(pData + recordOffset, recordBuf, recordLen);

    // === �����²� ===
    int slotIndex = pageHdr.recordCount;
    RM_SlotEntry* slot = (RM_SlotEntry*)(pData + sizeof(RM_PageHdr) + slotIndex * sizeof(RM_SlotEntry));
    slot->offset = recordOffset;
    slot->length = recordLen;
    slot->flags = 1;

    // === ����ҳͷ ===
    pageHdr.recordCount++;
    pageHdr.freeSpaceOffset += sizeof(RM_SlotEntry);
    pageHdr.freeSpace -= (sizeof(RM_SlotEntry) + recordLen);

    WritePageHeader(pData, pageHdr);
    pfFileHandle->MarkDirty(pageNum);

    return 0;
}

// -------------------------------------------------------------
// ҳͷ��д����
// -------------------------------------------------------------
void RM_FileHandle::WritePageHeader(char* pData, const RM_PageHdr& hdr) {
    memcpy(pData, &hdr, sizeof(RM_PageHdr));
}

void RM_FileHandle::ReadPageHeader(const char* pData, RM_PageHdr& hdr) {
    memcpy(&hdr, pData, sizeof(RM_PageHdr));
}

// -------------------------------------------------------------
// UpdateFileHeaderOnInsert()
// -------------------------------------------------------------
RC RM_FileHandle::UpdateFileHeaderOnInsert(PageNum pageNum, const RM_PageHdr& pageHdr) {
    if (pageHdr.freeSpace < sizeof(RM_SlotEntry) + relInfo.fixedRecordSize)
        fileHdr.firstFree = pageHdr.nextFree;

    headerChanged = true;
    return 0;
}


// -------------------------------------------------------------
// CompactFile()
// �Ա��ļ���ÿһҳ���н��գ�ȥ����ɾ����¼�������ؽ�����ҳ����
// -------------------------------------------------------------
RC RM_FileHandle::CompactFile() {
    if (!pfFileHandle) return -1;

    // ��С��Ҫ�ռ��ж�
    int minNeeded = sizeof(RM_SlotEntry) + relInfo.fixedRecordSize;

    int totalValidRecords = 0;

    PF_PageHandle pageHandle;
    if (pfFileHandle->GetFirstPage(pageHandle)!=0) {
        cout << "�ñ�Ϊ�ա�\n";
        return 0;
    }
    int filepagenum = 1;



    while(true){
        PageNum pageNum;
        pageHandle.GetPageNum(pageNum);
        // ����ÿ������ҳ��ҳ 0 Ϊ�ļ�ͷ��������
        if (pageNum==0) {
            PF_PageHandle nextPage;
            if (pfFileHandle->GetNextPage(pageNum, nextPage) != 0) break;
            pageHandle = nextPage;
        }
        // ��ȡ��ҳ��Ϣ
        char* pData = nullptr;
        pageHandle.GetData(pData);
        if (!pData) {
            pfFileHandle->UnpinPage(pageNum);
            continue;
        }

        // ��ȡ��ҳͷ
        RM_PageHdr oldHdr;
        ReadPageHeader(pData, oldHdr);

        // ��ʱҳ������
        std::vector<char> temp(PF_PAGE_SIZE);
        // ��ʼ����ʱҳͷ�����ڽ���ʱд�أ�
        RM_PageHdr tempHdr;
        tempHdr.nextFree = -1;
        tempHdr.recordCount = 0;
        tempHdr.freeSpaceOffset = sizeof(RM_PageHdr);
        tempHdr.freeSpace = PF_PAGE_SIZE - sizeof(RM_PageHdr);

        // ��¼��β����ҳβ��ǰд��
        int recordAreaEnd = PF_PAGE_SIZE;
        int keptSlots = 0;

        // ɨ��ԭҳ��ÿ���ۣ�������Ч��¼�� temp
        for (int i = 0; i < oldHdr.recordCount; ++i) {
            RM_SlotEntry* oldSlot = reinterpret_cast<RM_SlotEntry*>(
                pData + sizeof(RM_PageHdr) + i * sizeof(RM_SlotEntry));
            if (!(oldSlot->flags & 0x1)) {
                // ��ɾ��������
                continue;
            }

            int recLen = oldSlot->length;
            int recOffsetSrc = oldSlot->offset;
            char* recSrc = pData + recOffsetSrc;

            // ����Ŀ��λ�ã��� recordAreaEnd ��ǰ���䣩
            int recOffsetDst = recordAreaEnd - recLen;
            if (recOffsetDst < (int)(sizeof(RM_PageHdr) + (tempHdr.recordCount + 1) * sizeof(RM_SlotEntry))) {
                // ����������ռ䲻�㣨�����ϲ��ᣬ��Ϊ����ֻ��������ͬһҳ�ļ�¼��
                // ֹͣ����������ʣ��ļ�¼������ɾ����Щ��¼��
                break;
            }

            // ������¼����
            memcpy(&temp[recOffsetDst], recSrc, recLen);

            // д�� temp �еĲ���Ϣ�����ڲ�Ŀ¼֮��
            RM_SlotEntry* tempSlot = reinterpret_cast<RM_SlotEntry*>(
                temp.data() + sizeof(RM_PageHdr) + tempHdr.recordCount * sizeof(RM_SlotEntry));
            tempSlot->offset = recOffsetDst;
            tempSlot->length = recLen;
            tempSlot->flags = 1;

            // ������ʱҳͷ�Ĳ�����Ϣ��������ҳͷ�ֶν���ѭ��ĩβд�룩
            ++tempHdr.recordCount;
            recordAreaEnd = recOffsetDst;
            ++keptSlots;
        } // end for slots

        // ����д����ʱҳͷ�� freeSpaceOffset/freeSpace
        tempHdr.freeSpaceOffset = sizeof(RM_PageHdr) + tempHdr.recordCount * sizeof(RM_SlotEntry);
        tempHdr.freeSpace = recordAreaEnd - tempHdr.freeSpaceOffset;
        // nextFree ����Ϊ -1���Ժ�ͳһ�ؽ�����

        // дҳͷ�� temp ������
        memcpy(temp.data(), &tempHdr, sizeof(RM_PageHdr));

        // ���ǻ�ԭҳ����ҳ������
        memcpy(pData, temp.data(), PF_PAGE_SIZE);

        // �����ҳ���ͷ� pin
        pfFileHandle->MarkDirty(pageNum);
        pfFileHandle->UnpinPage(pageNum);

        totalValidRecords += tempHdr.recordCount;

        filepagenum++;
        PF_PageHandle nextPage;
        if (pfFileHandle->GetNextPage(pageNum,nextPage)!=0) break;
        pageHandle = nextPage;


    } // end for pages

    // �����ļ�ͷ�ļ�¼��
    fileHdr.recordCount = totalValidRecords;
    headerChanged = true;
    fileHdr.numPages = filepagenum;

    // ���¹�������ҳ����fileHdr.firstFree + �� pageHdr.nextFree��
    // �ռ��������� freeSpace >= minNeeded ��ҳ�ţ��� 1 .. numPages-1��
    std::vector<PageNum> freePages;
    for (PageNum pageNum = 1; pageNum < (PageNum)fileHdr.numPages; ++pageNum) {
        PF_PageHandle pageHandle;
        RC rc = pfFileHandle->GetThisPage(pageNum, pageHandle);
        if (rc != 0) continue;
        char* pData = nullptr;
        pageHandle.GetData(pData);
        if (!pData) {
            pfFileHandle->UnpinPage(pageNum);
            continue;
        }

        RM_PageHdr hdr;
        ReadPageHeader(pData, hdr);
        if (hdr.freeSpace >= minNeeded) {
            freePages.push_back(pageNum);
        }
        pfFileHandle->UnpinPage(pageNum);
    }

    // ��˳��� nextFree ��д����Щҳ��
    for (size_t idx = 0; idx < freePages.size(); ++idx) {
        PageNum pageNum = freePages[idx];
        PF_PageHandle ph;
        RC rc = pfFileHandle->GetThisPage(pageNum, ph);
        if (rc != 0) continue;
        char* pData = nullptr;
        ph.GetData(pData);
        RM_PageHdr hdr;
        ReadPageHeader(pData, hdr);
        if (idx + 1 < freePages.size()) hdr.nextFree = freePages[idx + 1];
        else hdr.nextFree = -1;
        WritePageHeader(pData, hdr);
        pfFileHandle->MarkDirty(pageNum);
        pfFileHandle->UnpinPage(pageNum);
    }

    // ���� fileHdr.firstFree
    if (!freePages.empty()) fileHdr.firstFree = freePages[0];
    else fileHdr.firstFree = -1;
    headerChanged = true;

    // ���д���ļ�ͷҳ��ҳ 0��
    FlushFileHeader();

    return 0;
}