/*
 * notebook.cxx
 *
 * copyright (2011) Benoit Gschwind
 *
 */

#include "notebook.hxx"
#include <cmath>

namespace page {

notebook_t::notebook_t(theme_layout_t const * theme) : layout(theme) {

}

notebook_t::~notebook_t() {

}

managed_window_t * notebook_t::find_client_tab(int x, int y) {
	if (_allocation.is_inside(x, y)) {
		if (!_clients.empty()) {

			int number_of_client = _clients.size();
			int selected_index = -1;

			if(!_selected.empty()) {
				list<managed_window_t *>::iterator i = find(_clients.begin(), _clients.end(), _selected.front());
				selected_index = distance(_clients.begin(), i);
			}

			list<box_int_t> tabs = layout->compute_client_tab(_allocation, number_of_client, selected_index);

			list<box_int_t>::iterator i = tabs.begin();
			while(i != tabs.end()) {
				if((*i).is_inside(x, y))
					break;
				++i;
			}

			if (i != tabs.end()) {
				int index = distance(tabs.begin(), i);
				list<managed_window_t *>::iterator x = _clients.begin();
				advance(x, index);
				return *x;
			}
		}
	}

	return 0;

}

void notebook_t::update_close_area() {

	int number_of_client = _clients.size();
	int selected_index = -1;

	if(!_selected.empty()) {
		list<managed_window_t *>::iterator i = find(_clients.begin(), _clients.end(), _selected.front());
		selected_index = distance(_clients.begin(), i);
	}

	close_client_area = layout->compute_notebook_close_window_position(_allocation, number_of_client, selected_index);
	undck_client_area = layout->compute_notebook_unbind_window_position(_allocation, number_of_client, selected_index);

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
	return box_int_t(_allocation.x + layout->notebook_margin.left,
			_allocation.y + layout->notebook_margin.top,
			_allocation.w - layout->notebook_margin.right - layout->notebook_margin.left,
			_allocation.h - layout->notebook_margin.top - layout->notebook_margin.bottom);
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
	assert(std::find(_clients.begin(), _clients.end(), c) != _clients.end());
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

	c->set_wished_position(client_size);
	c->reconfigure();
	c->fake_configure();
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
	return _allocation;
}

region_t<int> notebook_t::get_area() {
	if (!_selected.empty()) {
		region_t<int> area = _allocation;
		area -= _selected.front()->get_base_position();
		return area;
	} else {
		return region_t<int>(_allocation);
	}
}

void notebook_t::set_allocation(box_int_t const & area) {
	if (area == _allocation)
		return;

	_allocation = area;

	int number_of_client = _clients.size();
	int selected_index = -1;

	if(!_selected.empty()) {
		list<managed_window_t *>::iterator i = find(_clients.begin(), _clients.end(), _selected.front());
		selected_index = distance(_clients.begin(), i);
	}

	button_close = layout->compute_notebook_close_position(_allocation, number_of_client, selected_index);
	button_vsplit = layout->compute_notebook_vsplit_position(_allocation, number_of_client, selected_index);
	button_hsplit = layout->compute_notebook_hsplit_position(_allocation, number_of_client, selected_index);
	button_pop = layout->compute_notebook_bookmark_position(_allocation, number_of_client, selected_index);

	tab_area.x = _allocation.x;
	tab_area.y = _allocation.y;
	tab_area.w = _allocation.w;
	tab_area.h = layout->notebook_margin.top;

	top_area.x = _allocation.x;
	top_area.y = _allocation.y + layout->notebook_margin.top;
	top_area.w = _allocation.w;
	top_area.h = (_allocation.h - layout->notebook_margin.top) * 0.2;

	bottom_area.x = _allocation.x;
	bottom_area.y = _allocation.y + layout->notebook_margin.top + (0.8 * (_allocation.h - layout->notebook_margin.top));
	bottom_area.w = _allocation.w;
	bottom_area.h = (_allocation.h - layout->notebook_margin.top) * 0.2;

	left_area.x = _allocation.x;
	left_area.y = _allocation.y + layout->notebook_margin.top;
	left_area.w = _allocation.w * 0.2;
	left_area.h = (_allocation.h - layout->notebook_margin.top);

	right_area.x = _allocation.x + _allocation.w * 0.8;
	right_area.y = _allocation.y + layout->notebook_margin.top;
	right_area.w = _allocation.w * 0.2;
	right_area.h = (_allocation.h - layout->notebook_margin.top);

	popup_top_area.x = _allocation.x;
	popup_top_area.y = _allocation.y + layout->notebook_margin.top;
	popup_top_area.w = _allocation.w;
	popup_top_area.h = (_allocation.h - layout->notebook_margin.top) * 0.5;

	popup_bottom_area.x = _allocation.x;
	popup_bottom_area.y = _allocation.y + layout->notebook_margin.top
			+ (0.5 * (_allocation.h - layout->notebook_margin.top));
	popup_bottom_area.w = _allocation.w;
	popup_bottom_area.h = (_allocation.h - layout->notebook_margin.top) * 0.5;

	popup_left_area.x = _allocation.x;
	popup_left_area.y = _allocation.y + layout->notebook_margin.top;
	popup_left_area.w = _allocation.w * 0.5;
	popup_left_area.h = (_allocation.h - layout->notebook_margin.top);

	popup_right_area.x = _allocation.x + _allocation.w * 0.5;
	popup_right_area.y = _allocation.y + layout->notebook_margin.top;
	popup_right_area.w = _allocation.w * 0.5;
	popup_right_area.h = (_allocation.h - layout->notebook_margin.top);

	popup_center_area.x = _allocation.x + _allocation.w * 0.2;
	popup_center_area.y = _allocation.y + layout->notebook_margin.top + (_allocation.h - layout->notebook_margin.top) * 0.2;
	popup_center_area.w = _allocation.w * 0.6;
	popup_center_area.h = (_allocation.h - layout->notebook_margin.top) * 0.6;

	client_area.x = _allocation.x + layout->notebook_margin.left;
	client_area.y = _allocation.y + layout->notebook_margin.top;
	client_area.w = _allocation.w - layout->notebook_margin.left - layout->notebook_margin.right;
	client_area.h = _allocation.h - layout->notebook_margin.top - layout->notebook_margin.bottom;

//	printf("update client area xx %dx%d+%d+%d\n", client_area.w, client_area.h, client_area.x,
//			client_area.y);

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

	XSizeHints const size_hints = c->get_wm_normal_hints();

	//printf("XXX max : %d %d\n", max_width, max_height);

	/* default size if no size_hints is provided */
	width = max_width;
	height = max_height;

	if (size_hints.flags & PMaxSize) {
		if ((int)max_width > size_hints.max_width)
			max_width = size_hints.max_width;
		if ((int)max_height > size_hints.max_height)
			max_height = size_hints.max_height;
	}

	if (size_hints.flags & PBaseSize) {
		if ((int)max_width < size_hints.base_width)
			max_width = size_hints.base_width;
		if ((int)max_height < size_hints.base_height)
			max_height = size_hints.base_height;
	} else if (size_hints.flags & PMinSize) {
		if ((int)max_width < size_hints.min_width)
			max_width = size_hints.min_width;
		if ((int)max_height < size_hints.min_height)
			max_height = size_hints.min_height;
	}

	if (size_hints.flags & PAspect) {
		if (size_hints.flags & PBaseSize) {
			/* ICCCM say if base is set substract base before aspect checking ref : ICCCM*/
			if ((max_width - size_hints.base_width) * size_hints.min_aspect.y
					< (max_height - size_hints.base_height)
							* size_hints.min_aspect.x) {
				/* reduce h */
				max_height = size_hints.base_height
						+ ((max_width - size_hints.base_width)
								* size_hints.min_aspect.y)
								/ size_hints.min_aspect.x;

			} else if ((max_width - size_hints.base_width)
					* size_hints.max_aspect.y
					> (max_height - size_hints.base_height)
							* size_hints.max_aspect.x) {
				/* reduce w */
				max_width = size_hints.base_width
						+ ((max_height - size_hints.base_height)
								* size_hints.max_aspect.x)
								/ size_hints.max_aspect.y;
			}
		} else {
			if (max_width * size_hints.min_aspect.y
					< max_height * size_hints.min_aspect.x) {
				/* reduce h */
				max_height = (max_width * size_hints.min_aspect.y)
						/ size_hints.min_aspect.x;

			} else if (max_width * size_hints.max_aspect.y
					> max_height * size_hints.max_aspect.x) {
				/* reduce w */
				max_width = (max_height * size_hints.max_aspect.x)
						/ size_hints.max_aspect.y;
			}
		}

	}

	if (size_hints.flags & PResizeInc) {
		max_width -=
				((max_width - size_hints.base_width) % size_hints.width_inc);
		max_height -= ((max_height - size_hints.base_height)
				% size_hints.height_inc);
	}

	width = max_width;
	height = max_height;

	//printf("XXX result : %d %d\n", width, height);
}

box_int_t notebook_t::compute_client_size(managed_window_t * c) {
	unsigned int height, width;
	compute_client_size_with_constraint(c, _allocation.w - layout->notebook_margin.left - layout->notebook_margin.right,
			_allocation.h - layout->notebook_margin.top - layout->notebook_margin.bottom, width, height);

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
	return _allocation;
}

managed_window_t const * notebook_t::get_selected() {
	if (_selected.empty())
		return 0;
	else
		return _selected.front();
}

void notebook_t::set_theme(theme_layout_t const * theme) {
	this->layout = theme;
}

void notebook_t::get_childs(list<tree_t *> & lst) {
	/* has no child */
}


}
