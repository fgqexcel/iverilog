#ifndef __svector_H
#define __svector_H
/*
 * Copyright (c) 1999 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version. In order to redistribute the software in
 *    binary form, you will need a Picture Elements Binary Software
 *    License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#if !defined(WINNT)
#ident "$Id: svector.h,v 1.2 1999/05/01 02:57:53 steve Exp $"
#endif

# include  <assert.h>

/*
 * This is a way simplified vector class that cannot grow or shrink,
 * and is really only able to handle values. It is intended to be
 * lighter weight then the STL list class.
 */

template <class TYPE> class svector {

    public:
      svector(unsigned size) : nitems_(size), items_(new TYPE[size])
	    { for (unsigned idx = 0 ;  idx < size ;  idx += 1)
		  items_[idx] = 0;
	    }

      svector(const svector<TYPE>&that)
            : nitems_(that.nitems_), items_(new TYPE[nitems_])
	    { for (unsigned idx = 0 ;  idx < that.nitems_ ;  idx += 1)
		    items_[idx] = that[idx];
	    }

      svector(const svector<TYPE>&l, const svector<TYPE>&r)
            : nitems_(l.nitems_ + r.nitems_), items_(new TYPE[nitems_])
	    { for (unsigned idx = 0 ;  idx < l.nitems_ ;  idx += 1)
		    items_[idx] = l[idx];

	      for (unsigned idx = 0 ;  idx < r.nitems_ ;  idx += 1)
		    items_[l.nitems_+idx] = r[idx];
	    }

      ~svector() { delete[]items_; }

      unsigned count() const { return nitems_; }

      TYPE&operator[] (unsigned idx)
	    { assert(idx < nitems_);
	      return items_[idx];
	    }

      TYPE operator[] (unsigned idx) const
	    { assert(idx < nitems_);
	      return items_[idx];
	    }

    private:
      unsigned nitems_;
      TYPE* items_;

    private: // not implemented
      svector<TYPE>& operator= (const svector<TYPE>&);

};

/*
 * $Log: svector.h,v $
 * Revision 1.2  1999/05/01 02:57:53  steve
 *  Handle much more complex event expressions.
 *
 * Revision 1.1  1999/04/29 02:16:26  steve
 *  Parse OR of event expressions.
 *
 */
#endif
