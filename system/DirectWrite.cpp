#include "DirectWrite.h"

#include <cassert>
#include <cstdio>
#include <new>
#include <limits>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using Microsoft::WRL::ComPtr;

namespace
{
    template<class T>
    void SafeReset(ComPtr<T>& ptr)
    {
        ptr.Reset();
    }

    HRESULT HrIfNull(const void* p)
    {
        return p ? S_OK : E_POINTER;
    }

    std::wstring MultiByteToWide(UINT codePage, DWORD flags, std::string_view text)
    {
        if (text.empty()) { return {}; }

        if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            return {};
        }

        const int srcLen = static_cast<int>(text.size());
        const int len = MultiByteToWideChar(
            codePage,
            flags,
            text.data(),
            srcLen,
            nullptr,
            0);

        if (len <= 0) { return {}; }

        std::wstring result(static_cast<size_t>(len), L'\0');
        const int converted = MultiByteToWideChar(
            codePage,
            flags,
            text.data(),
            srcLen,
            result.data(),
            len);

        if (converted <= 0) { return {}; }
        return result;
    }

    bool FileExists(const std::wstring& path)
    {
        const DWORD attr = GetFileAttributesW(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES &&
               (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
}

//=============================================================================
// CustomFontFileEnumerator
//=============================================================================
CustomFontFileEnumerator::CustomFontFileEnumerator(
    IDWriteFactory* factory,
    std::vector<std::wstring> fontFilePaths)
    : m_factory(factory)
    , m_fontFilePaths(std::move(fontFilePaths))
{
}

HRESULT CustomFontFileEnumerator::QueryInterface(REFIID iid, void** ppvObject)
{
    if (!ppvObject) { return E_POINTER; }

    if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontFileEnumerator))
    {
        *ppvObject = static_cast<IDWriteFontFileEnumerator*>(this);
        AddRef();
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG CustomFontFileEnumerator::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

ULONG CustomFontFileEnumerator::Release()
{
    const ULONG count = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
    if (count == 0) { delete this; }
    return count;
}

HRESULT CustomFontFileEnumerator::MoveNext(BOOL* hasCurrentFile) noexcept
{
    if (!hasCurrentFile) { return E_POINTER; }

    ++m_currentFileIndex;
    *hasCurrentFile =
        (m_currentFileIndex >= 0 &&
         m_currentFileIndex < static_cast<int>(m_fontFilePaths.size()))
        ? TRUE
        : FALSE;

    return S_OK;
}

HRESULT CustomFontFileEnumerator::GetCurrentFontFile(IDWriteFontFile** fontFile) noexcept
{
    if (!fontFile) { return E_POINTER; }
    *fontFile = nullptr;

    if (!m_factory) { return E_FAIL; }
    if (m_currentFileIndex < 0 ||
        m_currentFileIndex >= static_cast<int>(m_fontFilePaths.size()))
    {
        return E_FAIL;
    }

    return m_factory->CreateFontFileReference(
        m_fontFilePaths[static_cast<size_t>(m_currentFileIndex)].c_str(),
        nullptr,
        fontFile);
}

//=============================================================================
// CustomFontCollectionLoader
//=============================================================================
HRESULT CustomFontCollectionLoader::QueryInterface(REFIID iid, void** ppvObject)
{
    if (!ppvObject) { return E_POINTER; }

    if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontCollectionLoader))
    {
        *ppvObject = static_cast<IDWriteFontCollectionLoader*>(this);
        AddRef();
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG CustomFontCollectionLoader::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

ULONG CustomFontCollectionLoader::Release()
{
    const ULONG count = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
    if (count == 0) { delete this; }
    return count;
}

HRESULT CustomFontCollectionLoader::CreateEnumeratorFromKey(
    IDWriteFactory* factory,
    void const* /*collectionKey*/,
    UINT32 /*collectionKeySize*/,
    IDWriteFontFileEnumerator** fontFileEnumerator) noexcept
{
    if (!factory || !fontFileEnumerator) { return E_POINTER; }
    *fontFileEnumerator = nullptr;

    auto* enumerator = new (std::nothrow) CustomFontFileEnumerator(
        factory,
        m_fontFilePaths);

    if (!enumerator) { return E_OUTOFMEMORY; }

    *fontFileEnumerator = enumerator;
    return S_OK;
}

//=============================================================================
// DirectWrite
//=============================================================================
DirectWrite::~DirectWrite()
{
    Shutdown();
}

void DirectWrite::Shutdown()
{
    if (m_loaderRegistered && m_dwriteFactory && m_fontCollectionLoader)
    {
        m_dwriteFactory->UnregisterFontCollectionLoader(m_fontCollectionLoader.Get());
    }

    m_loaderRegistered = false;

    m_fontFiles.clear();
    m_fontNames.clear();
    m_currentText.clear();
    m_totalTime = 0.0f;

    fontCollection.Reset();
    m_textLayout.Reset();
    m_textFormat.Reset();
    m_textBrush.Reset();
    m_shadowBrush.Reset();
    m_fontCollectionLoader.Reset();
    m_dwriteFactory.Reset();
    m_renderTarget.Reset();
    m_backBuffer.Reset();
    m_d2dFactory.Reset();
}

HRESULT DirectWrite::Init(IDXGISwapChain* swapChain)
{
    if (!swapChain) { return E_POINTER; }

    Shutdown();

    HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(m_backBuffer.GetAddressOf()));
    if (FAILED(hr)) { return hr; }

    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        m_d2dFactory.GetAddressOf());
    if (FAILED(hr)) { return hr; }

    const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));

    hr = m_d2dFactory->CreateDxgiSurfaceRenderTarget(
        m_backBuffer.Get(),
        &props,
        m_renderTarget.GetAddressOf());
    if (FAILED(hr)) { return hr; }

    m_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    if (FAILED(hr)) { return hr; }

    m_fontCollectionLoader.Attach(new (std::nothrow) CustomFontCollectionLoader());
    if (!m_fontCollectionLoader) { return E_OUTOFMEMORY; }

    hr = m_dwriteFactory->RegisterFontCollectionLoader(m_fontCollectionLoader.Get());
    if (FAILED(hr)) { return hr; }
    m_loaderRegistered = true;

    hr = FontLoader();
    if (FAILED(hr)) { return hr; }

    return CreateTextFormat();
}

