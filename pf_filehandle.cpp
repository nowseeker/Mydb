#include <iostream>
#include <fstream>
#include <unordered_map>
#include <cstring>
#include "pf.h"

extern int PF_BUFFER_SIZE;
//
// PF_FileHandle
//
PF_FileHandle::PF_FileHandle()
{
    bFileOpen = FALSE;
    pBufferMgr = NULL;
}

//
// ~PF_FileHandle
//
PF_FileHandle::~PF_FileHandle()
{
    
}

//
// PF_FileHandle (copy constructor)
//
PF_FileHandle::PF_FileHandle(const PF_FileHandle& fileHandle)
{
    this->pBufferMgr = fileHandle.pBufferMgr;
    this->hdr = fileHandle.hdr;
    this->bFileOpen = fileHandle.bFileOpen;
    this->bHdrChanged = fileHandle.bHdrChanged;
    this->unixfd = fileHandle.unixfd;
}

//
// operator=
//
PF_FileHandle& PF_FileHandle::operator=(const PF_FileHandle& fileHandle)
{
    if (this != &fileHandle) {
        this->pBufferMgr = fileHandle.pBufferMgr;
        this->hdr = fileHandle.hdr;
        this->bFileOpen = fileHandle.bFileOpen;
        this->bHdrChanged = fileHandle.bHdrChanged;
        this->unixfd = fileHandle.unixfd;
    }
    return *this;
}

//
// GetFirstPage
//
RC PF_FileHandle::GetFirstPage(PF_PageHandle& pageHandle) const
{
    return GetNextPage((PageNum)-1, pageHandle);
}

//
// GetLastPage
//
RC PF_FileHandle::GetLastPage(PF_PageHandle& pageHandle) const
{
    return GetPrevPage((PageNum)hdr.numPages, pageHandle);
}

//
// GetNextPage
//
RC PF_FileHandle::GetNextPage(PageNum current, PF_PageHandle& pageHandle) const
{
    int rc;

    if (!bFileOpen)
        return PF_CLOSEDFILE;

    if (current != -1 && !IsValidPageNum(current))
        return PF_INVALIDPAGE;

    for (current++; current < hdr.numPages; current++) {
        if (!(rc = GetThisPage(current, pageHandle)))
            return 0;
        if (rc != PF_INVALIDPAGE)
            return rc;
    }

    return PF_EOF;
}

//
// GetPrevPage
//
RC PF_FileHandle::GetPrevPage(PageNum current, PF_PageHandle& pageHandle) const
{
    int rc;

    if (!bFileOpen)
        return PF_CLOSEDFILE;

    if (current != hdr.numPages && !IsValidPageNum(current))
        return PF_INVALIDPAGE;

    for (current--; current >= 0; current--) {
        if (!(rc = GetThisPage(current, pageHandle)))
            return 0;
        if (rc != PF_INVALIDPAGE)
            return rc;
    }

    return PF_EOF;
}

//
// GetThisPage
//
RC PF_FileHandle::GetThisPage(PageNum pageNum, PF_PageHandle& pageHandle) const
{
    int rc;
    char* pPageBuf;

    if (!bFileOpen)
        return PF_CLOSEDFILE;

    if (!IsValidPageNum(pageNum))
        return PF_INVALIDPAGE;

    if ((rc = pBufferMgr->GetPage(unixfd, pageNum, &pPageBuf)))
        return rc;

    if (((PF_PageHdr*)pPageBuf)->nextFree == PF_PAGE_USED) {
        pageHandle.pageNum = pageNum;
        pageHandle.pPageData = pPageBuf + sizeof(PF_PageHdr);
        return 0;
    }

    if ((rc = UnpinPage(pageNum)))
        return rc;

    return PF_INVALIDPAGE;
}

