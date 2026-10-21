// Reads resources from executables as data, on systems without the Win32 resource API:
// icon resources of the original Indy3D.exe and the launcher resources of the Steam version's Jones3D.exe.
// The PE resource directory is walked to the entry, icons are wrapped in an .ico file and decoded by wx.
#include "JonesWx.h"

#include <std/General/std.h>

#include <wx/file.h>
#include <wx/mstream.h>

#include <stdint.h>
#include <string.h>
#include <vector>

namespace
{
    constexpr const char* resourceModuleName = "Indy3D.exe";

    constexpr uint32_t RESTYPE_ICON      = 3;
    constexpr uint32_t RESTYPE_GROUPICON = 14;

    uint16_t ReadU16(const std::vector<uint8_t>& data, size_t offset)
    {
        return offset + 2 <= data.size() ? (uint16_t)(data[offset] | data[offset + 1] << 8) : 0;
    }

    uint32_t ReadU32(const std::vector<uint8_t>& data, size_t offset)
    {
        return offset + 4 <= data.size() ? (uint32_t)(data[offset] | data[offset + 1] << 8 | data[offset + 2] << 16 | (uint32_t)data[offset + 3] << 24) : 0;
    }

    class PeResources
    {
    public:
        bool Load(const char* pFilename)
        {
            tFileHandle fh = std_g_pHS->pFileOpen(pFilename, "rb");
            if ( !fh ) {
                return false;
            }

            uint8_t aBuf[65536];
            size_t nRead;
            while ( (nRead = std_g_pHS->pFileRead(fh, aBuf, sizeof(aBuf))) > 0 ) {
                data.insert(data.end(), aBuf, aBuf + nRead);
            }
            std_g_pHS->pFileClose(fh);
            return Parse();
        }

        // Loads executable at host file system path
        bool LoadNative(const wxString& path)
        {
            wxFile file;
            if ( !wxFile::Exists(path) || !file.Open(path) ) {
                return false;
            }

            data.resize((size_t)file.Length());
            if ( file.Read(data.data(), data.size()) != (ssize_t)data.size() ) {
                return false;
            }

            return Parse();
        }

        bool Parse()
        {
            // DOS header -> PE header
            if ( ReadU16(data, 0) != 0x5A4D ) { // MZ
                return false;
            }

            const size_t peOffset = ReadU32(data, 0x3C);
            if ( ReadU32(data, peOffset) != 0x00004550 ) { // PE\0\0
                return false;
            }

            const size_t fileHeader     = peOffset + 4;
            const size_t numSections    = ReadU16(data, fileHeader + 2);
            const size_t optHeaderSize  = ReadU16(data, fileHeader + 16);
            const size_t optHeader      = fileHeader + 20;
            if ( ReadU16(data, optHeader) != 0x10B ) { // PE32
                return false;
            }

            // Data directory 2 is the resource directory
            rsrcRva = ReadU32(data, optHeader + 96 + 2 * 8);
            if ( !rsrcRva ) {
                return false;
            }

            const size_t sectionTable = optHeader + optHeaderSize;
            for ( size_t i = 0; i < numSections; ++i )
            {
                const size_t section = sectionTable + i * 40;
                aSections.push_back({ ReadU32(data, section + 12), ReadU32(data, section + 8), ReadU32(data, section + 20) });
            }

            rsrcOffset = RvaToOffset(rsrcRva);
            return rsrcOffset != 0;
        }

        // Returns the data of resource type/id (first language), or false if there's none.
        // pTypeName is the name of a named resource type (e.g. "WAVE"), type is used when it's null.
        bool Find(uint32_t type, uint32_t id, size_t* pOffset, size_t* pSize, const char* pTypeName = nullptr) const
        {
            size_t typeDir = pTypeName ? FindNamedEntry(rsrcOffset, pTypeName) : FindEntry(rsrcOffset, type);
            if ( !typeDir || !(typeDir & 0x80000000) ) {
                return false;
            }

            size_t nameDir = FindEntry(rsrcOffset + (typeDir & 0x7FFFFFFF), id);
            if ( !nameDir || !(nameDir & 0x80000000) ) {
                return false;
            }

            // First language entry
            const size_t langDir = rsrcOffset + (nameDir & 0x7FFFFFFF);
            if ( ReadU16(data, langDir + 12) + ReadU16(data, langDir + 14) == 0 ) {
                return false;
            }

            const uint32_t dataEntry = ReadU32(data, langDir + 16 + 4);
            if ( dataEntry & 0x80000000 ) {
                return false;
            }

            const size_t entry = rsrcOffset + dataEntry;
            *pOffset = RvaToOffset(ReadU32(data, entry));
            *pSize   = ReadU32(data, entry + 4);
            return *pOffset && *pOffset + *pSize <= data.size();
        }

        const std::vector<uint8_t>& GetData() const { return data; }

    private:
        struct Section
        {
            uint32_t rva;
            uint32_t size;
            uint32_t fileOffset;
        };

        size_t RvaToOffset(uint32_t rva) const
        {
            for ( const Section& section : aSections )
            {
                if ( rva >= section.rva && rva < section.rva + section.size ) {
                    return section.fileOffset + (rva - section.rva);
                }
            }
            return 0;
        }

