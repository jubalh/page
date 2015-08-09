/*
 * managed_window.cxx
 *
 * copyright (2010-2014) Benoit Gschwind
 *
 * This code is licensed under the GPLv3. see COPYING file for more details.
 *
 */

#include "leak_checker.hxx"

#include <cairo.h>
#include <cairo-xlib.h>
#include <cairo-xcb.h>

#include "composite_surface_manager.hxx"

#include "renderable_floating_outer_gradien.hxx"
#include "client_managed.hxx"
#include "notebook.hxx"
#include "utils.hxx"
#include "grab_handlers.hxx"

namespace page {

using namespace std;

client_managed_t::client_managed_t(page_context_t * ctx, xcb_atom_t net_wm_type, shared_ptr<client_properties_t> props) :
				client_base_t{ctx, props},
				_managed_type(MANAGED_FLOATING),
				_net_wm_type(net_wm_type),
				_floating_wished_position(),
				_notebook_wished_position(),
				_wished_position(),
				_orig_position(),
				_base_position(),
				_surf(0),
				_top_buffer(0),
				_bottom_buffer(0),
				_left_buffer(0),
				_right_buffer(0),
				_icon(nullptr),
				_orig_visual(0),
				_orig_depth(-1),
				_deco_visual(0),
				_deco_depth(-1),
				_orig(props->id()),
				_base(XCB_WINDOW_NONE),
				_deco(XCB_WINDOW_NONE),
				_floating_area(0),
				_is_focused(false),
				_is_iconic(true),
				_demands_attention(false),
				_is_durty{true},
				_shadow{nullptr},
				_base_renderable{nullptr}
{

	_update_title();
	rect pos{_properties->position()};

	printf("window default position = %s\n", pos.to_string().c_str());

	if(net_wm_type == A(_NET_WM_WINDOW_TYPE_DOCK))
		_managed_type = MANAGED_DOCK;

	_floating_wished_position = pos;
	_notebook_wished_position = pos;
	_base_position = pos;
	_orig_position = pos;

	if(_properties->wm_normal_hints()!= nullptr) {
		XSizeHints const * s = _properties->wm_normal_hints();

		if (s->flags & PBaseSize) {
			if (_floating_wished_position.w < s->base_width)
				_floating_wished_position.w = s->base_width;
			if (_floating_wished_position.h < s->base_height)
				_floating_wished_position.h = s->base_height;
		} else if (s->flags & PMinSize) {
			if (_floating_wished_position.w < s->min_width)
				_floating_wished_position.w = s->min_width;
			if (_floating_wished_position.h < s->min_height)
				_floating_wished_position.h = s->min_height;
		}

	}


	_orig_visual = _properties->wa()->visual;
	_orig_depth = _properties->geometry()->depth;

	/**
	 * if x == 0 then place window at center of the screen
	 **/
	if (_floating_wished_position.x == 0 and not is(MANAGED_DOCK)) {
		_floating_wished_position.x =
				(_properties->geometry()->width - _floating_wished_position.w) / 2;
	}

	if(_floating_wished_position.x - _ctx->theme()->floating.margin.left < 0) {
		_floating_wished_position.x = _ctx->theme()->floating.margin.left;
	}

	/**
	 * if y == 0 then place window at center of the screen
	 **/
	if (_floating_wished_position.y == 0 and not is(MANAGED_DOCK)) {
		_floating_wished_position.y = (_properties->geometry()->height - _floating_wished_position.h) / 2;
	}

	if(_floating_wished_position.y - _ctx->theme()->floating.margin.top < 0) {
		_floating_wished_position.y = _ctx->theme()->floating.margin.top;
	}

	/**
	 * Create the base window, window that will content managed window
	 **/

	xcb_window_t wbase;
	xcb_window_t wdeco;
	rect b = _floating_wished_position;

	xcb_visualid_t root_visual = cnx()->root_visual()->visual_id;
	int root_depth = cnx()->find_visual_depth(cnx()->root_visual()->visual_id);

	/**
	 * If window visual is 32 bit (have alpha channel, and root do not
	 * have alpha channel, use the window visual, otherwise always prefer
	 * root visual.
	 **/
	if (_orig_depth == 32 and root_depth != 32) {
		_deco_visual = _orig_visual;
		_deco_depth = _orig_depth;

		/** if visual is 32 bits, this values are mandatory **/
		xcb_colormap_t cmap = xcb_generate_id(cnx()->xcb());
		xcb_create_colormap(cnx()->xcb(), XCB_COLORMAP_ALLOC_NONE, cmap, cnx()->root(), _deco_visual);

		uint32_t value_mask = 0;
		uint32_t value[4];

		value_mask |= XCB_CW_BACK_PIXEL;
		value[0] = cnx()->xcb_screen()->black_pixel;

		value_mask |= XCB_CW_BORDER_PIXEL;
		value[1] = cnx()->xcb_screen()->black_pixel;

		value_mask |= XCB_CW_OVERRIDE_REDIRECT;
		value[2] = True;

		value_mask |= XCB_CW_COLORMAP;
		value[3] = cmap;

		wbase = xcb_generate_id(cnx()->xcb());
		xcb_create_window(cnx()->xcb(), _deco_depth, wbase, cnx()->root(), -10, -10, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, _deco_visual, value_mask, value);
		wdeco = xcb_generate_id(cnx()->xcb());
		xcb_create_window(cnx()->xcb(), _deco_depth, wdeco, wbase, b.x, b.y, b.w, b.h, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, _deco_visual, value_mask, value);

	} else {

		/**
		 * Create RGB window for back ground
		 **/

		_deco_visual = cnx()->default_visual()->visual_id;
		_deco_depth = 32;

		/** if visual is 32 bits, this values are mandatory **/
		xcb_colormap_t cmap = xcb_generate_id(cnx()->xcb());
		xcb_create_colormap(cnx()->xcb(), XCB_COLORMAP_ALLOC_NONE, cmap, cnx()->root(), _deco_visual);

		/**
		 * To create RGBA window, the following field MUST bet set, for unknown
		 * reason. i.e. border_pixel, background_pixel and colormap.
		 **/
		uint32_t value_mask = 0;
		uint32_t value[4];

		value_mask |= XCB_CW_BACK_PIXEL;
		value[0] = cnx()->xcb_screen()->black_pixel;

		value_mask |= XCB_CW_BORDER_PIXEL;
		value[1] = cnx()->xcb_screen()->black_pixel;

		value_mask |= XCB_CW_OVERRIDE_REDIRECT;
		value[2] = True;

		value_mask |= XCB_CW_COLORMAP;
		value[3] = cmap;

		wbase = xcb_generate_id(cnx()->xcb());
		xcb_create_window(cnx()->xcb(), _deco_depth, wbase, cnx()->root(), -10, -10, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, _deco_visual, value_mask, value);
		wdeco = xcb_generate_id(cnx()->xcb());
		xcb_create_window(cnx()->xcb(), _deco_depth, wdeco, wbase, b.x, b.y, b.w, b.h, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, _deco_visual, value_mask, value);

	}

	_base = wbase;
	_deco = wdeco;

	update_floating_areas();

	uint32_t cursor;

	cursor = cnx()->xc_top_side;
	_input_top = cnx()->create_input_only_window(_deco, _area_top, XCB_CW_CURSOR, &cursor);
	cursor = cnx()->xc_bottom_side;
	_input_bottom = cnx()->create_input_only_window(_deco, _area_bottom, XCB_CW_CURSOR, &cursor);
	cursor = cnx()->xc_left_side;
	_input_left = cnx()->create_input_only_window(_deco, _area_left, XCB_CW_CURSOR, &cursor);
	cursor = cnx()->xc_right_side;
	_input_right = cnx()->create_input_only_window(_deco, _area_right, XCB_CW_CURSOR, &cursor);

	cursor = cnx()->xc_top_left_corner;
	_input_top_left = cnx()->create_input_only_window(_deco, _area_top_left, XCB_CW_CURSOR, &cursor);
	cursor = cnx()->xc_top_right_corner;
	_input_top_right = cnx()->create_input_only_window(_deco, _area_top_right, XCB_CW_CURSOR, &cursor);
	cursor = cnx()->xc_bottom_left_corner;
	_input_bottom_left = cnx()->create_input_only_window(_deco, _area_bottom_left, XCB_CW_CURSOR, &cursor);
	cursor = cnx()->xc_bottom_righ_corner;
	_input_bottom_right = cnx()->create_input_only_window(_deco, _area_bottom_right, XCB_CW_CURSOR, &cursor);

	if (lock()) {
		cnx()->add_to_save_set(orig());
		/* set border to zero */
		select_inputs();
		/* Grab button click */
		grab_button_unfocused();

		cnx()->set_border_width(orig(), 0);
		cnx()->reparentwindow(_orig, _base, 0, 0);
		unlock();
	}

	_surf = cairo_xcb_surface_create(cnx()->xcb(), _deco, cnx()->find_visual(_deco_visual), b.w, b.h);

	update_icon();

	_ctx->csm()->register_window(_base);


}

client_managed_t::~client_managed_t() {

	on_destroy.signal(this);

	unselect_inputs();

	if (_surf != nullptr) {
		warn(cairo_surface_get_reference_count(_surf) == 1);
		cairo_surface_destroy(_surf);
		_surf = nullptr;
	}

	if (_floating_area != nullptr) {
		delete _floating_area;
		_floating_area = nullptr;
	}

	destroy_back_buffer();

	xcb_destroy_window(cnx()->xcb(), _input_top);
	xcb_destroy_window(cnx()->xcb(), _input_left);
	xcb_destroy_window(cnx()->xcb(), _input_right);
	xcb_destroy_window(cnx()->xcb(), _input_bottom);
	xcb_destroy_window(cnx()->xcb(), _input_top_left);
	xcb_destroy_window(cnx()->xcb(), _input_top_right);
	xcb_destroy_window(cnx()->xcb(), _input_bottom_left);
	xcb_destroy_window(cnx()->xcb(), _input_bottom_right);

	xcb_destroy_window(cnx()->xcb(), _deco);
	xcb_destroy_window(cnx()->xcb(), _base);

	_ctx->csm()->unregister_window(_base);

	_ctx->add_global_damage(_visible_region_cache);

}

auto client_managed_t::shared_from_this() -> shared_ptr<client_managed_t> {
	return dynamic_pointer_cast<client_managed_t>(tree_t::shared_from_this());
}

void client_managed_t::reconfigure() {

	_damage_cache += get_visible_region();

	if (is(MANAGED_FLOATING)) {
		_wished_position = _floating_wished_position;

		if (prefer_window_border()) {
			_base_position.x = _wished_position.x
					- _ctx->theme()->floating.margin.left;
			_base_position.y = _wished_position.y - _ctx->theme()->floating.margin.top;
			_base_position.w = _wished_position.w + _ctx->theme()->floating.margin.left
					+ _ctx->theme()->floating.margin.right;
			_base_position.h = _wished_position.h + _ctx->theme()->floating.margin.top
					+ _ctx->theme()->floating.margin.bottom + _ctx->theme()->floating.title_height;

			_orig_position.x = _ctx->theme()->floating.margin.left;
			_orig_position.y = _ctx->theme()->floating.margin.top + _ctx->theme()->floating.title_height;
			_orig_position.w = _wished_position.w;
			_orig_position.h = _wished_position.h;

		} else {
			/* floating window without borders */
			_base_position = _wished_position;

			_orig_position.x = 0;
			_orig_position.y = 0;
			_orig_position.w = _wished_position.w;
			_orig_position.h = _wished_position.h;

		}

		/* avoid to hide title bar of floating windows */
		if(_base_position.y < 0) {
			_base_position.y = 0;
		}

		cairo_xcb_surface_set_size(_surf, _base_position.w, _base_position.h);

		destroy_back_buffer();
		create_back_buffer();

	} else if (is(MANAGED_DOCK)) {
		_wished_position = _floating_wished_position;
		_base_position = _wished_position;

		_orig_position.x = 0;
		_orig_position.y = 0;
		_orig_position.w = _wished_position.w;
		_orig_position.h = _wished_position.h;
	} else {
		_wished_position = _notebook_wished_position;
		_base_position = _notebook_wished_position;
		_orig_position = rect(0, 0, _base_position.w, _base_position.h);

		destroy_back_buffer();

	}

	update_floating_areas();

	if (lock()) {

		if(_is_iconic or not _is_visible) {
			/* if iconic move outside visible area */
			cnx()->move_resize(_base, rect{_ctx->left_most_border()-1-_base_position.w, _ctx->top_most_border(), _base_position.w, _base_position.h});
		} else {
			cnx()->move_resize(_base, _base_position);
		}
		cnx()->move_resize(_deco,
				rect{0, 0, _base_position.w, _base_position.h});
		cnx()->move_resize(_orig, _orig_position);

		cnx()->move_resize(_input_top, _area_top);
		cnx()->move_resize(_input_bottom, _area_bottom);
		cnx()->move_resize(_input_right, _area_right);
		cnx()->move_resize(_input_left, _area_left);

		cnx()->move_resize(_input_top_left, _area_top_left);
		cnx()->move_resize(_input_top_right, _area_top_right);
		cnx()->move_resize(_input_bottom_left, _area_bottom_left);
		cnx()->move_resize(_input_bottom_right, _area_bottom_right);

		fake_configure();
		unlock();

	}

	_update_visible_region();
	_damage_cache += get_visible_region();

}

void client_managed_t::fake_configure() {
	//printf("fake_reconfigure = %dx%d+%d+%d\n", _wished_position.w,
	//		_wished_position.h, _wished_position.x, _wished_position.y);
	cnx()->fake_configure(_orig, _wished_position, 0);
}

void client_managed_t::delete_window(xcb_timestamp_t t) {
	printf("request close for '%s'\n", title().c_str());

	if (lock()) {
		xcb_client_message_event_t xev;
		xev.response_type = XCB_CLIENT_MESSAGE;
		xev.type = A(WM_PROTOCOLS);
		xev.format = 32;
		xev.window = _orig;
		xev.data.data32[0] = A(WM_DELETE_WINDOW);
		xev.data.data32[1] = t;

		xcb_send_event(cnx()->xcb(), 0, _orig, XCB_EVENT_MASK_NO_EVENT,
				reinterpret_cast<char*>(&xev));
		unlock();
	}
}

void client_managed_t::set_managed_type(managed_window_type_e type) {
	if (lock()) {
		if(_managed_type == MANAGED_DOCK) {
			std::list<atom_e> net_wm_allowed_actions;
			_properties->net_wm_allowed_actions_set(net_wm_allowed_actions);
			reconfigure();
		} else {

			std::list<atom_e> net_wm_allowed_actions;
			net_wm_allowed_actions.push_back(_NET_WM_ACTION_CLOSE);
			net_wm_allowed_actions.push_back(_NET_WM_ACTION_FULLSCREEN);
			_properties->net_wm_allowed_actions_set(net_wm_allowed_actions);

			_managed_type = type;

			reconfigure();
		}

		unlock();
	}
}

void client_managed_t::focus(xcb_timestamp_t t) {
	set_focus_state(true);
	icccm_focus(t);
}

rect client_managed_t::get_base_position() const {
	return _base_position;
}

managed_window_type_e client_managed_t::get_type() {
	return _managed_type;
}

bool client_managed_t::is(managed_window_type_e type) {
	return _managed_type == type;
}

void client_managed_t::expose() {
	if (is(MANAGED_FLOATING)) {

		theme_managed_window_t fw;

		if (_bottom_buffer != nullptr) {
			fw.cairo_bottom = cairo_create(_bottom_buffer);
		} else {
			fw.cairo_bottom = nullptr;
		}

		if (_top_buffer != nullptr) {
			fw.cairo_top = cairo_create(_top_buffer);
		} else {
			fw.cairo_top = nullptr;
		}

		if (_right_buffer != nullptr) {
			fw.cairo_right = cairo_create(_right_buffer);
		} else {
			fw.cairo_right = nullptr;
		}

		if (_left_buffer != nullptr) {
			fw.cairo_left = cairo_create(_left_buffer);
		} else {
			fw.cairo_left = nullptr;
		}

		fw.focuced = is_focused();
		fw.position = base_position();
		fw.icon = icon();
		fw.title = title();
		fw.demand_attention = _demands_attention;

		_ctx->theme()->render_floating(&fw);

		cairo_xcb_surface_set_size(_surf, _base_position.w, _base_position.h);

		cairo_t * _cr = cairo_create(_surf);

		/** top **/
		if (_top_buffer != nullptr) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr, 0, 0, _base_position.w,
					_ctx->theme()->floating.margin.top+_ctx->theme()->floating.title_height);
			cairo_set_source_surface(_cr, _top_buffer, 0, 0);
			cairo_fill(_cr);
		}

