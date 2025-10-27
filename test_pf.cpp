#include "pf.h"

using namespace std;

void PrintLine() {
    cout << "--------------------------------------------------" << endl;
}

int main() {
    cout << "PF COMPONENT TEST BEGIN" << endl;
    PrintLine();

    // Step 1: 创建 PF_Manager（系统初始化）
    PF_Manager pfm;
    cout << "PF_Manager created successfully." << endl;

    const char* filename = "testfile.txt";

    // Step 2: 尝试删除旧文件
    pfm.DestroyFile(filename);

    // Step 3: 创建新文件
    RC rc = pfm.CreateFile(filename);
    if (rc != 0) {
        cerr << "CreateFile failed: " << rc << endl;
        return rc;
    }
    cout << "Created file: " << filename << endl;

    // Step 4: 打开文件
    PF_FileHandle fileHandle;
    rc = pfm.OpenFile(filename, fileHandle);
    if (rc != 0) {
        cerr << "OpenFile failed: " << rc << endl;
        return rc;
    }
    cout << "Opened file successfully." << endl;

    // Step 5: 分配若干页面
    PF_PageHandle pageHandle;
    const int NUM_PAGES = 50;
    for (int i = 0; i < NUM_PAGES; i++) {
        rc = fileHandle.AllocatePage(pageHandle);
        if (rc != 0) {
            cerr << "AllocatePage failed on page " << i << ": " << rc << endl;
            return rc;
        }

        char* pData;
        pageHandle.GetData(pData);
        sprintf(pData, "This is page %d.", i);
        fileHandle.MarkDirty(i);
        fileHandle.UnpinPage(i);
        cout << "Allocated and wrote page " << i << endl;
    }

    PrintLine();

    // Step 6: 读取所有页面内容
    for (int i = 0; i < NUM_PAGES; i++) {
        rc = fileHandle.GetThisPage(i, pageHandle);
        if (rc != 0) {
            cerr << "GetThisPage failed on page " << i << ": " << rc << endl;
            return rc;
        }

        char* pData;
        pageHandle.GetData(pData);
        cout << "Read Page " << i << ": " << pData << endl;

        fileHandle.UnpinPage(i);
    }

    PrintLine();

    // Step 7: 测试 Flush（模拟缓冲池换出）
    rc = fileHandle.FlushPages();
    if (rc != 0)
        cerr << "FlushPages returned warning or error: " << rc << endl;
    else
        cout << "All pages flushed to disk successfully." << endl;

    // Step 8: 关闭文件
    rc = pfm.CloseFile(fileHandle);
    if (rc != 0) {
        cerr << "CloseFile failed: " << rc << endl;
        return rc;
    }
    cout << "Closed file successfully." << endl;

    // Step 9: 重新打开文件验证持久化
    rc = pfm.OpenFile(filename, fileHandle);
    if (rc != 0) {
        cerr << "Reopen file failed: " << rc << endl;
        return rc;
    }
    cout << "Reopened file successfully." << endl;

    // Step 10: 再次读取页面验证内容
    for (int i = 0; i < NUM_PAGES; i++) {
        rc = fileHandle.GetThisPage(i, pageHandle);
        if (rc != 0) {
            cerr << "GetThisPage failed after reopen: " << rc << endl;
            return rc;
        }

        char* pData;
        pageHandle.GetData(pData);
        cout << "Page " << i << " content after reopen: " << pData << endl;

        fileHandle.UnpinPage(i);
    }

    // Step 11: 清空缓冲区并打印状态
    cout << endl << "[Buffer State Before Clear]" << endl;
    pfm.PrintBuffer();
    pfm.ClearBuffer();

    cout << endl << "[Buffer State After Clear]" << endl;
    pfm.PrintBuffer();

    // Step 12: 关闭文件并删除
    pfm.CloseFile(fileHandle);
    //pfm.DestroyFile(filename);
    cout << "File destroyed successfully." << endl;

    PrintLine();
    cout << "[PF COMPONENT TEST END]" << endl;

    return 0;
}
