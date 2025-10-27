#include <bits/stdc++.h>
#include "rm.h"
using namespace std;

// -----------------------------------------------------
// ���ߺ�������ȡ�û�����ı�ṹ
// -----------------------------------------------------
void InputTableSchema(RelCatEntry& relEntry, vector<AttrCatEntry>& attrs) {
    cout << "���������: ";
    cin >> relEntry.relName;
    relEntry.indexCount = 0;
    relEntry.recordCount = 0;
    relEntry.fixedRecordSize = 0;
    relEntry.varAttrCount = 0;

    cout << "���������ԣ�ÿ��һ������ʽ: ������ ����[INT|FLOAT|STRING]�����н�����:\n";
    cin.ignore(); // ������

    string line;
    int attrCount = 0;
    while (true) {
        cout << "����" << attrCount + 1 << ": ";
        getline(cin, line);
        if (line.empty()) break;

        AttrCatEntry attr{};
        strcpy(attr.relName, relEntry.relName);

        char name[64], type[32];
        if (sscanf(line.c_str(), "%s %s", name, type) != 2) {
            cout << "�����ʽ�������������롣\n";
            continue;
        }

        strcpy(attr.attrName, name);
        if (strcmp(type, "INT") == 0)
            attr.attrType = AttrType::INT;
        else if (strcmp(type, "FLOAT") == 0)
            attr.attrType = AttrType::FLOAT;
        else
            attr.attrType = AttrType::STRING;

        // STRING ���Ͳ�ָ����С������������ȷ��
        attr.offset = relEntry.fixedRecordSize;
        if (attr.attrType == AttrType::STRING) {
            attr.attrSize = 0;  // �䳤����
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
// ���������ֵ�
// -----------------------------------------------------
void SaveCatalog(const RelCatEntry& relEntry, const vector<AttrCatEntry>& attrs) {
    ofstream relFile("relcat.tbl", ios::binary);
    relFile.write((char*)&relEntry, sizeof(RelCatEntry));
    relFile.close();

    ofstream attrFile("attrcat.tbl", ios::binary);
    for (const auto& a : attrs)
        attrFile.write((char*)&a, sizeof(AttrCatEntry));
    attrFile.close();

    cout << "�����ֵ���д�� relcat.tbl �� attrcat.tbl\n";
}

// -----------------------------------------------------
// ���������ֵ�
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
// ������¼������
// -----------------------------------------------------
void BuildRecordBuffer(const vector<AttrCatEntry>& attrs, vector<char>& buffer) {
    buffer.clear();
    int offset = 0;
    for (auto& attr : attrs) {
        cout << "���������� " << attr.attrName << ": ";
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
            buffer.insert(buffer.end(), (char*)&len, (char*)&len + sizeof(int)); // ��¼����ǰ׺
            buffer.insert(buffer.end(), s.begin(), s.end());
        }
    }
    cin.ignore();
}

// -----------------------------------------------------
// ��ʾ��¼����
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
        std::cerr << "�޷����ļ�: " << filename << std::endl;
        return;
    }

    char buffer[1024];
    file.read(buffer, sizeof(buffer));

    std::cout << "�ļ����� (ʮ������):" << std::endl;
    for (int i = 0; i < file.gcount(); i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
            << (int)(unsigned char)buffer[i] << " ";
        if ((i + 1) % 16 == 0) std::cout << std::endl;
    }
}

// -----------------------------------------------------
// ��������������������
// -----------------------------------------------------
int main() {
    PF_Manager pfMgr;
    RM_Manager rmMgr(pfMgr);

    cout << "=========== �����ֵ䴴�� ===========" << endl;
    RelCatEntry relEntry{};
    vector<AttrCatEntry> attrs;
    InputTableSchema(relEntry, attrs);
    SaveCatalog(relEntry, attrs);

    cout << "\n=========== ��ȡ�����ֵ� ===========" << endl;
    RelCatEntry readRel;
    vector<AttrCatEntry> readAttrs;
    LoadCatalog(readRel, readAttrs, relEntry.relName);

    cout << "����: " << readRel.relName << "��������: " << readRel.attrCount << endl;
    for (auto& a : readAttrs)
        cout << "  - " << a.attrName << " (" << (a.attrType == AttrType::INT ? "INT" :
            a.attrType == AttrType::FLOAT ? "FLOAT" : "STRING") << ")" << endl;

    cout << "\n=========== �������ļ� ===========" << endl;
    string fileName = string(readRel.relName) + ".tbl";
    rmMgr.CreateFile(fileName.c_str());

    RM_FileHandle fh;
    rmMgr.OpenFile(fileName.c_str(), fh);

    cout << "\n=========== �����¼ ===========" << endl;
    vector<char> buf;
    BuildRecordBuffer(readAttrs, buf);
    fh.InsertRec(buf.data(), buf.size());
    cout << "����ɹ���\n";

    cout << "\n=========== �����¼2 ===========" << endl;
    vector<char> buf_2;
    BuildRecordBuffer(readAttrs, buf_2);
    fh.InsertRec(buf_2.data(), buf_2.size());
    cout << "����ɹ�2��\n";

    cout << "\n=========== �����¼3 ===========" << endl;
    vector<char> buf_3;
    BuildRecordBuffer(readAttrs, buf_3);
    fh.InsertRec(buf_3.data(), buf_3.size());
    cout << "����ɹ�3��\n";

    // ��ȡҳ1��0��¼����ʾ�ã�
    RID rid(1, 0);
    RM_Record rec_1;
    fh.GetRec(rid, rec_1);
    char* pData;
    rec_1.GetData(pData);

    cout << "\n=========== ��ȡ��¼1 ===========" << endl;
    PrintRecord(readAttrs, pData);

    // ��ȡҳ1��1��¼����ʾ�ã�
    RID rid_2(1, 1);
    RM_Record rec_2;
    fh.GetRec(rid_2, rec_2);
    char* pData_2;
    rec_2.GetData(pData_2);

    cout << "\n=========== ��ȡ��¼2 ===========" << endl;
    PrintRecord(readAttrs, pData_2);

    // ��ȡҳ1��2��¼����ʾ�ã�
    RID rid_3(1, 2);
    RM_Record rec_3;
    fh.GetRec(rid_3, rec_3);
    char* pData_3;
    rec_3.GetData(pData_3);

    cout << "\n=========== ��ȡ��¼3 ===========" << endl;
    PrintRecord(readAttrs, pData_3);


    rmMgr.CloseFile(fh);
    cout << "\n������ɡ�\n";
    return 0;
}