		/** bottom **/
		if (_bottom_buffer != nullptr) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr, 0,
					_base_position.h - _ctx->theme()->floating.margin.bottom,
					_base_position.w, _ctx->theme()->floating.margin.bottom);
			cairo_set_source_surface(_cr, _bottom_buffer, 0,
					_base_position.h - _ctx->theme()->floating.margin.bottom);
			cairo_fill(_cr);
		}

		/** left **/
		if (_left_buffer != nullptr) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr, 0.0, _ctx->theme()->floating.margin.top + _ctx->theme()->floating.title_height,
					_ctx->theme()->floating.margin.left,
					_base_position.h - _ctx->theme()->floating.margin.top
							- _ctx->theme()->floating.margin.bottom - _ctx->theme()->floating.title_height);
			cairo_set_source_surface(_cr, _left_buffer, 0.0,
					_ctx->theme()->floating.margin.top + _ctx->theme()->floating.title_height);
			cairo_fill(_cr);
		}

		/** right **/
		if (_right_buffer != nullptr) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr,
					_base_position.w - _ctx->theme()->floating.margin.right,
					_ctx->theme()->floating.margin.top + _ctx->theme()->floating.title_height, _ctx->theme()->floating.margin.right,
					_base_position.h - _ctx->theme()->floating.margin.top
							- _ctx->theme()->floating.margin.bottom - _ctx->theme()->floating.title_height);
			cairo_set_source_surface(_cr, _right_buffer,
					_base_position.w - _ctx->theme()->floating.margin.right,
					_ctx->theme()->floating.margin.top + _ctx->theme()->floating.title_height);
			cairo_fill(_cr);
		}

		cairo_surface_flush(_surf);

		warn(cairo_get_reference_count(_cr) == 1);
		cairo_destroy(_cr);
	}
}

