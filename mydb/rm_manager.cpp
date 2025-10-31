#include "rm.h"
// -------------------------------------------------------------
// 构造与析构
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
        std::cerr << "[RM_Manager] CreateFile: PF 层创建文件失败 rc=" << rc << std::endl;
        return rc;
    }

    PF_FileHandle pfFH;
    rc = pfManager.OpenFile(fileName, pfFH);
    if (rc != 0) {
        std::cerr << "[RM_Manager] CreateFile: 打开新文件失败 rc=" << rc << std::endl;
        return rc;
    }


    PF_PageHandle pageHandle;
    pfFH.AllocatePage(pageHandle);

    char* pData = nullptr;

    pageHandle.GetData(pData);


    // 初始化 RM 文件头
    RM_FileHeader fh;
    fh.firstFree = -1;   // 无空闲页
    fh.numPages = 1;    // 仅有页0（文件头页）
    fh.recordCount = 0;

    memcpy(pData, &fh, sizeof(RM_FileHeader));

    pfFH.MarkDirty(0);
    pfFH.UnpinPage(0);
    pfFH.FlushPages();

    pfManager.CloseFile(pfFH);

    std::cout << "[RM_Manager] CreateFile: 成功创建文件 " << fileName << std::endl;
    return 0;
}

// -------------------------------------------------------------
// DestroyFile
// -------------------------------------------------------------
RC RM_Manager::DestroyFile(const char* fileName) {
    if (!fileName) return -1;
    RC rc = pfManager.DestroyFile(fileName);
    if (rc != 0)
        std::cerr << "[RM_Manager] DestroyFile: 删除文件失败 rc=" << rc << std::endl;
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
        std::cerr << "[RM_Manager] OpenFile: PF 层打开文件失败 rc=" << rc << std::endl;
        delete pfFH;
        return rc;
    }

    // 读取页0文件头
    PF_PageHandle pageHandle;
    rc = pfFH->GetThisPage(0, pageHandle);
    if (rc != 0) {
        std::cerr << "[RM_Manager] OpenFile: 读取页0失败 rc=" << rc << std::endl;
        pfManager.CloseFile(*pfFH);
        delete pfFH;
        return rc;
    }

    char* pData = nullptr;
    pageHandle.GetData(pData);
    if (!pData) {
        std::cerr << "[RM_Manager] OpenFile: 页数据为空" << std::endl;
        pfFH->UnpinPage(0);
        pfManager.CloseFile(*pfFH);
        delete pfFH;
        return -1;
    }

    RM_FileHeader fh;
    memcpy(&fh, pData, sizeof(RM_FileHeader));
    pfFH->UnpinPage(0);

    // 将 PF 文件句柄与 RM 文件句柄绑定
    fileHandle.SetPFFileHandle(pfFH);
    fileHandle.SetFileHeader(fh);

    // std::cout << "[RM_Manager] OpenFile: 成功打开文件 " << fileName << std::endl;
    return 0;
}

// -------------------------------------------------------------
// CloseFile（核心改进版）
// -------------------------------------------------------------
// 关闭逻辑：
// 若文件头已修改，则调用 FlushFileHeader() 写回页0
// 无论是否修改，都调用 ForcePages() 写回脏页
// 调用 PF_Manager::CloseFile() 关闭物理文件
// delete 内部 PF_FileHandle
// -------------------------------------------------------------
RC RM_Manager::CloseFile(RM_FileHandle& fileHandle) {
    RC rc = 0;

    PF_FileHandle* pfFH = fileHandle.GetPFFileHandle();
    if (pfFH == nullptr) {
        std::cerr << "[RM_Manager] CloseFile: PF_FileHandle 为空" << std::endl;
        return -1;
    }

    // 若文件头被修改，则写回
    if (fileHandle.IsHeaderChanged()) {
        rc = fileHandle.FlushFileHeader();
        if (rc != 0)
            std::cerr << "[RM_Manager] CloseFile: 文件头写回失败 rc=" << rc << std::endl;
    }

    // 刷新所有脏页
    rc = fileHandle.ForcePages();
    if (rc != 0)
        // std::cerr << "[RM_Manager] CloseFile: ForcePages 失败 rc=" << rc << std::endl;

    // 调用 PF 层关闭文件
    rc = pfManager.CloseFile(*pfFH);
    if (rc != 0)
        // std::cerr << "[RM_Manager] CloseFile: PF 层关闭失败 rc=" << rc << std::endl;

    // 清理指针
    delete pfFH;
    fileHandle.SetPFFileHandle(nullptr);

    // std::cout << "[RM_Manager] CloseFile: 文件关闭完成" << std::endl;
    return 0;
}
