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

#include <assert.h>
#include "Overlay.h"
#include "Config.h"
#include "OverlayDebug.h"

class OverlayStandings : public Overlay
{
public:

    enum class Columns { POSITION, CAR_NUMBER, NAME, DELTA, BEST, LAST, LICENSE, IRATING, INCIDENTS, PIT };

    OverlayStandings()
        : Overlay("OverlayStandings")
    {}

    virtual void onEnable()
    {
        onConfigChanged();  // trigger font load
    }

    virtual void onConfigChanged()
    {
        const std::string font = g_cfg.getString( m_name, "font" );
        const float fontSize = g_cfg.getFloat( m_name, "font_size" );
        const int fontWeight = g_cfg.getInt( m_name, "font_weight" );
        HRCHECK(m_dwriteFactory->CreateTextFormat( toWide(font).c_str(), NULL, (DWRITE_FONT_WEIGHT)fontWeight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", &m_textFormat ));
        m_textFormat->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
        m_textFormat->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );

        HRCHECK(m_dwriteFactory->CreateTextFormat( toWide(font).c_str(), NULL, (DWRITE_FONT_WEIGHT)fontWeight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize*0.8f, L"en-us", &m_textFormatSmall ));
        m_textFormatSmall->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
        m_textFormatSmall->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );

        // Determine widths of text columns
        m_columns.reset();
        m_columns.add( (int)Columns::POSITION,   computeTextExtent( L"P99", m_dwriteFactory.Get(), m_textFormat.Get() ).x, fontSize/2 );
        m_columns.add( (int)Columns::CAR_NUMBER, computeTextExtent( L"#999", m_dwriteFactory.Get(), m_textFormat.Get() ).x, fontSize/2 );
        m_columns.add( (int)Columns::NAME,       0, fontSize/2 );
        m_columns.add( (int)Columns::PIT,        computeTextExtent( L"P.Age", m_dwriteFactory.Get(), m_textFormat.Get() ).x, fontSize/2 );
        m_columns.add( (int)Columns::LICENSE,    computeTextExtent( L"A 4.44", m_dwriteFactory.Get(), m_textFormatSmall.Get() ).x, fontSize/6 );
        m_columns.add( (int)Columns::IRATING,    computeTextExtent( L"999.9k", m_dwriteFactory.Get(), m_textFormatSmall.Get() ).x, fontSize/6 );
        m_columns.add( (int)Columns::INCIDENTS,  computeTextExtent( L"999x", m_dwriteFactory.Get(), m_textFormat.Get() ).x, fontSize/2 );
        m_columns.add( (int)Columns::BEST,       computeTextExtent( L"999.99.999", m_dwriteFactory.Get(), m_textFormat.Get() ).x, fontSize/2 );
        m_columns.add( (int)Columns::LAST,       computeTextExtent( L"999.99.999", m_dwriteFactory.Get(), m_textFormat.Get() ).x, fontSize/2 );
        m_columns.add( (int)Columns::DELTA,      computeTextExtent( L"9999.9999", m_dwriteFactory.Get(), m_textFormat.Get() ).x, fontSize/2 );
    }