void DirectWrite::SetFont(FontData setting)
{
    m_setting = std::move(setting);

    const HRESULT hr = CreateTextFormat();
    assert(SUCCEEDED(hr) && "DirectWrite::SetFont failed.");
}

void DirectWrite::SetFonts(
    FontList::FontName fontName,
    DWRITE_FONT_WEIGHT fontWeight,
    DWRITE_FONT_STYLE fontStyle,
    DWRITE_FONT_STRETCH fontStretch,
    FLOAT fontSize,
    std::wstring localeName,
    DWRITE_TEXT_ALIGNMENT textAlignment,
    D2D1_COLOR_F color,
    D2D1_COLOR_F shadowColor,
    D2D1_POINT_2F shadowOffset)
{
    FontData setting;
    setting.font = fontName;
    setting.fontWeight = fontWeight;
    setting.fontStyle = fontStyle;
    setting.fontStretch = fontStretch;
    setting.fontSize = fontSize;
    setting.localeName = std::move(localeName);
    setting.textAlignment = textAlignment;
    setting.Color = color;
    setting.shadowColor = shadowColor;
    setting.shadowOffset = shadowOffset;

    SetFont(setting);
}

HRESULT DirectWrite::CreateTextFormat()
{
    if (!m_dwriteFactory || !m_renderTarget) { return E_FAIL; }

    m_textFormat.Reset();
    m_textLayout.Reset();
    m_currentText.clear();

    std::wstring familyName = ResolveFontFamilyName(m_setting.font);
    IDWriteFontCollection* collection = fontCollection.Get();

    if (familyName.empty())
    {
        familyName = L"Yu Gothic UI";
        collection = nullptr;
    }

    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        familyName.c_str(),
        collection,
        m_setting.fontWeight,
        m_setting.fontStyle,
        m_setting.fontStretch,
        m_setting.fontSize,
        m_setting.localeName.c_str(),
        m_textFormat.GetAddressOf());
    if (FAILED(hr))
    {
        // Fallback to a system font.
        hr = m_dwriteFactory->CreateTextFormat(
            L"Yu Gothic UI",
            nullptr,
            m_setting.fontWeight,
            m_setting.fontStyle,
            m_setting.fontStretch,
            m_setting.fontSize,
            m_setting.localeName.c_str(),
            m_textFormat.GetAddressOf());
        if (FAILED(hr)) { return hr; }
    }

    hr = m_textFormat->SetTextAlignment(m_setting.textAlignment);
    if (FAILED(hr)) { return hr; }

    return CreateBrushes();
}

