/*
 * viewport.cxx
 *
 * copyright (2012) Benoit Gschwind
 *
 */

#include <cassert>
#include <algorithm>
#include "notebook.hxx"
#include "viewport.hxx"

namespace page {

viewport_t::viewport_t(box_t<int> const & area) : raw_aera(area), effective_aera(area), fullscreen_client(0) {
	_subtree = 0;
	_is_visible = true;
}

//bool viewport_t::add_client(window_t * x) {
//	assert(_subtree != 0);
//	return _subtree->add_client(x);
//}
//
//box_int_t viewport_t::get_new_client_size() {
//	assert(_subtree != 0);
//	return _subtree->get_new_client_size();
//}

void viewport_t::replace(tree_t * src, tree_t * by) {
	printf("replace %p by %p\n", src, by);

	if (_subtree == src) {
		_subtree = by;
		_subtree->set_parent(this);
		_subtree->set_allocation(effective_aera);
	}
}

void viewport_t::close(tree_t * src) {

}

void viewport_t::remove(tree_t * src) {

}

void viewport_t::reconfigure() {
	if(_subtree != 0) {
		_subtree->set_allocation(effective_aera);
	}
}

void viewport_t::set_allocation(box_int_t const & area) {
	raw_aera = area;
	reconfigure();
	//fix_allocation();
	//_subtree->reconfigure(effective_aera);
}

void viewport_t::repair1(cairo_t * cr, box_int_t const & area) {
//	assert(_subtree != 0);
//
//	if (fullscreen_client != 0) {
//		if (fullscreen_client->is_visible()) {
//			cairo_save(cr);
//			cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
//			fullscreen_client->repair1(cr, area);
//			cairo_restore(cr);
//		}
//	} else {
//		_subtree->repair1(cr, area);
//	}
}

box_int_t viewport_t::get_absolute_extend() {
	return raw_aera;
}

region_t<int> viewport_t::get_area() {
	if(_is_visible) {
		return _subtree->get_area();
	} else {
		return box_int_t();
	}
}

//void viewport_t::activate_client(window_t * c) {
//	assert(_subtree != 0);
//	if(c != fullscreen_client)
//		_subtree->activate_client(c);
//	else {
//		fullscreen_client->focus();
////		page.set_focus(c);
////		page.get_xconnection().raise_window(c->get_xwin());
//	}
//}

window_set_t viewport_t::get_windows() {
	assert(_subtree != 0);
	return _subtree->get_windows();
}

//void viewport_t::remove_client(window_t * c) {
//	if (fullscreen_client == c) {
//		fullscreen_client = 0;
//	} else {
//		assert(_subtree != 0);
//		_subtree->remove_client(c);
//	}
//}
//
//void viewport_t::iconify_client(window_t * c) {
//	assert(_subtree != 0);
//	_subtree->iconify_client(c);
//}
//
//void viewport_t::delete_all() {
//	assert(_subtree != 0);
//	_subtree->delete_all();
//	delete _subtree;
//}
//
//void viewport_t::unmap_all() {
//	assert(_subtree != 0);
//	_subtree->unmap_all();
//}
//
//void viewport_t::map_all() {
//	assert(_subtree != 0);
//	_subtree->map_all();
//}

//void viewport_t::fix_allocation() {
//	printf("update_allocation %dx%d+%d+%d\n", raw_aera.x, raw_aera.y,
//			raw_aera.w, raw_aera.h);
//
//	enum {
//		X_LEFT = 0,
//		X_RIGHT = 1,
//		X_TOP = 2,
//		X_BOTTOM = 3,
//		X_LEFT_START_Y = 4,
//		X_LEFT_END_Y = 5,
//		X_RIGHT_START_Y = 6,
//		X_RIGHT_END_Y = 7,
//		X_TOP_START_X = 8,
//		X_TOP_END_X = 9,
//		X_BOTTOM_START_X = 10,
//		X_BOTTOM_END_X = 11,
//	};
//
//	effective_aera = raw_aera;
//	int xtop = 0, xleft = 0, xright = 0, xbottom = 0;
//
//	window_list_t::iterator j = page.windows_stack.begin();
//	while (j != page.windows_stack.end()) {
//		long const * ps = (*j)->get_partial_struct();
//		if (ps) {
//			window_t * c = (*j);
//			/* this is very crappy, but there is a way to do it better ? */
//			if (ps[X_LEFT] >= raw_aera.x
//					&& ps[X_LEFT] <= raw_aera.x + raw_aera.w) {
//				int top = std::max<int const>(ps[X_LEFT_START_Y], raw_aera.y);
//				int bottom = std::min<int const>(ps[X_LEFT_END_Y],
//						raw_aera.y + raw_aera.h);
//				if (bottom - top > 0) {
//					xleft = std::max<int const>(xleft,
//							ps[X_LEFT] - effective_aera.x);
//				}
//			}
//
//			if (page.get_xconnection().root_size.w - ps[X_RIGHT] >= raw_aera.x
//					&& page.get_xconnection().root_size.w - ps[X_RIGHT]
//							<= raw_aera.x + raw_aera.w) {
//				int top = std::max<int const>(ps[X_RIGHT_START_Y], raw_aera.y);
//				int bottom = std::min<int const>(ps[X_RIGHT_END_Y],
//						raw_aera.y + raw_aera.h);
//				if (bottom - top > 0) {
//					xright = std::max<int const>(xright,
//							(raw_aera.x + raw_aera.w)
//									- (page.get_xconnection().root_size.w
//											- ps[X_RIGHT]));
//				}
//			}
//
//			if (ps[X_TOP] >= raw_aera.y
//					&& ps[X_TOP] <= raw_aera.y + raw_aera.h) {
//				int left = std::max<int const>(ps[X_TOP_START_X], raw_aera.x);
//				int right = std::min<int const>(ps[X_TOP_END_X],
//						raw_aera.x + raw_aera.w);
//				if (right - left > 0) {
//					xtop = std::max<int const>(xtop, ps[X_TOP] - raw_aera.y);
//				}
//			}
//
//			if (page.get_xconnection().root_size.h - ps[X_BOTTOM] >= raw_aera.y
//					&& page.get_xconnection().root_size.h - ps[X_BOTTOM]
//							<= raw_aera.y + raw_aera.h) {
//				int left = std::max<int const>(ps[X_BOTTOM_START_X],
//						raw_aera.x);
//				int right = std::min<int const>(ps[X_BOTTOM_END_X],
//						raw_aera.x + raw_aera.w);
//				if (right - left > 0) {
//					xbottom = std::max<int const>(xbottom,
//							(effective_aera.h + effective_aera.y)
//									- (page.get_xconnection().root_size.h
//											- ps[X_BOTTOM]));
//				}
//			}
//
//		}
//		++j;
//	}
//
//	effective_aera.x += xleft;
//	effective_aera.w -= xleft + xright;
//	effective_aera.y += xtop;
//	effective_aera.h -= xtop + xbottom;
//
//	printf("subarea %dx%d+%d+%d\n", effective_aera.w, effective_aera.h,
//			effective_aera.x, effective_aera.y);
//}

notebook_t * viewport_t::get_nearest_notebook() {
	if(_subtree != 0)
		return _subtree->get_nearest_notebook();
	return 0;
}

}
