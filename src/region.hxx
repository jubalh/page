/*
 * region.hxx
 *
 * copyright (2010-2014) Benoit Gschwind
 *
 * This code is licensed under the GPLv3. see COPYING file for more details.
 *
 */


#ifndef REGION_HXX_
#define REGION_HXX_

#include <sstream>
#include <string>
#include <vector>
#include <iostream>

#include "box.hxx"

namespace page {

using namespace std;

template<typename T>
class region_t : public std::vector<i_rect_t<T>> {
	/** short cut for the superior class **/
	using super = std::vector<i_rect_t<T> >;
	using _box_t = i_rect_t<T>;

	/** this function reduce the number of boxes if possible **/
	static region_t & clean_up(region_t & lst) {
		merge_area_macro(lst);
		return lst;
	}

	/** merge 2 i_rects when it is possible **/
	static void merge_area_macro(region_t & lst) {
		if(lst.size() < 2)
			return;
		/* store the current end of the list */
		auto xend = lst.end();
		bool end = false;
		while (not end) {
			end = true;
			for (auto i = lst.begin(); i != xend; ++i) {

				for (auto j = i + 1; j != xend; ++j) {

					/** left/right **/
					if (i->x + i->w == j->x and i->y == j->y and i->h == j->h) {
						*i = _box_t{i->x, i->y, j->w + i->w, i->h};
						--xend;
						*j = *xend;
						--j;
						end = false;
						continue;
					}

					/** right/left **/
					if (i->x == j->x + j->w and i->y == j->y and i->h == j->h) {
						*i = _box_t{j->x, j->y, j->w + i->w, j->h};
						--xend;
						*j = *xend;
						--j;
						end = false;
						continue;
					}

					/** top/bottom **/
					if (i->y == j->y + j->h and i->x == j->x and i->w == j->w) {
						*i = _box_t{j->x, j->y, j->w, j->h + i->h};
						--xend;
						*j = *xend;
						--j;
						end = false;
						continue;
					}

					/** bottom/top **/
					if (i->y + i->h == j->y and i->x == j->x and i->w == j->w) {
						*i = _box_t{i->x, i->y, i->w, j->h + i->h};
						--xend;
						*j = *xend;
						--j;
						end = false;
						continue;
					}
				}
			}
		}

		lst.resize(xend - lst.begin());

//		for(auto const & i: lst) {
//			for(auto const & j: lst) {
//				if(&i == &j) {
//					continue;
//				}
//				if(i.has_intersection(j)) {
//					cout << "WARNING : box overlaps" << endl;
//				}
//			}
//		}

	}

//	/** remove empty boxes **/
//	static void remove_empty(region_t & list) {
//		auto i = list.begin();
//		auto j = list.begin();
//		while (j != list.end()) {
//			if(not j->is_null()) {
//				*i = *j;
//				++i;
//			}
//			++j;
//		}
//
//		/* reduce the list size */
//		list.resize(distance(list.begin(), i));
//	}

	bool is_null() {
		return this->empty();
	}

	/* box0 - box1 */
	static region_t substract_box(_box_t const & box0,
			_box_t const & box1) {
		region_t result;

		_box_t intersection = box0 & box1;

		if (not intersection.is_null()) {
			/* top box */
			if (intersection.y != box0.y) {
				result.push_back(_box_t(box0.x, box0.y, box0.w, intersection.y - box0.y));
			}

			/* bottom box */
			if (intersection.y + intersection.h != box0.y + box0.h) {
				result.push_back(_box_t(box0.x, intersection.y + intersection.h, box0.w, (box0.y + box0.h) - (intersection.y + intersection.h)));
			}

			/* left box */
			if (intersection.x != box0.x) {
				result.push_back(_box_t(box0.x, intersection.y, intersection.x - box0.x, intersection.h));
			}

			/* right box */
			if(intersection.x + intersection.w != box0.x + box0.w) {
				result.push_back(_box_t(intersection.x + intersection.w, intersection.y, (box0.x+box0.w)-(intersection.x+intersection.w), intersection.h));
			}

		} else {
			result.push_back(box0);
		}

		return clean_up(result);

	}

public:

