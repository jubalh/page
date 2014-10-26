/*
 * tree.hxx
 *
 * copyright (2010-2014) Benoit Gschwind
 *
 * This code is licensed under the GPLv3. see COPYING file for more details.
 *
 */

#ifndef TREE_HXX_
#define TREE_HXX_

#include <stdint.h>

#include <X11/Xlib.h>
#include <cairo.h>
#include <list>
#include <vector>
#include "region.hxx"
#include "time.hxx"
#include "renderable.hxx"

namespace page {

using namespace std;

class tree_t {
public:
	tree_t() { }

	virtual ~tree_t();
	virtual tree_t * parent() const = 0;

	virtual string get_node_name() const = 0;

	/**
	 * Raise a child over others respecting stack priority
	 * if t == nullptr, this mean we raise ourself.
	 **/
	virtual void raise_child(tree_t * t = nullptr) = 0;
	virtual void remove(tree_t * t) = 0;
	virtual void set_parent(tree_t * parent) = 0;
	virtual void get_all_children(vector<tree_t *> & out) const = 0;
	virtual void children(vector<tree_t *> & out) const = 0;

	template<char const c>
	string _get_node_name() const {
		char buffer[64];
		snprintf(buffer, 64, "%c #%016lx #%016lx", c, (uintptr_t) parent(),
				(uintptr_t) this);
		return string(buffer);
	}

	virtual void prepare_render(vector<ptr<renderable_t>> & out, page::time_t const & time) = 0;

	void print_tree(int level = 0) const {
		char space[] = "                               ";
		space[level] = 0;
		cout << space << get_node_name() << endl;
		for(auto i: children()) {
			i->print_tree(level+1);
		}

	}

	vector<tree_t *> children() const {
		vector<tree_t *> ret;
		children(ret);
		return ret;
	}


};


}

#endif /* TREE_HXX_ */
