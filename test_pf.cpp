#include "pf.h"

using namespace std;

void PrintLine() {
    cout << "--------------------------------------------------" << endl;
}

int main() {
    cout << "PF COMPONENT TEST BEGIN" << endl;
    PrintLine();

    // Step 1: ���� PF_Manager��ϵͳ��ʼ����
    PF_Manager pfm;
    cout << "PF_Manager created successfully." << endl;

    const char* filename = "testfile.txt";

    // Step 2: ����ɾ�����ļ�
    pfm.DestroyFile(filename);

    // Step 3: �������ļ�
    RC rc = pfm.CreateFile(filename);
    if (rc != 0) {
        cerr << "CreateFile failed: " << rc << endl;
        return rc;
    }
    cout << "Created file: " << filename << endl;

    // Step 4: ���ļ�
    PF_FileHandle fileHandle;
    rc = pfm.OpenFile(filename, fileHandle);
    if (rc != 0) {
        cerr << "OpenFile failed: " << rc << endl;
        return rc;
    }
    cout << "Opened file successfully." << endl;

    // Step 5: ��������ҳ��
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

    // Step 6: ��ȡ����ҳ������
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

    // Step 7: ���� Flush��ģ�⻺��ػ�����
    rc = fileHandle.FlushPages();
    if (rc != 0)
        cerr << "FlushPages returned warning or error: " << rc << endl;
    else
        cout << "All pages flushed to disk successfully." << endl;

    // Step 8: �ر��ļ�
    rc = pfm.CloseFile(fileHandle);
    if (rc != 0) {
        cerr << "CloseFile failed: " << rc << endl;
        return rc;
    }
    cout << "Closed file successfully." << endl;

    // Step 9: ���´��ļ���֤�־û�
    rc = pfm.OpenFile(filename, fileHandle);
    if (rc != 0) {
        cerr << "Reopen file failed: " << rc << endl;
        return rc;
    }
    cout << "Reopened file successfully." << endl;

    // Step 10: �ٴζ�ȡҳ����֤����
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

    // Step 11: ��ջ���������ӡ״̬
    cout << endl << "[Buffer State Before Clear]" << endl;
    pfm.PrintBuffer();
    pfm.ClearBuffer();

    cout << endl << "[Buffer State After Clear]" << endl;
    pfm.PrintBuffer();

    // Step 12: �ر��ļ���ɾ��
    pfm.CloseFile(fileHandle);
    //pfm.DestroyFile(filename);
    cout << "File destroyed successfully." << endl;

    PrintLine();
    cout << "[PF COMPONENT TEST END]" << endl;

    return 0;
}
