/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <algorithm>
#include <cmath>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma comment(lib, "imm32")
#include <CommCtrl.h>
#include <Shlwapi.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "color_abgr.hpp"
namespace gdi = sigma_lib::W32::GDI;

////////////////////////////////
// 主要情報源の変数アドレス．
////////////////////////////////
inline constinit struct ExEdit092 {
	AviUtl::FilterPlugin* fp;
	constexpr static auto info_exedit092 = "拡張編集(exedit) version 0.92 by ＫＥＮくん";
	bool init(AviUtl::FilterPlugin* this_fp)
	{
		if (fp != nullptr) return true;

		AviUtl::SysInfo si; this_fp->exfunc->get_sys_info(nullptr, &si);

		for (int i = 0; i < si.filter_n; i++) {
			auto that_fp = this_fp->exfunc->get_filterp(i);
			if (that_fp->information != nullptr &&
				0 == std::strcmp(that_fp->information, info_exedit092)) {
				fp = that_fp;
				init_pointers();
				return true;
			}
		}
		return false;
	}

	int32_t*	layer_size_mode;		// 0x1539d4; 0: large, 1: medium, 2: small.
	int32_t(*	layer_size_preset)[3];	// 0x0a3e08
	int32_t(*	midpt_mk_sz_preset)[3];	// 0x0a3e14
	int32_t*	layer_size;				// 0x0a3e20
	int32_t*	midpt_marker_size;		// 0x0a3e24

private:
	void init_pointers()
	{
		auto pick_addr = [exedit_base = reinterpret_cast<uintptr_t>(fp->dll_hinst)]
			<class T>(T& target, ptrdiff_t offset) { target = reinterpret_cast<T>(exedit_base + offset); };

		pick_addr(layer_size_mode,			0x1539d4);
		pick_addr(layer_size_preset,		0x0a3e08);
		pick_addr(midpt_mk_sz_preset,		0x0a3e14);
		pick_addr(layer_size,				0x0a3e20);
		pick_addr(midpt_marker_size,		0x0a3e24);
	}
} exedit;


