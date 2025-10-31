#include "rm.h"
using namespace std;

// -------------------------------------------------------------
// 构造 / 析构
// -------------------------------------------------------------
RM_FileHandle::RM_FileHandle()
    : pfFileHandle(nullptr), headerChanged(false) {
    memset(&fileHdr, 0, sizeof(RM_FileHeader));
    memset(&relInfo, 0, sizeof(RelCatEntry));
}

RM_FileHandle::~RM_FileHandle() {}

// -------------------------------------------------------------
// Set / Get 接口
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
// 找到可插入的空闲页（FindFreePage）
// 若无空闲页则分配新页（InitNewPage）
// 插入记录（InsertIntoPage）
// 更新文件头信息
// -------------------------------------------------------------
RC RM_FileHandle::InsertRec(const char* recordBuf, int recordLen) {
    if (!pfFileHandle || !recordBuf || recordLen <= 0)
        return -1;

    RC rc;
    PF_PageHandle pageHandle;
    PageNum pageNum;

    // === 1. 找空闲页 ===
    rc = FindFreePage(pageNum, pageHandle);
    if (rc != 0) {
        // 若无空闲页，则分配新页
        rc = pfFileHandle->AllocatePage(pageHandle);
        if (rc != 0) return rc;

        pageHandle.GetPageNum(pageNum);

        rc = InitNewPage(pageHandle, pageNum);
        if (rc != 0) {
            pfFileHandle->UnpinPage(pageNum);// newadd
            return rc;
        }
    }

    // === 2. 插入记录 ===
    rc = InsertIntoPage(pageHandle, recordBuf, recordLen);
    if (rc != 0) {
        // 当前页放不下，尝试新页
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

    // === 3. 成功插入后更新文件头 ===
    fileHdr.recordCount++;
    headerChanged = true;

    // 写回页
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
        return -1; // 已删除
    }

    // === 逻辑删除 ===
    slot->flags &= ~0x1;

    // 不回收槽或记录区，仅标记删除
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
    // 若 firstFree 无效或文件中无页，则返回错误
    if (fileHdr.firstFree < 0 || fileHdr.numPages == 0)
        return -1;

    RC rc = pfFileHandle->GetThisPage(fileHdr.firstFree, pageHandle);
    if (rc != 0) return rc;

    pageNum = fileHdr.firstFree;

    // 检查该页是否仍有空间
    char* pData;
    pageHandle.GetData(pData);
    RM_PageHdr hdr;
    ReadPageHeader(pData, hdr);

    int minNeeded = sizeof(RM_SlotEntry) + relInfo.fixedRecordSize;
    if (hdr.freeSpace < minNeeded) {
        // 页已满，从链表移除
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
        pfFileHandle->UnpinPage(pageNum);   // 补：无法访问数据释放页
        return -1;
    }

    RM_PageHdr pageHdr;
    pageHdr.nextFree = -1;
    pageHdr.recordCount = 0;
    pageHdr.freeSpaceOffset = sizeof(RM_PageHdr);
    pageHdr.freeSpace = PF_PAGE_SIZE - sizeof(RM_PageHdr);

    WritePageHeader(pData, pageHdr);
    pfFileHandle->MarkDirty(pageNum);

    // 更新文件头
    fileHdr.numPages++;
    fileHdr.firstFree = pageNum;
    headerChanged = true;

    return 0;
}

// -------------------------------------------------------------
// InsertIntoPage()
// -------------------------------------------------------------
// 改进版：删除后不会覆盖旧记录
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
        return -1; // 空间不足

    // === 新算法：重新扫描槽目录，找到当前记录区顶部 ===
    int recordMinOffset = PF_PAGE_SIZE;
    for (int i = 0; i < pageHdr.recordCount; i++) {
        RM_SlotEntry* s = (RM_SlotEntry*)(pData + sizeof(RM_PageHdr) + i * sizeof(RM_SlotEntry));
        if (s->flags & 0x01) {
            recordMinOffset = std::min(recordMinOffset, s->offset);
        }
    }

    if (recordMinOffset == PF_PAGE_SIZE)
        recordMinOffset = PF_PAGE_SIZE; // 页面为空，直接从末尾写

    int recordOffset = recordMinOffset - recordLen;
    if (recordOffset < (int)(sizeof(RM_PageHdr) + pageHdr.recordCount * sizeof(RM_SlotEntry))) {
        // 记录区与槽区冲突
        return -1;
    }

    // === 写入记录数据 ===
    memcpy(pData + recordOffset, recordBuf, recordLen);

    // === 分配新槽 ===
    int slotIndex = pageHdr.recordCount;
    RM_SlotEntry* slot = (RM_SlotEntry*)(pData + sizeof(RM_PageHdr) + slotIndex * sizeof(RM_SlotEntry));
    slot->offset = recordOffset;
    slot->length = recordLen;
    slot->flags = 1;

    // === 更新页头 ===
    pageHdr.recordCount++;
    pageHdr.freeSpaceOffset += sizeof(RM_SlotEntry);
    pageHdr.freeSpace -= (sizeof(RM_SlotEntry) + recordLen);

    WritePageHeader(pData, pageHdr);
    pfFileHandle->MarkDirty(pageNum);

    return 0;
}

