/*
 * renderable.hxx
 *
 * copyright (2012) Benoit Gschwind
 *
 */

#ifndef RENDERABLE_HXX_
#define RENDERABLE_HXX_

#include <list>
#include <cairo.h>
#include "box.hxx"
#include "region.hxx"

namespace page {

/**
 * Renderable class are object that can be draw on screen, mainly Window
 */
class renderable_t {
public:
	/**
	 * renderable default constructor.
	 */
	renderable_t() { }

	/**
	 * Destroy renderable
	 */
	virtual ~renderable_t() { }

	/**
	 * draw the area of a renderable to the destination surface
	 * @param cr the destination surface context
	 * @param area the area to redraw
	 */
	virtual void render(cairo_t * cr) = 0;

};

/**
 * short cut for renderable list.
 */
typedef std::list<renderable_t *> renderable_list_t;

}


#endif /* RENDERABLE_HXX_ */