void client_managed_t::icccm_focus(xcb_timestamp_t t) {
	//fprintf(stderr, "Focus time = %lu\n", t);

	if (lock()) {

		if(_demands_attention) {
			_demands_attention = false;
			_properties->net_wm_state_remove(_NET_WM_STATE_DEMANDS_ATTENTION);
		}

		if (_properties->wm_hints() != nullptr) {
			if (_properties->wm_hints()->input != False) {
				cnx()->set_input_focus(_orig, XCB_INPUT_FOCUS_NONE, t);
			}
		} else {
			/** no WM_HINTS, guess hints.input == True **/
			cnx()->set_input_focus(_orig, XCB_INPUT_FOCUS_NONE, t);
		}

		if (_properties->wm_protocols() != nullptr) {
			if (has_key(*(_properties->wm_protocols()),
					static_cast<xcb_atom_t>(A(WM_TAKE_FOCUS)))) {

				xcb_client_message_event_t ev;
				ev.response_type = XCB_CLIENT_MESSAGE;
				ev.format = 32;
				ev.type = A(WM_PROTOCOLS);
				ev.window = _orig;
				ev.data.data32[0] = A(WM_TAKE_FOCUS);
				ev.data.data32[1] = t;

				xcb_send_event(cnx()->xcb(), 0, _orig, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<char*>(&ev));

			}
		}

		unlock();

	}

}