HRESULT DirectWrite::CreateBrushes()
{
    if (!m_renderTarget) { return E_FAIL; }

    m_textBrush.Reset();
    m_shadowBrush.Reset();

    HRESULT hr = m_renderTarget->CreateSolidColorBrush(
        m_setting.Color,
        m_textBrush.GetAddressOf());
    if (FAILED(hr)) { return hr; }

    return m_renderTarget->CreateSolidColorBrush(
        m_setting.shadowColor,
        m_shadowBrush.GetAddressOf());
}

HRESULT DirectWrite::DrawString(
    const std::wstring& text,
    DirectX::SimpleMath::Vector2 pos,
    D2D1_DRAW_TEXT_OPTIONS options,
    bool shadow)
{
    if (!m_renderTarget || !m_dwriteFactory || !m_textFormat) { return E_FAIL; }

    const std::wstring normalized = NormalizeText(text);

    HRESULT hr = CreateTextLayoutIfNeeded(normalized);
    if (FAILED(hr)) { return hr; }

    return DrawCurrentTextLayout(
        D2D1::Point2F(pos.x, pos.y),
        options,
        shadow);
}

HRESULT DirectWrite::DrawString(
    const std::string& textUtf8,
    D2D1_RECT_F rect,
    D2D1_DRAW_TEXT_OPTIONS options,
    bool shadow)
{
    if (!m_renderTarget || !m_textFormat || !m_textBrush) { return E_FAIL; }

    const std::wstring text = NormalizeText(Utf8OrAcpToWide(textUtf8));

    m_renderTarget->BeginDraw();

    if (shadow && m_shadowBrush)
    {
        const D2D1_RECT_F shadowRect = D2D1::RectF(
            rect.left + m_setting.shadowOffset.x,
            rect.top + m_setting.shadowOffset.y,
            rect.right + m_setting.shadowOffset.x,
            rect.bottom + m_setting.shadowOffset.y);

        m_renderTarget->DrawTextW(
            text.c_str(),
            static_cast<UINT32>(text.size()),
            m_textFormat.Get(),
            shadowRect,
            m_shadowBrush.Get(),
            options);
    }

    m_renderTarget->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        m_textFormat.Get(),
        rect,
        m_textBrush.Get(),
        options);

    return m_renderTarget->EndDraw();
}

HRESULT DirectWrite::CreateTextLayoutIfNeeded(const std::wstring& text)
{
    if (!m_dwriteFactory || !m_textFormat || !m_renderTarget) { return E_FAIL; }

    if (m_textLayout && m_currentText == text) { return S_OK; }

    m_currentText = text;
    m_textLayout.Reset();

    const D2D1_SIZE_F targetSize = m_renderTarget->GetSize();

    return m_dwriteFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        m_textFormat.Get(),
        targetSize.width,
        targetSize.height,
        m_textLayout.GetAddressOf());
}

HRESULT DirectWrite::DrawCurrentTextLayout(
    D2D1_POINT_2F pos,
    D2D1_DRAW_TEXT_OPTIONS options,
    bool shadow)
{
    if (!m_renderTarget || !m_textLayout || !m_textBrush) { return E_FAIL; }

    m_renderTarget->BeginDraw();

    if (shadow && m_shadowBrush)
    {
        m_renderTarget->DrawTextLayout(
            D2D1::Point2F(
                pos.x + m_setting.shadowOffset.x,
                pos.y + m_setting.shadowOffset.y),
            m_textLayout.Get(),
            m_shadowBrush.Get(),
            options);
    }

    m_renderTarget->DrawTextLayout(
        pos,
        m_textLayout.Get(),
        m_textBrush.Get(),
        options);

    return m_renderTarget->EndDraw();
}