	/** create an empty region **/
	region_t() : super() {
		//printf("capacity = %lu\n", this->capacity());
	}

	region_t(region_t const & r) : super(r) {
		//printf("capacity = %lu\n", this->capacity());
	}

	region_t(region_t const && r) : super(r) { }

	template<typename U>
	region_t(std::vector<U> const & v) {
		for (int i = 0; i + 3 < v.size(); i += 4) {
			super::push_back(_box_t ( v[i + 0], v[i + 1], v[i + 2], v[i + 3] ));
		}
	}

	region_t(_box_t const & b) {
		if (!b.is_null())
			this->push_back(b);
	}

	region_t(T x, T y, T w, T h) {
		_box_t b(x, y, w, h);
		if (!b.is_null())
			this->push_back(b);
	}

	~region_t() {
		//printf("capacity = %lu\n", this->capacity());
	}

	region_t & operator =(_box_t const & b) {
		this->clear();
		if (!b.is_null())
			this->push_back(b);
		return *this;
	}

	region_t & operator =(region_t const & r) {
		/** call the superior class assign operator **/
		super::operator =(r);
		return *this;
	}

	region_t & operator =(region_t const && r) {
		/** call the superior class assign operator **/
		super::operator =(r);
		return *this;
	}

	region_t & operator -=(_box_t const & box1) {
		region_t tmp;
		for (auto & i : *this) {
			region_t x = substract_box(i, box1);
			tmp.insert(tmp.end(), x.begin(), x.end());
		}
		return (*this = tmp);
	}

	region_t operator -(_box_t const & b) {
		region_t r = *this;
		return (r -= b);
	}

	region_t & operator -=(region_t const & b) {

		if(this == &b) {
			this->clear();
			return *this;
		}

		for (auto & i : b) {
			*this -= i;
		}

		return *this;
	}

	region_t operator -(region_t const & b) const {
		region_t result = *this;
		return (result -= b);
	}

	region_t & operator +=(_box_t const & b) {
		if (!b.is_null()) {
			*this -= b;
			this->push_back(b);
			clean_up(*this);
		}
		return *this;
	}

	region_t & operator +=(region_t const & r) {
		/** sum same regions give same region **/
		if(this == &r)
			return *this;

		for(auto & i : r) {
			*this += i;
		}

		return *this;
	}

	region_t operator +(_box_t const & b) const {
		region_t result = *this;
		return result += b;
	}

	region_t operator +(region_t const & r) const {
		region_t result = *this;
		return result += r;
	}

	region_t operator &(_box_t const & b) const {
		region_t result;
		for(auto & i : *this) {
			_box_t x = b & i;
			/**
			 * since this is a region, no over lap is possible, just add the
			 * intersection if not null. (do not use operator+= for optimal
			 * result)
			 **/
			if(!x.is_null()) {
				result.push_back(x);
			}
		}
		return clean_up(result);
	}

	region_t operator &(region_t const & r) const {
		region_t result;
		for(auto const & i : *this) {
			region_t clipped = r & i;
			result.insert(result.end(), clipped.begin(), clipped.end());
		}
		return clean_up(result);
	}

	region_t & operator &=(_box_t const & b) {
		*this = *this & b;
		return *this;
	}

	region_t & operator &=(region_t const & r) {
		if(this != &r)
			*this = *this & r;
		return *this;
	}


	/** make std::string **/
	std::string to_string() const {
		std::ostringstream os;
		auto i = this->begin();
		if(i == this->end())
			return os.str();
		os << i->to_string();
		++i;
		while(i != this->end()) {
			os << "," << i->to_string();
			++i;
		}

		return os.str();
	}

	void translate(T x, T y) {
		for(auto & i : *this) {
			i.x += x;
			i.y += y;
		}
	}


	T area() {
		T ret{T()};
		for(auto &i: *this) {
			ret += i.w * i.h;
		}
		return ret;
	}

	bool is_inside(T x, T y) const {
		for(auto & b: *this) {
			if(b.is_inside(x, y))
				return true;
		}
		return false;
	}

};

typedef region_t<int> region;

}

#endif /* REGION_HXX_ */
