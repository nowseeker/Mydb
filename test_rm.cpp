#include <bits/stdc++.h>
#include "rm.h"
using namespace std;

// -----------------------------------------------------
// 工具函数：读取用户定义的表结构
// -----------------------------------------------------
void InputTableSchema(RelCatEntry& relEntry, vector<AttrCatEntry>& attrs) {
    cout << "请输入表名: ";
    cin >> relEntry.relName;
    relEntry.indexCount = 0;
    relEntry.recordCount = 0;
    relEntry.fixedRecordSize = 0;
    relEntry.varAttrCount = 0;

    cout << "请输入属性（每行一个，格式: 属性名 类型[INT|FLOAT|STRING]，空行结束）:\n";
    cin.ignore(); // 清理换行

    string line;
    int attrCount = 0;
    while (true) {
        cout << "属性" << attrCount + 1 << ": ";
        getline(cin, line);
        if (line.empty()) break;

        AttrCatEntry attr{};
        strcpy(attr.relName, relEntry.relName);

        char name[64], type[32];
        if (sscanf(line.c_str(), "%s %s", name, type) != 2) {
            cout << "输入格式错误，请重新输入。\n";
            continue;
        }

        strcpy(attr.attrName, name);
        if (strcmp(type, "INT") == 0)
            attr.attrType = AttrType::INT;
        else if (strcmp(type, "FLOAT") == 0)
            attr.attrType = AttrType::FLOAT;
        else
            attr.attrType = AttrType::STRING;

        // STRING 类型不指定大小，由输入数据确定
        attr.offset = relEntry.fixedRecordSize;
        if (attr.attrType == AttrType::STRING) {
            attr.attrSize = 0;  // 变长属性
            attr.offset = -1;
            attr.varIndex = relEntry.varAttrCount++;
        }
        else if (attr.attrType == AttrType::INT) {
            attr.attrSize = sizeof(int);
            relEntry.fixedRecordSize += sizeof(int);
        }
        else if (attr.attrType == AttrType::FLOAT) {
            attr.attrSize = sizeof(float);
            relEntry.fixedRecordSize += sizeof(float);
        }

        attr.attrSpecs = 0;
        attr.indexNo = -1;

        attrs.push_back(attr);
        attrCount++;
    }
    relEntry.attrCount = attrCount;
}

// -----------------------------------------------------
// 保存数据字典
// -----------------------------------------------------
void SaveCatalog(const RelCatEntry& relEntry, const vector<AttrCatEntry>& attrs) {
    ofstream relFile("relcat.tbl", ios::binary);
    relFile.write((char*)&relEntry, sizeof(RelCatEntry));
    relFile.close();

    ofstream attrFile("attrcat.tbl", ios::binary);
    for (const auto& a : attrs)
        attrFile.write((char*)&a, sizeof(AttrCatEntry));
    attrFile.close();

    cout << "数据字典已写入 relcat.tbl 和 attrcat.tbl\n";
}

// -----------------------------------------------------
// 加载数据字典
// -----------------------------------------------------
void LoadCatalog(RelCatEntry& relEntry, vector<AttrCatEntry>& attrs, const string& tableName) {
    ifstream relFile("relcat.tbl", ios::binary);
    if (relFile.is_open()) {
        while (relFile.read((char*)&relEntry, sizeof(RelCatEntry))) {
            if (strcmp(relEntry.relName, tableName.c_str()) == 0)
                break;
        }
        relFile.close();
    }

    ifstream attrFile("attrcat.tbl", ios::binary);
    AttrCatEntry attr;
    while (attrFile.read((char*)&attr, sizeof(AttrCatEntry))) {
        if (strcmp(attr.relName, tableName.c_str()) == 0)
            attrs.push_back(attr);
    }
    attrFile.close();
}