        // Finds the entry with integer id in the resource directory at dirOffset, returns its offset field or 0
        size_t FindEntry(size_t dirOffset, uint32_t id) const
        {
            const size_t numNamed = ReadU16(data, dirOffset + 12);
            const size_t numIds   = ReadU16(data, dirOffset + 14);
            for ( size_t i = 0; i < numNamed + numIds; ++i )
            {
                const size_t entry = dirOffset + 16 + i * 8;
                const uint32_t name = ReadU32(data, entry);
                if ( !(name & 0x80000000) && name == id ) {
                    return ReadU32(data, entry + 4);
                }
            }
            return 0;
        }

        // Finds the entry with name pName (ASCII, case-insensitive like the resource API) in the resource directory at dirOffset
        size_t FindNamedEntry(size_t dirOffset, const char* pName) const
        {
            const size_t numNamed = ReadU16(data, dirOffset + 12);
            const size_t nameLen  = strlen(pName);
            for ( size_t i = 0; i < numNamed; ++i )
            {
                const size_t entry  = dirOffset + 16 + i * 8;
                const uint32_t name = ReadU32(data, entry);
                if ( !(name & 0x80000000) ) {
                    continue;
                }

                // Counted UTF-16 string relative to the resource section
                const size_t str = rsrcOffset + (name & 0x7FFFFFFF);
                if ( ReadU16(data, str) != nameLen ) {
                    continue;
                }

                bool bMatch = true;
                for ( size_t c = 0; c < nameLen && bMatch; ++c ) {
                    bMatch = wxTolower(ReadU16(data, str + 2 + c * 2)) == wxTolower((unsigned char)pName[c]);
                }

                if ( bMatch ) {
                    return ReadU32(data, entry + 4);
                }
            }
            return 0;
        }

        std::vector<uint8_t> data;
        std::vector<Section> aSections;
        uint32_t rsrcRva    = 0;
        size_t rsrcOffset   = 0;
    };

    const PeResources* GetResources()
    {
        static PeResources resources;
        static bool bLoaded = false;
        static bool bValid  = false;
        if ( !bLoaded )
        {
            bLoaded = true;
            bValid  = resources.Load(resourceModuleName);
            if ( !bValid ) {
                STDLOG_ERROR("JonesWx: Couldn't read resources from %s, dialog icons won't be shown.\n", resourceModuleName);
            }
        }

        return bValid ? &resources : nullptr;
    }
}

wxImage JonesWx::LoadExeIconImage(int iconID, int size)
{
    const PeResources* pRes = GetResources();
    size_t groupOffset, groupSize;
    if ( !pRes || !pRes->Find(RESTYPE_GROUPICON, (uint32_t)iconID, &groupOffset, &groupSize) ) {
        return wxImage();
    }

    // GRPICONDIR: reserved, type, count, followed by 14 byte GRPICONDIRENTRY structs
    const std::vector<uint8_t>& data = pRes->GetData();
    const size_t count = ReadU16(data, groupOffset + 4);

    // Prefer the smallest image at least the requested size, then the highest color depth
    size_t best = SIZE_MAX;
    int bestWidth = 0, bestBpp = 0;
    for ( size_t i = 0; i < count && 6 + (i + 1) * 14 <= groupSize; ++i )
    {
        const size_t entry = groupOffset + 6 + i * 14;
        const int width = data[entry] ? data[entry] : 256;
        const int bpp   = ReadU16(data, entry + 6);

        const bool bBetterSize = best == SIZE_MAX
            || (width >= size && (bestWidth < size || width < bestWidth))
            || (bestWidth < size && width > bestWidth);
        if ( bBetterSize || (width == bestWidth && bpp > bestBpp) )
        {
            best = i;
            bestWidth = width;
            bestBpp = bpp;
        }
    }

    if ( best == SIZE_MAX ) {
        return wxImage();
    }

    const size_t entry = groupOffset + 6 + best * 14;
    size_t imageOffset, imageSize;
    if ( !pRes->Find(RESTYPE_ICON, ReadU16(data, entry + 12), &imageOffset, &imageSize) ) {
        return wxImage();
    }

    // Single image .ico: ICONDIR (6 bytes) + ICONDIRENTRY (16 bytes) + image data
    std::vector<uint8_t> ico(6 + 16 + imageSize);
    ico[2] = 1; // type icon
    ico[4] = 1; // 1 image
    memcpy(&ico[6], &data[entry], 12); // width .. bytes in resource match the GRPICONDIRENTRY layout
    const uint32_t imageFileOffset = 6 + 16;
    memcpy(&ico[6 + 12], &imageFileOffset, 4);
    memcpy(&ico[6 + 16], &data[imageOffset], imageSize);

    wxMemoryInputStream stream(ico.data(), ico.size());
    wxImage image(stream, wxBITMAP_TYPE_ICO);
    if ( image.IsOk() && (image.GetWidth() != size || image.GetHeight() != size) ) {
        image.Rescale(size, size, wxIMAGE_QUALITY_HIGH);
    }

    return image;
}

std::vector<uint8_t> JonesWx::ReadExeResource(const wxString& exePath, uint32_t type, uint32_t id, const char* pTypeName)
{
    PeResources resources;
    size_t offset, size;
    if ( !resources.LoadNative(exePath) || !resources.Find(type, id, &offset, &size, pTypeName) ) {
        return {};
    }

    const std::vector<uint8_t>& data = resources.GetData();
    return std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + size);
}
