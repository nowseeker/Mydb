// 未使用
#include "rm.h"
#include <cstring>
#include <iostream>
using namespace std;

// ==============================
// RM_Scan 实现
// ==============================

RM_Scan::RM_Scan()
    : bOpen(false), currPage(-1), currSlot(-1), compOp(NO_OP),
    value(nullptr), attrLength(0), attrOffset(0) {
}

RM_Scan::~RM_Scan() {
    if (bOpen) CloseScan();
}

// -----------------------------------------------------
// 启动扫描：设置条件并定位到第一页
// -----------------------------------------------------
RC RM_Scan::OpenScan(RM_FileHandle& fileHandle,
    AttrType attrType, int attrLength,
    int attrOffset, CompOp compOp, const void* value) {
    if (bOpen) return -1;

    this->fileHandle = &fileHandle;
    this->attrType = attrType;
    this->attrLength = attrLength;
    this->attrOffset = attrOffset;
    this->compOp = compOp;
    this->value = value;

    currPage = 1;   // 从第1页开始扫描（第0页是文件头）
    currSlot = -1;
    bOpen = true;
    return 0;
}

// -----------------------------------------------------
// 获取下一条满足条件的记录
// -----------------------------------------------------
RC RM_Scan::GetNextRec(RM_Record& rec) {
    if (!bOpen) return -1;

    PF_PageHandle pageHandle;
    char* pageData = nullptr;
    RC rc;

    // 遍历页面
    while (currPage < fileHandle->hdr.numPages) {
        rc = fileHandle->pfFileHandle.GetThisPage(currPage, pageHandle);
        if (rc) {
            currPage++;
            continue;
        }

        pageHandle.GetData(pageData);
        RM_PageHdr* pageHdr = (RM_PageHdr*)pageData;

        // 遍历当前页槽
        for (++currSlot; currSlot < pageHdr->recordCount; currSlot++) {
            RM_SlotEntry* slot = (RM_SlotEntry*)(pageData + sizeof(RM_PageHdr)
                + currSlot * sizeof(RM_SlotEntry));
            if (!(slot->flags & 1)) continue; // 已删除

            // 获取记录内容
            char* recordPtr = pageData + slot->offset;
            if (SatisfyCondition(recordPtr)) {
                RID rid(currPage, currSlot);
                rec.Set(recordPtr, slot->length, rid);
                fileHandle->pfFileHandle.UnpinPage(currPage);
                return 0;
            }
        }

        // 当前页结束，跳到下一页
        fileHandle->pfFileHandle.UnpinPage(currPage);
        currPage++;
        currSlot = -1;
    }

    return RM_EOF;
}

// -----------------------------------------------------
// 判断记录是否满足查询条件
// -----------------------------------------------------
bool RM_Scan::SatisfyCondition(const char* recordData) const {
    if (compOp == NO_OP) return true; // 无条件扫描

    const char* attrData = recordData + attrOffset;

    switch (attrType) {
    case INT: {
        int lhs = *(int*)attrData;
        int rhs = *(int*)value;
        return Compare(lhs, rhs);
    }
    case FLOAT: {
        float lhs = *(float*)attrData;
        float rhs = *(float*)value;
        return Compare(lhs, rhs);
    }
    case STRING: {
        return CompareStr(attrData, (const char*)value);
    }
    default:
        return false;
    }
}

// -----------------------------------------------------
// 不同类型的比较逻辑
// -----------------------------------------------------
template<typename T>
bool RM_Scan::Compare(T lhs, T rhs) const {
    switch (compOp) {
    case EQ_OP: return lhs == rhs;
    case LT_OP: return lhs < rhs;
    case GT_OP: return lhs > rhs;
    case LE_OP: return lhs <= rhs;
    case GE_OP: return lhs >= rhs;
    case NE_OP: return lhs != rhs;
    default:    return false;
    }
}

bool RM_Scan::CompareStr(const char* lhs, const char* rhs) const {
    int cmp = strncmp(lhs, rhs, attrLength);
    switch (compOp) {
    case EQ_OP: return cmp == 0;
    case LT_OP: return cmp < 0;
    case GT_OP: return cmp > 0;
    case LE_OP: return cmp <= 0;
    case GE_OP: return cmp >= 0;
    case NE_OP: return cmp != 0;
    default:    return false;
    }
}

// -----------------------------------------------------
// 关闭扫描
// -----------------------------------------------------
RC RM_Scan::CloseScan() {
    if (!bOpen) return -1;
    bOpen = false;
    fileHandle = nullptr;
    currPage = -1;
    currSlot = -1;
    return 0;
}