////////////////////////////////
// データ授受の中核クラス．
////////////////////////////////
struct layer_sizes {
	int32_t L, M, S;
	int32_t& operator[](int32_t i) {
		return i > 0 ? i > 1 ? S : M : L;
	}
};
static constinit struct GuiData {
	int32_t mode_curr = 0;
	int32_t size_curr = 0, size_pend = 0;
	AviUtl::FilterPlugin* fp = nullptr;
	layer_sizes def_layer_sizes{ 0, 0, 0 };

	void init(AviUtl::FilterPlugin* this_fp) {
		fp = this_fp;
	}
	void exit() {
		kill_timer();
	}
	// returns whether the state had changed by pulling data.
	bool pull()
	{
		kill_timer();

		if (auto mode_prev = std::exchange(mode_curr, *exedit.layer_size_mode);
			mode_prev != mode_curr) {
			size_curr = (*exedit.layer_size_preset)[mode_curr];
			return true;
		}

		if (auto size_prev = std::exchange(size_curr, (*exedit.layer_size_preset)[mode_curr]);
			size_prev != size_curr) {
			return true;
		}

		return false;
	}
	// returns whether the new value is (or is being) applied.
	bool push(int32_t size_new, int delay)
	{
		if (size_pend == size_new) return false;
		if (size_curr == size_new) {
			kill_timer();
			return false;
		}

		if (delay <= 0) return push_core(size_new);

		size_pend = size_new;
		::SetTimer(fp->hwnd, timer_id(), delay, on_timer);
		return true;
	}
	bool flush()
	{
		if (!is_timer_active()) return false;

		::KillTimer(fp->hwnd, timer_id());
		on_timer();
		return true;
	}
	bool set_mode(int32_t mode)
	{
		if (mode_curr == mode) return false;
		flush();

		mode_curr = *exedit.layer_size_mode = mode;
		*exedit.midpt_marker_size = (*exedit.midpt_mk_sz_preset)[mode];
		return push_core((*exedit.layer_size_preset)[mode]);
	}

	void load(int32_t prev_L, int32_t prev_M, int32_t prev_S) {
		def_layer_sizes = save();

		if (prev_L > 0) (*exedit.layer_size_preset)[0] = prev_L;
		if (prev_M > 0) (*exedit.layer_size_preset)[1] = prev_M;
		if (prev_S > 0) (*exedit.layer_size_preset)[2] = prev_S;

		*exedit.layer_size = (*exedit.layer_size_preset)[*exedit.layer_size_mode];
	}
	static layer_sizes save() {
		return {
			.L = (*exedit.layer_size_preset)[0],
			.M = (*exedit.layer_size_preset)[1],
			.S = (*exedit.layer_size_preset)[2],
		};
	}

	constexpr int32_t gui_value() const { return is_timer_active() ? size_pend : size_curr; }

private:
	uintptr_t timer_id() const { return reinterpret_cast<uintptr_t>(this); }
	constexpr bool is_timer_active() const { return size_pend > 0; }
	void kill_timer() {
		if (!is_timer_active()) return;

		::KillTimer(fp->hwnd, timer_id());
		size_pend = 0;
	}
	static void CALLBACK on_timer(HWND hwnd, UINT, UINT_PTR id, DWORD)
	{
		::KillTimer(hwnd, id);
		if (auto that = reinterpret_cast<GuiData*>(id);
			that != nullptr && that->is_timer_active() && hwnd == that->fp->hwnd)
			that->on_timer();
	}
	void on_timer()
	{
		push_core(std::exchange(size_pend, 0));
	}

	bool push_core(int32_t size_new)
	{
		if (mode_curr != *exedit.layer_size_mode) return pull();
		if (size_curr == size_new) return false;

		*exedit.layer_size = (*exedit.layer_size_preset)[mode_curr] = size_curr = size_new;
		recalc_window_size();

		return true;
	}

	int32_t prev_tl_ht1 = -1, prev_tl_ht2 = -1;
	void recalc_window_size()
	{
		// promote redraw.
		::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);

		// resize the window to fit with integral number of layers.
		RECT rc;
		::GetWindowRect(exedit.fp->hwnd, &rc);

		if (prev_tl_ht2 == rc.bottom - rc.top)
			// if the height didn't change from the last adjustment,
			// restore the previous original height.
			rc.bottom = rc.top + prev_tl_ht1;
		else prev_tl_ht1 = rc.bottom - rc.top; // backup the original height otherwise.

		// let exedit window adjust the window height.
		::SendMessageW(exedit.fp->hwnd, WM_SIZING, WMSZ_BOTTOM, reinterpret_cast<LPARAM>(&rc));
		prev_tl_ht2 = rc.bottom - rc.top; // backup the adjusted height.

		// then change the window size.
		::SetWindowPos(exedit.fp->hwnd, nullptr, 0, 0,
			rc.right - rc.left, rc.bottom - rc.top, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
	}
} gui_data;