std::vector<floating_event_t> * client_managed_t::
compute_floating_areas(theme_managed_window_t * mw) const {

	std::vector<floating_event_t> * ret = new std::vector<floating_event_t>();

	floating_event_t fc(FLOATING_EVENT_CLOSE);
	fc.position = compute_floating_close_position(mw->position);
	ret->push_back(fc);

	floating_event_t fb(FLOATING_EVENT_BIND);
	fb.position = compute_floating_bind_position(mw->position);
	ret->push_back(fb);

	int x0 = _ctx->theme()->floating.margin.left;
	int x1 = mw->position.w - _ctx->theme()->floating.margin.right;

	int y0 = _ctx->theme()->floating.margin.bottom;
	int y1 = mw->position.h - _ctx->theme()->floating.margin.bottom;

	int w0 = mw->position.w - _ctx->theme()->floating.margin.left
			- _ctx->theme()->floating.margin.right;
	int h0 = mw->position.h - _ctx->theme()->floating.margin.bottom
			- _ctx->theme()->floating.margin.bottom;

	floating_event_t ft(FLOATING_EVENT_TITLE);
	ft.position = rect(x0, y0, w0,
			_ctx->theme()->floating.title_height);
	ret->push_back(ft);

	floating_event_t fgt(FLOATING_EVENT_GRIP_TOP);
	fgt.position = rect(x0, 0, w0, _ctx->theme()->floating.margin.top);
	ret->push_back(fgt);

	floating_event_t fgb(FLOATING_EVENT_GRIP_BOTTOM);
	fgb.position = rect(x0, y1, w0, _ctx->theme()->floating.margin.bottom);
	ret->push_back(fgb);

	floating_event_t fgl(FLOATING_EVENT_GRIP_LEFT);
	fgl.position = rect(0, y0, _ctx->theme()->floating.margin.left, h0);
	ret->push_back(fgl);

	floating_event_t fgr(FLOATING_EVENT_GRIP_RIGHT);
	fgr.position = rect(x1, y0, _ctx->theme()->floating.margin.right, h0);
	ret->push_back(fgr);

	floating_event_t fgtl(FLOATING_EVENT_GRIP_TOP_LEFT);
	fgtl.position = rect(0, 0, _ctx->theme()->floating.margin.left,
			_ctx->theme()->floating.margin.top);
	ret->push_back(fgtl);

	floating_event_t fgtr(FLOATING_EVENT_GRIP_TOP_RIGHT);
	fgtr.position = rect(x1, 0, _ctx->theme()->floating.margin.right,
			_ctx->theme()->floating.margin.top);
	ret->push_back(fgtr);

	floating_event_t fgbl(FLOATING_EVENT_GRIP_BOTTOM_LEFT);
	fgbl.position = rect(0, y1, _ctx->theme()->floating.margin.left,
			_ctx->theme()->floating.margin.bottom);
	ret->push_back(fgbl);

	floating_event_t fgbr(FLOATING_EVENT_GRIP_BOTTOM_RIGHT);
	fgbr.position = rect(x1, y1, _ctx->theme()->floating.margin.right,
			_ctx->theme()->floating.margin.bottom);
	ret->push_back(fgbr);

	return ret;

}

rect client_managed_t::compute_floating_close_position(rect const & allocation) const {

	rect position;
	position.x = allocation.w - _ctx->theme()->floating.close_width;
	position.y = 0.0;
	position.w = _ctx->theme()->floating.close_width;
	position.h = _ctx->theme()->floating.title_height;

	return position;
}

rect client_managed_t::
compute_floating_bind_position(rect const & allocation) const {

	rect position;
	position.x = allocation.w - _ctx->theme()->floating.bind_width - _ctx->theme()->floating.close_width;
	position.y = 0.0;
	position.w = _ctx->theme()->floating.bind_width;
	position.h = _ctx->theme()->floating.title_height;

	return position;
}


void client_managed_t::grab_button_focused() {
	if (lock()) {
		/** First ungrab all **/
		ungrab_all_button();

		/** for decoration, grab all **/
		xcb_grab_button(cnx()->xcb(), false, _deco, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_1, XCB_MOD_MASK_ANY);
		xcb_grab_button(cnx()->xcb(), false, _deco, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_2, XCB_MOD_MASK_ANY);
		xcb_grab_button(cnx()->xcb(), false, _deco, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_3, XCB_MOD_MASK_ANY);

		/** for base, just grab some modified buttons **/
		xcb_grab_button(cnx()->xcb(), false, _base, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_3, XCB_MOD_MASK_1/*ALT*/);

		/** for base, just grab some modified buttons **/
		xcb_grab_button(cnx()->xcb(), false, _base, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_3, XCB_MOD_MASK_CONTROL);

		unlock();
	}
}

