﻿#include "DeskBand.h"
#include<Shlwapi.h>
#include<dwmapi.h>
#include<Uxtheme.h>
#include"CpuUsage.h"
#include"MemoryUsage.h"
#include"CpuTemperature.h"
#include<cassert>
#include<sstream>
#include<iomanip>
#include"Log.h"
#include"resource.h"

LPCTSTR CDeskBand::sm_lpszClassName = TEXT("MonitorBarClass");
extern CLSID CLSID_DeskBand;

extern HINSTANCE g_hInst;
extern ULONG g_lDllRef;
STDAPI DllShowMonitorBar(BOOL fShowOrHide);

CDeskBand::CDeskBand()
	:m_lRef(1)
	, m_hWnd(nullptr)
	, m_hWndParent(nullptr)
	, m_dwBandID(0)
	, m_bCanCompositionEnabled(FALSE)
	, m_bIsDirty(FALSE)
	, m_pSite(nullptr)
	, m_bHasFocus(FALSE)
	, m_hFont(nullptr)
	, m_bIsRegisterClassed(false)
	, m_hToolTip(nullptr)
	, nTIMER_ID(1)
	, m_hMenu(nullptr)
	, hbrUsed(CreateSolidBrush(RGB(0, 255, 0)))//绿
	, hbrTotal(CreateSolidBrush(RGB(255, 0, 0)))//红
{
	for (size_t i = 0; i < _countof(m_iMonitors); ++i)
		__Init(i);
	InterlockedIncrement(&g_lDllRef);
#ifdef _DEBUG
	char str[64];
	sprintf_s(str, "g_lDllRef=%lu,CDeskBand构造", g_lDllRef);
	LOGOUT(str);
#endif
}

CDeskBand::~CDeskBand()
{
	if (m_pSite)
		m_pSite->Release();
	for (auto& m : m_iMonitors)
		if (m)
		{
			delete m;
			m = nullptr;
		}
	if (m_bIsRegisterClassed)
		UnregisterClass(sm_lpszClassName, g_hInst);
	InterlockedDecrement(&g_lDllRef);
	if(hbrUsed) DeleteObject(hbrUsed);
	if (hbrTotal) DeleteObject(hbrTotal);
#ifdef _DEBUG
	char str[64];
	sprintf_s(str, "g_lDllRef=%lu,CDeskBand析构", g_lDllRef);
	LOGOUT(str);
#endif
}

void CDeskBand::__Init(size_t i)
{
	switch (i)
	{
	case 0:m_iMonitors[i] = new CCpuUsage(); break;
	case 1:m_iMonitors[i] = new CMemoryUsage(); break;
	case 2:m_iMonitors[i] = new CCpuTemperature(); break;
	}
	if (!m_iMonitors[i]->Init())
	{
		delete m_iMonitors[i];
		m_iMonitors[i] = nullptr;
	}
}

STDMETHODIMP CDeskBand::QueryInterface(REFIID riid, void** ppv)
{
	QITAB qitab[] =
	{
		QITABENT(CDeskBand, IOleWindow),
		QITABENT(CDeskBand, IDockingWindow),
		QITABENT(CDeskBand, IDeskBand),
		QITABENT(CDeskBand, IDeskBand2),
		QITABENT(CDeskBand, IPersist),
		QITABENT(CDeskBand, IPersistStream),
		QITABENT(CDeskBand, IObjectWithSite),
		QITABENT(CDeskBand, IInputObject),
		{ 0 },
	};
	return QISearch(this, qitab, riid, ppv);
}

STDMETHODIMP_(ULONG) CDeskBand::AddRef()
{
	return InterlockedIncrement(&m_lRef);
}

STDMETHODIMP_(ULONG) CDeskBand::Release()
{
	auto l = InterlockedDecrement(&m_lRef);
	if (!l) delete this;
	return l;
}

STDMETHODIMP CDeskBand::GetWindow(HWND *phwnd)
{
	if (!phwnd)return E_INVALIDARG;
	*phwnd = m_hWnd;
	return S_OK;
}

STDMETHODIMP CDeskBand::ShowDW(BOOL fShow)
{
	if (m_hWnd)
		::ShowWindow(m_hWnd, fShow ? SW_SHOW : SW_HIDE);
	return S_OK;
}

STDMETHODIMP CDeskBand::CloseDW(DWORD)
{
	if (m_hWnd)
	{
		DestroyWindow(m_hWnd);
		m_hWnd = nullptr;
	}
	return S_OK;
}

