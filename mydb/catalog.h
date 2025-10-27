#pragma once
#include "db.h"

// ���Ŀ¼�ṹ
struct RelCatEntry {
    char relName[MAXNAME + 1];   // ����
    int attrCount;               // ���Ը���
    int indexCount;              // ��������
    int recordCount;             // ��¼��
    int fixedRecordSize;         // ��¼�Ĺ̶����ִ�С
	int varAttrCount;            // �䳤��������
};

// ���Ե�Ŀ¼�ṹ
struct AttrCatEntry {
    char relName[MAXNAME + 1];   // ��������
    char attrName[MAXNAME + 1];  // ������
    AttrType attrType;           // �������� (INT, FLOAT, STRING)
    int attrSize;               // �������Դ�С���䳤��������ֽڳ���),����Ҫ
    int offset;                 // ���Զ���������Ч��-1��ʾ�䳤����
	int varIndex;              // �䳤���Ե���������i���䳤���ԣ�
    //int attrDisplayLength;      // �����ʾ��ȣ����岻��
    //bool isVariable;            // �Ƿ�Ϊ�䳤���ԣ�����offset�����Ƿ�䳤
    int attrSpecs;             // Լ����־���������ǿյȣ�
    int indexNo;               // �����ţ�-1��ʾ������
};