////////////////////////////////
// 設定項目．
////////////////////////////////
static constinit struct Settings {
	enum class Orientation : byte {
		L2R = 0, R2L = 1, T2B = 2, B2T = 3,
	};
	enum class Scale : byte {
		linear = 0, logarithmic = 1, harmonic = 2,
	};

	struct {
		gdi::Color
			fill_lt{ 0x80, 0xc0, 0xff },
			fill_rb{ 0x00, 0x80, 0xff },
			back_lt{ 0x00, 0x00, 0x40 },
			back_rb{ 0x00, 0x00, 0xc0 };
	} color;

	struct {
		Orientation orientation = Orientation::T2B;
		byte margin_x = 0, margin_y = 0;
		static constexpr byte coerce_margin(int32_t margin) {
			constexpr int32_t min = 0, max = 64;
			return std::clamp(margin, min, max);
		}
	} layout;

	struct {
		bool show = true;
		int8_t offset_x = 0, offset_y = 24;
		static constexpr int8_t coerce_offset(int32_t offset) {
			constexpr int32_t min = -128, max = 127;
			return std::clamp(offset, min, max);
		}
		gdi::Color text_color{ -1 };
	} tooltip;

	struct {
		Scale scale = Scale::logarithmic;

		byte size_min = 15, size_max = 50;
		constexpr static int32_t size_min_min = 4;
		static constexpr byte coerce_size(int32_t size) {
			constexpr int32_t min = size_min_min, max = 255;
			return std::clamp(size, min, max);
		}

		bool hide_cursor = false;

		int16_t delay_ms = 10;
		static constexpr int16_t coerce_delay(int32_t delay) {
			constexpr int32_t min = USER_TIMER_MINIMUM, max = 1'000;
			if (delay <= 0) return 0;
			return std::clamp(delay, min, max);
		}

		int8_t wheel = +1;
		static constexpr int8_t coerce_wheel(int32_t wheel) {
			constexpr int32_t min = -1, max = +1;
			return std::clamp(wheel, min, max);
		}
	} behavior;

	// helper functions for variants of scales.
	double scale_from_val(int32_t value) const {
		switch (behavior.scale) {
		case Scale::linear:
		default:
			return (value - behavior.size_min) / double(behavior.size_max - behavior.size_min);
		case Scale::logarithmic:
			return std::log(value / double(behavior.size_min)) / std::log(behavior.size_max / double(behavior.size_min));
		case Scale::harmonic:
			return (value - behavior.size_min) / double(behavior.size_max - behavior.size_min) * behavior.size_max / value;
		}
	}
	int32_t scale_to_val(double pos) const {
		return static_cast<int32_t>(std::round([&, this] {
			switch (behavior.scale) {
			case Scale::linear:
			default:
				return pos * (behavior.size_max - behavior.size_min) + behavior.size_min;
			case Scale::logarithmic:
				return  std::exp(pos * std::log(behavior.size_max / double(behavior.size_min))) * behavior.size_min;
			case Scale::harmonic:
				return behavior.size_min * behavior.size_max / (pos * (behavior.size_min - behavior.size_max) + behavior.size_max);
			}
		}()));
	}

	void load(char const* inifile)
	{
		auto read_int = [=](char const* section, char const* key, int def) -> int {
			return ::GetPrivateProfileIntA(section, key, def, inifile);
		};
	#define load_int(hdr, fld, sec, coerce)	hdr##fld = coerce(read_int(sec, #fld, static_cast<int>(hdr##fld)))
	#define load_col(hdr, fld, sec)			hdr##fld = gdi::Color::fromARGB(read_int(sec, #fld, hdr##fld.to_formattable()))
	#define load_bool(hdr, fld, sec)		hdr##fld = (0 != read_int(sec, #fld, hdr##fld ? 1 : 0))
	#define load_enum(hdr, fld, sec)		load_int(hdr, fld, sec, static_cast<decltype(hdr##fld)>)

		load_enum(layout., orientation,	"layout");
		load_int(layout., margin_x,		"layout", layout.coerce_margin);
		load_int(layout., margin_y,		"layout", layout.coerce_margin);

		load_col(color., fill_lt,		"color");
		load_col(color., fill_rb,		"color");
		load_col(color., back_lt,		"color");
		load_col(color., back_rb,		"color");

		load_enum(behavior., scale,		"behavior");
		load_int(behavior., size_min,	"behavior", behavior.coerce_size);
		load_int(behavior., size_max,	"behavior", behavior.coerce_size);
		load_bool(behavior., hide_cursor, "behavior");
		load_int(behavior., delay_ms,	"behavior", behavior.coerce_delay);
		load_int(behavior., wheel,		"behavior", behavior.coerce_wheel);

		// make sure that size_min < size_max.
		if (behavior.size_min >= behavior.size_max) {
			if (behavior.size_min > behavior.size_min_min) behavior.size_min--;
			behavior.size_max = behavior.size_min + 1;
		}

		load_bool(tooltip., show,		"tooltip");
		load_int(tooltip., offset_x,	"tooltip", tooltip.coerce_offset);
		load_int(tooltip., offset_y,	"tooltip", tooltip.coerce_offset);
		load_col(tooltip., text_color,	"tooltip");

	#undef load_enum
	#undef load_bool
	#undef load_col
	#undef load_int

		// load the previous states.
		int L = read_int("states", "L", 0),
			M = read_int("states", "M", 0),
			S = read_int("states", "S", 0);
		if (L > 0) L = behavior.coerce_size(L);
		if (M > 0) M = behavior.coerce_size(M);
		if (S > 0) S = behavior.coerce_size(S);
		gui_data.load(L, M, S);
	}
	void save(char const* inifile)
	{
		auto write_int = [=](char const* section, char const* key, int32_t val) {
			char buf[16]; ::sprintf_s(buf, "%d", val);
			::WritePrivateProfileStringA(section, key, buf, inifile);
		};

	#define save_int(fld)	write_int("states", #fld, fld)

		// save the current states.
		auto [L, M, S] = gui_data.save();
		save_int(L);
		save_int(M);
		save_int(S);

	#undef save_int
	}
} settings;


