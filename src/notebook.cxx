/*
 * notebook.cxx
 *
 * copyright (2011) Benoit Gschwind
 *
 */

#include "notebook.hxx"
#include <cmath>

namespace page {

notebook_t::notebook_t(theme_t const * theme) : _theme(theme) {
	_is_default = false;
}

notebook_t::~notebook_t() {

}

bool notebook_t::add_client(managed_window_t * x, bool prefer_activate) {
	_clients.push_front(x);
	_client_map.insert(x);

	if (_selected.empty()) {
		x->normalize();
		_selected.push_front(x);
	} else {
		if (prefer_activate) {
			_selected.front()->iconify();
			x->normalize();
			_selected.push_front(x);
		} else {
			x->iconify();
			_selected.push_back(x);
		}
	}

	update_client_position(x);

	return true;
}

box_int_t notebook_t::get_new_client_size() {
	return box_int_t(allocation().x + _theme->notebook_margin.left,
			allocation().y + _theme->notebook_margin.top,
			allocation().w - _theme->notebook_margin.right - _theme->notebook_margin.left,
			allocation().h - _theme->notebook_margin.top - _theme->notebook_margin.bottom);
}

void notebook_t::replace(tree_t * src, tree_t * by) {

}

void notebook_t::close(tree_t * src) {

}

void notebook_t::remove(tree_t * src) {

}

void notebook_t::activate_client(managed_window_t * x) {
	if ((_client_map.find(x)) != _client_map.end()) {
		set_selected(x);
	}
}

list<managed_window_t *> const & notebook_t::get_clients() {
	return _clients;
}

void notebook_t::remove_client(managed_window_t * x) {
	if (x == _selected.front())
		select_next();

	// cleanup
	_selected.remove(x);
	_clients.remove(x);
	_client_map.erase(x);
}

void notebook_t::select_next() {
	if (!_selected.empty()) {
		_selected.pop_front();
		if (!_selected.empty()) {
			managed_window_t * x = _selected.front();
			update_client_position(x);
			x->normalize();
		}
	}
}

void notebook_t::set_selected(managed_window_t * c) {
	update_client_position(c);
	c->normalize();

	if (!_selected.empty()) {
		if (c != _selected.front()) {
			managed_window_t * x = _selected.front();
			x->iconify();
		}
	}

	_selected.remove(c);
	_selected.push_front(c);

}

void notebook_t::update_client_position(managed_window_t * c) {
	/* compute the window placement within notebook */
	box_int_t client_size = compute_client_size(c);

	c->set_notebook_wished_position(client_size);
	c->reconfigure();
}

void notebook_t::iconify_client(managed_window_t * x) {
	if ((_client_map.find(x)) != _client_map.end()) {
		if (!_selected.empty()) {
			if (_selected.front() == x) {
				_selected.pop_front();
				if (!_selected.empty()) {
					set_selected(_selected.front());
				}
			}
		}
	}
}

notebook_t * notebook_t::get_nearest_notebook() {
	return this;
}

void notebook_t::delete_all() {

}

void notebook_t::unmap_all() {
	if (!_selected.empty()) {
		_selected.front()->iconify();
	}
}

void notebook_t::map_all() {
	if (!_selected.empty()) {
		_selected.front()->normalize();
	}
}

box_int_t notebook_t::get_absolute_extend() {
	return allocation();
}

region_t<int> notebook_t::get_area() {
	if (!_selected.empty()) {
		region_t<int> area = allocation();
		area -= _selected.front()->get_base_position();
		return area;
	} else {
		return region_t<int>(allocation());
	}
}

void notebook_t::set_allocation(box_int_t const & area) {
	if (area == allocation())
		return;

	tree_t::set_allocation(area);

	tab_area.x = allocation().x;
	tab_area.y = allocation().y;
	tab_area.w = allocation().w;
	tab_area.h = _theme->notebook_margin.top;

	top_area.x = allocation().x;
	top_area.y = allocation().y + _theme->notebook_margin.top;
	top_area.w = allocation().w;
	top_area.h = (allocation().h - _theme->notebook_margin.top) * 0.2;

	bottom_area.x = allocation().x;
	bottom_area.y = allocation().y + _theme->notebook_margin.top + (0.8 * (allocation().h - _theme->notebook_margin.top));
	bottom_area.w = allocation().w;
	bottom_area.h = (allocation().h - _theme->notebook_margin.top) * 0.2;

	left_area.x = allocation().x;
	left_area.y = allocation().y + _theme->notebook_margin.top;
	left_area.w = allocation().w * 0.2;
	left_area.h = (allocation().h - _theme->notebook_margin.top);

	right_area.x = allocation().x + allocation().w * 0.8;
	right_area.y = allocation().y + _theme->notebook_margin.top;
	right_area.w = allocation().w * 0.2;
	right_area.h = (allocation().h - _theme->notebook_margin.top);

	popup_top_area.x = allocation().x;
	popup_top_area.y = allocation().y + _theme->notebook_margin.top;
	popup_top_area.w = allocation().w;
	popup_top_area.h = (allocation().h - _theme->notebook_margin.top) * 0.5;

	popup_bottom_area.x = allocation().x;
	popup_bottom_area.y = allocation().y + _theme->notebook_margin.top
			+ (0.5 * (allocation().h - _theme->notebook_margin.top));
	popup_bottom_area.w = allocation().w;
	popup_bottom_area.h = (allocation().h - _theme->notebook_margin.top) * 0.5;

	popup_left_area.x = allocation().x;
	popup_left_area.y = allocation().y + _theme->notebook_margin.top;
	popup_left_area.w = allocation().w * 0.5;
	popup_left_area.h = (allocation().h - _theme->notebook_margin.top);

	popup_right_area.x = allocation().x + allocation().w * 0.5;
	popup_right_area.y = allocation().y + _theme->notebook_margin.top;
	popup_right_area.w = allocation().w * 0.5;
	popup_right_area.h = (allocation().h - _theme->notebook_margin.top);

	popup_center_area.x = allocation().x + allocation().w * 0.2;
	popup_center_area.y = allocation().y + _theme->notebook_margin.top + (allocation().h - _theme->notebook_margin.top) * 0.2;
	popup_center_area.w = allocation().w * 0.6;
	popup_center_area.h = (allocation().h - _theme->notebook_margin.top) * 0.6;

	client_area.x = allocation().x + _theme->notebook_margin.left;
	client_area.y = allocation().y + _theme->notebook_margin.top;
	client_area.w = allocation().w - _theme->notebook_margin.left - _theme->notebook_margin.right;
	client_area.h = allocation().h - _theme->notebook_margin.top - _theme->notebook_margin.bottom;

	if (_selected.empty()) {
		if (!_clients.empty()) {
			_selected.push_front(_clients.front());
		}
	}

	for(set<managed_window_t *>::iterator i = _client_map.begin(); i != _client_map.end(); ++i) {
		update_client_position((*i));
	}

}


void notebook_t::compute_client_size_with_constraint(managed_window_t * c,
		unsigned int max_width, unsigned int max_height, unsigned int & width,
		unsigned int & height) {

	//printf("XXX max : %d %d\n", max_width, max_height);

	/* default size if no size_hints is provided */
	width = max_width;
	height = max_height;

	XSizeHints size_hints;
	if(!c->get_wm_normal_hints(&size_hints)) {
		return;
	}

	::page::compute_client_size_with_constraint(size_hints, max_width,
			max_height, width, height);

	//printf("XXX result : %d %d\n", width, height);
}

box_int_t notebook_t::compute_client_size(managed_window_t * c) {
	unsigned int height, width;
	compute_client_size_with_constraint(c, allocation().w - _theme->notebook_margin.left - _theme->notebook_margin.right,
			allocation().h - _theme->notebook_margin.top - _theme->notebook_margin.bottom, width, height);

	/* compute the window placement within notebook */
	box_int_t client_size;
	client_size.x = (client_area.w - (int)width) / 2;
	client_size.y = (client_area.h - (int)height) / 2;
	client_size.w = (int)width;
	client_size.h = (int)height;

	if (client_size.x < 0)
		client_size.x = 0;
	if (client_size.y < 0)
		client_size.y = 0;
	if (client_size.w > client_area.w)
		client_size.w = client_area.w;
	if (client_size.h > client_area.h)
		client_size.h = client_area.h;

	client_size.x += client_area.x;
	client_size.y += client_area.y;

	//printf("Computed client size %s\n", client_size.to_string().c_str());
	return client_size;

}

box_int_t const & notebook_t::get_allocation() {
	return allocation();
}

managed_window_t const * notebook_t::get_selected() {
	if (_selected.empty())
		return 0;
	else
		return _selected.front();
}

void notebook_t::set_theme(theme_t const * theme) {
	_theme = theme;
}

void notebook_t::get_childs(list<tree_t *> & lst) {
	/* has no child */
}


}
