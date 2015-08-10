/*
 * popup_alt_tab.cxx
 *
 * copyright (2010-2014) Benoit Gschwind
 *
 * This code is licensed under the GPLv3. see COPYING file for more details.
 *
 */

#include "popup_alt_tab.hxx"

#include "client_managed.hxx"

namespace page {

using namespace std;

popup_alt_tab_t::popup_alt_tab_t(page_context_t * ctx, list<shared_ptr<cycle_window_entry_t>> client_list, int selected) :
	_ctx{ctx},
	_selected{},
	_client_list{client_list},
	_is_durty{true},
	_exposed{true},
	_damaged{true}
{

	_selected = _client_list.begin();

	_position.x = 0;
	_position.y = 0;
	_position.w = 80 * 4;
	_position.h = 80 * (_client_list.size()/4 + 1);

	_create_composite_window();

	_surf = make_shared<pixmap_t>(_ctx->dpy(), PIXMAP_RGB, _position.w, _position.h);
	_ctx->dpy()->map(_wid);

	for(auto const & x: _client_list) {
		if(not x->id.expired()) {
			x->destroy_func = x->id.lock()->on_destroy.connect(this, &popup_alt_tab_t::destroy_client);
		}
	}

}

popup_alt_tab_t::~popup_alt_tab_t() {
	xcb_destroy_window(_ctx->dpy()->xcb(), _wid);
	_client_list.clear();
}

void popup_alt_tab_t::_create_composite_window() {
	xcb_colormap_t cmap = xcb_generate_id(_ctx->dpy()->xcb());
	xcb_create_colormap(_ctx->dpy()->xcb(), XCB_COLORMAP_ALLOC_NONE, cmap, _ctx->dpy()->root(), _ctx->dpy()->root_visual()->visual_id);

	uint32_t value_mask = 0;
	uint32_t value[5];

	value_mask |= XCB_CW_BACK_PIXEL;
	value[0] = _ctx->dpy()->xcb_screen()->black_pixel;

	value_mask |= XCB_CW_BORDER_PIXEL;
	value[1] = _ctx->dpy()->xcb_screen()->black_pixel;

	value_mask |= XCB_CW_OVERRIDE_REDIRECT;
	value[2] = True;

	value_mask |= XCB_CW_EVENT_MASK;
	value[3] = XCB_EVENT_MASK_EXPOSURE;

	value_mask |= XCB_CW_COLORMAP;
	value[4] = cmap;

	_wid = xcb_generate_id(_ctx->dpy()->xcb());
	xcb_create_window(_ctx->dpy()->xcb(), _ctx->dpy()->root_depth(), _wid, _ctx->dpy()->root(),
			_position.x, _position.y, _position.w, _position.h, 0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT, _ctx->dpy()->root_visual()->visual_id,
			value_mask, value);
}

void popup_alt_tab_t::move(int x, int y) {
	_ctx->add_global_damage(_position);
	_position.x = x;
	_position.y = y;
	_ctx->dpy()->move_resize(_wid, _position);
	_ctx->add_global_damage(_position);


	int n = 0;
	for (auto i : _clients_thumbnails) {
		int x = n % 4;
		int y = n / 4;
		rect pos{_position.x + x * 80, _position.y + y * 80, 80, 80};
		i->move_to(pos);
		++n;
	}

}

void popup_alt_tab_t::show() {
	_is_visible = true;
	_ctx->dpy()->map(_wid);
}

void popup_alt_tab_t::_init() {
	int n = 0;
	for (auto i : _client_list) {
		if(i->id.expired())
			continue;

		int x = n % 4;
		int y = n / 4;
		rect pos{100 + x * 80, 100 + y * 80, 80, 80};

		_clients_thumbnails.push_back(make_shared<renderable_thumbnail_t>(_ctx, pos, i->id.lock()));
		_clients_thumbnails.back()->set_parent(shared_from_this());
		_clients_thumbnails.back()->show();
		if (i == *_selected) {
			/** draw a beautiful yellow box **/
			_clients_thumbnails.back()->set_mouse_over(true);
		}
		++n;
	}
}

void popup_alt_tab_t::hide() {
	_is_visible = false;
	_ctx->dpy()->unmap(_wid);
}

rect const & popup_alt_tab_t::position() {
	return _position;
}



void popup_alt_tab_t::select_next() {
	++_selected;
	if(_selected == _client_list.end())
		_selected = _client_list.begin();
	_is_durty = true;
	_exposed = true;
}

weak_ptr<client_managed_t> popup_alt_tab_t::get_selected() {
	return (*_selected)->id;
}

void popup_alt_tab_t::update_backbuffer() {
	if(not _is_durty)
		return;

	_is_durty = false;
	_damaged = true;

	rect a{0,0,_position.w,_position.h};
	cairo_t * cr = cairo_create(_surf->get_cairo_surface());
	//cairo_clip(cr, a);
	//cairo_translate(cr, _position.x, _position.y);
	cairo_rectangle(cr, 0, 0, _position.w, _position.h);
	cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 1.0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_fill(cr);

	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

	int n = 0;
	for (auto i : _client_list) {
		int x = n % 4;
		int y = n / 4;

		if (i->icon != nullptr) {
			if (i->icon->get_cairo_surface() != nullptr) {
				cairo_set_source_surface(cr,
						i->icon->get_cairo_surface(), x * 80 + 8,
						y * 80 + 8);
				cairo_mask_surface(cr, i->icon->get_cairo_surface(),
						x * 80 + 8, y * 80 + 8);

			}
		}

		if (i == *_selected) {
			/** draw a beautiful yellow box **/
			cairo_set_line_width(cr, 2.0);
			::cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 1.0);
			cairo_rectangle(cr, x * 80 + 8, y * 80 + 8, 64, 64);
			cairo_stroke(cr);
		}

		++n;

	}
	cairo_destroy(cr);
}