////////////////////////////////
// 設定ロードセーブ．
////////////////////////////////

// replacing a file extension when it's known.
template<class T, size_t len_max, size_t len_old, size_t len_new>
static void replace_tail(T(&dst)[len_max], size_t len, const T(&tail_old)[len_old], const T(&tail_new)[len_new])
{
	if (len < len_old || len - len_old + len_new > len_max) return;
	std::memcpy(dst + len - len_old, tail_new, len_new * sizeof(T));
}
static inline void load_settings(HMODULE h_dll)
{
	char path[MAX_PATH];
	replace_tail(path, ::GetModuleFileNameA(h_dll, path, std::size(path)) + 1, "auf", "ini");

	settings.load(path);
}
static inline void save_settings(HMODULE h_dll)
{
	char path[MAX_PATH];
	replace_tail(path, ::GetModuleFileNameA(h_dll, path, std::size(path)) + 1, "auf", "ini");

	settings.save(path);
}


////////////////////////////////
// ツールチップ．
////////////////////////////////
static struct {
	HWND hwnd = nullptr;
	constexpr bool is_active() const { return hwnd != nullptr; }

	// create and initialize a tooltip.
	void init()
	{
		if (!settings.tooltip.show || is_active()) return;

		hwnd = ::CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, TOOLTIPS_CLASSA, nullptr,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			gui_data.fp->hwnd, nullptr, gui_data.fp->hinst_parent, nullptr);

		info.hwnd = gui_data.fp->hwnd;
		info.hinst = gui_data.fp->hinst_parent;
		info.lpszText = const_cast<wchar_t*>(L"");
		info.uId = reinterpret_cast<uintptr_t>(info.hwnd);

		::SendMessageW(hwnd, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));

		// required for multi-line tooltips.
		::SendMessageW(hwnd, TTM_SETMAXTIPWIDTH, 0, (1 << 15) - 1);
	}
	// destroy the tooltip if exists.
	void exit()
	{
		if (is_active()) {
			::DestroyWindow(hwnd);
			hwnd = nullptr;
		}
	}

	// turn the tooltip visible.
	void show(int x, int y, int32_t value) {
		if (!is_active()) return;

		change(value); // change the content first, or the tooltip is kept hidden otherwise.
		move(x, y);
		::SendMessageW(hwnd, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&info));
	}
	// turn the tooltip hidden.
	void hide() const {
		if (!is_active()) return;

		::SendMessageW(hwnd, TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&info));
	}
	// change the position.
	void move(int x, int y) const {
		if (!is_active()) return;

		POINT pt{ x, y };
		::ClientToScreen(info.hwnd, &pt);
		::SendMessageW(hwnd, TTM_TRACKPOSITION, 0,
			((pt.x + settings.tooltip.offset_x) & 0xffff) | ((pt.y + settings.tooltip.offset_y) << 16));
	}
	// change the content string.
	void change(int32_t value) {
		if (!is_active()) return;

		wchar_t buf[16]; ::swprintf_s(buf, L"%d", value);
		info.lpszText = buf;
		::SendMessageW(hwnd, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&info));
	}

private:
	// using TTTOOLINFOW_V1_SIZE to allow TTF_TRACK work for common controls version < 6.
	TTTOOLINFOW info{
		.cbSize = TTTOOLINFOW_V1_SIZE /* sizeof(info) */,
		.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE | TTF_TRANSPARENT,
	};
} tooltip;