void client_managed_t::grab_button_unfocused() {
	if (lock()) {
		/** First ungrab all **/
		ungrab_all_button();

		xcb_grab_button(cnx()->xcb(), false, _base, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_1, XCB_MOD_MASK_ANY);

		xcb_grab_button(cnx()->xcb(), false, _base, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_2, XCB_MOD_MASK_ANY);

		xcb_grab_button(cnx()->xcb(), false, _base, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_3, XCB_MOD_MASK_ANY);

		/** for decoration, grab all **/
		xcb_grab_button(cnx()->xcb(), false, _deco, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_1, XCB_MOD_MASK_ANY);

		xcb_grab_button(cnx()->xcb(), false, _deco, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_2, XCB_MOD_MASK_ANY);

		xcb_grab_button(cnx()->xcb(), false, _deco, DEFAULT_BUTTON_EVENT_MASK,
				XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
				XCB_NONE, XCB_BUTTON_INDEX_3, XCB_MOD_MASK_ANY);

		unlock();
	}
}


void client_managed_t::ungrab_all_button() {
	xcb_ungrab_button(cnx()->xcb(), XCB_BUTTON_INDEX_ANY, _base, XCB_MOD_MASK_ANY);
	xcb_ungrab_button(cnx()->xcb(), XCB_BUTTON_INDEX_ANY, _deco, XCB_MOD_MASK_ANY);
	if (cnx()->lock(_orig)) {
		xcb_ungrab_button(cnx()->xcb(), XCB_BUTTON_INDEX_ANY, _orig, XCB_MOD_MASK_ANY);
		cnx()->unlock();
	}
}

void client_managed_t::select_inputs() {
	cnx()->select_input(_base, MANAGED_BASE_WINDOW_EVENT_MASK);
	cnx()->select_input(_deco, MANAGED_DECO_WINDOW_EVENT_MASK);
	if (lock()) {
		cnx()->select_input(_orig, MANAGED_ORIG_WINDOW_EVENT_MASK);
		xcb_shape_select_input(cnx()->xcb(), _orig, 1);
		unlock();
	}
}

void client_managed_t::unselect_inputs() {
	cnx()->select_input(_base, XCB_EVENT_MASK_NO_EVENT);
	cnx()->select_input(_deco, XCB_EVENT_MASK_NO_EVENT);
	if (cnx()->lock(_orig)) {
		cnx()->select_input(_orig, XCB_EVENT_MASK_NO_EVENT);
		xcb_shape_select_input(cnx()->xcb(), _orig, false);
		cnx()->unlock();
	}
}

bool client_managed_t::is_fullscreen() {
	if (_properties->net_wm_state() != nullptr) {
		return has_key(*(_properties->net_wm_state()),
				static_cast<xcb_atom_t>(A(_NET_WM_STATE_FULLSCREEN)));
	}
	return false;
}

bool client_managed_t::skip_task_bar() {
	if (_properties->net_wm_state() != nullptr) {
		return has_key(*(_properties->net_wm_state()),
				static_cast<xcb_atom_t>(A(_NET_WM_STATE_SKIP_TASKBAR)));
	}
	return false;
}

xcb_atom_t client_managed_t::net_wm_type() {
	return _net_wm_type;
}

bool client_managed_t::get_wm_normal_hints(XSizeHints * size_hints) {
	if(_properties->wm_normal_hints() != nullptr) {
		*size_hints = *(_properties->wm_normal_hints());
		return true;
	} else {
		return false;
	}
}

void client_managed_t::net_wm_state_add(atom_e atom) {
	if (lock()) {
		_properties->net_wm_state_add(atom);
		unlock();
	}
}

void client_managed_t::net_wm_state_remove(atom_e atom) {
	if (lock()) {
		_properties->net_wm_state_remove(atom);
		unlock();
	}
}

void client_managed_t::net_wm_state_delete() {
	/**
	 * This one is for removing the window manager tag, thus only check if the window
	 * still exist. (don't need lock);
	 **/
	if (cnx()->lock(_orig)) {
		cnx()->delete_property(_orig, _NET_WM_STATE);
		cnx()->unlock();
	}
}

void client_managed_t::normalize() {
	if(not _is_iconic)
		return;

	if (lock()) {
		_is_iconic = false;
		_properties->set_wm_state(NormalState);
		for (auto c : filter_class<client_managed_t>(_children)) {
			c->normalize();
		}
		unlock();
	}
}

void client_managed_t::iconify() {
	if(_is_iconic)
		return;

	if (lock()) {
		_is_iconic = true;
		_properties->set_wm_state(IconicState);
		for (auto c : filter_class<client_managed_t>(_children)) {
			c->iconify();
		}
		unlock();
	}
}

void client_managed_t::wm_state_delete() {
	/**
	 * This one is for removing the window manager tag, thus only check if the window
	 * still exist. (don't need lock);
	 **/

	if(cnx()->lock(_orig)) {
		cnx()->delete_property(_orig, WM_STATE);
		cnx()->unlock();
	}
}

void client_managed_t::set_floating_wished_position(rect const & pos) {
	_floating_wished_position = pos;
}

void client_managed_t::set_notebook_wished_position(rect const & pos) {
	_notebook_wished_position = pos;
}

rect const & client_managed_t::get_wished_position() {
	return _wished_position;
}

rect const & client_managed_t::get_floating_wished_position() {
	return _floating_wished_position;
}

void client_managed_t::destroy_back_buffer() {

	if(_top_buffer != nullptr) {
		warn(cairo_surface_get_reference_count(_top_buffer) == 1);
		cairo_surface_destroy(_top_buffer);
		_top_buffer = nullptr;
	}

	if(_bottom_buffer != nullptr) {
		warn(cairo_surface_get_reference_count(_bottom_buffer) == 1);
		cairo_surface_destroy(_bottom_buffer);
		_bottom_buffer = nullptr;
	}

	if(_left_buffer != nullptr) {
		warn(cairo_surface_get_reference_count(_left_buffer) == 1);
		cairo_surface_destroy(_left_buffer);
		_left_buffer = nullptr;
	}

	if(_right_buffer != nullptr) {
		warn(cairo_surface_get_reference_count(_right_buffer) == 1);
		cairo_surface_destroy(_right_buffer);
		_right_buffer = nullptr;
	}

}

