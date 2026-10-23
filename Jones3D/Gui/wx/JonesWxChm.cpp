// HTML Help (.chm) reader of the help viewer (JonesWxHelp.cpp), added for the GOG release's Jones3D.chm.
// The CHM file is unpacked with libmspack into a case-insensitive file system ("jhchm:") with a generated .hhp for
// wxHtmlHelpController, so the help shows in the built-in viewer on all platforms.
#include "JonesWxHlp.h"

#if wxUSE_WXHTML_HELP

#include <wx/filesys.h>
#include <wx/image.h>
#include <wx/mstream.h>

#include <mspack.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
    using Bytes = std::vector<uint8_t>;

    // Files of the unpacked book by lower case path
    std::map<wxString, Bytes> chmFiles;

    // Case-insensitive file system over chmFiles, CHM links rely on Windows' case-insensitive file names
    class ChmFSHandler : public wxFileSystemHandler
    {
    public:
        bool CanOpen(const wxString& location) override
        {
            return GetProtocol(location) == "jhchm";
        }

        wxFSFile* OpenFile(wxFileSystem& fs, const wxString& location) override
        {
            (void)fs;

            // Resolve "." and ".." segments, the path is relative to the book root
            wxArrayString parts;
            for ( const wxString& part : wxSplit(GetRightLocation(location), '/', 0) )
            {
                if ( part == ".." )
                {
                    if ( !parts.empty() ) {
                        parts.pop_back();
                    }
                }
                else if ( !part.empty() && part != "." ) {
                    parts.push_back(part);
                }
            }

            const auto it = chmFiles.find(wxJoin(parts, '/', 0).Lower());
            if ( it == chmFiles.end() ) {
                return nullptr;
            }

            const wxString ext = it->first.AfterLast('.');
            wxString mime = "application/octet-stream";
            if ( ext == "htm" || ext == "html" ) mime = "text/html";
            else if ( ext == "gif" ) mime = "image/gif";
            else if ( ext == "jpg" || ext == "jpeg" ) mime = "image/jpeg";
            else if ( ext == "png" ) mime = "image/png";
            else if ( ext == "bmp" ) mime = "image/bmp";
            else if ( ext == "css" ) mime = "text/css";
            else if ( ext == "hhc" || ext == "hhk" || ext == "hhp" ) mime = "text/plain";

            return new wxFSFile(new wxMemoryInputStream(it->second.data(), it->second.size()), location, mime, GetAnchor(location), wxDateTime::Now());
        }
    };

    // libmspack I/O: the CHM file is read from disk, extracted files are written to memory
    struct ChmIoFile
    {
        FILE* pFile  = nullptr;
        Bytes* pData = nullptr;
    };

    struct ChmSystem
    {
        mspack_system sys;
        Bytes* pOutput = nullptr; // Target of the next file opened for writing
    };

    mspack_file* ChmOpen(mspack_system* pSys, const char* pFilename, int mode)
    {
        ChmIoFile* pIo = new ChmIoFile;
        if ( mode == MSPACK_SYS_OPEN_READ )
        {
        #ifdef _WIN32
            pIo->pFile = _wfopen(wxString::FromUTF8(pFilename).wc_str(), L"rb");
        #else
            pIo->pFile = fopen(pFilename, "rb");
        #endif
        }
        else if ( mode == MSPACK_SYS_OPEN_WRITE ) {
            pIo->pData = reinterpret_cast<ChmSystem*>(pSys)->pOutput;
        }

        if ( !pIo->pFile && !pIo->pData )
        {
            delete pIo;
            return nullptr;
        }
        return reinterpret_cast<mspack_file*>(pIo);
    }

    void ChmClose(mspack_file* pFile)
    {
        ChmIoFile* pIo = reinterpret_cast<ChmIoFile*>(pFile);
        if ( pIo && pIo->pFile ) {
            fclose(pIo->pFile);
        }
        delete pIo;
    }

    int ChmRead(mspack_file* pFile, void* pBuffer, int bytes)
    {
        ChmIoFile* pIo = reinterpret_cast<ChmIoFile*>(pFile);
        if ( !pIo || !pIo->pFile || bytes < 0 ) {
            return -1;
        }
        const size_t count = fread(pBuffer, 1, (size_t)bytes, pIo->pFile);
        return ferror(pIo->pFile) ? -1 : (int)count;
    }

    int ChmWrite(mspack_file* pFile, void* pBuffer, int bytes)
    {
        ChmIoFile* pIo = reinterpret_cast<ChmIoFile*>(pFile);
        if ( !pIo || !pIo->pData || bytes < 0 ) {
            return -1;
        }
        const uint8_t* pBytes = static_cast<const uint8_t*>(pBuffer);
        pIo->pData->insert(pIo->pData->end(), pBytes, pBytes + bytes);
        return bytes;
    }

    int ChmSeek(mspack_file* pFile, off_t offset, int mode)
    {
        ChmIoFile* pIo = reinterpret_cast<ChmIoFile*>(pFile);
        if ( !pIo || !pIo->pFile ) {
            return -1;
        }
        const int whence = mode == MSPACK_SYS_SEEK_START ? SEEK_SET : mode == MSPACK_SYS_SEEK_CUR ? SEEK_CUR : SEEK_END;
    #ifdef _WIN32
        return _fseeki64(pIo->pFile, (long long)offset, whence) ? -1 : 0;
    #else
        return fseeko(pIo->pFile, offset, whence) ? -1 : 0;
    #endif
    }

    off_t ChmTell(mspack_file* pFile)
    {
        ChmIoFile* pIo = reinterpret_cast<ChmIoFile*>(pFile);
        if ( !pIo || !pIo->pFile ) {
            return -1;
        }
    #ifdef _WIN32
        return (off_t)_ftelli64(pIo->pFile);
    #else
        return ftello(pIo->pFile);
    #endif
    }

    void ChmMessage(mspack_file*, const char*, ...) {}
    void* ChmAlloc(mspack_system*, size_t bytes) { return malloc(bytes); }
    void ChmFree(void* p) { free(p); }
    void ChmCopy(void* pSrc, void* pDst, size_t bytes) { memcpy(pDst, pSrc, bytes); }

    // #SYSTEM entries of the CHM: code 0 contents file, 1 index file, 2 default topic, 3 title
    std::map<int, std::string> ReadSystemEntries(const Bytes& data)
    {
        std::map<int, std::string> entries;
        size_t pos = 4; // version
        while ( pos + 4 <= data.size() )
        {
            const int code    = data[pos] | (data[pos + 1] << 8);
            const size_t size = data[pos + 2] | (data[pos + 3] << 8);
            pos += 4;
            if ( pos + size > data.size() ) {
                break;
            }
            entries[code] = std::string(reinterpret_cast<const char*>(data.data()) + pos, strnlen(reinterpret_cast<const char*>(data.data()) + pos, size));
            pos += size;
        }
        return entries;
    }
}

