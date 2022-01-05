/*
MIT License

Copyright (c) 2021 lespalt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <windows.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <unordered_map>

#define HRCHECK( x_ ) do{ \
    HRESULT hr_ = x_; \
    if( FAILED(hr_) ) { \
        printf("ERROR: failed call to %s (%s:%d), hr=0x%x\n", #x_, __FILE__, __LINE__,hr_); \
        exit(1); \
    } } while(0)

struct float2
{
    union { float r; float x; };
    union { float g; float y; };
    float2() = default;
    float2( float _x, float _y ) : x(_x), y(_y) {}
    float2( const D2D1_POINT_2F& p ) : x(p.x), y(p.y) {}
    operator D2D1_POINT_2F() const { return {x,y}; }
    float* operator&() { return &x; }
    const float* operator&() const { return &x; }
};

struct float4
{
    union { float r; float x; };
    union { float g; float y; };
    union { float b; float z; };
    union { float a; float w; };
    float4() = default;
    float4( float _x, float _y, float _z, float _w ) : x(_x), y(_y), z(_z), w(_w) {}
    float4( const D2D1_COLOR_F& c ) : r(c.r), g(c.g), b(c.b), a(c.a) {}
    operator D2D1_COLOR_F() const { return {r,g,b,a}; }
    float* operator&() { return &x; }
    const float* operator&() const { return &x; }
};

inline bool loadFile( const std::string& fname, std::string& output )
{
    FILE* fp = fopen( fname.c_str(), "rb" );
    if( !fp )
        return false;

    fseek( fp, 0, SEEK_END );
    const long sz = ftell( fp );
    fseek( fp, 0, SEEK_SET );

    char* buf = new char[sz];

    fread( buf, 1, sz, fp );
    fclose( fp );
    output = std::string( buf, sz );

    delete[] buf;
    return true;
}

inline bool saveFile( const std::string& fname, const std::string& s )
{
    FILE* fp = fopen( fname.c_str(), "wb" );
    if( !fp )
        return false;

    fwrite( s.data(), 1, s.length(), fp );

    fclose( fp );
    return true;
}

inline std::wstring toWide( const std::string& narrow )
{
    return std::wstring(narrow.begin(),narrow.end());
}

class ColumnLayout
{
    public:

        struct Column
        {
            int             id = 0;
            float           textWidth = 0;
            float           borderL = 0;
            float           borderR = 0;
            float           textL = 0;
            float           textR = 0;
            bool            autoWidth = false;
        };

        void reset()
        {
            m_columns.clear();
        }

        // Pass in zero width for auto-scale
        void add( int id, float textWidth, float borderL, float borderR )
        {
            Column clm;
            clm.id = id;
            clm.textWidth = textWidth;
            clm.borderL = borderL;
            clm.borderR = borderR;
            clm.autoWidth = textWidth == 0;
            m_columns.emplace_back( clm );
        }
        void add( int id, float textWidth, float border )
        {
            add( id, textWidth, border, border );
        }

        void layout( float totalWidth )
        {
            int autoWidthCnt = 0;
            float fixedWidth = 0;
            for( const Column& clm : m_columns )
            {
                if( clm.autoWidth )
                {
                    autoWidthCnt++;
                    fixedWidth += clm.borderL + clm.borderR;
                }
                else
                {
                    fixedWidth += clm.textWidth + clm.borderL + clm.borderR;
                }
            }

            const float autoTextWidth = std::max( 0.0f, (totalWidth - fixedWidth) / autoWidthCnt );

            float x = 0;
            for( Column& clm : m_columns )
            {
                if( clm.autoWidth )
                    clm.textWidth = autoTextWidth;

                clm.textL = x + clm.borderL;
                clm.textR = clm.textL + clm.textWidth;

                x = clm.textR + clm.borderR;
            }
        }

        const Column* get( int id ) const
        {
            for( int i=0; i<(int)m_columns.size(); ++i )
            {
                if( m_columns[i].id == id )
                    return &m_columns[i];
            }
            return nullptr;
        }

    private:

        std::vector<Column>     m_columns;
};

//-----------------------------------------------------------------------------
// MurmurHash2, by Austin Appleby

// Note - This code makes a few assumptions about how your machine behaves -

// 1. We can read a 4-byte value from any address without crashing
// 2. sizeof(int) == 4

// And it has a few limitations -

// 1. It will not work incrementally.
// 2. It will not produce the same results on little-endian and big-endian
//    machines.

inline unsigned int MurmurHash2 ( const void * key, int len, unsigned int seed )
{
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.

    const unsigned int m = 0x5bd1e995;
    const int r = 24;

    // Initialize the hash to a 'random' value

    unsigned int h = seed ^ len;

    // Mix 4 bytes at a time into the hash

    const unsigned char * data = (const unsigned char *)key;

    while(len >= 4)
    {
        unsigned int k = *(unsigned int *)data;

        k *= m; 
        k ^= k >> r; 
        k *= m; 

        h *= m; 
        h ^= k;

        data += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array

    switch(len)
    {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
        h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
} 
// End MurmurHash2
//-----------------------------------------------------------------------------

class TextCache
{
    public:

        ~TextCache()
        {
            reset();
        }

        void reset( IDWriteFactory* factory=nullptr )
        {
            for( auto& it : m_cache )
                it.second->Release();

            m_cache.clear();
            m_factory = factory;
        }

        //
        // Render some text, using a cached TextLayout if possible.
        // This works around spending ungodly amount of CPU cycles on ID2D1RenderTarget::DrawText.
        //
        // Assumption: all values stored in 'textFormat' are invariant between calls to this function, except horizontal alignment.
        // Which is why we're including alignment in the hash explicitly, and otherwise just include the text format pointer.
        // This isn't bullet proof, since a user could get the same address again for a newly (re-)created text format. But in our usage
        // patterns, recreating text formats always implies nuking this cache anyway, so don't bother with a more complicated design.
        //
        // Assumption: textFormat is set to DWRITE_PARAGRAPH_ALIGNMENT_CENTER, so ycenter +/- fontSize is enough vertical room in all
        // cases. I.e. we only care about rendering single-line text.
        //
        void render( ID2D1RenderTarget* renderTarget, const wchar_t* str, IDWriteTextFormat* textFormat, float xmin, float xmax, float ycenter, ID2D1SolidColorBrush* brush, DWRITE_TEXT_ALIGNMENT align )
        {
            if( xmax < xmin )
                return;

            const float fontSize = textFormat->GetFontSize();
            const float width = xmax - xmin;

            textFormat->SetTextAlignment( align );

            const int len = (int)wcslen( str );
            unsigned hash = MurmurHash2( str, len*sizeof(wchar_t), 0x12341234 );
            hash ^= (unsigned)(uint64_t(textFormat) & 0xffffffff);
            hash ^= (unsigned)(uint64_t(textFormat) >> 32);
            hash ^= *((unsigned*)&width);
            hash ^= (unsigned)align;

            IDWriteTextLayout* textLayout = nullptr;

            auto it = m_cache.find( hash );

            if( it == m_cache.end() )
            {
                m_factory->CreateTextLayout( str, len, textFormat, width, fontSize*2, &textLayout );
                m_cache.insert( std::make_pair(hash, textLayout) );
            }
            else
            {
                textLayout = it->second;
            }

            const D2D1_RECT_F r = { xmin, ycenter-fontSize, xmax, ycenter+fontSize };
            renderTarget->DrawTextLayout( float2(xmin,ycenter-fontSize), textLayout, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP );
        }

    private:

        std::unordered_map<unsigned int,IDWriteTextLayout*>  m_cache;
        IDWriteFactory*                                      m_factory = nullptr;
};

inline float2 computeTextExtent( const wchar_t* str, IDWriteFactory* factory, IDWriteTextFormat* textFormat )
{
    IDWriteTextLayout* textLayout = nullptr;

    factory->CreateTextLayout( str, (int)wcslen(str), textFormat, 99999, 99999, &textLayout );
    DWRITE_TEXT_METRICS m = {};
    textLayout->GetMetrics( &m );

    textLayout->Release();

    return float2( m.width, m.height );
}
