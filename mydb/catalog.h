#pragma once
#include "db.h"

// 表的目录结构
struct RelCatEntry {
    char relName[MAXNAME + 1];   // 表名
    int attrCount;               // 属性个数
    int indexCount;              // 索引数量
    int recordCount;             // 记录数
    int fixedRecordSize;         // 记录的固定部分大小
	int varAttrCount;            // 变长属性数量
};

// 属性的目录结构
struct AttrCatEntry {
    char relName[MAXNAME + 1];   // 所属表名
    char attrName[MAXNAME + 1];  // 属性名
    AttrType attrType;           // 属性类型 (INT, FLOAT, STRING)
    int attrSize;               // 定长属性大小（变长属性最大字节长度),不需要
    int offset;                 // 仅对定长属性有效，-1表示变长属性
	int varIndex;              // 变长属性的索引（第i个变长属性）
    //int attrDisplayLength;      // 输出显示宽度，意义不明
    //bool isVariable;            // 是否为变长属性，可由offset设置是否变长
    int attrSpecs;             // 约束标志（主键、非空等）
    int indexNo;               // 索引号，-1表示无索引
};