STDMETHODIMP CDeskBand::GetBandInfo(DWORD dwBandID, DWORD, DESKBANDINFO *pdbi)
{
	if (!pdbi)
		return E_INVALIDARG;
	else
	{
		m_dwBandID = dwBandID;
		if (pdbi->dwMask & DBIM_MINSIZE)
		{
			pdbi->ptMinSize.x = 80;
			pdbi->ptMinSize.y = 30;
		}
		if (pdbi->dwMask & DBIM_MAXSIZE)
			pdbi->ptMaxSize.y = -1;
		if (pdbi->dwMask & DBIM_INTEGRAL)
			pdbi->ptIntegral.y = 1;

		if (pdbi->dwMask & DBIM_ACTUAL)
		{
			pdbi->ptActual.x = 80;
			pdbi->ptActual.y = 30;
		}
		if (pdbi->dwMask & DBIM_TITLE)
			pdbi->dwMask &= ~DBIM_TITLE;
		if (pdbi->dwMask & DBIM_MODEFLAGS)
			pdbi->dwModeFlags = DBIMF_NORMAL | DBIMF_VARIABLEHEIGHT;
		if (pdbi->dwMask & DBIM_BKCOLOR)
			pdbi->dwMask &= ~DBIM_BKCOLOR;
		return S_OK;
	}
}

STDMETHODIMP CDeskBand::CanRenderComposited(BOOL *pfCanRenderComposited)
{
	return DwmIsCompositionEnabled(pfCanRenderComposited);
}

STDMETHODIMP CDeskBand::SetCompositionState(BOOL fCompositionEnabled)
{
	m_bCanCompositionEnabled = fCompositionEnabled;
	InvalidateRect(m_hWnd, nullptr, TRUE);
	UpdateWindow(m_hWnd);
	return S_OK;
}

STDMETHODIMP CDeskBand::GetCompositionState(BOOL *pfCompositionEnabled)
{
	if (!pfCompositionEnabled)return E_INVALIDARG;
	*pfCompositionEnabled = m_bCanCompositionEnabled;
	return S_OK;
}

STDMETHODIMP CDeskBand::GetClassID(CLSID *pclsid)
{
	if (!pclsid)return E_INVALIDARG;
	*pclsid = CLSID_DeskBand;
	return S_OK;
}

STDMETHODIMP CDeskBand::IsDirty()
{
	return m_bIsDirty ? S_OK : S_FALSE;
}

STDMETHODIMP CDeskBand::Save(IStream *, BOOL fClearDirty)
{
	if (fClearDirty)
		m_bIsDirty = FALSE;
	return S_OK;
}

STDMETHODIMP CDeskBand::SetSite(IUnknown *pUnkSite)
{
	if (m_pSite)m_pSite->Release();
	if (!pUnkSite)return S_OK;
	HRESULT hr = S_OK;
	do
	{
		IOleWindow *pow;
		hr = pUnkSite->QueryInterface(IID_IOleWindow, reinterpret_cast<void **>(&pow));
		if (FAILED(hr)) break;
		m_hWndParent = nullptr;
		hr = pow->GetWindow(&m_hWndParent);
		if (FAILED(hr))break;
		WNDCLASS wc =
		{
			CS_HREDRAW | CS_VREDRAW, __WndProc, 0, 0, g_hInst, nullptr,
			LoadCursor(nullptr, IDC_ARROW), (HBRUSH)::GetStockObject(BLACK_BRUSH),
			nullptr, sm_lpszClassName
		};
		hr = E_FAIL;
		if (!RegisterClass(&wc))break;
		m_bIsRegisterClassed = true;
		m_hWnd = CreateWindow(sm_lpszClassName, nullptr,
			WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
			0, 0, 0, 0, m_hWndParent, nullptr, g_hInst, this);
		if (!m_hWnd)break;
		pow->Release();
		hr = pUnkSite->QueryInterface(IID_IInputObjectSite,
			reinterpret_cast<void **>(&m_pSite));
	} while (false);
	return hr;
}

STDMETHODIMP CDeskBand::GetSite(REFIID riid, void **ppv)
{
	if (m_pSite)
		return m_pSite->QueryInterface(riid, ppv);
	else if (!ppv)
		return E_INVALIDARG;
	else
	{
		*ppv = nullptr;
		return E_FAIL;
	}
}

STDMETHODIMP CDeskBand::UIActivateIO(BOOL fActivate, MSG *)
{
	if (fActivate)SetFocus(m_hWnd);
	return S_OK;
}

STDMETHODIMP CDeskBand::HasFocusIO()
{
	return m_bHasFocus ? S_OK : S_FALSE;
}

