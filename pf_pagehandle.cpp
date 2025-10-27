// pf_pagehandle.cpp
#include "pf.h"
#include <fstream>
#include <iostream>

#define INVALID_PAGE   (-1)

PF_PageHandle::PF_PageHandle()
{
    pageNum = INVALID_PAGE;
    pPageData = nullptr;
}

PF_PageHandle::~PF_PageHandle()
{
    // Nothing to clean up; the buffer manager handles memory
}


PF_PageHandle::PF_PageHandle(const PF_PageHandle& pageHandle)
{
    this->pageNum = pageHandle.pageNum;
    this->pPageData = pageHandle.pPageData;
}


PF_PageHandle& PF_PageHandle::operator=(const PF_PageHandle& pageHandle)
{
    if (this != &pageHandle) {
        this->pageNum = pageHandle.pageNum;
        this->pPageData = pageHandle.pPageData;
    }
    return *this;
}

//
// GetData
// Access the contents of a page
RC PF_PageHandle::GetData(char*& pData) const
{
    // Page must refer to a pinned page
    if (pPageData == nullptr)
        return PF_PAGEUNPINNED;

    // Return pointer to the page data (excluding PF_PageHdr)
    pData = pPageData;
    return 0;
}

//
// GetPageNum
// Access the page number
RC PF_PageHandle::GetPageNum(PageNum& _pageNum) const
{
    if (pPageData == nullptr)
        return PF_PAGEUNPINNED;

    _pageNum = this->pageNum;
    return 0;
}