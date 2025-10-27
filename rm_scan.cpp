#include "rm.h"
#include <cstring>
#include <iostream>
using namespace std;

// ==============================
// RM_Scan ʵ��
// ==============================

RM_Scan::RM_Scan()
    : bOpen(false), currPage(-1), currSlot(-1), compOp(NO_OP),
    value(nullptr), attrLength(0), attrOffset(0) {
}

RM_Scan::~RM_Scan() {
    if (bOpen) CloseScan();
}

// -----------------------------------------------------
// ����ɨ�裺������������λ����һҳ
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

    currPage = 1;   // �ӵ�1ҳ��ʼɨ�裨��0ҳ���ļ�ͷ��
    currSlot = -1;
    bOpen = true;
    return 0;
}

// -----------------------------------------------------
// ��ȡ��һ�����������ļ�¼
// -----------------------------------------------------
RC RM_Scan::GetNextRec(RM_Record& rec) {
    if (!bOpen) return -1;

    PF_PageHandle pageHandle;
    char* pageData = nullptr;
    RC rc;

    // ����ҳ��
    while (currPage < fileHandle->hdr.numPages) {
        rc = fileHandle->pfFileHandle.GetThisPage(currPage, pageHandle);
        if (rc) {
            currPage++;
            continue;
        }

        pageHandle.GetData(pageData);
        RM_PageHdr* pageHdr = (RM_PageHdr*)pageData;

        // ������ǰҳ��
        for (++currSlot; currSlot < pageHdr->recordCount; currSlot++) {
            RM_SlotEntry* slot = (RM_SlotEntry*)(pageData + sizeof(RM_PageHdr)
                + currSlot * sizeof(RM_SlotEntry));
            if (!(slot->flags & 1)) continue; // ��ɾ��

            // ��ȡ��¼����
            char* recordPtr = pageData + slot->offset;
            if (SatisfyCondition(recordPtr)) {
                RID rid(currPage, currSlot);
                rec.Set(recordPtr, slot->length, rid);
                fileHandle->pfFileHandle.UnpinPage(currPage);
                return 0;
            }
        }

        // ��ǰҳ������������һҳ
        fileHandle->pfFileHandle.UnpinPage(currPage);
        currPage++;
        currSlot = -1;
    }

    return RM_EOF;
}

// -----------------------------------------------------
// �жϼ�¼�Ƿ������ѯ����
// -----------------------------------------------------
bool RM_Scan::SatisfyCondition(const char* recordData) const {
    if (compOp == NO_OP) return true; // ������ɨ��

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
// ��ͬ���͵ıȽ��߼�
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
// �ر�ɨ��
// -----------------------------------------------------
RC RM_Scan::CloseScan() {
    if (!bOpen) return -1;
    bOpen = false;
    fileHandle = nullptr;
    currPage = -1;
    currSlot = -1;
    return 0;
}