HRESULT DirectWrite::DrawStringPartial(
    const std::wstring& text,
    DirectX::SimpleMath::Vector2 pos,
    bool shadow,
    const std::vector<PartialStyle>& styles,
    float delayTime,
    float elapsedTime,
    D2D1_DRAW_TEXT_OPTIONS options)
{
    if (!m_renderTarget || !m_dwriteFactory || !m_textFormat) { return E_FAIL; }

    const std::wstring normalized = NormalizeText(text);

    m_totalTime += elapsedTime;

    size_t displayCount = normalized.size();
    if (delayTime > 0.0f)
    {
        displayCount = static_cast<size_t>(m_totalTime / delayTime);
        displayCount = std::min(displayCount, normalized.size());
    }

    const std::wstring partialText = normalized.substr(0, displayCount);

    const bool needsNewLayout =
        !m_textLayout ||
        m_currentText != partialText ||
        !styles.empty(); // Style brushes are layout drawing effects, so rebuild when styles are supplied.

    if (needsNewLayout)
    {
        m_currentText = partialText;
        m_textLayout.Reset();

        const D2D1_SIZE_F targetSize = m_renderTarget->GetSize();
        HRESULT hr = m_dwriteFactory->CreateTextLayout(
            partialText.c_str(),
            static_cast<UINT32>(partialText.size()),
            m_textFormat.Get(),
            targetSize.width,
            targetSize.height,
            m_textLayout.GetAddressOf());
        if (FAILED(hr)) { return hr; }

        const UINT32 totalLen = static_cast<UINT32>(partialText.size());
        for (const PartialStyle& style : styles)
        {
            if (style.start >= totalLen || style.length == 0) { continue; }

            const UINT32 length = std::min<UINT32>(
                style.length,
                totalLen - style.start);

            const DWRITE_TEXT_RANGE range = { style.start, length };

            if (style.useFont)
            {
                const std::wstring familyName = ResolveFontFamilyName(style.font);
                if (!familyName.empty())
                {
                    hr = m_textLayout->SetFontFamilyName(familyName.c_str(), range);
                    if (FAILED(hr)) { return hr; }
                }
            }

            if (style.useColor)
            {
                ComPtr<ID2D1SolidColorBrush> brush;
                hr = m_renderTarget->CreateSolidColorBrush(style.color, brush.GetAddressOf());
                if (FAILED(hr)) { return hr; }

                hr = m_textLayout->SetDrawingEffect(brush.Get(), range);
                if (FAILED(hr)) { return hr; }
            }
        }
    }

    return DrawCurrentTextLayout(
        D2D1::Point2F(pos.x, pos.y),
        options,
        shadow);
}

HRESULT DirectWrite::FontLoader()
{
    return RebuildFontCollection();
}

HRESULT DirectWrite::RebuildFontCollection()
{
    if (!m_dwriteFactory || !m_fontCollectionLoader) { return E_FAIL; }

    const std::vector<std::wstring> paths = FontList::RebuildAllFontPaths();

    HRESULT hr = LoadFontFiles(paths);
    if (FAILED(hr)) { return hr; }

    m_fontCollectionLoader->SetFontFilePaths(paths);
    fontCollection.Reset();

    hr = m_dwriteFactory->CreateCustomFontCollection(
        m_fontCollectionLoader.Get(),
        &m_collectionKey,
        sizeof(m_collectionKey),
        fontCollection.GetAddressOf());
    if (FAILED(hr)) { return hr; }

    hr = RefreshFontNames(fontCollection.Get(), L"ja-jp", false);
    if (FAILED(hr)) { return hr; }

    ResetText();
    return S_OK;
}

