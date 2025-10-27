#include <bits/stdc++.h>
#include "rm.h"
using namespace std;

// ģ�������������
int g_diskBlocks = 10000;

// ====================== �˵���ӡ ==========================
void PrintMenu() {
    cout << "\n========== MyDB ���ݿ�ϵͳ ==========\n";
    cout << "1. ����\n";
    cout << "2. �鿴�����ֵ�����\n";
    cout << "3. ѡ���\n";
    cout << "4. ��ѯ��¼\n";
    cout << "5. ��������\n";
    cout << "6. ɾ������\n";
    cout << "7. ��������\n";
    cout << "8. �رյ�ǰ��\n";
    cout << "9. ɾ����\n";
    cout << "10. ��������м�¼\n";
    cout << "11. ������������\n";
    cout << "12. ��ʾ�������Ϣ\n";
    cout << "13. help����ʾ�˵���\n";
    cout << "0. ���沢�˳�ϵͳ\n";
    cout << "=======================================\n";
}

// ====================== �����ֵ���� ======================
void SaveCatalog(const RelCatEntry& relEntry, const vector<AttrCatEntry>& attrs) {
    ofstream relFile("relcat.tbl", ios::binary | ios::app);
    relFile.write((char*)&relEntry, sizeof(RelCatEntry));
    relFile.close();

    ofstream attrFile("attrcat.tbl", ios::binary | ios::app);
    for (const auto& a : attrs)
        attrFile.write((char*)&a, sizeof(AttrCatEntry));
    attrFile.close();
}

void LoadCatalog(RelCatEntry& relEntry, vector<AttrCatEntry>& attrs, const string& tableName) {
    ifstream relFile("relcat.tbl", ios::binary);
    while (relFile.read((char*)&relEntry, sizeof(RelCatEntry))) {
        if (strcmp(relEntry.relName, tableName.c_str()) == 0)
            break;
    }
    relFile.close();

    ifstream attrFile("attrcat.tbl", ios::binary);
    AttrCatEntry attr;
    while (attrFile.read((char*)&attr, sizeof(AttrCatEntry))) {
        if (strcmp(attr.relName, tableName.c_str()) == 0)
            attrs.push_back(attr);
    }
    attrFile.close();
}

// ====================== ��ӡ�����ֵ����� ======================
void PrintCatalog() {
    ifstream relFile("relcat.tbl", ios::binary);
    ifstream attrFile("attrcat.tbl", ios::binary);
    if (!relFile.is_open() || !attrFile.is_open()) {
        cout << "�����ֵ��ļ������ڡ�\n";
        return;
    }

    vector<RelCatEntry> tables;
    RelCatEntry rel;
    while (relFile.read((char*)&rel, sizeof(RelCatEntry))) {
        tables.push_back(rel);
    }
    relFile.close();

    vector<AttrCatEntry> attrs;
    AttrCatEntry a;
    while (attrFile.read((char*)&a, sizeof(AttrCatEntry))) {
        attrs.push_back(a);
    }
    attrFile.close();

    cout << "\n========== �����ֵ����� ==========\n";
    for (auto& t : tables) {
        cout << "����: " << t.relName
            << " | ������: " << t.attrCount
            << " | ��¼��: " << t.recordCount
            << " | �̶����ִ�С: " << t.fixedRecordSize
            << " | �䳤������: " << t.varAttrCount << endl;

        for (auto& attr : attrs) {
            if (strcmp(attr.relName, t.relName) == 0) {
                cout << "  - " << attr.attrName << " ("
                    << (attr.attrType == AttrType::INT ? "INT" :
                        attr.attrType == AttrType::FLOAT ? "FLOAT" : "STRING")
                    << "), offset=" << attr.offset << endl;
            }
        }
        cout << "-----------------------------------\n";
    }
}