void client_managed_t::create_back_buffer() {

	if (_ctx->theme()->floating.margin.top > 0) {
		_top_buffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
				_base_position.w, _ctx->theme()->floating.margin.top + _ctx->theme()->floating.title_height);
	} else {
		_top_buffer = nullptr;
	}

	if (_ctx->theme()->floating.margin.bottom > 0) {
		_bottom_buffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
				_base_position.w, _ctx->theme()->floating.margin.bottom);
	} else {
		_bottom_buffer = nullptr;
	}

	if (_ctx->theme()->floating.margin.left > 0) {
		_left_buffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
				_ctx->theme()->floating.margin.left,
				_base_position.h - _ctx->theme()->floating.margin.top
						- _ctx->theme()->floating.margin.bottom);
	} else {
		_left_buffer = nullptr;
	}

	if (_ctx->theme()->floating.margin.right > 0) {
		_right_buffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
				_ctx->theme()->floating.margin.right,
				_base_position.h - _ctx->theme()->floating.margin.top
						- _ctx->theme()->floating.margin.bottom);
	} else {
		_right_buffer = nullptr;
	}

}

std::vector<floating_event_t> const * client_managed_t::floating_areas() {
	return _floating_area;
}

void client_managed_t::update_floating_areas() {

	if (_floating_area != 0) {
		delete _floating_area;
	}

	theme_managed_window_t tm;
	tm.position = _base_position;
	tm.title = _title;

	_floating_area = compute_floating_areas(&tm);

	int x0 = _ctx->theme()->floating.margin.left;
	int x1 = _base_position.w - _ctx->theme()->floating.margin.right;

	int y0 = _ctx->theme()->floating.margin.bottom;
	int y1 = _base_position.h - _ctx->theme()->floating.margin.bottom;

	int w0 = _base_position.w - _ctx->theme()->floating.margin.left
			- _ctx->theme()->floating.margin.right;
	int h0 = _base_position.h - _ctx->theme()->floating.margin.bottom
			- _ctx->theme()->floating.margin.bottom;

	_area_top = rect(x0, 0, w0, _ctx->theme()->floating.margin.bottom);
	_area_bottom = rect(x0, y1, w0, _ctx->theme()->floating.margin.bottom);
	_area_left = rect(0, y0, _ctx->theme()->floating.margin.left, h0);
	_area_right = rect(x1, y0, _ctx->theme()->floating.margin.right, h0);

	_area_top_left = rect(0, 0, _ctx->theme()->floating.margin.left,
			_ctx->theme()->floating.margin.bottom);
	_area_top_right = rect(x1, 0, _ctx->theme()->floating.margin.right,
			_ctx->theme()->floating.margin.bottom);
	_area_bottom_left = rect(0, y1, _ctx->theme()->floating.margin.left,
			_ctx->theme()->floating.margin.bottom);
	_area_bottom_right = rect(x1, y1, _ctx->theme()->floating.margin.right,
			_ctx->theme()->floating.margin.bottom);

}

bool client_managed_t::has_window(xcb_window_t w) const {
	return w == _properties->id() or w == _base or w == _deco;
}

std::string client_managed_t::get_node_name() const {
	std::string s = _get_node_name<'M'>();
	std::ostringstream oss;
	oss << s << " " << orig() << " " << title();

	if(_properties->geometry() != nullptr) {
		oss << " " << _properties->geometry()->width << "x" << _properties->geometry()->height << "+" << _properties->geometry()->x << "+" << _properties->geometry()->y;
	}

	return oss.str();
}

display_t * client_managed_t::cnx() {
	return _properties->cnx();
}

rect const & client_managed_t::base_position() const {
	return _base_position;
}

rect const & client_managed_t::orig_position() const {
	return _orig_position;
}

region client_managed_t::get_visible_region() {
	return _visible_region_cache;
}

region client_managed_t::get_opaque_region() {
	return _opaque_region_cache;
}

region client_managed_t::get_damaged() {
	return _damage_cache;
}

bool client_managed_t::lock() {
	cnx()->grab();
	cnx()->fetch_pending_events();
	if(cnx()->check_for_destroyed_window(_orig)
			or cnx()->check_for_fake_unmap_window(_orig)) {
		cnx()->ungrab();
		return false;
	}
	return true;
}

void client_managed_t::unlock() {
	cnx()->ungrab();
}

void client_managed_t::update_layout(time64_t const time) {
	if(not _is_visible)
		return;

	_update_opaque_region();

	/** update damage_cache **/
	region dmg = _ctx->csm()->get_damaged(_base);
	dmg.translate(_base_position.x, _base_position.y);
	_damage_cache += dmg;
	_ctx->csm()->clear_damaged(_base);

	if (_ctx->csm()->get_last_pixmap(_base) != nullptr) {

		rect loc{base_position()};

		if (prefer_window_border() and not is(MANAGED_DOCK)) {
			delete _shadow;
			if(is_focused()) {
				_shadow = new renderable_floating_outer_gradien_t(loc, 18.0, 8.0);
			} else {
				_shadow = new renderable_floating_outer_gradien_t(loc, 8.0, 8.0);
			}
		}
	}
}

void client_managed_t::render_finished() {
	_damage_cache.clear();
}


void client_managed_t::set_focus_state(bool is_focused) {
	if (lock()) {
		_is_focused = is_focused;
		if (_is_focused) {
			net_wm_state_add(_NET_WM_STATE_FOCUSED);
			grab_button_focused();
			on_activate.signal(shared_from_this());
		} else {
			net_wm_state_remove(_NET_WM_STATE_FOCUSED);
			grab_button_unfocused();
			on_deactivate.signal(shared_from_this());
		}
		queue_redraw();
		unlock();
	}
}

void client_managed_t::net_wm_allowed_actions_add(atom_e atom) {
	if(lock()) {
		_properties->net_wm_allowed_actions_add(atom);
		unlock();
	}
}

void client_managed_t::map() {
	cnx()->map(_orig);
	cnx()->map(_deco);
	cnx()->map(_base);

	cnx()->map(_input_top);
	cnx()->map(_input_left);
	cnx()->map(_input_right);
	cnx()->map(_input_bottom);

	cnx()->map(_input_top_left);
	cnx()->map(_input_top_right);
	cnx()->map(_input_bottom_left);
	cnx()->map(_input_bottom_right);
}

void client_managed_t::unmap() {
	cnx()->unmap(_base);
	cnx()->unmap(_deco);
	cnx()->unmap(_orig);

	cnx()->unmap(_input_top);
	cnx()->unmap(_input_left);
	cnx()->unmap(_input_right);
	cnx()->unmap(_input_bottom);

	cnx()->unmap(_input_top_left);
	cnx()->unmap(_input_top_right);
	cnx()->unmap(_input_bottom_left);
	cnx()->unmap(_input_bottom_right);
}