    virtual void onUpdate()
    {
        struct CarInfo {
            int     carIdx = 0;
            int     lapCount = 0;
            float   pctAroundLap = 0;
            int     lapDelta = 0;
            float   delta = 0;
            int     position = 0;
            float   best = 0;
            float   last = 0;
            bool    hasFastestLap = false;
        };
        std::vector<CarInfo> carInfo;
        carInfo.reserve( IR_MAX_CARS );

        // Init array
        float fastestLapTime = FLT_MAX;
        int fastestLapIdx = -1;
        for( int i=0; i<IR_MAX_CARS; ++i )
        {
            const Car& car = ir_session.cars[i];

            if( car.isPaceCar || car.isSpectator || car.userName.empty() )
                continue;

            CarInfo ci;
            ci.carIdx = i;
            ci.lapCount = ir_CarIdxLapCompleted.getInt(i);
            ci.position = ir_CarIdxPosition.getInt(i) > 0 ? ir_CarIdxPosition.getInt(i) : car.qualifyingResultPosition;
            ci.pctAroundLap = ir_CarIdxLapDistPct.getFloat(i);
            ci.delta = -ir_CarIdxF2Time.getFloat(i);
            ci.best = ir_CarIdxBestLapTime.getFloat(i);
            ci.last = ir_CarIdxLastLapTime.getFloat(i);
            carInfo.push_back( ci );

            if( ci.best >= 0 && ci.best < fastestLapTime ) {
                fastestLapTime = ci.best;
                fastestLapIdx = i;
            }
        }

        if( fastestLapIdx >= 0 )
            carInfo[fastestLapIdx].hasFastestLap = true;

        // Sort by position
        std::sort( carInfo.begin(), carInfo.end(),
            []( const CarInfo& a, const CarInfo& b ) { 
                return a.position < b.position;
            } );

        // Compute delta to leader
        for( int i=0; i<(int)carInfo.size(); ++i )
        {
            const CarInfo& ciLeader = carInfo[0];
            CarInfo&       ci       = carInfo[i];

            if( ci.pctAroundLap > ciLeader.pctAroundLap )
                ci.lapDelta = ci.lapCount + 1 - ciLeader.lapCount;
            else
                ci.lapDelta = ci.lapCount - ciLeader.lapCount;
        }

        const float  fontSize           = g_cfg.getFloat( m_name, "font_size" );
        const float  lineSpacing        = g_cfg.getFloat( m_name, "line_spacing" );
        const float  lineHeight         = fontSize + lineSpacing;
        const float4 selfCol            = g_cfg.getFloat4( m_name, "self_col" );
        const float4 buddyCol           = g_cfg.getFloat4( m_name, "buddy_col" );
        const float4 otherCarCol        = g_cfg.getFloat4( m_name, "other_car_col" );
        const float4 headerCol          = g_cfg.getFloat4( m_name, "header_col" );
        const float4 carNumberBgCol     = g_cfg.getFloat4( m_name, "car_number_background_col" );
        const float4 carNumberTextCol   = g_cfg.getFloat4( m_name, "car_number_text_col" );
        const float4 alternateLineBgCol = g_cfg.getFloat4( m_name, "alternate_line_background_col" );
        const float4 iratingTextCol     = g_cfg.getFloat4( m_name, "irating_text_col" );
        const float4 iratingBgCol       = g_cfg.getFloat4( m_name, "irating_background_col" );
        const float4 licenseTextCol     = g_cfg.getFloat4( m_name, "license_text_col" );
        const float4 fastestLapCol      = g_cfg.getFloat4( m_name, "fastest_lap_col" );
        const float4 pitCol             = g_cfg.getFloat4( m_name, "pit_col" );
        const float  licenseBgAlpha     = g_cfg.getFloat( m_name, "license_background_alpha" );

        const float xoff = 10.0f;
        const float yoff = 10;
        m_columns.layout( (float)m_width - 2*xoff );
        float y = yoff + lineHeight/2;

        const ColumnLayout::Column* clm = nullptr;
        wchar_t s[512];
        D2D1_RECT_F r = {};
        D2D1_ROUNDED_RECT rr = {};

        m_renderTarget->BeginDraw();
        m_brush->SetColor( headerCol );

        // Headers
        clm = m_columns.get( (int)Columns::POSITION );
        r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
        swprintf( s, _countof(s), L"Pos." );
        m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
        m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

        clm = m_columns.get( (int)Columns::CAR_NUMBER );
        r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
        swprintf( s, _countof(s), L"No." );
        m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
        m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

        clm = m_columns.get( (int)Columns::NAME );
        r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
        swprintf( s, _countof(s), L"Driver" );
        m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_LEADING );
        m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

        clm = m_columns.get( (int)Columns::PIT );
        r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
        swprintf( s, _countof(s), L"P.Age" );
        m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
        m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

        clm = m_columns.get( (int)Columns::LICENSE );
        r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
        swprintf( s, _countof(s), L"SR" );
        m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
        m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

        clm = m_columns.get( (int)Columns::IRATING );
        r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
        swprintf( s, _countof(s), L"IR" );
        m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
        m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