////////////////////////////////
// ドラッグ操作の補助クラス．
////////////////////////////////
static constinit struct {
	bool active = false;
	int32_t drag_pos = 0; // client coordinate, clamped.
	int32_t prev_size = 0;

	int prev_x = 0, prev_y = 0;

	void mouse_down(int x, int y, bool& redraw)
	{
		if (active) return;
		prev_x = x; prev_y = y;

		// backup the starting state.
		prev_size = gui_data.gui_value();

		// transform the position to the value.
		auto [p, s] = get_pos_scale(x, y);
		drag_pos = p;

		// start dragging.
		active = true;
		::SetCapture(gui_data.fp->hwnd);

		// push the mouse position.
		auto value = settings.scale_to_val(s);
		gui_data.push(value, 0);
		redraw = true;

		// show the tooltip.
		tooltip.show(x, y, value);

		// hide the cursor if specified so.
		if (settings.behavior.hide_cursor)
			::SetCursor(nullptr);
	}

	void mouse_up(int x, int y, bool& redraw)
	{
		if (!active) return;

		// finish dragging.
		active = false;
		::ReleaseCapture();

		// make the dragging take effect.
		gui_data.flush();
		redraw = true;

		// hide the tooltip.
		tooltip.hide();
	}

	void mouse_move(int x, int y, bool& redraw)
	{
		if (!active) return;
		if (x == prev_x && y == prev_y) return;
		prev_x = x; prev_y = y;
		tooltip.move(x, y);

		// transform the position to the value.
		auto [p, s] = get_pos_scale(x, y);
		if (drag_pos == p) return;
		drag_pos = p;

		// push the mouse position.
		auto value = settings.scale_to_val(s);
		gui_data.push(value, settings.behavior.delay_ms);
		redraw = true;

		// update the tooltip.
		tooltip.change(value);
	}

	void cancel(bool& redraw)
	{
		// either user right-clicked or mouse capture is unexpectedly lost.
		if (!active) return;
		active = false;
		if (::GetCapture() == gui_data.fp->hwnd) ::ReleaseCapture();

		// rewind to the state at the drag start.
		gui_data.push(prev_size, 0);
		redraw = true;

		// hide the tooltip.
		tooltip.hide();
	}

private:
	static std::pair<int, double> pt_to_pos_scale(int x, int y, int w, int h)
	{
		int v, l;
		switch (settings.layout.orientation) {
			using enum Settings::Orientation;
		default:
		case L2R: v = x; l = +w; break;
		case R2L: v = x; l = -w; break;
		case T2B: v = y; l = +h; break;
		case B2T: v = y; l = -h; break;
		}

		v = std::clamp(v, 0, l < 0 ? -l : l);
		double pos = v / double(l);
		if (l < 0) pos += 1.0;

		return { v, pos };
	}
	static std::tuple<int, int, int, int> get_xywh(int x, int y)
	{
		RECT rc; ::GetClientRect(gui_data.fp->hwnd, &rc);
		return {
			x - settings.layout.margin_x, y - settings.layout.margin_y,
			std::max<int>(rc.right - 2 * settings.layout.margin_x, 1),
			std::max<int>(rc.bottom - 2 * settings.layout.margin_y, 1)
		};
	}
	static std::pair<int, double> get_pos_scale(int x, int y) {
		auto [X, Y, w, h] = get_xywh(x, y);
		return pt_to_pos_scale(X, Y, w, h);
	}
} drag_state;


////////////////////////////////
// コンテキストメニュー表示．
////////////////////////////////
static inline bool contextmenu(HWND hwnd, int x, int y)
{
	// prepare a context menu.
	HMENU menu = ::CreatePopupMenu();

	constexpr uintptr_t id_to_L = 1, id_to_M = 2, id_to_S = 3, id_reset = 4, id_reset_all = 5;
	wchar_t buf[16];
	::swprintf_s(buf, L"大 (%d)", (*exedit.layer_size_preset)[0]);
	::AppendMenuW(menu, MF_STRING, id_to_L, buf);
	::swprintf_s(buf, L"中 (%d)", (*exedit.layer_size_preset)[1]);
	::AppendMenuW(menu, MF_STRING, id_to_M, buf);
	::swprintf_s(buf, L"小 (%d)", (*exedit.layer_size_preset)[2]);
	::AppendMenuW(menu, MF_STRING, id_to_S, buf);
	::CheckMenuRadioItem(menu, 0, 2, gui_data.mode_curr, MF_BYPOSITION);
	::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
	::AppendMenuW(menu, MF_STRING, id_reset, L"リセット");
	::AppendMenuW(menu, MF_STRING, id_reset_all, L"全てリセット");

	// show the context menu.
	uintptr_t id = ::TrackPopupMenu(menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON,
		x, y, 0, hwnd, nullptr);
	::DestroyMenu(menu);

	// process the returned command.
	switch (id) {
	case id_to_L: return gui_data.set_mode(0);
	case id_to_M: return gui_data.set_mode(1);
	case id_to_S: return gui_data.set_mode(2);

	case id_reset:
	on_reset_current:
		// rewind the size of the currently selected mode.
		return gui_data.push(gui_data.def_layer_sizes[gui_data.mode_curr], 0);

	case id_reset_all:
	{
		// rewind the sizes of all modes.
		for (int i : {0, 1, 2}) {
			if (i != gui_data.mode_curr)
				(*exedit.layer_size_preset)[i] = gui_data.def_layer_sizes[i];
		}
		goto on_reset_current;
	}
	}

	return false;
}