void popup_alt_tab_t::paint_exposed() {
	cairo_surface_t * surf = cairo_xcb_surface_create(_ctx->dpy()->xcb(), _wid,
			_ctx->dpy()->root_visual(), _position.w, _position.h);
	cairo_t * cr = cairo_create(surf);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(cr, _surf->get_cairo_surface(), 0.0, 0.0);
	cairo_rectangle(cr, 0, 0, _position.w, _position.h);
	cairo_fill(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(surf);
}

region popup_alt_tab_t::get_damaged() {
	if(_damaged) {
		return region{_position};
	} else {
		return region{};
	}
}

region popup_alt_tab_t::get_opaque_region() {
	return region{_position};
}

region popup_alt_tab_t::get_visible_region() {
	return region{_position};
}

void popup_alt_tab_t::render(cairo_t * cr, region const & area) {
//	cairo_save(cr);
//	for (auto & a : area) {
//		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
//		cairo_clip(cr, a);
//		cairo_set_source_surface(cr, _surf->get_cairo_surface(), _position.x, _position.y);
//		cairo_paint(cr);
//	}
//	cairo_restore(cr);
}

void popup_alt_tab_t::destroy_client(client_managed_t * c) {
	_client_list.remove_if([](shared_ptr<cycle_window_entry_t> const & x) -> bool { return x->id.expired(); });
}

xcb_window_t popup_alt_tab_t::get_xid() const {
	return _wid;
}

xcb_window_t popup_alt_tab_t::get_parent_xid () const {
	return _wid;
}

string popup_alt_tab_t::get_node_name() const {
	return string{"popup_alt_tab_t"};
}

void popup_alt_tab_t::trigger_redraw() {
	if(_exposed) {
		_exposed = false;
		update_backbuffer();
		paint_exposed();
	}
}

void popup_alt_tab_t::render_finished() {
	_damaged = false;
}

void popup_alt_tab_t::update_layout(time64_t const time) {
	update_backbuffer();
}

void popup_alt_tab_t::expose(xcb_expose_event_t const * ev) {
	if(ev->window == _wid)
		_exposed = true;
}

void popup_alt_tab_t::append_children(vector<shared_ptr<tree_t>> & out) const {
	out.insert(out.end(), _clients_thumbnails.begin(), _clients_thumbnails.end());
}




}