// ====================== ������ṹ ======================
void InputTableSchema(RelCatEntry& relEntry, vector<AttrCatEntry>& attrs) {
    cout << "���������: ";
    cin >> relEntry.relName;
    relEntry.indexCount = 0;
    relEntry.recordCount = 0;
    relEntry.fixedRecordSize = 0;
    relEntry.varAttrCount = 0;

    cout << "���������ԣ���ʽ: ���� ����[INT|FLOAT|STRING]�����н�����:\n";
    cin.ignore();
    string line;
    int count = 0;

    while (true) {
        cout << "����" << count + 1 << ": ";
        getline(cin, line);
        if (line.empty()) break;

        char name[64], type[32];
        if (sscanf(line.c_str(), "%s %s", name, type) != 2) continue;

        AttrCatEntry a{};
        strcpy(a.relName, relEntry.relName);
        strcpy(a.attrName, name);
        if (strcmp(type, "INT") == 0)
            a.attrType = AttrType::INT;
        else if (strcmp(type, "FLOAT") == 0)
            a.attrType = AttrType::FLOAT;
        else
            a.attrType = AttrType::STRING;

        if (a.attrType == AttrType::STRING) {
            a.attrSize = 0;
            a.offset = -1;
            a.varIndex = relEntry.varAttrCount++;
        }
        else if (a.attrType == AttrType::INT) {
            a.attrSize = sizeof(int);
            a.offset = relEntry.fixedRecordSize;
            relEntry.fixedRecordSize += sizeof(int);
        }
        else {
            a.attrSize = sizeof(float);
            a.offset = relEntry.fixedRecordSize;
            relEntry.fixedRecordSize += sizeof(float);
        }

        attrs.push_back(a);
        count++;
    }
    relEntry.attrCount = count;
}

// ====================== ������¼������ ======================
void BuildRecordBuffer(const vector<AttrCatEntry>& attrs, vector<char>& buffer) {
    buffer.clear();
    for (auto& a : attrs) {
        cout << "���������� " << a.attrName << ": ";
        if (a.attrType == AttrType::INT) {
            int v; cin >> v;
            buffer.insert(buffer.end(), (char*)&v, (char*)&v + sizeof(int));
        }
        else if (a.attrType == AttrType::FLOAT) {
            float f; cin >> f;
            buffer.insert(buffer.end(), (char*)&f, (char*)&f + sizeof(float));
        }
        else {
            string s; cin >> s;
            int len = s.size();
            buffer.insert(buffer.end(), (char*)&len, (char*)&len + sizeof(int));
            buffer.insert(buffer.end(), s.begin(), s.end());
        }
    }
}

// ====================== ��ӡ��¼���� ======================
void PrintRecord(const vector<AttrCatEntry>& attrs, const char* pData) {
    int offset = 0;
    for (auto& a : attrs) {
        cout << "  " << a.attrName << " = ";
        if (a.attrType == AttrType::INT) {
            cout << *(int*)(pData + offset);
            offset += sizeof(int);
        }
        else if (a.attrType == AttrType::FLOAT) {
            cout << *(float*)(pData + offset);
            offset += sizeof(float);
        }
        else {
            int len = *(int*)(pData + offset);
            offset += sizeof(int);
            string s(pData + offset, len);
            cout << s;
            offset += len;
        }
        cout << endl;
    }
}

