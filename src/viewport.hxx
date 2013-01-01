/*
 * viewport.hxx
 *
 * copyright (2012) Benoit Gschwind
 *
 */

#ifndef VIEWPORT_HXX_
#define VIEWPORT_HXX_

#include "page_base.hxx"
#include "tree.hxx"
#include "box.hxx"
#include "notebook.hxx"

namespace page {

struct viewport_t: public tree_t {
public:
	//page_base_t & page;
	box_t<int> raw_aera;
	box_t<int> effective_aera;
	tree_t * _subtree;
	window_t * fullscreen_client;

	bool _is_visible;

	viewport_t(box_t<int> const & area);

	void add_notebook(notebook_t * n) {
		_subtree = n;
		_subtree->set_parent(this);
		_subtree->set_allocation(effective_aera);
	}

	void reconfigure();

	virtual void replace(tree_t * src, tree_t * by);
	virtual void remove(tree_t * src);
	virtual void close(tree_t * src);
	virtual window_set_t get_windows();

	virtual bool add_client(window_t * x);
	virtual box_int_t get_new_client_size();
	virtual void remove_client(window_t * x);
	virtual void activate_client(window_t * x);
	virtual void iconify_client(window_t * x);
	virtual void delete_all();
	virtual void unmap_all();
	virtual void map_all();

	notebook_t * get_nearest_notebook();

	virtual void repair1(cairo_t * cr, box_int_t const & area);
	virtual box_int_t get_absolute_extend();
	virtual region_t<int> get_area();
	virtual void set_allocation(box_int_t const & area);

	void fix_allocation();

	virtual bool is_visible() {
		return true;
	}

};

}

#endif /* VIEWPORT_HXX_ */
