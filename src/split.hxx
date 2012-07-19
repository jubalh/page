/*
 * split.hxx
 *
 *  Created on: 27 févr. 2011
 *      Author: gschwind
 */

#ifndef SPLIT_HXX_
#define SPLIT_HXX_

#include <cairo.h>

#include "xconnection.hxx"
#include "render_context.hxx"
#include "box.hxx"
#include "tree.hxx"
#include "page_base.hxx"

namespace page {

enum split_type_e {
	HORIZONTAL_SPLIT, VERTICAL_SPLIT,
};

class split_t: public tree_t {
	static int const GRIP_SIZE = 3;

	class xevent_handler_t : public ::page::xevent_handler_t {
		split_t & split;
	public:
		xevent_handler_t(split_t & split) : split(split) { }
		virtual ~xevent_handler_t() { }
		virtual void process_event(XEvent const & e);
	};


	page_base_t & page;

	xevent_handler_t even_handler;

	Cursor cursor;
	box_t<int> separetion_bar;
	split_type_e _split_type;
	double _split;
	tree_t * _pack0;
	tree_t * _pack1;

	static Bool drag_and_drop_filter(Display * dpy, XEvent * ev, char * arg);
	void process_drag_and_drop();

public:
	split_t(page_base_t & page, split_type_e type);
	~split_t();
	void update_allocation(box_t<int> & alloc);
	void replace(tree_t * src, tree_t * by);
	cairo_t * get_cairo();
	void close(tree_t * src);
	void remove(tree_t * src);

	window_list_t get_windows();

	virtual bool add_client(window_t *c);
	virtual void remove_client(window_t * c);
	virtual void activate_client(window_t * c);
	virtual void iconify_client(window_t * c);

	void update_allocation_pack0();
	void update_allocation_pack1();
	void delete_all();

	void compute_slider_area(box_int_t & area);

	virtual void repair1(cairo_t * cr, box_int_t const & area);
	virtual box_int_t get_absolute_extend();
	virtual void reconfigure(box_int_t const & area);

};

}

#endif /* SPLIT_HXX_ */