// -----------------------------------------------------
// 构建记录缓冲区
// -----------------------------------------------------
void BuildRecordBuffer(const vector<AttrCatEntry>& attrs, vector<char>& buffer) {
    buffer.clear();
    int offset = 0;
    for (auto& attr : attrs) {
        cout << "请输入属性 " << attr.attrName << ": ";
        if (attr.attrType == AttrType::INT) {
            int v;
            cin >> v;
            buffer.insert(buffer.end(), (char*)&v, (char*)&v + sizeof(int));
        }
        else if (attr.attrType == AttrType::FLOAT) {
            float f;
            cin >> f;
            buffer.insert(buffer.end(), (char*)&f, (char*)&f + sizeof(float));
        }
        else { // STRING
            string s;
            cin >> s;
            int len = s.size();
            buffer.insert(buffer.end(), (char*)&len, (char*)&len + sizeof(int)); // 记录长度前缀
            buffer.insert(buffer.end(), s.begin(), s.end());
        }
    }
    cin.ignore();
}

// -----------------------------------------------------
// 显示记录内容
// -----------------------------------------------------
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

void viewTblFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return;
    }

    char buffer[1024];
    file.read(buffer, sizeof(buffer));

    std::cout << "文件内容 (十六进制):" << std::endl;
    for (int i = 0; i < file.gcount(); i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
            << (int)(unsigned char)buffer[i] << " ";
        if ((i + 1) % 16 == 0) std::cout << std::endl;
    }
}

// -----------------------------------------------------
// 主函数：完整测试流程
// -----------------------------------------------------
int main() {
    PF_Manager pfMgr;
    RM_Manager rmMgr(pfMgr);

    cout << "=========== 数据字典创建 ===========" << endl;
    RelCatEntry relEntry{};
    vector<AttrCatEntry> attrs;
    InputTableSchema(relEntry, attrs);
    SaveCatalog(relEntry, attrs);

    cout << "\n=========== 读取数据字典 ===========" << endl;
    RelCatEntry readRel;
    vector<AttrCatEntry> readAttrs;
    LoadCatalog(readRel, readAttrs, relEntry.relName);

    cout << "表名: " << readRel.relName << "，属性数: " << readRel.attrCount << endl;
    for (auto& a : readAttrs)
        cout << "  - " << a.attrName << " (" << (a.attrType == AttrType::INT ? "INT" :
            a.attrType == AttrType::FLOAT ? "FLOAT" : "STRING") << ")" << endl;

    cout << "\n=========== 创建表文件 ===========" << endl;
    string fileName = string(readRel.relName) + ".tbl";
    rmMgr.CreateFile(fileName.c_str());

    RM_FileHandle fh;
    rmMgr.OpenFile(fileName.c_str(), fh);

    cout << "\n=========== 插入记录 ===========" << endl;
    vector<char> buf;
    BuildRecordBuffer(readAttrs, buf);
    fh.InsertRec(buf.data(), buf.size());
    cout << "插入成功。\n";

    cout << "\n=========== 插入记录2 ===========" << endl;
    vector<char> buf_2;
    BuildRecordBuffer(readAttrs, buf_2);
    fh.InsertRec(buf_2.data(), buf_2.size());
    cout << "插入成功2。\n";

    cout << "\n=========== 插入记录3 ===========" << endl;
    vector<char> buf_3;
    BuildRecordBuffer(readAttrs, buf_3);
    fh.InsertRec(buf_3.data(), buf_3.size());
    cout << "插入成功3。\n";

    // 获取页1槽0记录（演示用）
    RID rid(1, 0);
    RM_Record rec_1;
    fh.GetRec(rid, rec_1);
    char* pData;
    rec_1.GetData(pData);

    cout << "\n=========== 读取记录1 ===========" << endl;
    PrintRecord(readAttrs, pData);

    // 获取页1槽1记录（演示用）
    RID rid_2(1, 1);
    RM_Record rec_2;
    fh.GetRec(rid_2, rec_2);
    char* pData_2;
    rec_2.GetData(pData_2);

    cout << "\n=========== 读取记录2 ===========" << endl;
    PrintRecord(readAttrs, pData_2);

    // 获取页1槽2记录（演示用）
    RID rid_3(1, 2);
    RM_Record rec_3;
    fh.GetRec(rid_3, rec_3);
    char* pData_3;
    rec_3.GetData(pData_3);

    cout << "\n=========== 读取记录3 ===========" << endl;
    PrintRecord(readAttrs, pData_3);


    rmMgr.CloseFile(fh);
    cout << "\n测试完成。\n";
    return 0;
}