// -------------------------------------------------------------
// 页头读写辅助
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
// 对表文件的每一页进行紧凑（去除已删除记录），并重建空闲页链。
// -------------------------------------------------------------
RC RM_FileHandle::CompactFile() {
    if (!pfFileHandle) return -1;

    // 最小需要空间判定
    int minNeeded = sizeof(RM_SlotEntry) + relInfo.fixedRecordSize;

    int totalValidRecords = 0;

    PF_PageHandle pageHandle;
    if (pfFileHandle->GetFirstPage(pageHandle)!=0) {
        cout << "该表为空。\n";
        return 0;
    }
    int filepagenum = 1;



    while(true){
        PageNum pageNum;
        pageHandle.GetPageNum(pageNum);
        // 遍历每个数据页（页 0 为文件头，跳过）
        if (pageNum==0) {
            PF_PageHandle nextPage;
            if (pfFileHandle->GetNextPage(pageNum, nextPage) != 0) break;
            pageHandle = nextPage;
        }
        // 获取旧页信息
        char* pData = nullptr;
        pageHandle.GetData(pData);
        if (!pData) {
            pfFileHandle->UnpinPage(pageNum);
            continue;
        }

        // 读取旧页头
        RM_PageHdr oldHdr;
        ReadPageHeader(pData, oldHdr);

        // 临时页缓冲区
        std::vector<char> temp(PF_PAGE_SIZE);
        // 初始化临时页头（会在结束时写回）
        RM_PageHdr tempHdr;
        tempHdr.nextFree = -1;
        tempHdr.recordCount = 0;
        tempHdr.freeSpaceOffset = sizeof(RM_PageHdr);
        tempHdr.freeSpace = PF_PAGE_SIZE - sizeof(RM_PageHdr);

        // 记录区尾（从页尾向前写）
        int recordAreaEnd = PF_PAGE_SIZE;
        int keptSlots = 0;

        // 扫描原页的每个槽，拷贝有效记录到 temp
        for (int i = 0; i < oldHdr.recordCount; ++i) {
            RM_SlotEntry* oldSlot = reinterpret_cast<RM_SlotEntry*>(
                pData + sizeof(RM_PageHdr) + i * sizeof(RM_SlotEntry));
            if (!(oldSlot->flags & 0x1)) {
                // 已删除，跳过
                continue;
            }

            int recLen = oldSlot->length;
            int recOffsetSrc = oldSlot->offset;
            char* recSrc = pData + recOffsetSrc;

            // 计算目标位置（从 recordAreaEnd 向前分配）
            int recOffsetDst = recordAreaEnd - recLen;
            if (recOffsetDst < (int)(sizeof(RM_PageHdr) + (tempHdr.recordCount + 1) * sizeof(RM_SlotEntry))) {
                // 防御：如果空间不足（理论上不会，因为我们只拷贝来自同一页的记录）
                // 停止拷贝，保持剩余的记录（不会删除这些记录）
                break;
            }

            // 拷贝记录数据
            memcpy(&temp[recOffsetDst], recSrc, recLen);

            // 写入 temp 中的槽信息（放在槽目录之后）
            RM_SlotEntry* tempSlot = reinterpret_cast<RM_SlotEntry*>(
                temp.data() + sizeof(RM_PageHdr) + tempHdr.recordCount * sizeof(RM_SlotEntry));
            tempSlot->offset = recOffsetDst;
            tempSlot->length = recLen;
            tempSlot->flags = 1;

            // 更新临时页头的插入信息（但最终页头字段将在循环末尾写入）
            ++tempHdr.recordCount;
            recordAreaEnd = recOffsetDst;
            ++keptSlots;
        } // end for slots

        // 现在写入临时页头的 freeSpaceOffset/freeSpace
        tempHdr.freeSpaceOffset = sizeof(RM_PageHdr) + tempHdr.recordCount * sizeof(RM_SlotEntry);
        tempHdr.freeSpace = recordAreaEnd - tempHdr.freeSpaceOffset;
        // nextFree 保持为 -1，稍后统一重建链表

        // 写页头到 temp 缓冲区
        memcpy(temp.data(), &tempHdr, sizeof(RM_PageHdr));

        // 覆盖回原页（整页拷贝）
        memcpy(pData, temp.data(), PF_PAGE_SIZE);

        // 标记脏页并释放 pin
        pfFileHandle->MarkDirty(pageNum);
        pfFileHandle->UnpinPage(pageNum);

        totalValidRecords += tempHdr.recordCount;

        filepagenum++;
        PF_PageHandle nextPage;
        if (pfFileHandle->GetNextPage(pageNum,nextPage)!=0) break;
        pageHandle = nextPage;


    } // end for pages

    // 更新文件头的记录数
    fileHdr.recordCount = totalValidRecords;
    headerChanged = true;
    fileHdr.numPages = filepagenum;

    // 重新构建空闲页链（fileHdr.firstFree + 各 pageHdr.nextFree）
    // 收集所有满足 freeSpace >= minNeeded 的页号（从 1 .. numPages-1）
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

    // 按顺序把 nextFree 链写回这些页面
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

    // 设置 fileHdr.firstFree
    if (!freePages.empty()) fileHdr.firstFree = freePages[0];
    else fileHdr.firstFree = -1;
    headerChanged = true;

    // 最后写回文件头页（页 0）
    FlushFileHeader();

    return 0;
}