namespace JonesWxHlp
{
    wxString LoadChmBook(const wxString& filePath, wxString* pError)
    {
        static bool bHandler = false;
        if ( !bHandler )
        {
            wxFileSystem::AddHandler(new ChmFSHandler);
            wxInitAllImageHandlers(); // GIF and JPEG pictures
            bHandler = true;
        }

        ChmSystem sys;
        memset(&sys.sys, 0, sizeof(sys.sys));
        sys.sys.open    = ChmOpen;
        sys.sys.close   = ChmClose;
        sys.sys.read    = ChmRead;
        sys.sys.write   = ChmWrite;
        sys.sys.seek    = ChmSeek;
        sys.sys.tell    = ChmTell;
        sys.sys.message = ChmMessage;
        sys.sys.alloc   = ChmAlloc;
        sys.sys.free    = ChmFree;
        sys.sys.copy    = ChmCopy;

        mschm_decompressor* pChm = mspack_create_chm_decompressor(&sys.sys);
        mschmd_header* pHeader   = pChm ? pChm->open(pChm, filePath.utf8_str()) : nullptr;
        if ( !pHeader )
        {
            if ( pError ) {
                *pError = "not an HTML Help file";
            }
            if ( pChm ) {
                mspack_destroy_chm_decompressor(pChm);
            }
            return wxString();
        }

        chmFiles.clear();
        Bytes system;
        for ( mschmd_file* pFile = pHeader->files; pFile; pFile = pFile->next )
        {
            const wxString name = wxString::FromUTF8(pFile->filename).AfterFirst('/');
            if ( name.empty() || (name[0] == '#' && name != "#SYSTEM") || name[0] == '$' ) {
                continue; // Internal files
            }

            Bytes data;
            sys.pOutput = &data;
            if ( pChm->extract(pChm, pFile, const_cast<char*>("memory")) != MSPACK_ERR_OK ) {
                continue;
            }

            if ( name == "#SYSTEM" ) {
                system = std::move(data);
            }
            else {
                chmFiles[name.Lower()] = std::move(data);
            }
        }

        pChm->close(pChm, pHeader);
        mspack_destroy_chm_decompressor(pChm);

        // Book project from #SYSTEM, the contents and index files are found by extension when it doesn't name them
        std::map<int, std::string> entries = ReadSystemEntries(system);
        for ( const auto& file : chmFiles )
        {
            if ( entries[0].empty() && file.first.EndsWith(".hhc") ) {
                entries[0] = std::string(file.first.utf8_str());
            }
            if ( entries[1].empty() && file.first.EndsWith(".hhk") ) {
                entries[1] = std::string(file.first.utf8_str());
            }
        }

        // The CHM's texts are in Windows-1252, as is the project file
        std::string hhp = "[OPTIONS]\nCharset=windows-1252\n";
        hhp += "Title=" + entries[3] + "\n";
        hhp += "Default topic=" + entries[2] + "\n";
        hhp += "Contents file=" + entries[0] + "\n";
        hhp += "Index file=" + entries[1] + "\n";
        chmFiles["book.hhp"] = Bytes(hhp.begin(), hhp.end());
        return "jhchm:book.hhp";
    }
}

#else // !wxUSE_WXHTML_HELP

namespace JonesWxHlp
{
    wxString LoadChmBook(const wxString& filePath, wxString* pError)
    {
        (void)filePath;
        if ( pError ) {
            *pError = "wxHTML help is not available";
        }
        return wxString();
    }
}

#endif // wxUSE_WXHTML_HELP