LRESULT CALLBACK CDeskBand::__WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static CDeskBand* pDeskBand = nullptr;
	if (WM_NCCREATE == uMsg)
	{
		LPCREATESTRUCT lp = reinterpret_cast<LPCREATESTRUCT>(lParam);
		if (lp)
			pDeskBand = static_cast<CDeskBand*>(lp->lpCreateParams);
	}
	else if (pDeskBand)
	{
		switch (uMsg)
		{
		case WM_CREATE:return pDeskBand->__OnCreate(hWnd);
		case WM_SETFOCUS:return pDeskBand->__OnFocus(TRUE);
		case WM_KILLFOCUS:return pDeskBand->__OnFocus(FALSE);
		case WM_PAINT:return pDeskBand->__OnPaint(hWnd);
		case WM_PRINTCLIENT:return pDeskBand->__OnPaint(hWnd, reinterpret_cast<HDC>(wParam));
		case WM_ERASEBKGND:if (!pDeskBand->m_bCanCompositionEnabled)break; return 1;
		case WM_DESTROY:return pDeskBand->__OnDestroy(hWnd);
		case WM_TIMER:if (wParam != pDeskBand->nTIMER_ID)break; return pDeskBand->__OnTimer(hWnd);
		case WM_RBUTTONUP:return pDeskBand->__OnRButtonUp(hWnd);
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case ID_RESET: return pDeskBand->__OnMenuReset();
			case ID_CLOSE: return pDeskBand->__OnMenuClose(hWnd);
			}
			break;
		}
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CDeskBand::__OnCreate(HWND hWnd)
{
	SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	HDC hdc = GetDC(hWnd);
	m_hFont = CreateFont(-MulDiv(8, GetDeviceCaps(hdc, LOGPIXELSY), 72), 6,
		0, 0, FW_NORMAL, FALSE, FALSE, FALSE, OEM_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, TEXT("Consolas UI"));
	ReleaseDC(hWnd, hdc);
	m_hToolTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
		WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON | TTS_NOPREFIX,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		hWnd, nullptr, g_hInst, nullptr);
	SetWindowPos(m_hToolTip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	TOOLINFO ti = { sizeof(TOOLINFO), TTF_SUBCLASS | TTF_IDISHWND, m_hWndParent, (UINT_PTR)hWnd };
	ti.hinst = g_hInst;
	ti.lpszText = TEXT("");
	GetClientRect(hWnd, &ti.rect);
	SendMessage(m_hToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
	SendMessage(m_hToolTip, TTM_SETTITLE, TTI_NONE, (LPARAM)TEXT("详细信息"));
	SendMessage(m_hToolTip, TTM_SETMAXTIPWIDTH, 0, SHRT_MAX);
	//SendMessage(m_hToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 10000);
	//SendMessage(m_hToolTip, TTM_SETDELAYTIME, TTDT_INITIAL, 1000);
	m_hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU_MAIN));
	SetTimer(hWnd, nTIMER_ID, 1000, nullptr);
	return 0;
}