//
// AllocatePage
// Allocate a new page in the file
RC PF_FileHandle::AllocatePage(PF_PageHandle& pageHandle)
{
    int rc;
    int pageNum;
    char* pPageBuf;

    if (!bFileOpen)
        return PF_CLOSEDFILE;

    if (hdr.firstFree != PF_PAGE_LIST_END) {
        pageNum = hdr.firstFree;
        if ((rc = pBufferMgr->GetPage(unixfd, pageNum, &pPageBuf)))
            return rc;
        hdr.firstFree = ((PF_PageHdr*)pPageBuf)->nextFree;
    }
    else {
        pageNum = hdr.numPages;
        if ((rc = pBufferMgr->AllocatePage(unixfd, pageNum, &pPageBuf)))
            return rc;
        hdr.numPages++;
    }
    // Mark the header as changed
    bHdrChanged = TRUE;
    // Mark this page as used
    ((PF_PageHdr*)pPageBuf)->nextFree = PF_PAGE_USED;
	// Initialize page data to zero
    memset(pPageBuf + sizeof(PF_PageHdr), 0, PF_PAGE_SIZE);
    // Mark the page dirty because we changed the next pointer
    if ((rc = MarkDirty(pageNum)))
        return rc;

    pageHandle.pageNum = pageNum;
    pageHandle.pPageData = pPageBuf + sizeof(PF_PageHdr);
    return 0;
}

//
// DisposePage
//
RC PF_FileHandle::DisposePage(PageNum pageNum)
{
    int rc;
    char* pPageBuf;

    if (!bFileOpen)
        return PF_CLOSEDFILE;

    if (!IsValidPageNum(pageNum))
        return PF_INVALIDPAGE;

    if ((rc = pBufferMgr->GetPage(unixfd, pageNum, &pPageBuf, FALSE)))
        return rc;
    // Page must be valid (used)
    if (((PF_PageHdr*)pPageBuf)->nextFree != PF_PAGE_USED) {
        if ((rc = UnpinPage(pageNum)))
            return rc;
        return PF_PAGEFREE;
    }
    // Put this page onto the free list
    ((PF_PageHdr*)pPageBuf)->nextFree = hdr.firstFree;
    hdr.firstFree = pageNum;
    bHdrChanged = TRUE;
    // Mark the page dirty because we changed the next pointer
    if ((rc = MarkDirty(pageNum)))
        return rc;

    if ((rc = UnpinPage(pageNum)))
        return rc;

    return 0;
}

//
// MarkDirty
//
RC PF_FileHandle::MarkDirty(PageNum pageNum) const
{
    if (!bFileOpen)
        return PF_CLOSEDFILE;
    if (!IsValidPageNum(pageNum))
        return PF_INVALIDPAGE;
    return pBufferMgr->MarkDirty(unixfd, pageNum);
}

//
// UnpinPage
//
RC PF_FileHandle::UnpinPage(PageNum pageNum) const
{
    if (!bFileOpen)
        return PF_CLOSEDFILE;
    if (!IsValidPageNum(pageNum))
        return PF_INVALIDPAGE;
    // Tell the buffer manager to unpin the page
    return pBufferMgr->UnpinPage(unixfd, pageNum);
}

//
// FlushPages
//
RC PF_FileHandle::FlushPages() const
{
    if (!bFileOpen)
        return PF_CLOSEDFILE;

    if (bHdrChanged) {
        auto it = g_fdToStream.find(unixfd);
        auto tmp_test = g_fdToStream.end();
        if (it == g_fdToStream.end() || !it->second)
            return PF_UNIX;

        std::fstream* fs = it->second;
        fs->seekp(0, std::ios::beg);
        fs->write(reinterpret_cast<const char*>(&hdr), sizeof(PF_FileHdr));
        fs->flush();
        if (!fs->good())
            return PF_HDRWRITE;

        PF_FileHandle* dummy = (PF_FileHandle*)this;
        dummy->bHdrChanged = FALSE;
    }

    return pBufferMgr->FlushPages(unixfd);
}

//
// ForcePages
//
RC PF_FileHandle::ForcePages(PageNum pageNum) const
{
    if (!bFileOpen)
        return PF_CLOSEDFILE;

    if (bHdrChanged) {
        auto it = g_fdToStream.find(unixfd);
        if (it == g_fdToStream.end() || !it->second)
            return PF_UNIX;

        std::fstream* fs = it->second;
        fs->seekp(0, std::ios::beg);
        fs->write(reinterpret_cast<const char*>(&hdr), sizeof(PF_FileHdr));
        fs->flush();
        if (!fs->good())
            return PF_HDRWRITE;

        PF_FileHandle* dummy = (PF_FileHandle*)this;
        dummy->bHdrChanged = FALSE;
    }

    return pBufferMgr->ForcePages(unixfd, pageNum);
}

//
// IsValidPageNum
//
int PF_FileHandle::IsValidPageNum(PageNum pageNum) const
{
    return (bFileOpen && pageNum >= 0 && pageNum < hdr.numPages);
}