HRESULT DirectWrite::LoadFontFiles(const std::vector<std::wstring>& fontPaths)
{
    if (!m_dwriteFactory) { return E_FAIL; }

    m_fontFiles.clear();
    m_fontFiles.reserve(fontPaths.size());

    for (const auto& path : fontPaths)
    {
        if (!FileExists(path))
        {
            std::fwprintf(stderr, L"Font file not found: %s\n", path.c_str());
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        ComPtr<IDWriteFontFile> fontFile;
        HRESULT hr = m_dwriteFactory->CreateFontFileReference(
            path.c_str(),
            nullptr,
            fontFile.GetAddressOf());
        if (FAILED(hr))
        {
            std::fwprintf(stderr, L"Failed to load font file: %s\n", path.c_str());
            return hr;
        }

        m_fontFiles.emplace_back(fontFile);
    }

    return S_OK;
}

std::wstring DirectWrite::GetFontName(int index) const
{
    if (m_fontNames.empty()) { return {}; }

    if (index < 0 || index >= static_cast<int>(m_fontNames.size()))
    {
        return m_fontNames.front();
    }

    return m_fontNames[static_cast<size_t>(index)];
}

int DirectWrite::GetFontNameNum() const
{
    return static_cast<int>(m_fontNames.size());
}

HRESULT DirectWrite::GetFontFamilyName(
    IDWriteFontCollection* customFontCollection,
    std::wstring locale)
{
    return RefreshFontNames(customFontCollection, locale, false);
}

HRESULT DirectWrite::GetAllFontFamilyName(IDWriteFontCollection* customFontCollection)
{
    return RefreshFontNames(customFontCollection, L"ja-jp", true);
}

HRESULT DirectWrite::RefreshFontNames(
    IDWriteFontCollection* collection,
    const std::wstring& locale,
    bool allLocalizedNames)
{
    if (!collection) { return E_POINTER; }

    m_fontNames.clear();

    const UINT32 familyCount = collection->GetFontFamilyCount();
    for (UINT32 i = 0; i < familyCount; ++i)
    {
        ComPtr<IDWriteFontFamily> fontFamily;
        HRESULT hr = collection->GetFontFamily(i, fontFamily.GetAddressOf());
        if (FAILED(hr)) { return hr; }

        ComPtr<IDWriteLocalizedStrings> familyNames;
        hr = fontFamily->GetFamilyNames(familyNames.GetAddressOf());
        if (FAILED(hr)) { return hr; }

        if (allLocalizedNames)
        {
            const UINT32 nameCount = familyNames->GetCount();
            for (UINT32 j = 0; j < nameCount; ++j)
            {
                UINT32 length = 0;
                hr = familyNames->GetStringLength(j, &length);
                if (FAILED(hr)) { return hr; }

                std::wstring name(length + 1, L'\0');
                hr = familyNames->GetString(j, name.data(), length + 1);
                if (FAILED(hr)) { return hr; }

                if (!name.empty() && name.back() == L'\0') { name.pop_back(); }
                if (std::find(m_fontNames.begin(), m_fontNames.end(), name) == m_fontNames.end())
                {
                    m_fontNames.emplace_back(std::move(name));
                }
            }
        }
        else
        {
            UINT32 index = 0;
            BOOL exists = FALSE;

            hr = familyNames->FindLocaleName(locale.c_str(), &index, &exists);
            if (FAILED(hr)) { return hr; }

            if (!exists)
            {
                hr = familyNames->FindLocaleName(L"ja-jp", &index, &exists);
                if (FAILED(hr)) { return hr; }
            }

            if (!exists)
            {
                hr = familyNames->FindLocaleName(L"en-us", &index, &exists);
                if (FAILED(hr)) { return hr; }
            }

            if (!exists) { index = 0; }

            UINT32 length = 0;
            hr = familyNames->GetStringLength(index, &length);
            if (FAILED(hr)) { return hr; }

            std::wstring name(length + 1, L'\0');
            hr = familyNames->GetString(index, name.data(), length + 1);
            if (FAILED(hr)) { return hr; }

            if (!name.empty() && name.back() == L'\0') { name.pop_back(); }
            if (std::find(m_fontNames.begin(), m_fontNames.end(), name) == m_fontNames.end())
            {
                m_fontNames.emplace_back(std::move(name));
            }
        }
    }

    return m_fontNames.empty() ? S_FALSE : S_OK;
}

void DirectWrite::ResetText()
{
    m_textLayout.Reset();
    m_currentText.clear();
    m_totalTime = 0.0f;
}

HRESULT DirectWrite::ChangeTextFont(
    IDWriteTextLayout* textLayout,
    FontList::FontName fontName,
    UINT32 startChar,
    UINT32 length)
{
    if (!textLayout) { return E_POINTER; }

    const std::wstring familyName = ResolveFontFamilyName(fontName);
    if (familyName.empty()) { return E_FAIL; }

    const DWRITE_TEXT_RANGE range = { startChar, length };
    return textLayout->SetFontFamilyName(familyName.c_str(), range);
}

int DirectWrite::AddFont(const std::wstring& fontPath)
{
    if (!FileExists(fontPath)) { return -1; }

    const auto& defaults = FontList::GetDefaultFontPaths();
    for (size_t i = 0; i < defaults.size(); ++i)
    {
        if (defaults[i] == fontPath) { return static_cast<int>(i); }
    }

    auto& runtime = FontList::GetRuntimeFontPaths();
    for (size_t i = 0; i < runtime.size(); ++i)
    {
        if (runtime[i] == fontPath)
        {
            return static_cast<int>(defaults.size() + i);
        }
    }

    runtime.emplace_back(fontPath);
    FontList::RebuildAllFontPaths();

    HRESULT hr = RebuildFontCollection();
    if (FAILED(hr))
    {
        runtime.pop_back();
        FontList::RebuildAllFontPaths();
        RebuildFontCollection();
        return -1;
    }

    return static_cast<int>(m_fontNames.size()) - 1;
}

void DirectWrite::SetFontByName(const std::wstring& familyName, FontData setting)
{
    m_setting = std::move(setting);

    if (!m_dwriteFactory || !m_renderTarget) { return; }

    m_textFormat.Reset();
    m_textLayout.Reset();
    m_currentText.clear();

    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        familyName.c_str(),
        fontCollection.Get(),
        m_setting.fontWeight,
        m_setting.fontStyle,
        m_setting.fontStretch,
        m_setting.fontSize,
        m_setting.localeName.c_str(),
        m_textFormat.GetAddressOf());

    if (FAILED(hr))
    {
        hr = m_dwriteFactory->CreateTextFormat(
            familyName.c_str(),
            nullptr,
            m_setting.fontWeight,
            m_setting.fontStyle,
            m_setting.fontStretch,
            m_setting.fontSize,
            m_setting.localeName.c_str(),
            m_textFormat.GetAddressOf());
    }

    assert(SUCCEEDED(hr) && "DirectWrite::SetFontByName failed.");
    if (FAILED(hr)) { return; }

    m_textFormat->SetTextAlignment(m_setting.textAlignment);
    CreateBrushes();
}

std::wstring DirectWrite::NormalizeText(std::wstring text) const
{
    for (wchar_t& ch : text)
    {
        if (ch == m_lineBreakMarker)
        {
            ch = L'\n';
        }
    }
    return text;
}

std::wstring DirectWrite::ResolveFontFamilyName(FontList::FontName fontName) const
{
    const int index = static_cast<int>(fontName);
    if (index >= 0 && index < static_cast<int>(m_fontNames.size()))
    {
        return m_fontNames[static_cast<size_t>(index)];
    }

    if (!m_fontNames.empty())
    {
        return m_fontNames.front();
    }

    return L"Yu Gothic UI";
}

std::wstring DirectWrite::Utf8OrAcpToWide(std::string_view text) const
{
    std::wstring result = MultiByteToWide(CP_UTF8, MB_ERR_INVALID_CHARS, text);
    if (!result.empty() || text.empty())
    {
        return result;
    }

    return MultiByteToWide(CP_ACP, 0, text);
}