LRESULT CDeskBand::__OnPaint(HWND hWnd, HDC _hdc)
{
	HDC hdc = _hdc;
	PAINTSTRUCT paint;
	RECT &rc = paint.rcPaint;
	if (!_hdc)
		hdc = BeginPaint(hWnd, &paint);
	else
		GetClientRect(hWnd, &rc);

	try
	{
		if (hdc)
		{
			if (m_bCanCompositionEnabled)
			{
				HTHEME hTheme = OpenThemeData(nullptr, L"Button");
				if (hTheme)
				{
					HDC hdcPaint = nullptr;
					HPAINTBUFFER hBufferPaint =
						BeginBufferedPaint(hdc, &rc,
							BP_BUFFERFORMAT::BPBF_TOPDOWNDIB, nullptr, &hdcPaint);
					DrawThemeParentBackground(hWnd, hdcPaint, &rc);
					EndBufferedPaint(hBufferPaint, TRUE);
					CloseThemeData(hTheme);
				}
				int nOldBkMode = ::SetBkMode(hdc, TRANSPARENT);
				HGLOBAL hOldFont = nullptr;
				if (m_hFont) hOldFont = ::SelectObject(hdc, m_hFont);
				SIZE sz;
				const auto &str = m_iMonitors[0]->ToString();
				if (str.length() < 1) return 1;
				GetTextExtentPoint(hdc, str.c_str(), (int)str.length(), &sz);
				LONG barH = (rc.bottom - rc.top) / (LONG)_countof(m_iMonitors);
				LONG barW = rc.right - rc.left;
				LONG txtH = sz.cy > 0 && sz.cy < barH ? sz.cy : barH;
				RECT rcRect = {
					rc.left,
					rc.top + 2,
					rc.left + 4,
					rc.bottom - 2
				};
				LONG rcH = rcRect.bottom - rcRect.top;
				RECT rcText =
				{
					rc.left + 8,
					rc.top + (barH - txtH) / 2 + 1,
					rc.right - 8,
					rc.top + barH - 1
				};
				std::wstring sout;
				for (size_t i = 0; i < _countof(m_iMonitors); i++)
				{
					if (!m_iMonitors[i])
					{
						continue;
					}

					//CPU使用率
					if (i == 0)
					{
						rcRect.top = rc.bottom - 2 - (LONG)ceil(rcH * m_iMonitors[i]->GetValue() / 100.0);
						FillRect(hdc, &rcRect, hbrTotal);
						rcRect.bottom = rcRect.top - 1;
						rcRect.top = rc.top + 2;
						if(rcRect.top < rcRect.bottom) FillRect(hdc, &rcRect, hbrUsed);
					}
					//内存使用率
					if (i == 1)
					{
						rcRect.top = rc.top + 2;
						rcRect.left += barW - 8;
						rcRect.right += barW - 8;
						rcRect.bottom = rc.bottom - 2;
						rcRect.top = rc.bottom - 2 - (LONG)ceil(rcH * m_iMonitors[i]->GetValue() / 100.0);
						FillRect(hdc, &rcRect, hbrTotal);
						rcRect.bottom = rcRect.top - 1;
						rcRect.top = rc.top + 2;
						if (rcRect.top < rcRect.bottom) FillRect(hdc, &rcRect, hbrUsed);
					}
					
					//数据
					const auto &str = m_iMonitors[i]->ToString();
					if (str.length() < 1) continue;
					SetTextColor(hdc, RGB(255, 255, 255));
					DrawText(hdc, str.c_str(), (int)str.length(), &rcText, 0);
					rcText.top += barH;
					rcText.bottom += barH;

					//Tip数据
					sout += __ChangeString(m_iMonitors[i]->ToLongString()) + L"\n";
				}

				TOOLINFOW ti = { sizeof(TOOLINFOW), 0, m_hWndParent, (UINT_PTR)hWnd };
				ti.hinst = g_hInst;
				ti.lpszText = (LPWSTR)sout.c_str();
				SendMessageW(m_hToolTip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
				if (m_hFont) ::SelectObject(hdc, hOldFont);
				::SetBkMode(hdc, nOldBkMode);
			}
		}
	}
	catch (...) {}

	if (!_hdc)
		EndPaint(hWnd, &paint);
	return 0;
}

LRESULT CDeskBand::__OnFocus(BOOL fFocus)
{
	m_bHasFocus = fFocus;
	if (m_pSite)
		m_pSite->OnFocusChangeIS(dynamic_cast<IOleWindow*>(this), m_bHasFocus);
	return 0;
}

LRESULT CDeskBand::__OnDestroy(HWND hWnd)
{
	KillTimer(hWnd, nTIMER_ID);
	if (m_hFont)
	{
		DeleteObject(m_hFont);
		m_hFont = nullptr;
	}
	if (m_hMenu)
	{
		DestroyMenu(m_hMenu);
		m_hMenu = nullptr;
	}
	return 0;
}

LRESULT CDeskBand::__OnTimer(HWND hWnd)
{
	try
	{
		for (size_t i = 0; i < _countof(m_iMonitors); ++i)
		{
			if (!m_iMonitors[i])
				__Init(i);
			else
				m_iMonitors[i]->Update();
		}
		HDC hdc = GetDC(hWnd);
		__OnPaint(hWnd, hdc);
		ReleaseDC(hWnd, hdc);
	}
	catch (...) {}
	return 0;
}

LRESULT CDeskBand::__OnRButtonUp(HWND hWnd)
{
	if (m_hMenu)
	{
		POINT pt;
		GetCursorPos(&pt);
		RECT rc;
		GetClientRect(hWnd, &rc);
		TrackPopupMenu(GetSubMenu(m_hMenu, 0), 0, pt.x, pt.y, 0, hWnd, &rc);
	}
	return 0;
}

LRESULT CDeskBand::__OnMenuReset()
{
	for (auto& m : m_iMonitors)
		if (m)
			m->Reset();
	return 0;
}

LRESULT CDeskBand::__OnMenuClose(HWND hWnd)
{
	DllShowMonitorBar(FALSE);
	return 0;
}

const std::wstring CDeskBand::__ChangeString(const std::wstring& str)const
{
	std::wostringstream ret;
	for (const auto& s : str)
	{
		if (s == L' ')
			ret << L"  ";
		else
			ret << s;
	}
	return ret.str();
}