#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <dxgi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "CommonTypes.h"
#include <dcommon.h>
#include <SimpleMath.h>
#include <d2d1helper.h>
#include <utility>

// Link libraries are intentionally NOT placed in this header.
// Add these to the .cpp file or project linker settings:
//   d2d1.lib
//   dwrite.lib

namespace FontList
{
    enum class FontName
    {
        MelodyLine = 0,
        Google,
    };

    inline const std::vector<std::wstring>& GetDefaultFontPaths()
    {
        static const std::vector<std::wstring> kPaths = {
            L"Assets\\font\\MelodyLine-free.otf",
            L"Assets\\font\\NotoSansJP-Black.otf",
        };
        return kPaths;
    }

    inline std::vector<std::wstring>& GetRuntimeFontPaths()
    {
        static std::vector<std::wstring> sPaths;
        return sPaths;
    }

    inline std::vector<std::wstring>& GetAllFontPathsCache()
    {
        static std::vector<std::wstring> sCache;
        return sCache;
    }

    inline const std::vector<std::wstring>& RebuildAllFontPaths()
    {
        auto& cache = GetAllFontPathsCache();
        cache.clear();

        const auto& defaults = GetDefaultFontPaths();
        const auto& runtime = GetRuntimeFontPaths();

        cache.reserve(defaults.size() + runtime.size());

        for (const auto& path : defaults)
        {
            if (std::find(cache.begin(), cache.end(), path) == cache.end())
            {
                cache.emplace_back(path);
            }
        }

        for (const auto& path : runtime)
        {
            if (std::find(cache.begin(), cache.end(), path) == cache.end())
            {
                cache.emplace_back(path);
            }
        }

        return cache;
    }

    inline const std::vector<std::wstring>& GetAllFontPaths()
    {
        auto& cache = GetAllFontPathsCache();
        if (cache.empty())
        {
            return RebuildAllFontPaths();
        }
        return cache;
    }
}

struct FontData
{
    FontList::FontName    font = FontList::FontName::Google;
    DWRITE_FONT_WEIGHT    fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_STYLE     fontStyle = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STRETCH   fontStretch = DWRITE_FONT_STRETCH_NORMAL;
    FLOAT                 fontSize = 40.0f;
    std::wstring          localeName = L"ja-jp";
    DWRITE_TEXT_ALIGNMENT textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
    D2D1_COLOR_F          Color = D2D1::ColorF(D2D1::ColorF::White);
    D2D1_COLOR_F          shadowColor = D2D1::ColorF(D2D1::ColorF::Black);
    D2D1_POINT_2F         shadowOffset = D2D1::Point2F(2.0f, 2.0f);
};

struct PartialStyle
{
    UINT32              start = 0;
    UINT32              length = 0;
    bool                useFont = false;
    FontList::FontName  font = FontList::FontName::Google;
    bool                useColor = false;
    D2D1_COLOR_F        color = D2D1::ColorF(D2D1::ColorF::White);
};

class CustomFontCollectionLoader;

class DirectWrite
{
public:
    DirectWrite() = default;
    explicit DirectWrite(const FontData& setting) : m_setting(setting) {}
    explicit DirectWrite(const FontData* setting)
    {
        if (setting) { m_setting = *setting; }
    }

    ~DirectWrite();

    DirectWrite(const DirectWrite&) = delete;
    DirectWrite& operator=(const DirectWrite&) = delete;

    HRESULT Init(IDXGISwapChain* swapChain);
    void    Shutdown();

    void SetFont(FontData setting);

    void SetFonts(
        FontList::FontName    fontName,
        DWRITE_FONT_WEIGHT    fontWeight = DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE     fontStyle = DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH   fontStretch = DWRITE_FONT_STRETCH_NORMAL,
        FLOAT                 fontSize = 20.0f,
        std::wstring          localeName = L"ja-jp",
        DWRITE_TEXT_ALIGNMENT textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING,
        D2D1_COLOR_F          color = D2D1::ColorF(D2D1::ColorF::White),
        D2D1_COLOR_F          shadowColor = D2D1::ColorF(D2D1::ColorF::Black),
        D2D1_POINT_2F         shadowOffset = D2D1::Point2F(2.0f, 2.0f));

    HRESULT DrawString(
        const std::wstring& text,
        Vector2 pos,
        D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE,
        bool shadow = false);