void client_managed_t::hide() {
	for(auto x: _children) {
		x->hide();
	}

	_is_visible = false;
	net_wm_state_add(_NET_WM_STATE_HIDDEN);
	// do not unmap, just put it outside the screen.
	//unmap();
	reconfigure();
}

void client_managed_t::show() {
	_is_visible = true;
	net_wm_state_remove(_NET_WM_STATE_HIDDEN);
	reconfigure();
	map();
	for(auto x: _children) {
		x->show();
	}
}

bool client_managed_t::is_iconic() {
	return _is_iconic;
}

bool client_managed_t::is_stiky() {
	if(_properties->net_wm_state() != nullptr) {
		return has_key(*_properties->net_wm_state(), A(_NET_WM_STATE_STICKY));
	}
	return false;
}

bool client_managed_t::is_modal() {
	if(_properties->net_wm_state() != nullptr) {
		return has_key(*_properties->net_wm_state(), A(_NET_WM_STATE_MODAL));
	}
	return false;
}

void client_managed_t::activate() {

	if(not _parent.expired()) {
		_parent.lock()->activate(shared_from_this());
	}

	if(is_iconic()) {
		normalize();
		queue_redraw();
	}

}

bool client_managed_t::button_press(xcb_button_press_event_t const * e) {

	if (not has_window(e->event)) {
		return false;
	}

	if (is(MANAGED_FLOATING)
			and e->detail == XCB_BUTTON_INDEX_3
			and (e->state & (XCB_MOD_MASK_1 | XCB_MOD_MASK_CONTROL))) {

		if ((e->state & XCB_MOD_MASK_CONTROL)) {
			_ctx->grab_start(new grab_floating_resize_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y, RESIZE_BOTTOM_RIGHT});
		} else {
			_ctx->grab_start(new grab_floating_move_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y});
		}

		return true;
	} else if (is(MANAGED_FLOATING)
			and e->detail == XCB_BUTTON_INDEX_1
			and e->child != orig()
			and e->event == deco()) {

		auto const * l = floating_areas();
		floating_event_t const * b = nullptr;
		for (auto &i : (*l)) {
			if(i.position.is_inside(e->event_x, e->event_y)) {
				b = &i;
				break;
			}
		}

		if (b != nullptr) {

			if (b->type == FLOATING_EVENT_CLOSE) {
				delete_window(e->time);
			} else if (b->type == FLOATING_EVENT_BIND) {
				rect absolute_position = b->position;
				absolute_position.x += base_position().x;
				absolute_position.y += base_position().y;
				_ctx->grab_start(new grab_bind_client_t{_ctx, shared_from_this(), e->detail, absolute_position});
			} else if (b->type == FLOATING_EVENT_TITLE) {
				_ctx->grab_start(new grab_floating_move_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y});
			} else {
				if (b->type == FLOATING_EVENT_GRIP_TOP) {
					_ctx->grab_start(new grab_floating_resize_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y, RESIZE_TOP});
				} else if (b->type == FLOATING_EVENT_GRIP_BOTTOM) {
					_ctx->grab_start(new grab_floating_resize_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y, RESIZE_BOTTOM});
				} else if (b->type == FLOATING_EVENT_GRIP_LEFT) {
					_ctx->grab_start(new grab_floating_resize_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y, RESIZE_LEFT});
				} else if (b->type == FLOATING_EVENT_GRIP_RIGHT) {
					_ctx->grab_start(new grab_floating_resize_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y, RESIZE_RIGHT});
				} else if (b->type == FLOATING_EVENT_GRIP_TOP_LEFT) {
					_ctx->grab_start(new grab_floating_resize_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y, RESIZE_TOP_LEFT});
				} else if (b->type == FLOATING_EVENT_GRIP_TOP_RIGHT) {
					_ctx->grab_start(new grab_floating_resize_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y, RESIZE_TOP_RIGHT});
				} else if (b->type == FLOATING_EVENT_GRIP_BOTTOM_LEFT) {
					_ctx->grab_start(new grab_floating_resize_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y, RESIZE_BOTTOM_LEFT});
				} else if (b->type == FLOATING_EVENT_GRIP_BOTTOM_RIGHT) {
					_ctx->grab_start(new grab_floating_resize_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y, RESIZE_BOTTOM_RIGHT});
				} else {
					_ctx->grab_start(new grab_floating_move_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y});
				}
			}

			return true;

		}

	} else if (is(MANAGED_FULLSCREEN)
			and e->detail == (XCB_BUTTON_INDEX_3)
			and (e->state & (XCB_MOD_MASK_1))) {
		//fprintf(stderr, "start FULLSCREEN MOVE\n");
		/** start moving fullscreen window **/
		_ctx->grab_start(new grab_fullscreen_client_t{_ctx, shared_from_this(), e->detail, e->root_x, e->root_y});
		return true;
	} else if (is(MANAGED_NOTEBOOK) and e->detail == (XCB_BUTTON_INDEX_3)
			and (e->state & (XCB_MOD_MASK_1))) {
		_ctx->grab_start(new grab_bind_client_t{_ctx, shared_from_this(), e->detail, rect{e->root_x-10, e->root_y-10, 20, 20}});
		return true;
	}

	return false;
}

void client_managed_t::queue_redraw() {
	if(is(MANAGED_FLOATING)) {
		_is_durty = true;
	} else {
		tree_t::queue_redraw();
	}
}

