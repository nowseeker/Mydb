#include <bits/stdc++.h>
#include "rm.h"
using namespace std;

// 模拟磁盘容量上限
int g_diskBlocks = 10000;

// ====================== 菜单打印 ==========================
void PrintMenu() {
    cout << "\n========== MyDB 数据库系统 ==========\n";
    cout << "1. 建表\n";
    cout << "2. 查看数据字典内容\n";
    cout << "3. 选择表\n";
    cout << "4. 查询记录\n";
    cout << "5. 插入数据\n";
    cout << "6. 删除数据\n";
    cout << "7. 更新数据\n";
    cout << "8. 关闭当前表\n";
    cout << "9. 删除表\n";
    cout << "10. 输出表所有记录\n";
    cout << "11. 批量插入数据\n";
    cout << "12. 显示缓冲池信息\n";
    cout << "13. help（显示菜单）\n";
    cout << "0. 保存并退出系统\n";
    cout << "=======================================\n";
}

// ====================== 数据字典管理 ======================
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

// ====================== 打印数据字典内容 ======================
void PrintCatalog() {
    ifstream relFile("relcat.tbl", ios::binary);
    ifstream attrFile("attrcat.tbl", ios::binary);
    if (!relFile.is_open() || !attrFile.is_open()) {
        cout << "数据字典文件不存在。\n";
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

    cout << "\n========== 数据字典内容 ==========\n";
    for (auto& t : tables) {
        cout << "表名: " << t.relName
            << " | 属性数: " << t.attrCount
            << " | 记录数: " << t.recordCount
            << " | 固定部分大小: " << t.fixedRecordSize
            << " | 变长属性数: " << t.varAttrCount << endl;

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

// ====================== 构建表结构 ======================
void InputTableSchema(RelCatEntry& relEntry, vector<AttrCatEntry>& attrs) {
    cout << "请输入表名: ";
    cin >> relEntry.relName;
    relEntry.indexCount = 0;
    relEntry.recordCount = 0;
    relEntry.fixedRecordSize = 0;
    relEntry.varAttrCount = 0;

    cout << "请输入属性（格式: 名称 类型[INT|FLOAT|STRING]，空行结束）:\n";
    cin.ignore();
    string line;
    int count = 0;

    while (true) {
        cout << "属性" << count + 1 << ": ";
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

// ====================== 构建记录缓冲区 ======================
void BuildRecordBuffer(const vector<AttrCatEntry>& attrs, vector<char>& buffer) {
    buffer.clear();
    for (auto& a : attrs) {
        cout << "请输入属性 " << a.attrName << ": ";
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

// ====================== 打印记录内容 ======================
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

// ====================== 主程序 ======================
int main() {
    cout << "==== MyDB 系统初始化 ====\n";
    int bufferSize;
    cout << "请输入缓冲池块数: ";
    cin >> bufferSize;
    cout << "请输入磁盘块数上限: ";
    cin >> g_diskBlocks;

    extern int PF_BUFFER_SIZE;
    PF_BUFFER_SIZE = bufferSize;

    PF_Manager g_pfMgr;
    RM_Manager g_rmMgr(g_pfMgr);
    RM_FileHandle g_openFile;
    bool g_hasOpenTable = false;
    string g_currentTableName;

    cout << "系统初始化完成。\n";
    PrintMenu();

    int choice;
    while (true) {
        cout << "\n>>> 请输入操作编号 (13 查看帮助): ";
        cin >> choice;

        if (choice == 0) {
            cout << "保存并退出系统...\n";
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
            cout << "表创建成功: " << rel.relName << endl;
        }

        else if (choice == 2) {
            PrintCatalog();
        }

        else if (choice == 3) {
            cout << "请输入要选择的表名: ";
            cin >> g_currentTableName;
            string fileName = g_currentTableName + ".tbl";
            RM_FileHandle fh;
            if (g_rmMgr.OpenFile(fileName.c_str(), fh) == 0) {
                g_openFile = fh;
                g_hasOpenTable = true;
                cout << "表已打开: " << g_currentTableName << endl;
            }
            else {
                cout << "打开表失败。\n";
            }
        }

        else if (choice == 4) {
            if (!g_hasOpenTable) { cout << "未选择表。\n"; continue; }
            int page, slot;
            cout << "请输入 RID (页号 槽号): ";
            cin >> page >> slot;
            RID rid(page, slot);

            RelCatEntry rel;
            vector<AttrCatEntry> attrs;
            LoadCatalog(rel, attrs, g_currentTableName);

            RM_Record rec;
            if (g_openFile.GetRec(rid, rec) != 0) {
                cout << "查询失败，该记录不存在。\n";
                continue;
            }

            char* pData;
            rec.GetData(pData);
            cout << "\n记录内容:\n";
            PrintRecord(attrs, pData);
        }

        else if (choice == 5) {
            if (!g_hasOpenTable) { cout << "请先选择表！\n"; continue; }
            RelCatEntry rel;
            vector<AttrCatEntry> attrs;
            LoadCatalog(rel, attrs, g_currentTableName);
            vector<char> buf;
            BuildRecordBuffer(attrs, buf);
            g_openFile.InsertRec(buf.data(), buf.size());
            cout << "插入成功！（RID 由系统自动分配）\n";
        }

        else if (choice == 6) {
            if (!g_hasOpenTable) { cout << "未选择表。\n"; continue; }
            int page, slot;
            cout << "请输入 RID (页号 槽号): ";
            cin >> page >> slot;
            RID rid(page, slot);
            g_openFile.DeleteRec(rid);
            cout << "删除成功。\n";
        }

        else if (choice == 7) {
            if (!g_hasOpenTable) { cout << "未选择表。\n"; continue; }
            int page, slot;
            cout << "请输入 RID (页号 槽号): ";
            cin >> page >> slot;
            RID rid(page, slot);
            RelCatEntry rel;
            vector<AttrCatEntry> attrs;
            LoadCatalog(rel, attrs, g_currentTableName);
            vector<char> buf;
            BuildRecordBuffer(attrs, buf);
            g_openFile.UpdateRec(rid, buf.data(), buf.size());
            cout << "更新成功。\n";
        }

        else if (choice == 8) {
            if (!g_hasOpenTable) {
                cout << "当前未打开任何表。\n"; continue;
            }
            g_rmMgr.CloseFile(g_openFile);
            g_hasOpenTable = false;
            cout << "表已关闭。\n";
        }

        else if (choice == 9) {
            cout << "请输入要删除的表名: ";
            string t; cin >> t;
            string fileName = t + ".tbl";
            g_rmMgr.DestroyFile(fileName.c_str());
            cout << "表 " << t << " 已删除。\n";
        }

        else if (choice == 10) {
            if (!g_hasOpenTable) {
                cout << "请先选择表！\n";
                continue;
            }

            cout << "\n=========== 输出表所有记录 ===========" << endl;
            RelCatEntry rel;
            vector<AttrCatEntry> attrs;
            LoadCatalog(rel, attrs, g_currentTableName);

            int pageCount = 0, recordCount = 0;
            PF_PageHandle pageHandle;
            if (g_openFile.GetPFFileHandle()->GetFirstPage(pageHandle) != 0) {
                cout << "该表为空。\n";
                continue;
            }

            while (true) {
                char* pPageData;
                PageNum pageNum;
                pageHandle.GetData(pPageData);
                pageHandle.GetPageNum(pageNum);

                // 使用已定义的结构
                RM_PageHdr* pHdr = reinterpret_cast<RM_PageHdr*>(pPageData);
                RM_SlotEntry* slotDir = reinterpret_cast<RM_SlotEntry*>(pPageData + sizeof(RM_PageHdr));

                cout << "\n--- 页 " << pageNum << " ---\n";
                for (int i = 0; i < pHdr->recordCount; i++) {
                    RM_SlotEntry& slot = slotDir[i];
                    if (slot.flags & 0x01) { // 有效记录
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

            cout << "\n=== 输出完成，共 " << pageCount + 1
                << " 页，" << recordCount << " 条记录 ===\n";
        }

        else if (choice == 11) {
            if (!g_hasOpenTable) {
                cout << "请先选择表！\n";
                continue;
            }

            int count;
            cout << "请输入批量插入的记录数: ";
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

            cout << "已随机生成并插入 " << count << " 条记录！\n";
        }

        else if (choice == 12) {
            cout << "\n=== 缓冲池状态 ===\n";
            g_pfMgr.PrintBuffer();
        }

        else if (choice == 13) {
            PrintMenu();
        }

        else {
            cout << "无效选项，请重新输入。\n";
        }
    }

    cout << "系统已退出。\n";
    return 0;
}