////////////////////////////////
// GUI 描画．
////////////////////////////////
static inline void grad_fill2(RECT const& rc1, gdi::Color c11, gdi::Color c12, 
	RECT const& rc2, gdi::Color c21, gdi::Color c22, bool vert, HDC dc)
{
	constexpr GRADIENT_RECT gr[] = { {.UpperLeft = 0, .LowerRight = 1 }, {.UpperLeft = 2, .LowerRight = 3 }, };
	constexpr auto tv = [](int x, int y, gdi::Color c) -> TRIVERTEX {
		return { .x = x, .y = y, .Red = static_cast<uint16_t>(c.R << 8), .Green = static_cast<uint16_t>(c.G << 8), .Blue = static_cast<uint16_t>(c.B << 8) };
	};
	TRIVERTEX v[] = {
		tv(rc1.left, rc1.top, c11), tv(rc1.right, rc1.bottom, c12),
		tv(rc2.left, rc2.top, c21), tv(rc2.right, rc2.bottom, c22),
	};
	::GdiGradientFill(dc, v, 4, const_cast<GRADIENT_RECT*>(gr), 2,
		vert ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H);
}
static inline void draw_gauge(int pos, RECT const& bound, HDC dc)
{
	// determine the filling region and the orientation.
	RECT fill{ bound }, back{ bound };
	bool vert;

	switch (settings.layout.orientation) {
		using enum Settings::Orientation;
	default:
	case L2R: fill.right = back.left = pos + bound.left; vert = true; break;
	case R2L: fill.left = back.right = pos + bound.left; vert = true; break;
	case T2B: fill.bottom = back.top = pos + bound.top; vert = false; break;
	case B2T: fill.top = back.bottom = pos + bound.top; vert = false; break;
	}

	// fill with ::GdiGradientFill().
	grad_fill2(fill, settings.color.fill_lt, settings.color.fill_rb,
		back, settings.color.back_lt, settings.color.back_rb, vert, dc);
}
static inline void draw()
{
	// get the drawing area.
	RECT rc; ::GetClientRect(gui_data.fp->hwnd, &rc);
	rc.left += settings.layout.margin_x; rc.right -= settings.layout.margin_x; rc.right = std::max(rc.left + 1, rc.right);
	rc.top += settings.layout.margin_y; rc.bottom -= settings.layout.margin_y; rc.bottom = std::max(rc.top + 1, rc.bottom);
	auto const w = rc.right - rc.left, h = rc.bottom - rc.top;

	// determine the position of the border.
	int gauge_pos;
	if (drag_state.active) gauge_pos = drag_state.drag_pos;
	else {
		auto pos = std::clamp(settings.scale_from_val(gui_data.gui_value()), 0.0, 1.0);
		constexpr auto rnd = [](double x) { return static_cast<int>(std::round(x)); };
		switch (settings.layout.orientation) {
			using enum Settings::Orientation;
		default:
		case L2R: gauge_pos = rnd(w * pos); break;
		case R2L: gauge_pos = rnd(w * (1 - pos)); break;
		case T2B: gauge_pos = rnd(h * pos); break;
		case B2T: gauge_pos = rnd(h * (1 - pos)); break;
		}
	}

	// as the drawing is such simple we don't need double buffering.
	HDC dc = ::GetDC(gui_data.fp->hwnd);
	draw_gauge(gauge_pos, rc, dc);
	::ReleaseDC(gui_data.fp->hwnd, dc);
}


