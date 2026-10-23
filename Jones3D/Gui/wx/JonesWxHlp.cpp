#include "JonesWxHlp.h"

#if wxUSE_WXHTML_HELP

#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/fs_mem.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/textfile.h>
#include <wx/tokenzr.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
    using Bytes = std::vector<uint8_t>;

    // Bounds-checked little-endian reader. A read past the end sets 'ok' to false and returns zeros,
    // callers check 'ok' once a structure is read.
    struct Reader
    {
        const uint8_t* pData = nullptr;
        size_t size          = 0;
        size_t pos           = 0;
        bool ok              = true;

        Reader() = default;
        Reader(const uint8_t* pData_, size_t size_) : pData(pData_), size(size_) {}
        explicit Reader(const Bytes& data) : pData(data.data()), size(data.size()) {}

        size_t Remaining() const { return pos < size ? size - pos : 0; }

        bool Need(size_t count)
        {
            if ( !ok || count > Remaining() ) {
                ok = false;
            }
            return ok;
        }

        uint8_t U8() { return Need(1) ? pData[pos++] : 0; }

        uint16_t U16()
        {
            if ( !Need(2) ) {
                return 0;
            }
            uint16_t value = (uint16_t)(pData[pos] | (pData[pos + 1] << 8));
            pos += 2;
            return value;
        }

        int16_t I16() { return (int16_t)U16(); }

        uint32_t U32()
        {
            if ( !Need(4) ) {
                return 0;
            }
            uint32_t value = (uint32_t)pData[pos] | ((uint32_t)pData[pos + 1] << 8) | ((uint32_t)pData[pos + 2] << 16) | ((uint32_t)pData[pos + 3] << 24);
            pos += 4;
            return value;
        }

        int32_t I32() { return (int32_t)U32(); }

        Reader Sub(size_t count)
        {
            if ( !Need(count) ) {
                return Reader();
            }
            Reader sub(pData + pos, count);
            pos += count;
            return sub;
        }

        Bytes Read(size_t count)
        {
            if ( !Need(count) ) {
                return Bytes();
            }
            Bytes bytes(pData + pos, pData + pos + count);
            pos += count;
            return bytes;
        }

        std::string CString()
        {
            std::string text;
            while ( ok && pos < size && pData[pos] ) {
                text += (char)pData[pos++];
            }
            if ( pos < size ) {
                ++pos; // terminator
            }
            else {
                ok = false;
            }
            return text;
        }

        // WinHelp compressed numbers, the low bit selects the long form
        uint16_t CompressedUShort()
        {
            uint8_t first = U8();
            return (first & 1) ? (uint16_t)(first / 2 + U8() * 128) : (uint16_t)(first / 2);
        }

        int16_t CompressedShort()
        {
            uint8_t first = U8();
            return (first & 1) ? (int16_t)(first / 2 + U8() * 128 - 16384) : (int16_t)(first / 2 - 64);
        }

        uint32_t CompressedULong()
        {
            uint16_t first = U16();
            return (first & 1) ? (uint32_t)(first / 2 + (uint32_t)U16() * 32768u) : (uint32_t)(first / 2);
        }

        int32_t CompressedLong()
        {
            uint16_t first = U16();
            return (first & 1) ? (int32_t)((int64_t)first / 2 + (int64_t)U16() * 32768 - 1073741824) : (int32_t)(first / 2 - 16384);
        }
    };

    // Block LZ77 of topic blocks, phrase images and pictures
    Bytes Lz77Decompress(const uint8_t* pIn, size_t inSize, size_t maxOut)
    {
        Bytes out;
        out.reserve(std::min(maxOut, inSize * 2));
        size_t pos = 0;
        while ( pos < inSize && out.size() < maxOut )
        {
            uint8_t flags = pIn[pos++];
            for ( int bit = 0; bit < 8 && pos < inSize && out.size() < maxOut; ++bit )
            {
                if ( !(flags & (1 << bit)) )
                {
                    out.push_back(pIn[pos++]);
                    continue;
                }

                if ( pos + 1 >= inSize ) {
                    return out;
                }

                uint16_t code = (uint16_t)(pIn[pos] | (pIn[pos + 1] << 8));
                pos += 2;
                size_t distance = (code & 0x0FFF) + 1u;
                size_t length   = (code >> 12) + 3u;
                if ( distance > out.size() ) {
                    return out;
                }
                for ( size_t i = 0; i < length && out.size() < maxOut; ++i ) {
                    out.push_back(out[out.size() - distance]);
                }
            }
        }
        return out;
    }

    Bytes RleDecompress(const Bytes& in, size_t expected)
    {
        Bytes out;
        out.reserve(expected);
        size_t pos = 0;
        while ( pos < in.size() && out.size() < expected )
        {
            uint8_t control = in[pos++];
            size_t count    = control & 0x7F;
            if ( control & 0x80 )
            {
                for ( size_t i = 0; i < count && pos < in.size(); ++i ) {
                    out.push_back(in[pos++]);
                }
            }
            else if ( pos < in.size() )
            {
                out.insert(out.end(), count, in[pos++]);
            }
        }
        out.resize(expected);
        return out;
    }

    // Context name hash of |CONTEXT and the contents file, letters fold case (table from HELPDECO)
    int32_t ContextHash(const std::string& name)
    {
        static const int8_t aTable[256] = {
              0, -47, -46, -45, -44, -43, -42, -41, -40, -39, -38, -37, -36, -35, -34, -33,
            -32, -31, -30, -29, -28, -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17,
            -16,  11, -14, -13, -12, -11, -10,  -9,  -8,  -7,  -6,  -5,  -4,  -3,  12,  -1,
             10,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,
             16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
             32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  11,  12,  13,  14,  13,
             16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
             32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
             80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,
             96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
            112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
           -128,-127,-126,-125,  11,-123,-122,-121,-120,-119,-118,-117,-116,-115,-114,-113,
           -112,-111,-110,-109,-108,-107,-106,-105,-104,-103,-102,-101,-100, -99, -98, -97,
            -96, -95, -94, -93, -92, -91, -90, -89, -88, -87, -86, -85, -84, -83, -82, -81,
            -80, -79, -78, -77, -76, -75, -74, -73, -72, -71, -70, -69, -68, -67, -66, -65,
            -64, -63, -62, -61, -60, -59, -58, -57, -56, -55, -54, -53, -52, -51, -50, -49,
        };

        if ( name.empty() ) {
            return 1;
        }

        uint32_t hash = 0;
        for ( unsigned char c : name ) {
            hash = hash * 43u + (uint32_t)(int32_t)aTable[c];
        }
        return (int32_t)hash;
    }

    // Help file container: header, internal file directory (a B+ tree) and the internal files
    class HlpContainer
    {
    public:
        bool Load(const wxString& path)
        {
            wxFFile file(path, "rb");
            if ( !file.IsOpened() ) {
                return false;
            }

            data.resize((size_t)file.Length());
            if ( data.empty() || file.Read(data.data(), data.size()) != data.size() ) {
                return false;
            }

            Reader header(data);
            if ( header.U32() != 0x00035F3F ) {
                return false;
            }

            int32_t dirOffset = header.I32();
            Reader dir = File(dirOffset);
            return ForEachLeafEntry(dir, [this](Reader& entry) {
                std::string name = entry.CString();
                int32_t offset   = entry.I32();
                files[name]      = offset;
                return entry.ok;
            });
        }

        bool Has(const std::string& name) const { return files.count(name) != 0; }

        Reader Get(const std::string& name) const
        {
            auto it = files.find(name);
            return it != files.end() ? File(it->second) : Reader();
        }

        // Calls f for each entry of the leaf pages of a B+ tree, f reads one entry and returns false to stop
        template <typename F>
        static bool ForEachLeafEntry(Reader tree, F f)
        {
            if ( tree.U16() != 0x293B ) {
                return false;
            }

            tree.U16(); // flags
            uint16_t pageSize = tree.U16();
            tree.pos += 16 + 2 + 2; // structure, MustBeZero, PageSplits
            int16_t page = tree.I16();
            tree.pos += 2; // MustBeNegOne
            int16_t numPages = tree.I16();
            int16_t numLevels = tree.I16();
            tree.pos += 4; // TotalBtreeEntries
            if ( !tree.ok || pageSize < 8 || numPages <= 0 ) {
                return false;
            }

            const size_t pagesStart = tree.pos;
            auto pageReader = [&](int16_t pageNum) {
                Reader pageData = tree;
                pageData.pos = pagesStart + (size_t)pageNum * pageSize;
                return pageData.Sub(pageSize);
            };

            // Index pages: the leftmost child is stored as PreviousPage
            for ( int16_t level = 1; level < numLevels; ++level )
            {
                Reader index = pageReader(page);
                index.pos = 4;
                page = index.I16();
                if ( !index.ok || page < 0 || page >= numPages ) {
                    return false;
                }
            }

            std::set<int16_t> visited;
            while ( page >= 0 && page < numPages && visited.insert(page).second )
            {
                Reader leaf = pageReader(page);
                leaf.U16(); // unused bytes
                int16_t numEntries = leaf.I16();
                leaf.I16(); // previous page
                int16_t next = leaf.I16();
                for ( int16_t i = 0; i < numEntries && leaf.ok; ++i )
                {
                    if ( !f(leaf) ) {
                        return false;
                    }
                }
                page = next;
            }
            return true;
        }

    private:
        // Internal file: FILEHEADER (reserved, used size, flags) and its content
        Reader File(int32_t offset) const
        {
            Reader r(data);
            if ( offset < 0 || (size_t)offset + 9 > data.size() ) {
                return Reader();
            }
            r.pos = (size_t)offset;
            r.U32(); // reserved
            uint32_t used = r.U32();
            r.U8(); // flags
            return r.Sub(used);
        }

        Bytes data;
        std::map<std::string, int32_t> files;
    };

    struct Rgb
    {
        uint8_t r = 0, g = 0, b = 0;
        bool bSet = false;
    };

    struct Font
    {
        std::string face;
        int halfPoints = 20;
        bool bBold = false, bItalic = false, bUnderline = false, bStrike = false;
        bool bSymbol = false;
        Rgb color; // not set: the window's default text color
    };

    struct ParagraphFormat
    {
        int spaceAbove = 0, spaceBelow = 0;
        int leftIndent = 0, rightIndent = 0, firstIndent = 0;
        int align = 0; // 0 left, 1 right, 2 center
        std::vector<int> tabs;
    };

    // Inline content of a paragraph
    struct Inline
    {
        enum Kind { TEXT, LINEBREAK, TAB, PICTURE } kind = TEXT;
        std::string text;   // TEXT, raw Windows-1252 bytes
        uint16_t font = 0;  // TEXT
        int link = -1;      // TEXT: index into Topic links or -1
        std::string picture; // PICTURE: file name in the book
        int pictureAlign = 0; // PICTURE: 0 inline, 1 left, 2 right
    };

    struct Paragraph
    {
        ParagraphFormat format;
        std::vector<Inline> inlines;
        int column = -1; // Table cell column, -1 outside tables
    };

    struct Block
    {
        bool bTable = false;
        std::vector<int> columnWidths; // table
        std::vector<Paragraph> paragraphs;
    };

    struct Link
    {
        enum Kind { HASH, OFFSET, MACRO } kind = HASH;
        int32_t value = 0;
        std::string macro;
        bool bPopup = false;
    };

    struct Record
    {
        int32_t pos = 0;
        uint8_t type = 0;
        Bytes ld1;
        Bytes ld2;
    };

    struct Topic
    {
        int32_t pos = 0;
        std::string title;
        int32_t nonScrollStart = -1, scrollStart = -1, nextTopic = -1;
        std::vector<Record> records;
        int32_t startOffset = -1; // TOPICOFFSET of its first text record
    };

    class HlpBook
    {
    public:
        bool Load(const wxString& path, wxString* pError)
        {
            if ( !container.Load(path) ) {
                return Fail(pError, "not a WinHelp file");
            }

            if ( !LoadSystem() ) {
                return Fail(pError, "unsupported |SYSTEM (only WinHelp 3.1 and 4.0 files are supported)");
            }

            if ( !LoadPhrases() || !LoadTopics() ) {
                return Fail(pError, "couldn't read the topics");
            }

            LoadFonts();
            LoadContexts();
            LoadKeywords();
            return true;
        }

        // Writes the pages, pictures, contents and index to the memory file system under dir, returns the .hhp URL
        wxString Write(const wxString& dir, const wxString& cntPath);

    private:
        static bool Fail(wxString* pError, const char* pText)
        {
            if ( pError ) {
                *pError = pText;
            }
            return false;
        }

        bool LoadSystem();
        bool LoadPhrases();
        bool LoadTopics();
        void LoadFonts();
        void LoadContexts();
        void LoadKeywords();

        Bytes ExpandPhrases(const Bytes& stored, size_t expected) const;
        bool ReadLogical(int32_t pos, size_t length, Bytes& out) const;
        bool Locate(int32_t pos, size_t& block, size_t& offset) const;

        int TopicForOffset(int32_t offset) const;
        int TopicForHash(int32_t hash) const;
        int TopicForLink(const Link& link) const;

        void DecodeRecord(const Record& record, uint16_t& font, std::vector<Link>& links, std::vector<Block>& blocks);
        bool DecodeParagraphs(Reader& ld1, Reader& strings, int column, uint16_t& font, std::vector<Link>& links, Block& block);
        bool DecodeTableCells(Reader& ld1, Reader& strings, uint16_t& font, std::vector<Link>& links, Block& block, int depth);
        std::string DecodePicture(const Bytes& payload, uint8_t type);
        bool DecodeGraphicsStream(const uint8_t* pData, size_t size, wxImage& image, int& displayWidth, int& displayHeight);

        wxString TopicHtml(size_t topicIdx);
        wxString RunsHtml(const std::vector<Inline>& inlines, size_t begin, size_t end, const std::vector<Link>& links);
        wxString ParagraphHtml(const Paragraph& para, const std::vector<Link>& links);
        wxString BlockHtml(const Block& block, const std::vector<Link>& links);
        wxString TextHtml(const std::string& text, bool bSymbol) const;

        HlpContainer container;

        // |SYSTEM
        uint16_t minor        = 0;
        size_t physBlockSize  = 4096;
        size_t blockDataSize  = 4084;
        bool bLz77            = false;
        std::string title;
        int32_t contentsOffset = -1;
        Rgb scrollColor, nonScrollColor;

        // Phrases
        bool bHall = false;
        std::vector<std::string> phrases;

        // |TOPIC
        std::vector<Bytes> blockData;
        std::vector<Topic> topics;
        std::vector<std::pair<int32_t, int>> offsetAnchors; // TOPICOFFSET -> topic

        std::vector<Font> fonts;
        std::map<int32_t, int32_t> contexts; // hash -> TOPICOFFSET
        std::vector<std::pair<std::string, std::vector<int32_t>>> keywords;

        std::map<std::string, std::string> pictureFiles; // source key -> book file
        std::map<std::string, std::pair<int, int>> pictureSizes;
        wxString bookDir;
    };

    bool HlpBook::LoadSystem()
    {
        Reader sys = container.Get("|SYSTEM");
        if ( sys.U16() != 0x036C ) {
            return false;
        }

        minor = sys.U16();
        sys.U16(); // major
        sys.U32(); // generation date
        uint16_t flags = sys.U16();
        if ( !sys.ok || minor <= 16 ) {
            return false; // Windows 3.0 help files have a different topic layout
        }

        switch ( flags )
        {
            case 0: bLz77 = false; physBlockSize = 4096; blockDataSize = 4084; break;
            case 4: bLz77 = true;  physBlockSize = 4096; blockDataSize = 16384; break;
            case 8: bLz77 = true;  physBlockSize = 2048; blockDataSize = 16384; break;
            default: return false;
        }

        while ( sys.Remaining() >= 4 )
        {
            uint16_t type = sys.U16();
            uint16_t size = sys.U16();
            Reader rec    = sys.Sub(size);
            if ( !sys.ok ) {
                break;
            }

            if ( type == 1 ) {
                title = rec.CString();
            }
            else if ( type == 3 ) {
                contentsOffset = rec.I32();
            }
            else if ( type == 6 && size >= 90 )
            {
                // Window definition, the main window's colors are the page colors
                uint16_t winFlags = rec.U16();
                rec.pos += 10;
                std::string name((const char*)rec.pData + rec.pos, strnlen((const char*)rec.pData + rec.pos, 9));
                rec.pos += 9 + 51 + 5 * 2;
                uint32_t scroll    = rec.U32();
                uint32_t nonScroll = rec.U32();
                if ( name.empty() || wxString(name).IsSameAs("main", false) )
                {
                    if ( winFlags & 0x100 ) {
                        scrollColor = { (uint8_t)scroll, (uint8_t)(scroll >> 8), (uint8_t)(scroll >> 16), true };
                    }
                    if ( winFlags & 0x200 ) {
                        nonScrollColor = { (uint8_t)nonScroll, (uint8_t)(nonScroll >> 8), (uint8_t)(nonScroll >> 16), true };
                    }
                }
            }
        }
        return true;
    }

    bool HlpBook::LoadPhrases()
    {
        if ( container.Has("|PhrIndex") && container.Has("|PhrImage") )
        {
            // Hall compression (WinHelp 4): phrase lengths are a unary/binary bit stream
            Reader index = container.Get("|PhrIndex");
            Reader image = container.Get("|PhrImage");
            if ( index.I32() != 1 ) {
                return false;
            }

            int32_t count = index.I32();
            index.I32(); // compressed size
            int32_t imageSize = index.I32();
            int32_t storedSize = index.I32();
            index.I32();
            uint16_t bits = index.U16();
            index.U16();
            if ( !index.ok || count < 0 || imageSize < 0 || storedSize < 0 ) {
                return false;
            }

            const unsigned bitCount = bits & 0x0F;
            size_t bitPos = index.pos * 8;
            auto readBit = [&]() -> unsigned {
                size_t byte = bitPos / 8;
                unsigned value = byte < index.size ? (index.pData[byte] >> (bitPos % 8)) & 1u : 0u;
                ++bitPos;
                return value;
            };

            std::vector<size_t> lengths;
            for ( int32_t i = 0; i < count; ++i )
            {
                size_t unary = 0;
                while ( readBit() && bitPos < index.size * 8 ) {
                    ++unary;
                }
                size_t low = 0;
                for ( unsigned b = 0; b < bitCount; ++b ) {
                    low |= (size_t)readBit() << b;
                }
                lengths.push_back((unary << bitCount) + low + 1);
            }

            if ( (size_t)storedSize > image.size ) {
                return false;
            }
            Bytes decoded = (storedSize == imageSize) ? Bytes(image.pData, image.pData + storedSize) : Lz77Decompress(image.pData, (size_t)storedSize, (size_t)imageSize);
            size_t pos = 0;
            for ( size_t length : lengths )
            {
                if ( pos + length > decoded.size() ) {
                    return false;
                }
                phrases.emplace_back((const char*)decoded.data() + pos, length);
                pos += length;
            }
            bHall = true;
        }
        else if ( container.Has("|Phrases") )
        {
            // WinHelp 3.1 phrases: offset table and an LZ77 compressed image
            Reader table = container.Get("|Phrases");
            uint16_t count = table.U16();
            if ( table.U16() != 0x0100 ) {
                return false;
            }
            uint32_t imageSize = table.U32();
            std::vector<uint16_t> offsets;
            for ( uint16_t i = 0; i <= count; ++i ) {
                offsets.push_back(table.U16());
            }
            if ( !table.ok || offsets.empty() ) {
                return false;
            }
            Bytes image = Lz77Decompress(table.pData + table.pos, table.Remaining(), imageSize);
            const uint16_t base = offsets[0];
            for ( uint16_t i = 0; i < count; ++i )
            {
                size_t begin = offsets[i] - base, end = offsets[i + 1] - base;
                if ( end < begin || end > image.size() ) {
                    return false;
                }
                phrases.emplace_back((const char*)image.data() + begin, end - begin);
            }
        }
        return true;
    }

    Bytes HlpBook::ExpandPhrases(const Bytes& stored, size_t expected) const
    {
        if ( stored.size() >= expected ) {
            return Bytes(stored.begin(), stored.begin() + expected);
        }

        std::string out;
        auto phrase = [&](size_t idx) {
            if ( idx < phrases.size() ) {
                out += phrases[idx];
            }
        };

        size_t pos = 0;
        while ( pos < stored.size() && out.size() < expected )
        {
            uint8_t c = stored[pos++];
            if ( bHall )
            {
                if ( (c & 1) == 0 ) {
                    phrase(c >> 1);
                }
                else if ( (c & 3) == 1 ) {
                    phrase(128u + (c >> 2) * 256u + (pos < stored.size() ? stored[pos++] : 0u));
                }
                else if ( (c & 7) == 3 )
                {
                    size_t count = (c >> 3) + 1u;
                    for ( size_t i = 0; i < count && pos < stored.size(); ++i ) {
                        out += (char)stored[pos++];
                    }
                }
                else if ( (c & 0x0F) == 0x07 ) {
                    out.append((c >> 4) + 1u, ' ');
                }
                else {
                    out.append((c >> 4) + 1u, '\0');
                }
            }
            else
            {
                // Classic: bytes 1-15 with the next byte select a phrase, odd selectors add a space
                if ( c > 0 && c < 16 && pos < stored.size() )
                {
                    size_t idx = (size_t)(c - 1) * 256u + stored[pos++];
                    phrase(idx / 2);
                    if ( idx & 1 ) {
                        out += ' ';
                    }
                }
                else {
                    out += (char)c;
                }
            }
        }

        out.resize(expected, '\0');
        return Bytes(out.begin(), out.end());
    }

    bool HlpBook::Locate(int32_t pos, size_t& block, size_t& offset) const
    {
        if ( pos < 12 ) {
            return false;
        }
        block  = (size_t)(pos - 12) / blockDataSize;
        offset = (size_t)(pos - 12) % blockDataSize;
        return block < blockData.size() && offset < blockData[block].size();
    }

    // Reads a TOPICLINK, which may continue in the following blocks
    bool HlpBook::ReadLogical(int32_t pos, size_t length, Bytes& out) const
    {
        size_t block, offset;
        if ( !Locate(pos, block, offset) ) {
            return false;
        }

        out.clear();
        while ( out.size() < length && block < blockData.size() )
        {
            const Bytes& data = blockData[block];
            size_t take = std::min(length - out.size(), data.size() - std::min(offset, data.size()));
            out.insert(out.end(), data.begin() + offset, data.begin() + offset + take);
            ++block;
            offset = 0;
        }
        return out.size() == length;
    }

    bool HlpBook::LoadTopics()
    {
        Reader stream = container.Get("|TOPIC");
        if ( !stream.ok || stream.size < 12 ) {
            return false;
        }

        int32_t firstLink = -1;
        for ( size_t start = 0; start + 12 <= stream.size; start += physBlockSize )
        {
            Reader header(stream.pData + start, 12);
            header.I32(); // last topic link
            int32_t first = header.I32();
            if ( firstLink < 0 && first >= 12 ) {
                firstLink = first;
            }

            size_t storedSize = std::min(physBlockSize, stream.size - start) - 12;
            const uint8_t* pStored = stream.pData + start + 12;
            blockData.push_back(bLz77 ? Lz77Decompress(pStored, storedSize, blockDataSize) : Bytes(pStored, pStored + std::min(storedSize, blockDataSize)));
        }

        std::vector<Record> records;
        std::set<int32_t> visited;
        for ( int32_t pos = firstLink; pos >= 12 && visited.insert(pos).second; )
        {
            Bytes header;
            if ( !ReadLogical(pos, 21, header) ) {
                break;
            }

            Reader r(header);
            int32_t blockSize = r.I32();
            int32_t dataLen2  = r.I32();
            r.I32(); // previous
            int32_t next     = r.I32();
            int32_t dataLen1 = r.I32();
            uint8_t type     = r.U8();
            if ( blockSize < 21 || dataLen1 < 21 || dataLen1 > blockSize || dataLen2 < 0 || blockSize > (1 << 24) ) {
                break;
            }

            Bytes raw;
            if ( !ReadLogical(pos, (size_t)blockSize, raw) ) {
                break;
            }

            Record rec;
            rec.pos  = pos;
            rec.type = type;
            rec.ld1.assign(raw.begin() + 21, raw.begin() + dataLen1);
            rec.ld2 = ExpandPhrases(Bytes(raw.begin() + dataLen1, raw.end()), (size_t)dataLen2);
            records.push_back(std::move(rec));

            if ( next <= 0 ) {
                break;
            }
            pos = next;
        }

        // TOPICOFFSETs: block number * 0x8000 + characters of the text records before it in the block
        std::map<size_t, int32_t> blockChars;
        for ( Record& rec : records )
        {
            if ( rec.type == 0x02 || rec.type == 0x21 )
            {
                Topic topic;
                topic.pos = rec.pos;
                Reader ld1(rec.ld1);
                ld1.I32(); // block size
                ld1.I32(); // browse back
                ld1.I32(); // browse forward
                ld1.I32(); // topic number
                topic.nonScrollStart = ld1.I32();
                topic.scrollStart    = ld1.I32();
                topic.nextTopic      = ld1.I32();
                Reader ld2(rec.ld2);
                topic.title = ld2.CString();
                topics.push_back(std::move(topic));
                continue;
            }

            if ( topics.empty() ) {
                continue;
            }

            if ( rec.type == 0x20 || rec.type == 0x23 )
            {
                size_t block = (size_t)(rec.pos - 12) / blockDataSize;
                int32_t& chars = blockChars[block];
                int32_t offset = (int32_t)(block * 0x8000) + chars;
                offsetAnchors.emplace_back(offset, (int)topics.size() - 1);
                if ( topics.back().startOffset < 0 ) {
                    topics.back().startOffset = offset;
                }

                Reader ld1(rec.ld1);
                ld1.CompressedLong();
                chars += ld1.CompressedUShort();
            }

            topics.back().records.push_back(std::move(rec));
        }

        std::sort(offsetAnchors.begin(), offsetAnchors.end());
        return !topics.empty();
    }

    void HlpBook::LoadFonts()
    {
        Reader font = container.Get("|FONT");
        uint16_t numFaces = font.U16();
        uint16_t numDescs = font.U16();
        uint16_t facesOffset = font.U16();
        uint16_t descsOffset = font.U16();
        if ( !font.ok || !numFaces || descsOffset < facesOffset )
        {
            fonts.push_back(Font());
            return;
        }

        const size_t stride = (size_t)(descsOffset - facesOffset) / numFaces;
        std::vector<std::string> faces;
        for ( uint16_t i = 0; i < numFaces; ++i )
        {
            size_t at = facesOffset + i * stride;
            if ( at + stride > font.size ) {
                break;
            }
            faces.emplace_back((const char*)font.pData + at, strnlen((const char*)font.pData + at, stride));
        }

        // Classic 11-byte descriptors: attributes, half points, family, face, foreground, background
        Reader descs = font;
        descs.pos = descsOffset;
        for ( uint16_t i = 0; i < numDescs; ++i )
        {
            uint8_t attr  = descs.U8();
            uint8_t half  = descs.U8();
            descs.U8(); // family
            uint16_t face = descs.U16();
            uint8_t r = descs.U8(), g = descs.U8(), b = descs.U8();
            descs.pos += 3; // background
            if ( !descs.ok ) {
                break;
            }

            Font f;
            f.face       = face < faces.size() ? faces[face] : std::string("Arial");
            f.halfPoints = half;
            f.bBold      = attr & 0x01;
            f.bItalic    = attr & 0x02;
            f.bUnderline = attr & 0x04;
            f.bStrike    = attr & 0x08;
            f.bSymbol    = wxString(f.face).IsSameAs("Symbol", false) || wxString(f.face).StartsWith("Wingdings");
            f.color      = { r, g, b, !(r == 1 && g == 1 && b == 0) }; // RGB(1,1,0) is the default color
            fonts.push_back(f);
        }

        if ( fonts.empty() ) {
            fonts.push_back(Font());
        }
    }

    void HlpBook::LoadContexts()
    {
        HlpContainer::ForEachLeafEntry(container.Get("|CONTEXT"), [this](Reader& entry) {
            int32_t hash   = entry.I32();
            int32_t offset = entry.I32();
            contexts[hash] = offset;
            return entry.ok;
        });
    }

    void HlpBook::LoadKeywords()
    {
        Reader data = container.Get("|KWDATA");
        HlpContainer::ForEachLeafEntry(container.Get("|KWBTREE"), [&](Reader& entry) {
            std::string keyword = entry.CString();
            uint16_t count      = entry.U16();
            uint32_t offset     = entry.U32();
            std::vector<int32_t> targets;
            Reader list = data;
            list.pos = offset;
            for ( uint16_t i = 0; i < count && list.ok; ++i ) {
                targets.push_back(list.I32());
            }
            keywords.emplace_back(keyword, targets);
            return entry.ok;
        });
    }

    int HlpBook::TopicForOffset(int32_t offset) const
    {
        if ( offset < 0 || offsetAnchors.empty() ) {
            return -1;
        }

        auto it = std::upper_bound(offsetAnchors.begin(), offsetAnchors.end(), std::make_pair(offset, INT32_MAX));
        if ( it == offsetAnchors.begin() ) {
            return -1;
        }
        --it;
        return it->first / 0x8000 == offset / 0x8000 || it->first == offset ? it->second : -1;
    }

    int HlpBook::TopicForHash(int32_t hash) const
    {
        auto it = contexts.find(hash);
        return it != contexts.end() ? TopicForOffset(it->second) : -1;
    }

    int HlpBook::TopicForLink(const Link& link) const
    {
        switch ( link.kind )
        {
            case Link::HASH:   return TopicForHash(link.value);
            case Link::OFFSET: return TopicForOffset(link.value);
            case Link::MACRO:
            {
                // JumpId/JI and PopupId/PI to a context of this file: JumpId(`file', `context')
                wxString macro(link.macro);
                wxString name = macro.BeforeFirst('(').Trim().Trim(false).Lower();
                if ( name != "jumpid" && name != "ji" && name != "popupid" && name != "pi" ) {
                    return -1;
                }

                wxArrayString args = wxStringTokenize(macro.AfterFirst('(').BeforeLast(')'), ",");
                if ( args.size() < 2 ) {
                    return -1;
                }

                wxString context = args[1];
                context.Replace("`", "");
                context.Replace("'", "");
                context.Replace("\"", "");
                context.Trim().Trim(false);
                return TopicForHash(ContextHash(std::string(context.mb_str(wxConvISO8859_1))));
            }
        }
        return -1;
    }

    // Converts bottom-up device independent bitmap pixels (rows padded to 4 bytes) to an image
    void DibToImage(const uint8_t* pPixels, uint32_t width, uint32_t height, uint16_t bitCount, const std::vector<Rgb>& palette, wxImage& image)
    {
        const size_t stride = ((size_t)width * bitCount + 31) / 32 * 4;
        image.Create((int)width, (int)height, false);
        unsigned char* pRgb = image.GetData();
        for ( uint32_t y = 0; y < height; ++y )
        {
            const uint8_t* pRow = pPixels + (size_t)(height - 1 - y) * stride;
            for ( uint32_t x = 0; x < width; ++x )
            {
                Rgb c;
                switch ( bitCount )
                {
                    case 1: { size_t i = (pRow[x / 8] >> (7 - x % 8)) & 1; c = i < palette.size() ? palette[i] : Rgb(); break; }
                    case 4: { size_t i = (x % 2 ? pRow[x / 2] & 0x0F : pRow[x / 2] >> 4); c = i < palette.size() ? palette[i] : Rgb(); break; }
                    case 8: { size_t i = pRow[x]; c = i < palette.size() ? palette[i] : Rgb(); break; }
                    case 24: c = { pRow[x * 3 + 2], pRow[x * 3 + 1], pRow[x * 3], true }; break;
                    default: c = { pRow[x * 4 + 2], pRow[x * 4 + 1], pRow[x * 4], true }; break;
                }
                unsigned char* pOut = pRgb + ((size_t)y * width + x) * 3;
                pOut[0] = c.r;
                pOut[1] = c.g;
                pOut[2] = c.b;
            }
        }
    }

    // BI_RLE8 compressed bitmap rows (bottom-up), escape 0: end of line, 1: end of bitmap, 2: delta, else literal bytes
    Bytes Rle8DibDecompress(const uint8_t* pIn, size_t inSize, size_t stride, size_t height)
    {
        Bytes out(stride * height, 0);
        size_t x = 0, y = 0, pos = 0;
        while ( pos + 1 < inSize && y < height )
        {
            uint8_t count = pIn[pos++];
            uint8_t value = pIn[pos++];
            if ( count )
            {
                for ( uint8_t i = 0; i < count && x < stride; ++i ) {
                    out[y * stride + x++] = value;
                }
            }
            else if ( value == 0 )
            {
                x = 0;
                ++y;
            }
            else if ( value == 1 ) {
                break;
            }
            else if ( value == 2 )
            {
                if ( pos + 1 >= inSize ) {
                    break;
                }
                x += pIn[pos++];
                y += pIn[pos++];
            }
            else
            {
                for ( uint8_t i = 0; i < value && pos < inSize; ++i, ++pos )
                {
                    if ( x < stride ) {
                        out[y * stride + x++] = pIn[pos];
                    }
                }
                pos += value & 1; // Runs are padded to words
            }
        }
        return out;
    }

    // Windows metafile: only the bitmaps it draws are shown, which covers the pictures that are a bitmap
    // wrapped in a metafile. The largest bitmap is taken.
    bool MetafileToImage(const Bytes& wmf, wxImage& image)
    {
        Reader header(wmf);
        header.U16(); // type
        uint16_t headerWords = header.U16();
        size_t pos = (size_t)headerWords * 2;
        size_t bestPixels = 0;
        while ( pos + 6 <= wmf.size() )
        {
            Reader rec(wmf.data() + pos, wmf.size() - pos);
            uint32_t words    = rec.U32();
            uint16_t function = rec.U16();
            if ( words < 3 || pos + (size_t)words * 2 > wmf.size() || function == 0 ) {
                break;
            }

            // Bitmap records: parameters, then BITMAPINFO and the bits
            size_t params = 0;
            switch ( function )
            {
                case 0x0F43: params = 4 + 9 * 2; break;  // StretchDIBits
                case 0x0B41: params = 4 + 8 * 2; break;  // DIBStretchBlt
                case 0x0940: params = 4 + 6 * 2; break;  // DIBBitBlt
                case 0x0D33: params = 9 * 2; break;      // SetDIBitsToDevice
                default: break;
            }

            if ( params && (size_t)words * 2 > 6 + params + 40 )
            {
                Reader dib(wmf.data() + pos + 6 + params, (size_t)words * 2 - 6 - params);
                uint32_t infoSize = dib.U32();
                int32_t width     = dib.I32();
                int32_t height    = dib.I32();
                dib.U16(); // planes
                uint16_t bitCount    = dib.U16();
                uint32_t compression = dib.U32();
                dib.pos += 12; // image size, resolution
                uint32_t clrUsed = dib.U32();
                dib.pos = infoSize;

                std::vector<Rgb> palette;
                if ( bitCount <= 8 )
                {
                    uint32_t numColors = clrUsed ? clrUsed : (1u << bitCount);
                    for ( uint32_t i = 0; i < numColors; ++i )
                    {
                        uint8_t b = dib.U8(), g = dib.U8(), r = dib.U8();
                        dib.U8();
                        palette.push_back({ r, g, b, true });
                    }
                }

                const size_t stride = width > 0 ? ((size_t)width * bitCount + 31) / 32 * 4 : 0;
                const bool bSupported = (compression == 0 || (compression == 1 && bitCount == 8)) && width > 0 && height > 0 && width <= 8192 && height <= 8192
                    && (bitCount == 1 || bitCount == 4 || bitCount == 8 || bitCount == 24 || bitCount == 32);
                if ( dib.ok && bSupported && (size_t)width * height > bestPixels )
                {
                    Bytes pixels;
                    if ( compression == 1 ) {
                        pixels = Rle8DibDecompress(dib.pData + dib.pos, dib.Remaining(), stride, (size_t)height);
                    }
                    else if ( dib.Remaining() >= stride * (size_t)height ) {
                        pixels.assign(dib.pData + dib.pos, dib.pData + dib.pos + stride * (size_t)height);
                    }

                    if ( !pixels.empty() )
                    {
                        DibToImage(pixels.data(), (uint32_t)width, (uint32_t)height, bitCount, palette, image);
                        bestPixels = (size_t)width * height;
                    }
                }
            }

            pos += (size_t)words * 2;
        }
        return bestPixels != 0;
    }

    // Pictures: WinHelp graphics stream with one or more alternatives (DIB, DDB, metafile)
    bool HlpBook::DecodeGraphicsStream(const uint8_t* pData, size_t size, wxImage& image, int& displayWidth, int& displayHeight)
    {
        Reader stream(pData, size);
        uint16_t magic = stream.U16();
        if ( magic != 0x506C && magic != 0x706C ) {
            return false;
        }

        uint16_t numPics = stream.U16();
        for ( uint16_t pic = 0; pic < numPics; ++pic )
        {
            Reader offsets(pData, size);
            offsets.pos = 4 + pic * 4u;
            uint32_t offset = offsets.U32();
            if ( !offsets.ok || offset + 2 > size ) {
                continue;
            }

            Reader rec(pData + offset, size - offset);
            uint8_t type    = rec.U8();
            uint8_t packing = rec.U8();
            if ( type == 8 )
            {
                // Metafile: mapping mode, logical size, sizes and offset of the (packed) metafile
                rec.CompressedUShort();
                rec.U16();
                rec.U16();
                uint32_t decompressedSize = rec.CompressedULong();
                uint32_t compressedSize   = rec.CompressedULong();
                rec.CompressedULong(); // hotspot size
                uint32_t dataOffset = rec.U32();
                if ( !rec.ok || decompressedSize > (64u << 20) || dataOffset + (size_t)compressedSize > rec.size ) {
                    continue;
                }

                const uint8_t* pPacked = rec.pData + dataOffset;
                Bytes wmf;
                switch ( packing )
                {
                    case 0: wmf.assign(pPacked, pPacked + compressedSize); break;
                    case 1: wmf = RleDecompress(Bytes(pPacked, pPacked + compressedSize), decompressedSize); break;
                    case 2: wmf = Lz77Decompress(pPacked, compressedSize, decompressedSize); break;
                    case 3: wmf = RleDecompress(Lz77Decompress(pPacked, compressedSize, (size_t)decompressedSize * 4 + 1024), decompressedSize); break;
                    default: continue;
                }

                if ( MetafileToImage(wmf, image) )
                {
                    displayWidth  = image.GetWidth();
                    displayHeight = image.GetHeight();
                    return true;
                }
                continue;
            }

            if ( type != 5 && type != 6 ) {
                continue;
            }

            uint32_t xRes     = rec.CompressedULong();
            uint32_t yRes     = rec.CompressedULong();
            uint16_t planes   = rec.CompressedUShort();
            uint16_t bitCount = rec.CompressedUShort();
            uint32_t width    = rec.CompressedULong();
            uint32_t height   = rec.CompressedULong();
            uint32_t clrUsed  = rec.CompressedULong();
            rec.CompressedULong(); // colors important
            uint32_t compressedSize = rec.CompressedULong();
            rec.CompressedULong(); // hotspot size
            uint32_t bitsOffset = rec.U32();
            rec.U32(); // hotspot offset
            if ( !rec.ok || planes != 1 || !width || !height || width > 8192 || height > 8192
                || (bitCount != 1 && bitCount != 4 && bitCount != 8 && bitCount != 24 && bitCount != 32) )
            {
                continue;
            }

            std::vector<Rgb> palette;
            if ( type == 6 && bitCount <= 8 )
            {
                uint32_t numColors = clrUsed ? clrUsed : (1u << bitCount);
                for ( uint32_t i = 0; i < numColors; ++i )
                {
                    uint8_t b = rec.U8(), g = rec.U8(), r = rec.U8();
                    rec.U8();
                    palette.push_back({ r, g, b, true });
                }
            }
            else if ( bitCount == 1 ) {
                palette = { { 0, 0, 0, true }, { 255, 255, 255, true } };
            }

            if ( !rec.ok || bitsOffset + (size_t)compressedSize > rec.size ) {
                continue;
            }

            const size_t stride = ((size_t)width * bitCount + 31) / 32 * 4;
            const size_t expected = stride * height;
            const uint8_t* pBits = rec.pData + bitsOffset;
            Bytes pixels;
            switch ( packing )
            {
                case 0: pixels.assign(pBits, pBits + std::min<size_t>(compressedSize, expected)); break;
                case 1: pixels = RleDecompress(Bytes(pBits, pBits + compressedSize), expected); break;
                case 2: pixels = Lz77Decompress(pBits, compressedSize, expected); break;
                case 3: pixels = RleDecompress(Lz77Decompress(pBits, compressedSize, expected * 4 + 1024), expected); break;
                default: continue;
            }
            pixels.resize(expected);

            DibToImage(pixels.data(), width, height, bitCount, palette, image);

            // The resolution is the authored pixels per inch, shown at 96 dpi
            displayWidth  = xRes ? (int)((uint64_t)width * 96 / xRes) : (int)width;
            displayHeight = yRes ? (int)((uint64_t)height * 96 / yRes) : (int)height;
            return true;
        }
        return false;
    }

    // Returns the book file name of a picture of a compact graphics record, or an empty string
    std::string HlpBook::DecodePicture(const Bytes& payload, uint8_t type)
    {
        if ( (type != 0x03 && type != 0x22) || payload.size() < 4 ) {
            return std::string();
        }

        const uint16_t selector = (uint16_t)(payload[0] | (payload[1] << 8));
        std::string key;
        Reader source;
        if ( selector == 0 )
        {
            // Picture file |bm<n>
            int16_t index = (int16_t)(payload[2] | (payload[3] << 8));
            key = "|bm" + std::to_string(index);
            source = container.Get(key);
        }
        else
        {
            key = "embedded" + std::to_string(pictureFiles.size());
            source = Reader(payload.data() + 2, payload.size() - 2);
        }

        auto it = pictureFiles.find(key);
        if ( it != pictureFiles.end() ) {
            return it->second;
        }

        wxImage image;
        int width = 0, height = 0;
        std::string file;
        if ( source.ok && DecodeGraphicsStream(source.pData, source.size, image, width, height) )
        {
            wxMemoryOutputStream out;
            if ( image.SaveFile(out, wxBITMAP_TYPE_PNG) )
            {
                file = "pic" + std::to_string(pictureFiles.size()) + ".png";
                wxMemoryFSHandler::AddFileWithMimeType(bookDir + "/" + file, out.GetOutputStreamBuffer()->GetBufferStart(), out.GetSize(), "image/png");
                pictureSizes[file] = { width, height };
            }
        }

        pictureFiles[key] = file;
        return file;
    }

    // Paragraphs of one display record or table cell: a paragraph format followed by its character stream.
    // Commands 0x82 end a paragraph that's followed by one with the same format, 0xFF ends the record.
    bool HlpBook::DecodeParagraphs(Reader& ld1, Reader& strings, int column, uint16_t& font, std::vector<Link>& links, Block& block)
    {
        ParagraphFormat format;
        ld1.CompressedLong();
        ld1.U16(); // id
        uint16_t bits = ld1.U16();
        if ( bits & 0x0001 ) { ld1.CompressedLong(); }
        if ( bits & 0x0002 ) { format.spaceAbove = ld1.CompressedShort(); }
        if ( bits & 0x0004 ) { format.spaceBelow = ld1.CompressedShort(); }
        if ( bits & 0x0008 ) { ld1.CompressedShort(); } // line spacing
        if ( bits & 0x0010 ) { format.leftIndent = ld1.CompressedShort(); }
        if ( bits & 0x0020 ) { format.rightIndent = ld1.CompressedShort(); }
        if ( bits & 0x0040 ) { format.firstIndent = ld1.CompressedShort(); }
        if ( bits & 0x0080 ) { ld1.CompressedShort(); } // default tab interval
        if ( bits & 0x0100 ) { ld1.U8(); ld1.U16(); } // border
        if ( bits & 0x0200 )
        {
            int16_t numTabs = ld1.CompressedShort();
            for ( int16_t i = 0; i < numTabs && ld1.ok; ++i )
            {
                uint16_t tab = ld1.CompressedUShort();
                if ( tab & 0x4000 ) {
                    ld1.CompressedUShort(); // alignment
                }
                format.tabs.push_back(tab & 0x3FFF);
            }
        }
        format.align = ((bits >> 10) & 3) == 2 ? 2 : ((bits >> 10) & 3) ? 1 : 0;
        if ( !ld1.ok ) {
            return false;
        }

        Paragraph para;
        para.format = format;
        para.column = column;
        int link    = -1;
        for ( ;; )
        {
            if ( strings.Remaining() )
            {
                std::string text = strings.CString();
                if ( !text.empty() )
                {
                    Inline run;
                    run.text = text;
                    run.font = font;
                    run.link = link;
                    para.inlines.push_back(run);
                }
            }

            if ( !ld1.Remaining() ) {
                break;
            }

            uint8_t cmd = ld1.U8();
            if ( cmd == 0xFF ) {
                break;
            }

            switch ( cmd )
            {
                case 0x80: font = ld1.U16(); break;
                case 0x81: para.inlines.push_back({ Inline::LINEBREAK }); break;
                case 0x82:
                    block.paragraphs.push_back(para);
                    para.inlines.clear();
                    break;
                case 0x83: para.inlines.push_back({ Inline::TAB }); break;
                case 0x85: ld1.I16(); break;
                case 0x86: case 0x87: case 0x88:
                {
                    // Nested compact record, a picture or an embedded window
                    uint8_t type = ld1.U8();
                    int32_t size = ld1.CompressedLong();
                    if ( type > 0x10 ) {
                        ld1.CompressedUShort(); // hotspots
                    }
                    Bytes payload = ld1.Read(size > 0 ? (size_t)size : 0);
                    std::string file = DecodePicture(payload, type);
                    if ( !file.empty() )
                    {
                        Inline pic;
                        pic.kind         = Inline::PICTURE;
                        pic.picture      = file;
                        pic.pictureAlign = cmd - 0x86;
                        pic.link         = link;
                        para.inlines.push_back(pic);
                    }
                    break;
                }
                case 0x89: link = -1; break;
                case 0xC8: case 0xCC:
                {
                    uint16_t len = ld1.U16();
                    Bytes macro  = ld1.Read(len);
                    Link l;
                    l.kind  = Link::MACRO;
                    l.macro = std::string(macro.begin(), std::find(macro.begin(), macro.end(), 0));
                    links.push_back(l);
                    link = (int)links.size() - 1;
                    break;
                }
                case 0xE0: case 0xE1:
                    links.push_back({ Link::OFFSET, ld1.I32(), std::string(), cmd == 0xE0 });
                    link = (int)links.size() - 1;
                    break;
                case 0xE2: case 0xE3: case 0xE6: case 0xE7:
                    links.push_back({ Link::HASH, ld1.I32(), std::string(), (cmd & 1) == 0 });
                    link = (int)links.size() - 1;
                    break;
                case 0xEA: case 0xEB: case 0xEE: case 0xEF:
                {
                    // Jump to another file or window: type, TOPICOFFSET, window number or names
                    int16_t len = ld1.I16();
                    Reader body = ld1.Sub(len > 0 ? (size_t)len : 0);
                    uint8_t type   = body.U8();
                    int32_t offset = body.I32();
                    // Only jumps within this file are followed, other files and windows are left as text
                    if ( type == 0 || type == 1 )
                    {
                        links.push_back({ Link::OFFSET, offset, std::string(), (cmd & 1) == 0 });
                        link = (int)links.size() - 1;
                    }
                    else {
                        link = -1;
                    }
                    break;
                }
                default:
                    if ( (cmd & 0xD8) == 0xC0 ) {
                        ld1.Read(4);
                    }
                    else if ( (cmd & 0xD8) == 0xC8 ) {
                        ld1.Read(ld1.U16());
                    }
                    else {
                        ld1.ok = false; // Unknown command, the rest of the record can't be decoded
                    }
                    link = -1;
                    break;
            }

            if ( !ld1.ok ) {
                break;
            }
        }

        block.paragraphs.push_back(para);
        return ld1.ok;
    }

    // Table cells: a column number and a compact record per cell, the list ends with column -1
    bool HlpBook::DecodeTableCells(Reader& ld1, Reader& strings, uint16_t& font, std::vector<Link>& links, Block& block, int depth)
    {
        for ( ;; )
        {
            int16_t column = ld1.I16();
            if ( !ld1.ok || column == -1 ) {
                break;
            }

            uint8_t type = ld1.U8();
            int32_t size = (type == 0x02 || type == 0x21) ? (int32_t)ld1.U32() : ld1.CompressedLong();
            if ( type == 0x21 ) {
                ld1.U16();
            }
            else if ( type > 0x10 && type != 0x21 ) {
                ld1.CompressedUShort();
            }

            Reader cell = ld1.Sub(size > 0 ? (size_t)size : 0);
            if ( !ld1.ok ) {
                return false;
            }

            if ( type == 0x20 || type == 0x01 ) {
                DecodeParagraphs(cell, strings, column, font, links, block);
            }
            else if ( type == 0x22 || type == 0x03 )
            {
                Paragraph para;
                para.column = column;
                std::string file = DecodePicture(Bytes(cell.pData, cell.pData + cell.size), type);
                if ( !file.empty() ) {
                    para.inlines.push_back({ Inline::PICTURE, std::string(), 0, -1, file, 0 });
                }
                block.paragraphs.push_back(para);
            }
            else if ( (type == 0x23 || type == 0x04) && depth < 4 )
            {
                // Nested tables are flattened into the cell
                cell.U8();
                uint8_t tableType = cell.U8();
                if ( tableType == 0 ) {
                    cell.U16();
                }
                DecodeTableCells(cell, strings, font, links, block, depth + 1);
            }
        }
        return ld1.ok;
    }

    void HlpBook::DecodeRecord(const Record& record, uint16_t& font, std::vector<Link>& links, std::vector<Block>& blocks)
    {
        Reader ld1(record.ld1);
        Reader strings(record.ld2);
        Block block;

        if ( record.type == 0x22 )
        {
            // Standalone picture
            int32_t size = ld1.CompressedLong();
            ld1.CompressedUShort();
            Bytes payload = ld1.Read(size > 0 ? (size_t)size : 0);
            std::string file = DecodePicture(payload, record.type);
            if ( !file.empty() )
            {
                Paragraph para;
                para.inlines.push_back({ Inline::PICTURE, std::string(), 0, -1, file, 0 });
                block.paragraphs.push_back(para);
                blocks.push_back(block);
            }
            return;
        }

        ld1.CompressedLong(); // topic size
        ld1.CompressedUShort(); // topic length
        if ( record.type == 0x23 )
        {
            block.bTable = true;
            uint8_t numColumns = ld1.U8();
            uint8_t tableType  = ld1.U8();
            if ( tableType == 0 ) {
                ld1.U16(); // minimum table width
            }
            for ( uint8_t i = 0; i < numColumns; ++i )
            {
                int width = ld1.I16();
                ld1.I16(); // gap
                block.columnWidths.push_back(width);
            }
            DecodeTableCells(ld1, strings, font, links, block, 0);
        }
        else {
            DecodeParagraphs(ld1, strings, -1, font, links, block);
        }

        blocks.push_back(std::move(block));
    }

    // Symbol font characters as Unicode
    wxUniChar SymbolChar(unsigned char c)
    {
        switch ( c )
        {
            case 0xB7: return 0x2022; // bullet
            case 0xA7: return 0x2663;
            case 0xA8: return 0x2666;
            case 0xA9: return 0x2665;
            case 0xAA: return 0x2660;
            case 0xAB: return 0x2194;
            case 0xAC: return 0x2190;
            case 0xAD: return 0x2191;
            case 0xAE: return 0x2192;
            case 0xAF: return 0x2193;
            case 0xB0: return 0x00B0;
            case 0xB1: return 0x00B1;
            case 0xB4: return 0x00D7;
            case 0xB8: return 0x00F7;
            case 0xB9: return 0x2260;
            case 0xBB: return 0x2248;
            case 0xBC: return 0x2026;
            case 0xD7: return 0x22C5;
            case 0xE0: return 0x25CA;
            case 0xD2: case 0xE2: return 0x00AE;
            case 0xD3: case 0xE3: return 0x00A9;
            case 0xD4: case 0xE4: return 0x2122;
            default: break;
        }
        return c < 0x80 ? wxUniChar((unsigned int)c) : wxUniChar(0x2022u);
    }

    wxString HlpBook::TextHtml(const std::string& text, bool bSymbol) const
    {
        static wxCSConv cp1252(wxFONTENCODING_CP1252);
        wxString out;
        for ( unsigned char c : text )
        {
            wxUniChar u = bSymbol ? SymbolChar(c) : wxUniChar((unsigned int)c);
            if ( !bSymbol && c >= 0x80 )
            {
                char ch = (char)c;
                wxString decoded(&ch, cp1252, 1);
                u = decoded.empty() ? wxUniChar('?') : decoded[0];
            }

            switch ( (wxChar)u.GetValue() )
            {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                case 0xA0: out += "&nbsp;"; break;
                default:
                    if ( u.GetValue() < 0x80 ) {
                        out += u;
                    }
                    else {
                        out += wxString::Format("&#%u;", (unsigned)u.GetValue());
                    }
                    break;
            }
        }
        return out;
    }

    wxString ColorHtml(const Rgb& c)
    {
        return wxString::Format("#%02X%02X%02X", c.r, c.g, c.b);
    }

    // HTML font size of half points: 7pt and smaller 1, 8-10pt 2, ...
    int HtmlFontSize(int halfPoints)
    {
        const int points = halfPoints / 2;
        return points <= 7 ? 1 : points <= 10 ? 2 : points <= 12 ? 3 : points <= 14 ? 4 : points <= 18 ? 5 : points <= 24 ? 6 : 7;
    }

    // WinHelp paragraph units are 1/144 inch (10 twips), pixels at 96 dpi
    int UnitsToPixels(int units)
    {
        return units * 2 / 3;
    }

    wxString HlpBook::RunsHtml(const std::vector<Inline>& inlines, size_t begin, size_t end, const std::vector<Link>& links)
    {
        wxString html;
        int openLink = -1;
        auto setLink = [&](int link) {
            if ( link == openLink ) {
                return;
            }
            if ( openLink >= 0 ) {
                html += "</a>";
            }
            openLink = -1;
            const int topic = link >= 0 && link < (int)links.size() ? TopicForLink(links[link]) : -1;
            if ( topic >= 0 )
            {
                html += wxString::Format("<a href=\"t%d.htm\">", topic);
                openLink = link;
            }
        };

        for ( size_t i = begin; i < end; ++i )
        {
            const Inline& in = inlines[i];
            switch ( in.kind )
            {
                case Inline::TEXT:
                {
                    setLink(in.link);
                    const Font& f = in.font < fonts.size() ? fonts[in.font] : fonts[0];
                    wxString text = TextHtml(in.text, f.bSymbol);
                    if ( f.bBold ) { text = "<b>" + text + "</b>"; }
                    if ( f.bItalic ) { text = "<i>" + text + "</i>"; }
                    if ( f.bUnderline ) { text = "<u>" + text + "</u>"; }
                    if ( f.bStrike ) { text = "<s>" + text + "</s>"; }
                    wxString attrs = wxString::Format(" size=\"%d\"", HtmlFontSize(f.halfPoints));
                    if ( !f.bSymbol && !f.face.empty() ) {
                        attrs += " face=\"" + TextHtml(f.face, false) + "\"";
                    }
                    if ( f.color.bSet && openLink < 0 ) {
                        attrs += " color=\"" + ColorHtml(f.color) + "\"";
                    }
                    html += "<font" + attrs + ">" + text + "</font>";
                    break;
                }
                case Inline::LINEBREAK:
                    html += "<br>";
                    break;
                case Inline::TAB:
                    html += "&nbsp;&nbsp;&nbsp;&nbsp;";
                    break;
                case Inline::PICTURE:
                {
                    setLink(in.link);
                    const auto size = pictureSizes[in.picture];
                    html += wxString::Format("<img src=\"%s\" width=\"%d\" height=\"%d\"%s>", in.picture, size.first, size.second,
                        in.pictureAlign == 1 ? " align=\"left\"" : in.pictureAlign == 2 ? " align=\"right\"" : "");
                    break;
                }
            }
        }

        setLink(-1);
        return html;
    }

    wxString HlpBook::ParagraphHtml(const Paragraph& para, const std::vector<Link>& links)
    {
        // Like in WinHelp, an empty paragraph is a blank line, except in table cells where it takes no space
        // (records end with an empty paragraph)
        if ( para.column >= 0 && para.inlines.empty() ) {
            return wxString();
        }

        const ParagraphFormat& fmt = para.format;
        const std::vector<Inline>& inlines = para.inlines;
        const int left  = std::max(0, UnitsToPixels(fmt.leftIndent));
        const int first = UnitsToPixels(fmt.firstIndent);

        bool bEmpty = true;
        for ( const Inline& in : inlines ) {
            bEmpty = bEmpty && in.kind != Inline::TEXT && in.kind != Inline::PICTURE;
        }

        wxString content;
        if ( first < 0 && left > 0 )
        {
            // Hanging indent, e.g. a bulleted list: the text before the first tab hangs left of the indent
            size_t tab = 0;
            while ( tab < inlines.size() && inlines[tab].kind != Inline::TAB ) {
                ++tab;
            }

            const int hangStart = std::max(0, left + first);
            if ( tab < inlines.size() )
            {
                content = wxString::Format("<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\"><tr>%s<td width=\"%d\" valign=\"top\" nowrap>%s</td><td valign=\"top\">%s</td></tr></table>",
                    hangStart ? wxString::Format("<td width=\"%d\"></td>", hangStart) : wxString(), left - hangStart,
                    RunsHtml(inlines, 0, tab, links), RunsHtml(inlines, tab + 1, inlines.size(), links));
            }
            else
            {
                content = wxString::Format("<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\"><tr><td width=\"%d\"></td><td valign=\"top\">%s</td></tr></table>",
                    hangStart, RunsHtml(inlines, 0, inlines.size(), links));
            }
        }
        else if ( left > 0 )
        {
            content = wxString::Format("<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\"><tr><td width=\"%d\"></td><td valign=\"top\">%s</td></tr></table>",
                left, RunsHtml(inlines, 0, inlines.size(), links));
        }
        else {
            content = RunsHtml(inlines, 0, inlines.size(), links);
        }

        if ( bEmpty ) {
            content = wxString::Format("<font size=\"%d\">&nbsp;</font>", HtmlFontSize(fonts[0].halfPoints));
        }

        static const char* const aAlign[] = { "left", "right", "center" };
        wxString html;
        // wxHTML has no paragraph margins, the space above and below is a transparent picture in a table
        // (in a paragraph of its own it would take at least a line)
        auto spacer = [](int units) {
            return wxString::Format("<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\"><tr><td><img src=\"spacer.png\" width=\"1\" height=\"%d\"></td></tr></table>", UnitsToPixels(units));
        };

        if ( fmt.spaceAbove > 0 ) {
            html += spacer(fmt.spaceAbove);
        }
        html += wxString::Format("<div align=\"%s\">%s</div>", aAlign[fmt.align], content);
        if ( fmt.spaceBelow > 0 ) {
            html += spacer(fmt.spaceBelow);
        }
        return html;
    }

    // Each table row is a record of its own, consecutive rows with the same columns are one HTML table
    std::vector<Block> MergeTables(std::vector<Block> blocks)
    {
        std::vector<Block> merged;
        for ( Block& block : blocks )
        {
            if ( block.bTable && !merged.empty() && merged.back().bTable && merged.back().columnWidths == block.columnWidths )
            {
                // A new row starts when the column number doesn't increase
                merged.back().paragraphs.insert(merged.back().paragraphs.end(), block.paragraphs.begin(), block.paragraphs.end());
                continue;
            }
            merged.push_back(std::move(block));
        }
        return merged;
    }

    wxString HlpBook::BlockHtml(const Block& block, const std::vector<Link>& links)
    {
        wxString html;
        if ( !block.bTable )
        {
            for ( const Paragraph& para : block.paragraphs ) {
                html += ParagraphHtml(para, links);
            }
            return html;
        }

        // Table rows start when the column number doesn't increase
        html += "<table border=\"0\" cellspacing=\"0\" cellpadding=\"2\"><tr>";
        int lastColumn = -1;
        int cellColumn = -1;
        wxString cell;
        auto flushCell = [&]() {
            if ( cellColumn >= 0 )
            {
                int width = cellColumn < (int)block.columnWidths.size() ? UnitsToPixels(block.columnWidths[cellColumn]) : 0;
                html += wxString::Format("<td valign=\"top\"%s>%s</td>", width > 0 ? wxString::Format(" width=\"%d\"", width) : wxString(), cell);
            }
            cell.clear();
        };

        for ( const Paragraph& para : block.paragraphs )
        {
            if ( para.column != cellColumn )
            {
                flushCell();
                if ( para.column <= lastColumn ) {
                    html += "</tr><tr>";
                }
                lastColumn = cellColumn = para.column;
            }
            cell += ParagraphHtml(para, links);
        }

        flushCell();
        html += "</tr></table>";
        return html;
    }

    wxString HlpBook::TopicHtml(size_t topicIdx)
    {
        const Topic& topic = topics[topicIdx];
        std::vector<Link> links;
        uint16_t font = 0; // The font selection carries over from record to record

        std::vector<Block> nonScroll, scroll;
        for ( const Record& rec : topic.records )
        {
            const bool bNonScroll = topic.nonScrollStart >= 12 && rec.pos >= topic.nonScrollStart && (topic.scrollStart < 12 || rec.pos < topic.scrollStart);
            DecodeRecord(rec, font, links, bNonScroll ? nonScroll : scroll);
        }

        wxString body;
        if ( !nonScroll.empty() )
        {
            wxString region;
            for ( const Block& block : MergeTables(nonScroll) ) {
                region += BlockHtml(block, links);
            }
            body += wxString::Format("<table width=\"100%%\" border=\"0\" cellspacing=\"0\" cellpadding=\"6\"%s><tr><td>%s</td></tr></table>",
                nonScrollColor.bSet ? " bgcolor=\"" + ColorHtml(nonScrollColor) + "\"" : wxString(), region);
        }

        for ( const Block& block : MergeTables(scroll) ) {
            body += BlockHtml(block, links);
        }

        return wxString::Format(
            "<html><head><title>%s</title></head>\n<body%s link=\"#008000\">\n%s\n</body></html>\n",
            TextHtml(topic.title, false),
            scrollColor.bSet ? " bgcolor=\"" + ColorHtml(scrollColor) + "\"" : wxString(),
            body);
    }

    wxString HlpBook::Write(const wxString& dir, const wxString& cntPath)
    {
        bookDir = dir;
        auto addText = [&](const wxString& name, const wxString& text) {
            const wxScopedCharBuffer utf8 = text.utf8_str();
            wxMemoryFSHandler::AddFileWithMimeType(dir + "/" + name, utf8.data(), utf8.length(), name.EndsWith(".htm") ? "text/html" : "text/plain");
        };

        for ( size_t i = 0; i < topics.size(); ++i ) {
            addText(wxString::Format("t%zu.htm", i), TopicHtml(i));
        }

        // Transparent picture of paragraph spacing
        {
            wxImage spacer(1, 1);
            spacer.SetAlpha();
            spacer.SetAlpha(0, 0, 0);
            wxMemoryOutputStream out;
            spacer.SaveFile(out, wxBITMAP_TYPE_PNG);
            wxMemoryFSHandler::AddFileWithMimeType(dir + "/spacer.png", out.GetOutputStreamBuffer()->GetBufferStart(), out.GetSize(), "image/png");
        }

        // Contents: the .cnt file next to the help file, else the topics in order
        auto item = [&](const wxString& name, int topic) {
            wxString html = "<li><object type=\"text/sitemap\"><param name=\"Name\" value=\"" + name + "\">";
            if ( topic >= 0 ) {
                html += wxString::Format("<param name=\"Local\" value=\"t%d.htm\">", topic);
            }
            return html + "</object>\n";
        };

        // A book entry opens the first topic in it
        struct ContentsEntry
        {
            int level;
            wxString name;
            int topic;
        };
        std::vector<ContentsEntry> entries;

        wxTextFile cnt;
        if ( !cntPath.empty() && wxFileName::FileExists(cntPath) && cnt.Open(cntPath, wxCSConv(wxFONTENCODING_CP1252)) )
        {
            // Lines: "<level> <book title>" and "<level> <title>=<context>[@file][>window]"
            for ( wxString line = cnt.GetFirstLine(); !cnt.Eof(); line = cnt.GetNextLine() )
            {
                line.Trim();
                long level = 0;
                if ( line.empty() || line[0] == ':' || !line.BeforeFirst(' ').ToLong(&level) || level < 1 ) {
                    continue;
                }

                wxString entry = line.AfterFirst(' ');
                int topic      = -1;
                if ( entry.Contains("=") )
                {
                    wxString context = entry.AfterFirst('=').BeforeFirst('>');
                    if ( !context.Contains("@") ) {
                        topic = TopicForHash(ContextHash(std::string(context.Trim().Trim(false).mb_str(wxConvISO8859_1))));
                    }
                }
                entries.push_back({ (int)level, TextHtml(std::string(entry.BeforeFirst('=').mb_str(wxCSConv(wxFONTENCODING_CP1252))), false), topic });
            }
        }

        const bool bContentsFile = !entries.empty();
        if ( !bContentsFile )
        {
            for ( size_t i = 0; i < topics.size(); ++i )
            {
                if ( !topics[i].title.empty() ) {
                    entries.push_back({ 1, TextHtml(topics[i].title, false), (int)i });
                }
            }
        }

        for ( size_t i = entries.size(); i-- > 0; )
        {
            if ( entries[i].topic < 0 && i + 1 < entries.size() && entries[i + 1].level > entries[i].level ) {
                entries[i].topic = entries[i + 1].topic;
            }
        }

        wxString hhc = "<html><body>\n<ul>\n";
        int curLevel = 1;
        for ( const ContentsEntry& entry : entries )
        {
            for ( ; curLevel < entry.level; ++curLevel ) {
                hhc += "<ul>\n";
            }
            for ( ; curLevel > entry.level; --curLevel ) {
                hhc += "</ul>\n";
            }
            hhc += item(entry.name, entry.topic);
        }
        for ( ; curLevel > 1; --curLevel ) {
            hhc += "</ul>\n";
        }
        hhc += "</ul>\n</body></html>\n";
        addText("contents.hhc", hhc);

        wxString hhk = "<html><body>\n<ul>\n";
        for ( const auto& kw : keywords )
        {
            for ( int32_t offset : kw.second )
            {
                const int topic = TopicForOffset(offset);
                if ( topic >= 0 ) {
                    hhk += item(TextHtml(kw.first, false), topic);
                }
            }
        }
        hhk += "</ul>\n</body></html>\n";
        addText("index.hhk", hhk);

        // The first page of the contents file, else the contents topic of the help file
        int defaultTopic = !bContentsFile && TopicForOffset(contentsOffset) >= 0 ? TopicForOffset(contentsOffset) : 0;
        for ( const ContentsEntry& entry : entries )
        {
            if ( bContentsFile && entry.topic >= 0 )
            {
                defaultTopic = entry.topic;
                break;
            }
        }
        addText("book.hhp", wxString::Format(
            "[OPTIONS]\nTitle=%s\nDefault topic=t%d.htm\nContents file=contents.hhc\nIndex file=index.hhk\nCharset=UTF-8\n",
            wxString(title.c_str(), wxCSConv(wxFONTENCODING_CP1252)), defaultTopic));

        return "memory:" + dir + "/book.hhp";
    }
}

namespace JonesWxHlp
{
    wxString LoadBook(const wxString& filePath, wxString* pError)
    {
        static bool bHandler = false;
        if ( !bHandler )
        {
            wxFileSystem::AddHandler(new wxMemoryFSHandler);
            if ( !wxImage::FindHandler(wxBITMAP_TYPE_PNG) ) {
                wxImage::AddHandler(new wxPNGHandler);
            }
            bHandler = true;
        }

        HlpBook book;
        if ( !book.Load(filePath, pError) ) {
            return wxString();
        }

        // Contents file next to the help file, same name with .cnt
        wxFileName cnt(filePath);
        cnt.SetExt("cnt");
        if ( !cnt.FileExists() ) {
            cnt.SetExt("CNT");
        }

        return book.Write("jones3dhlp", cnt.FileExists() ? cnt.GetFullPath() : wxString());
    }
}

#else // !wxUSE_WXHTML_HELP

namespace JonesWxHlp
{
    wxString LoadBook(const wxString& filePath, wxString* pError)
    {
        (void)filePath;
        if ( pError ) {
            *pError = "wxHTML help is not available";
        }
        return wxString();
    }
}

#endif // wxUSE_WXHTML_HELP