// ====================== ������ ======================
int main() {
    cout << "==== MyDB ϵͳ��ʼ�� ====\n";
    int bufferSize;
    cout << "�����뻺��ؿ���: ";
    cin >> bufferSize;
    cout << "��������̿�������: ";
    cin >> g_diskBlocks;

    extern int PF_BUFFER_SIZE;
    PF_BUFFER_SIZE = bufferSize;

    PF_Manager g_pfMgr;
    RM_Manager g_rmMgr(g_pfMgr);
    RM_FileHandle g_openFile;
    bool g_hasOpenTable = false;
    string g_currentTableName;

    cout << "ϵͳ��ʼ����ɡ�\n";
    PrintMenu();

    int choice;
    while (true) {
        cout << "\n>>> ������������ (13 �鿴����): ";
        cin >> choice;

        if (choice == 0) {
            cout << "���沢�˳�ϵͳ...\n";
            if (g_hasOpenTable)
                g_rmMgr.CloseFile(g_openFile);
            break;
        }

        else if (choice == 1) {
            RelCatEntry rel{};
            vector<AttrCatEntry> attrs;
            InputTableSchema(rel, attrs);
            SaveCatalog(rel, attrs);
            string fileName = string(rel.relName) + ".tbl";
            g_rmMgr.CreateFile(fileName.c_str());
            cout << "�����ɹ�: " << rel.relName << endl;
        }

        else if (choice == 2) {
            PrintCatalog();
        }

        else if (choice == 3) {
            cout << "������Ҫѡ��ı���: ";
            cin >> g_currentTableName;
            string fileName = g_currentTableName + ".tbl";
            RM_FileHandle fh;
            if (g_rmMgr.OpenFile(fileName.c_str(), fh) == 0) {
                g_openFile = fh;
                g_hasOpenTable = true;
                cout << "���Ѵ�: " << g_currentTableName << endl;
            }
            else {
                cout << "�򿪱�ʧ�ܡ�\n";
            }
        }

        else if (choice == 4) {
            if (!g_hasOpenTable) { cout << "δѡ���\n"; continue; }
            int page, slot;
            cout << "������ RID (ҳ�� �ۺ�): ";
            cin >> page >> slot;
            RID rid(page, slot);

            RelCatEntry rel;
            vector<AttrCatEntry> attrs;
            LoadCatalog(rel, attrs, g_currentTableName);

            RM_Record rec;
            if (g_openFile.GetRec(rid, rec) != 0) {
                cout << "��ѯʧ�ܣ��ü�¼�����ڡ�\n";
                continue;
            }

            char* pData;
            rec.GetData(pData);
            cout << "\n��¼����:\n";
            PrintRecord(attrs, pData);
        }

        else if (choice == 5) {
            if (!g_hasOpenTable) { cout << "����ѡ���\n"; continue; }
            RelCatEntry rel;
            vector<AttrCatEntry> attrs;
            LoadCatalog(rel, attrs, g_currentTableName);
            vector<char> buf;
            BuildRecordBuffer(attrs, buf);
            g_openFile.InsertRec(buf.data(), buf.size());
            cout << "����ɹ�����RID ��ϵͳ�Զ����䣩\n";
        }

        else if (choice == 6) {
            if (!g_hasOpenTable) { cout << "δѡ���\n"; continue; }
            int page, slot;
            cout << "������ RID (ҳ�� �ۺ�): ";
            cin >> page >> slot;
            RID rid(page, slot);
            g_openFile.DeleteRec(rid);
            cout << "ɾ���ɹ���\n";
        }

        else if (choice == 7) {
            if (!g_hasOpenTable) { cout << "δѡ���\n"; continue; }
            int page, slot;
            cout << "������ RID (ҳ�� �ۺ�): ";
            cin >> page >> slot;
            RID rid(page, slot);
            RelCatEntry rel;
            vector<AttrCatEntry> attrs;
            LoadCatalog(rel, attrs, g_currentTableName);
            vector<char> buf;
            BuildRecordBuffer(attrs, buf);
            g_openFile.UpdateRec(rid, buf.data(), buf.size());
            cout << "���³ɹ���\n";
        }

        else if (choice == 8) {
            if (!g_hasOpenTable) {
                cout << "��ǰδ���κα�\n"; continue;
            }
            g_rmMgr.CloseFile(g_openFile);
            g_hasOpenTable = false;
            cout << "���ѹرա�\n";
        }

        else if (choice == 9) {
            cout << "������Ҫɾ���ı���: ";
            string t; cin >> t;
            string fileName = t + ".tbl";
            g_rmMgr.DestroyFile(fileName.c_str());
            cout << "�� " << t << " ��ɾ����\n";
        }

        else if (choice == 10) {
            if (!g_hasOpenTable) {
                cout << "����ѡ���\n";
                continue;
            }

            cout << "\n=========== ��������м�¼ ===========" << endl;
            RelCatEntry rel;
            vector<AttrCatEntry> attrs;
            LoadCatalog(rel, attrs, g_currentTableName);

            int pageCount = 0, recordCount = 0;
            PF_PageHandle pageHandle;
            if (g_openFile.GetPFFileHandle()->GetFirstPage(pageHandle) != 0) {
                cout << "�ñ�Ϊ�ա�\n";
                continue;
            }

            while (true) {
                char* pPageData;
                PageNum pageNum;
                pageHandle.GetData(pPageData);
                pageHandle.GetPageNum(pageNum);

                // ʹ���Ѷ���Ľṹ
                RM_PageHdr* pHdr = reinterpret_cast<RM_PageHdr*>(pPageData);
                RM_SlotEntry* slotDir = reinterpret_cast<RM_SlotEntry*>(pPageData + sizeof(RM_PageHdr));

                cout << "\n--- ҳ " << pageNum << " ---\n";
                for (int i = 0; i < pHdr->recordCount; i++) {
                    RM_SlotEntry& slot = slotDir[i];
                    if (slot.flags & 0x01) { // ��Ч��¼
                        RID rid(pageNum, i);
                        RM_Record rec;
                        if (g_openFile.GetRec(rid, rec) == 0) {
                            char* recData;
                            rec.GetData(recData);
                            cout << "RID(" << pageNum << "," << i << "): ";
                            PrintRecord(attrs, recData);
                            recordCount++;
                        }
                    }
                }

                PF_PageHandle nextPage;
                if (g_openFile.GetPFFileHandle()->GetNextPage(pageNum, nextPage) != 0) break;
                pageHandle = nextPage;
				pageCount++;
            }

            cout << "\n=== �����ɣ��� " << pageCount + 1
                << " ҳ��" << recordCount << " ����¼ ===\n";
        }

        else if (choice == 11) {
            if (!g_hasOpenTable) {
                cout << "����ѡ���\n";
                continue;
            }

            int count;
            cout << "��������������ļ�¼��: ";
            cin >> count;

            RelCatEntry rel;
            vector<AttrCatEntry> attrs;
            LoadCatalog(rel, attrs, g_currentTableName);

            srand((unsigned)time(nullptr));
            for (int i = 0; i < count; ++i) {
                vector<char> buf;
                for (auto& a : attrs) {
                    if (a.attrType == AttrType::INT) {
                        int v = rand() % 10000;
                        buf.insert(buf.end(), (char*)&v, (char*)&v + sizeof(int));
                    }
                    else if (a.attrType == AttrType::FLOAT) {
                        float f = (float)(rand() % 10000) / 100.0f;
                        buf.insert(buf.end(), (char*)&f, (char*)&f + sizeof(float));
                    }
                    else {
                        int len = 3 + rand() % 8;
                        string s;
                        for (int j = 0; j < len; ++j)
                            s.push_back('A' + rand() % 26);
                        buf.insert(buf.end(), (char*)&len, (char*)&len + sizeof(int));
                        buf.insert(buf.end(), s.begin(), s.end());
                    }
                }
                g_openFile.InsertRec(buf.data(), buf.size());
            }

            cout << "��������ɲ����� " << count << " ����¼��\n";
        }

        else if (choice == 12) {
            cout << "\n=== �����״̬ ===\n";
            g_pfMgr.PrintBuffer();
        }

        else if (choice == 13) {
            PrintMenu();
        }

        else {
            cout << "��Чѡ����������롣\n";
        }
    }

    cout << "ϵͳ���˳���\n";
    return 0;
}