////////////////////////////////
// メニューコマンドの定義．
////////////////////////////////
namespace menu_commands
{
	constexpr struct {
		int32_t id; char const* title;
	}	decr_size{ 1, "レイヤー幅を小さく" },
		incr_size{ 2, "レイヤー幅を大きく" },
		reset_size{ 3, "レイヤー幅をリセット" };

	constexpr auto all = { decr_size, incr_size, reset_size, };
}


////////////////////////////////
// AviUtlに渡す関数の定義．
////////////////////////////////
std::remove_pointer_t<decltype(AviUtl::FilterPlugin::func_WndProc)> func_WndProc;
BOOL func_init(AviUtl::FilterPlugin* fp)
{
	// 情報源確保．
	if (!exedit.init(fp)) {
		::MessageBoxA(fp->hwnd, "拡張編集0.92が見つかりませんでした．",
			fp->name, MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	gui_data.init(fp);

	// 設定ロード．
	load_settings(fp->dll_hinst);

	// メニューコマンドを追加．
	for (auto& item : menu_commands::all)
		fp->exfunc->add_menu_item(fp, item.title, fp->hwnd, item.id,
			0, AviUtl::ExFunc::AddMenuItemFlag::None);

	// IME を無効化．
	::ImmReleaseContext(fp->hwnd, ::ImmAssociateContext(fp->hwnd, nullptr));

	// 初期化前に呼ばれないようにしていたコールバック関数をこのタイミングで設定．
	fp->func_WndProc = func_WndProc;
	::InvalidateRect(fp->hwnd, nullptr, FALSE);

	return TRUE;
}

BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp)
{
	BOOL ret = FALSE; bool redraw = false;
	switch (message) {
		using AUM = AviUtl::FilterPlugin::WindowMessage;
	case AUM::Init:
	{
		// first synchronization.
		gui_data.pull();
		tooltip.init();
		break;
	}
	case AUM::Exit:
	{
		tooltip.exit();
		gui_data.exit();
		save_settings(fp->dll_hinst);
		break;
	}
	case AUM::ChangeWindow:
	case AUM::Update:
	{
		// synchronize with the effective value.
		if (fp->exfunc->is_filter_window_disp(fp) != FALSE &&
			gui_data.pull()) redraw = true;
		break;
	}

	case AUM::Command:
	{
		// menu commands.
		int delta = 0;
		switch (wparam) {
			using namespace menu_commands;
		case decr_size.id: delta = -1; break;
		case incr_size.id: delta = +1; break;
		case reset_size.id:
		{
			delta = gui_data.def_layer_sizes[gui_data.mode_curr] - gui_data.gui_value();
			break;
		}
		}
		if (delta == 0) break;

		// apply the change, with no delay.
		redraw = gui_data.push(std::clamp<int32_t>(gui_data.gui_value() + delta,
			settings.behavior.size_min, settings.behavior.size_max), 0);
		break;
	}

	// drag operations.
	case WM_LBUTTONDOWN:
	{
		drag_state.mouse_down(static_cast<int16_t>(lparam & 0xffff),
			static_cast<int16_t>(lparam >> 16), redraw);
		break;
	}
	case WM_LBUTTONUP:
	{
		drag_state.mouse_up(static_cast<int16_t>(lparam & 0xffff),
			static_cast<int16_t>(lparam >> 16), redraw);
		break;
	}
	case WM_MOUSEMOVE:
	{
		drag_state.mouse_move(static_cast<int16_t>(lparam & 0xffff),
			static_cast<int16_t>(lparam >> 16), redraw);
		break;
	}
	case WM_RBUTTONDOWN:
	case WM_CAPTURECHANGED:
	{
		drag_state.cancel(redraw);
		break;
	}

	// wheel operation.
	case WM_MOUSEWHEEL:
	{
		// invalid when dragging or intentionally turned off.
		if (drag_state.active || settings.behavior.wheel == 0) break;

		auto const amount = static_cast<int16_t>(wparam >> 16);
		if (amount == 0) break;

		auto const delta = amount > 0 ? -settings.behavior.wheel : +settings.behavior.wheel;

		// apply the change, with no delay.
		redraw = gui_data.push(std::clamp<int32_t>(gui_data.gui_value() + delta,
			settings.behavior.size_min, settings.behavior.size_max), 0);
		break;
	}

		// context menu.
	case WM_CONTEXTMENU:
	{
		// determine the position of the context menu.
		POINT pt{ .x = static_cast<int16_t>(lparam & 0xffff), .y = static_cast<int16_t>(lparam >> 16) };
		if (pt.x == -1 && pt.y == -1) {
			// for the case of Shift+F10, show the menu at the gauge position.
			RECT rc; ::GetClientRect(hwnd, &rc);

			rc.left = settings.layout.margin_x; rc.right -= settings.layout.margin_x; rc.right = std::max(rc.left + 1, rc.right);
			rc.top = settings.layout.margin_y; rc.bottom -= settings.layout.margin_y; rc.bottom = std::max(rc.top + 1, rc.bottom);

			pt.x = (rc.left + rc.right) >> 1; pt.y = (rc.top + rc.bottom) >> 1;

			auto pos = std::clamp(settings.scale_from_val(gui_data.gui_value()), 0.0, 1.0);
			constexpr auto rnd = [](auto x) {return static_cast<int>(std::round(x)); };
			switch (settings.layout.orientation) {
				using enum Settings::Orientation;
			default:
			case L2R: pt.x = rnd((1 - pos) * rc.left + pos * rc.right); break;
			case R2L: pt.x = rnd((1 - pos) * rc.right + pos * rc.left); break;
			case T2B: pt.y = rnd((1 - pos) * rc.top + pos * rc.bottom); break;
			case B2T: pt.y = rnd((1 - pos) * rc.bottom + pos * rc.top); break;
			}

			::ClientToScreen(hwnd, &pt);
		}

		// apply the pending changes.
		gui_data.flush();

		// show the context menu.
		if (contextmenu(hwnd, pt.x, pt.y)) {
			redraw = true;
			ret = TRUE; // notify other plugins of possible changes.
		}

		// discard mouse messages posted to hwnd.
		for (MSG msg; ::PeekMessageW(&msg, hwnd, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) != FALSE;);
		break;
	}

	case WM_PAINT:
	{
		redraw = true;
		break;
	}

	case WM_NOTIFY:
	{
		// change the text color of the tooltip if specified.
		if (auto const hdr = reinterpret_cast<NMHDR*>(lparam);
			settings.tooltip.text_color.A == 0 &&
			tooltip.hwnd != nullptr && hdr->hwndFrom == tooltip.hwnd) {
			switch (hdr->code) {
			case NM_CUSTOMDRAW:
			{
				if (auto const dhdr = reinterpret_cast<NMTTCUSTOMDRAW*>(hdr);
					(dhdr->nmcd.dwDrawStage & CDDS_PREPAINT) != 0)
					::SetTextColor(dhdr->nmcd.hdc, settings.tooltip.text_color.raw);
				break;
			}
			}
		}
		break;
	}

	// let the parent window handle shortcut keys.
	case WM_KEYDOWN:
	case WM_CHAR:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSCHAR:
	case WM_SYSKEYUP:
	{
		::SendMessageW(fp->hwnd_parent, message, wparam, lparam);
		break;
	}
	}

	if (redraw) draw();
	return ret;
}


////////////////////////////////
// Entry point.
////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		::DisableThreadLibraryCalls(hinst);
		break;
	}
	return TRUE;
}


////////////////////////////////
// 看板．
////////////////////////////////
#define PLUGIN_NAME		"可変幅レイヤー"
#define PLUGIN_VERSION	"v1.00-beta3"
#define PLUGIN_AUTHOR	"sigma-axis"
#define PLUGIN_INFO_FMT(name, ver, author)	(name##" "##ver##" by "##author)
#define PLUGIN_INFO		PLUGIN_INFO_FMT(PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHOR)

extern "C" __declspec(dllexport) AviUtl::FilterPluginDLL* __stdcall GetFilterTable(void)
{
	constexpr int32_t def_size = 256;

	// （フィルタとは名ばかりの）看板．
	using Flag = AviUtl::FilterPlugin::Flag;
	static constinit AviUtl::FilterPluginDLL filter{
		.flag = Flag::AlwaysActive | Flag::ExInformation | Flag::DispFilter
			| Flag::WindowSize | Flag::WindowThickFrame,
		.x = def_size, .y = def_size,
		.name = PLUGIN_NAME,

		.func_init = func_init,
		.information = PLUGIN_INFO,
	};
	return &filter;
}