        clm = m_columns.get( (int)Columns::INCIDENTS );
        r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
        swprintf( s, _countof(s), L"Inc." );
        m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_TRAILING );
        m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

        clm = m_columns.get( (int)Columns::BEST );
        r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
        swprintf( s, _countof(s), L"Best" );
        m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_TRAILING );
        m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

        clm = m_columns.get( (int)Columns::LAST );
        r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
        swprintf( s, _countof(s), L"Last" );
        m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_TRAILING );
        m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

        clm = m_columns.get( (int)Columns::DELTA );
        r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
        swprintf( s, _countof(s), L"Delta" );
        m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_TRAILING );
        m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

        // Content
        for( int i=0; i<(int)carInfo.size(); ++i )
        {
            y = 2*yoff + lineHeight/2 + (i+1)*lineHeight;

            // Alternating line backgrounds
            if( i & 1 && alternateLineBgCol.a > 0 )
            {
                D2D1_RECT_F r = { 0, y-lineHeight/2, (float)m_width,  y+lineHeight/2 };
                m_brush->SetColor( alternateLineBgCol );
                m_renderTarget->FillRectangle( &r, m_brush.Get() );
            }

            const CarInfo&  ci  = carInfo[i];
            const Car&      car = ir_session.cars[ci.carIdx];            

            // Position
            clm = m_columns.get( (int)Columns::POSITION );
            m_brush->SetColor( car.isSelf ? selfCol : (car.isBuddy?buddyCol:otherCarCol) );
            swprintf( s, _countof(s), L"P%d", ci.position );
            r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
            m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_TRAILING );
            m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

            // Car number
            clm = m_columns.get( (int)Columns::CAR_NUMBER );
            swprintf( s, _countof(s), L"#%S", car.carNumberStr.c_str() );
            r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
            rr.rect = { r.left-2, r.top+1, r.right+2, r.bottom-1 };
            rr.radiusX = 3;
            rr.radiusY = 3;
            m_brush->SetColor( car.isSelf ? selfCol : (car.isBuddy ? buddyCol : carNumberBgCol) );
            m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );
            m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
            m_brush->SetColor( carNumberTextCol );
            m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

            // Name
            clm = m_columns.get( (int)Columns::NAME );
            swprintf( s, _countof(s), L"%S", car.userName.c_str() );
            r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
            m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_LEADING );
            m_brush->SetColor( car.isSelf ? selfCol : (car.isBuddy?buddyCol:otherCarCol) );
            m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

            // Pit age
            clm = m_columns.get( (int)Columns::PIT );
            m_brush->SetColor( pitCol );
            if( car.lastLapInPits >= 0 )
            {
                swprintf( s, _countof(s), L"%d", ir_CarIdxLap.getInt(ci.carIdx) - car.lastLapInPits );
                r = { xoff+clm->textL, y-lineHeight/2+2, xoff+clm->textR, y+lineHeight/2-2 };
                m_renderTarget->DrawRectangle( &r, m_brush.Get() );
                m_textFormatSmall->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
                m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormatSmall.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );
            }

            // License/SR
            clm = m_columns.get( (int)Columns::LICENSE );
            swprintf( s, _countof(s), L"%C %.1f", car.licenseChar, car.licenseSR );
            r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
            rr.rect = { r.left+1, r.top+1, r.right-1, r.bottom-1 };
            rr.radiusX = 3;
            rr.radiusY = 3;
            float4 c = car.licenseCol;
            c.a = licenseBgAlpha;
            m_brush->SetColor( c );
            m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );
            m_textFormatSmall->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
            m_brush->SetColor( licenseTextCol );
            m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormatSmall.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

            // Irating
            clm = m_columns.get( (int)Columns::IRATING );
            swprintf( s, _countof(s), L"%.1fk", (float)car.irating/1000.0f );
            r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
            rr.rect = { r.left+1, r.top+1, r.right-1, r.bottom-1 };
            rr.radiusX = 3;
            rr.radiusY = 3;
            m_brush->SetColor( iratingBgCol );
            m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );
            m_textFormatSmall->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
            m_brush->SetColor( iratingTextCol );
            m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormatSmall.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

            // Incidents
            clm = m_columns.get( (int)Columns::INCIDENTS );
            m_brush->SetColor( car.incidentCount ? otherCarCol : float4(otherCarCol.r,otherCarCol.g,otherCarCol.b,otherCarCol.a*0.5f) );
            swprintf( s, _countof(s), L"%dx", car.incidentCount );
            r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
            m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_TRAILING );
            m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

            // Best
            clm = m_columns.get( (int)Columns::BEST );
            if( ci.best <= 0 )
                s[0] = L'\0';
            else {
                const int mins = int(ci.last/60.0f);
                if( mins )
                    swprintf( s, _countof(s), L"%02d:%.03f", mins, fmodf(ci.last,60.0f) );
                else
                    swprintf( s, _countof(s), L"%.03f", ci.last );
            }
            r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
            m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_TRAILING );
            m_brush->SetColor( ci.hasFastestLap ? fastestLapCol : otherCarCol );
            m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

            // Last
            clm = m_columns.get( (int)Columns::LAST );
            if( ci.last <= 0 )
                s[0] = L'\0';
            else {
                const int mins = int(ci.last/60.0f);
                if( mins )
                    swprintf( s, _countof(s), L"%02d:%.03f", mins, fmodf(ci.last,60.0f) );
                else
                    swprintf( s, _countof(s), L"%.03f", ci.last );
            }
            r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
            m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_TRAILING );
            m_brush->SetColor( otherCarCol );
            m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

            // Delta
            clm = m_columns.get( (int)Columns::DELTA );
            if( ci.position == 1 )
                s[0] = L'\0';
            else
                swprintf( s, _countof(s), ci.lapDelta ? L"%.0f L" : L"%.3f", ci.lapDelta ? (float)ci.lapDelta : ci.delta );
            r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
            m_textFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_TRAILING );
            m_brush->SetColor( otherCarCol );
            m_renderTarget->DrawTextA( s, (int)wcslen(s), m_textFormat.Get(), &r, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );
        }
        m_renderTarget->EndDraw();
    }

    virtual bool canEnableWhileNotDriving() const
    {
        return true;
    }

protected:

    Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormatSmall;

    ColumnLayout m_columns;
};