#include "rm.h"
// -------------------------------------------------------------
// ����������
// -------------------------------------------------------------
RM_Manager::RM_Manager(PF_Manager& pfm)
    : pfManager(pfm) {
}

RM_Manager::~RM_Manager() {}

// -------------------------------------------------------------
// CreateFile
// -------------------------------------------------------------
RC RM_Manager::CreateFile(const char* fileName) {
    if (!fileName) return -1;

    RC rc = pfManager.CreateFile(fileName);
    if (rc != 0) {
        std::cerr << "[RM_Manager] CreateFile: PF �㴴���ļ�ʧ�� rc=" << rc << std::endl;
        return rc;
    }

    PF_FileHandle pfFH;
    rc = pfManager.OpenFile(fileName, pfFH);
    if (rc != 0) {
        std::cerr << "[RM_Manager] CreateFile: �����ļ�ʧ�� rc=" << rc << std::endl;
        return rc;
    }


    PF_PageHandle pageHandle;
    pfFH.AllocatePage(pageHandle);

    char* pData = nullptr;

    pageHandle.GetData(pData);


    // ��ʼ�� RM �ļ�ͷ
    RM_FileHeader fh;
    fh.firstFree = -1;   // �޿���ҳ
    fh.numPages = 1;    // ����ҳ0���ļ�ͷҳ��
    fh.recordCount = 0;

    memcpy(pData, &fh, sizeof(RM_FileHeader));

    pfFH.MarkDirty(0);
    pfFH.UnpinPage(0);
    pfFH.FlushPages();

    pfManager.CloseFile(pfFH);

    std::cout << "[RM_Manager] CreateFile: �ɹ������ļ� " << fileName << std::endl;
    return 0;
}

// -------------------------------------------------------------
// DestroyFile
// -------------------------------------------------------------
RC RM_Manager::DestroyFile(const char* fileName) {
    if (!fileName) return -1;
    RC rc = pfManager.DestroyFile(fileName);
    if (rc != 0)
        std::cerr << "[RM_Manager] DestroyFile: ɾ���ļ�ʧ�� rc=" << rc << std::endl;
    return rc;
}

// -------------------------------------------------------------
// OpenFile
// -------------------------------------------------------------
RC RM_Manager::OpenFile(const char* fileName, RM_FileHandle& fileHandle) {
    if (!fileName) return -1;

    PF_FileHandle* pfFH = new PF_FileHandle();
    RC rc = pfManager.OpenFile(fileName, *pfFH);
    if (rc != 0) {
        std::cerr << "[RM_Manager] OpenFile: PF ����ļ�ʧ�� rc=" << rc << std::endl;
        delete pfFH;
        return rc;
    }

    // ��ȡҳ0�ļ�ͷ
    PF_PageHandle pageHandle;
    rc = pfFH->GetThisPage(0, pageHandle);
    if (rc != 0) {
        std::cerr << "[RM_Manager] OpenFile: ��ȡҳ0ʧ�� rc=" << rc << std::endl;
        pfManager.CloseFile(*pfFH);
        delete pfFH;
        return rc;
    }

    char* pData = nullptr;
    pageHandle.GetData(pData);
    if (!pData) {
        std::cerr << "[RM_Manager] OpenFile: ҳ����Ϊ��" << std::endl;
        pfFH->UnpinPage(0);
        pfManager.CloseFile(*pfFH);
        delete pfFH;
        return -1;
    }

    RM_FileHeader fh;
    memcpy(&fh, pData, sizeof(RM_FileHeader));
    pfFH->UnpinPage(0);

    // �� PF �ļ������ RM �ļ������
    fileHandle.SetPFFileHandle(pfFH);
    fileHandle.SetFileHeader(fh);

    // std::cout << "[RM_Manager] OpenFile: �ɹ����ļ� " << fileName << std::endl;
    return 0;
}

// -------------------------------------------------------------
// CloseFile�����ĸĽ��棩
// -------------------------------------------------------------
// �ر��߼���
// ���ļ�ͷ���޸ģ������ FlushFileHeader() д��ҳ0
// �����Ƿ��޸ģ������� ForcePages() д����ҳ
// ���� PF_Manager::CloseFile() �ر������ļ�
// delete �ڲ� PF_FileHandle
// -------------------------------------------------------------
RC RM_Manager::CloseFile(RM_FileHandle& fileHandle) {
    RC rc = 0;

    PF_FileHandle* pfFH = fileHandle.GetPFFileHandle();
    if (pfFH == nullptr) {
        std::cerr << "[RM_Manager] CloseFile: PF_FileHandle Ϊ��" << std::endl;
        return -1;
    }

    // ���ļ�ͷ���޸ģ���д��
    if (fileHandle.IsHeaderChanged()) {
        rc = fileHandle.FlushFileHeader();
        if (rc != 0)
            std::cerr << "[RM_Manager] CloseFile: �ļ�ͷд��ʧ�� rc=" << rc << std::endl;
    }

    // ˢ��������ҳ
    rc = fileHandle.ForcePages();
    if (rc != 0)
        // std::cerr << "[RM_Manager] CloseFile: ForcePages ʧ�� rc=" << rc << std::endl;

    // ���� PF ��ر��ļ�
    rc = pfManager.CloseFile(*pfFH);
    if (rc != 0)
        // std::cerr << "[RM_Manager] CloseFile: PF ��ر�ʧ�� rc=" << rc << std::endl;

    // ����ָ��
    delete pfFH;
    fileHandle.SetPFFileHandle(nullptr);

    // std::cout << "[RM_Manager] CloseFile: �ļ��ر����" << std::endl;
    return 0;
}
