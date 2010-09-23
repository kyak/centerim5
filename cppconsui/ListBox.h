/*
 * Copyright (C) 2007 by Mark Pustjens <pustjens@dds.nl>
 * Copyright (C) 2010 by CenterIM developers
 *
 * This file is part of CenterIM.
 *
 * CenterIM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CenterIM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * */

/**
 * @file
 * ListBox class.
 *
 * @ingroup cppconsui
 */

#ifndef __LISTBOX_H__
#define __LISTBOX_H__

#include "AbstractListBox.h"

/**
 * Implementation of AbstractListBox class where widgets are placed
 * vertically.
 */
class ListBox
: public AbstractListBox
{
	public:
		ListBox(int w, int h);
		virtual ~ListBox() {}

		// Widget
		virtual void UpdateArea();
		virtual void Draw();

		// AbstractListBox
		virtual void AppendSeparator();
		virtual void AppendWidget(Widget& widget);

		// Container
		virtual void RemoveWidget(Widget& widget);
		virtual Curses::Window *GetSubPad(const Widget& child, int begin_x,
				int begin_y, int ncols, int nlines);

	protected:
		int children_height;
		int autosize_children;
		int autosize_height;
		std::set<const Widget *> autosize_extra;

		// Container
		virtual void OnChildMoveResize(Widget& widget, Rect& oldsize,
				Rect& newsize);

		void UpdateScrollHeight();

	private:
		ListBox(const ListBox&);
		ListBox& operator=(const ListBox&);
};

#endif /* __LISTBOX_H__ */