void client_managed_t::trigger_redraw() {
	/** trigger_redraw for childs **/
	tree_t::trigger_redraw();

	if (is(MANAGED_FLOATING) and _is_durty) {
		_is_durty = false;

		theme_managed_window_t fw;

		if (_bottom_buffer != nullptr) {
			fw.cairo_bottom = cairo_create(_bottom_buffer);
		} else {
			fw.cairo_bottom = nullptr;
		}

		if (_top_buffer != nullptr) {
			fw.cairo_top = cairo_create(_top_buffer);
		} else {
			fw.cairo_top = nullptr;
		}

		if (_right_buffer != nullptr) {
			fw.cairo_right = cairo_create(_right_buffer);
		} else {
			fw.cairo_right = nullptr;
		}

		if (_left_buffer != nullptr) {
			fw.cairo_left = cairo_create(_left_buffer);
		} else {
			fw.cairo_left = nullptr;
		}

		fw.focuced = is_focused();
		fw.position = base_position();
		fw.icon = icon();
		fw.title = title();
		fw.demand_attention = _demands_attention;

		_ctx->theme()->render_floating(&fw);

		cairo_xcb_surface_set_size(_surf, _base_position.w, _base_position.h);

		cairo_t * _cr = cairo_create(_surf);

		/** top **/
		if (_top_buffer != nullptr) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr, 0, 0, _base_position.w,
					_ctx->theme()->floating.margin.top+_ctx->theme()->floating.title_height);
			cairo_set_source_surface(_cr, _top_buffer, 0, 0);
			cairo_fill(_cr);
		}

		/** bottom **/
		if (_bottom_buffer != nullptr) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr, 0,
					_base_position.h - _ctx->theme()->floating.margin.bottom,
					_base_position.w, _ctx->theme()->floating.margin.bottom);
			cairo_set_source_surface(_cr, _bottom_buffer, 0,
					_base_position.h - _ctx->theme()->floating.margin.bottom);
			cairo_fill(_cr);
		}

		/** left **/
		if (_left_buffer != nullptr) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr, 0.0, _ctx->theme()->floating.margin.top + _ctx->theme()->floating.title_height,
					_ctx->theme()->floating.margin.left,
					_base_position.h - _ctx->theme()->floating.margin.top
							- _ctx->theme()->floating.margin.bottom - _ctx->theme()->floating.title_height);
			cairo_set_source_surface(_cr, _left_buffer, 0.0,
					_ctx->theme()->floating.margin.top + _ctx->theme()->floating.title_height);
			cairo_fill(_cr);
		}

		/** right **/
		if (_right_buffer != nullptr) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr,
					_base_position.w - _ctx->theme()->floating.margin.right,
					_ctx->theme()->floating.margin.top + _ctx->theme()->floating.title_height, _ctx->theme()->floating.margin.right,
					_base_position.h - _ctx->theme()->floating.margin.top
							- _ctx->theme()->floating.margin.bottom - _ctx->theme()->floating.title_height);
			cairo_set_source_surface(_cr, _right_buffer,
					_base_position.w - _ctx->theme()->floating.margin.right,
					_ctx->theme()->floating.margin.top + _ctx->theme()->floating.title_height);
			cairo_fill(_cr);
		}

		cairo_surface_flush(_surf);

		warn(cairo_get_reference_count(_cr) == 1);
		cairo_destroy(_cr);
	}
}

void client_managed_t::_update_title() {
		_is_durty = true;

		string name;
		if (_properties->net_wm_name() != nullptr) {
			_title = *(_properties->net_wm_name());
		} else if (_properties->wm_name() != nullptr) {
			_title = *(_properties->wm_name());
		} else {
			stringstream s(std::stringstream::in | std::stringstream::out);
			s << "#" << (_properties->id()) << " (noname)";
			_title = s.str();
		}

}

void client_managed_t::update_title() {
	_update_title();
	on_title_change.signal(shared_from_this());
}

bool client_managed_t::prefer_window_border() const {

	if (_properties->motif_hints() != nullptr) {
		if(not (_properties->motif_hints()->flags & MWM_HINTS_DECORATIONS))
			return true;

		if(_properties->motif_hints()->decorations != 0x00)
			return true;

		return false;

	}

	return true;
}

shared_ptr<icon16> client_managed_t::icon() const {
	return _icon;
}

void client_managed_t::update_icon() {
	_icon = make_shared<icon16>(this);
}

xcb_window_t client_managed_t::orig() const {
	return _properties->id();
}

xcb_window_t client_managed_t::base() const {
	return _base;
}

xcb_window_t client_managed_t::deco() const {
	return _deco;
}

xcb_atom_t client_managed_t::A(atom_e atom) {
	return cnx()->A(atom);
}


bool client_managed_t::is_focused() const {
	return _is_focused;
}

shared_ptr<pixmap_t> client_managed_t::get_last_pixmap() {
	return _ctx->csm()->get_last_pixmap(_base);
}

void client_managed_t::set_current_desktop(unsigned int n) {
	_properties->set_net_wm_desktop(n);
}

void client_managed_t::set_demands_attention(bool x) {
	if (x) {
		_properties->net_wm_state_add(_NET_WM_STATE_DEMANDS_ATTENTION);
	} else {
		_properties->net_wm_state_remove(_NET_WM_STATE_DEMANDS_ATTENTION);
	}
	_demands_attention = x;
}

bool client_managed_t::demands_attention() {
	return _demands_attention;
}

string const & client_managed_t::title() const {
	return _title;
}

void client_managed_t::render(cairo_t * cr, region const & area) {
	auto pix = _ctx->csm()->get_last_pixmap(_base);

	if (pix != nullptr) {
		cairo_save(cr);
		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		cairo_set_source_surface(cr, pix->get_cairo_surface(),
				_base_position.x, _base_position.y);
		region r = region{_base_position} & area;
		for (auto &i : r) {
			cairo_clip(cr, i);
			cairo_mask_surface(cr, pix->get_cairo_surface(),
					_base_position.x, _base_position.y);
		}
		cairo_restore(cr);
	}
}

void client_managed_t::_update_visible_region() {
	/** update visible cache **/
	rect vis{base_position()};

	/* add the shadow */
	if(_managed_type == MANAGED_FLOATING) {
		vis.x -= 32;
		vis.y -= 32;
		vis.w += 64;
		vis.h += 64;
	}

	_visible_region_cache = region{vis};
}

void client_managed_t::_update_opaque_region() {
	/** update opaque region cache **/
	_opaque_region_cache.clear();

	if (net_wm_opaque_region() != nullptr) {
		_opaque_region_cache = region{*(net_wm_opaque_region())};
	} else {
		if (geometry()->depth != 32) {
			_opaque_region_cache = rect{0, 0, _base_position.w, _base_position.h};
		}
	}

	if(shape() != nullptr) {
		_opaque_region_cache &= *shape();
	}

	_opaque_region_cache.translate(_base_position.x+_orig_position.x, _base_position.y+_orig_position.y);
}


}