    HRESULT DrawString(
        const std::string& textUtf8,
        D2D1_RECT_F rect,
        D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE,
        bool shadow = false);

    HRESULT DrawStringPartial(
        const std::wstring& text,
        DirectX::SimpleMath::Vector2 pos,
        bool shadow = false,
        const std::vector<PartialStyle>& styles = {},
        float delayTime = 0.2f,
        float elapsedTime = 0.1f,
        D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE);

    HRESULT FontLoader();
    HRESULT RebuildFontCollection();

    std::wstring GetFontName(int index) const;
    int          GetFontNameNum() const;
    const std::vector<std::wstring>& GetFontNames() const { return m_fontNames; }

    HRESULT GetFontFamilyName(
        IDWriteFontCollection* customFontCollection,
        std::wstring locale = L"ja-jp");

    HRESULT GetAllFontFamilyName(IDWriteFontCollection* customFontCollection);

    // Validates font files by trying to create IDWriteFontFile references.
    HRESULT LoadFontFiles(const std::vector<std::wstring>& fontPaths);

    void ResetText();
    void SetLineBreakMarker(wchar_t marker) { m_lineBreakMarker = marker; ResetText(); }

    HRESULT ChangeTextFont(
        IDWriteTextLayout* textLayout,
        FontList::FontName fontName,
        UINT32 startChar,
        UINT32 length);

    int     AddFont(const std::wstring& fontPath);
    void    SetFontByName(const std::wstring& familyName, FontData setting);

    // Kept public for compatibility with the existing code.
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fontCollection = nullptr;

private:
    HRESULT CreateTextFormat();
    HRESULT CreateBrushes();
    HRESULT CreateTextLayoutIfNeeded(const std::wstring& text);
    HRESULT DrawCurrentTextLayout(D2D1_POINT_2F pos, D2D1_DRAW_TEXT_OPTIONS options, bool shadow);

    std::wstring NormalizeText(std::wstring text) const;
    std::wstring ResolveFontFamilyName(FontList::FontName fontName) const;
    std::wstring Utf8OrAcpToWide(std::string_view text) const;
    HRESULT      RefreshFontNames(IDWriteFontCollection* collection, const std::wstring& locale, bool allLocalizedNames);

private:
    Microsoft::WRL::ComPtr<ID2D1Factory>          m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget>     m_renderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>  m_textBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>  m_shadowBrush;
    Microsoft::WRL::ComPtr<IDWriteFactory>        m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat>     m_textFormat;
    Microsoft::WRL::ComPtr<IDWriteTextLayout>     m_textLayout;
    Microsoft::WRL::ComPtr<IDXGISurface>          m_backBuffer;
    Microsoft::WRL::ComPtr<CustomFontCollectionLoader> m_fontCollectionLoader;

    FontData m_setting;
    std::vector<std::wstring> m_fontNames;
    std::vector<Microsoft::WRL::ComPtr<IDWriteFontFile>> m_fontFiles;

    std::wstring m_currentText;
    float        m_totalTime = 0.0f;
    wchar_t      m_lineBreakMarker = L'ワ';
    bool         m_loaderRegistered = false;
    UINT32       m_collectionKey = 0x44465731; // 'DFW1'
};

class CustomFontFileEnumerator final : public IDWriteFontFileEnumerator
{
public:
    CustomFontFileEnumerator(
        IDWriteFactory* factory,
        std::vector<std::wstring> fontFilePaths);

    IFACEMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP MoveNext(BOOL* hasCurrentFile) noexcept override;
    IFACEMETHODIMP GetCurrentFontFile(IDWriteFontFile** fontFile) noexcept override;

private:
    volatile LONG m_refCount = 1;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_factory;
    std::vector<std::wstring> m_fontFilePaths;
    int m_currentFileIndex = -1;
};

class CustomFontCollectionLoader final : public IDWriteFontCollectionLoader
{
public:
    CustomFontCollectionLoader() = default;

    void SetFontFilePaths(std::vector<std::wstring> paths)
    {
        m_fontFilePaths = std::move(paths);
    }

    IFACEMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP CreateEnumeratorFromKey(
        IDWriteFactory* factory,
        void const* collectionKey,
        UINT32 collectionKeySize,
        IDWriteFontFileEnumerator** fontFileEnumerator) noexcept override;

private:
    volatile LONG m_refCount = 1;
    std::vector<std::wstring> m_fontFilePaths;
};
