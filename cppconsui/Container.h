/*
 * Copyright (C) 2007 by Mark Pustjens <pustjens@dds.nl>
 * Copyright (C) 2010-2013 by CenterIM developers
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
 */

/**
 * @file
 * Container class.
 *
 * @ingroup cppconsui
 */

#ifndef __CONTAINER_H__
#define __CONTAINER_H__

#include "Widget.h"

#include "tree.hh"
#include <vector>

namespace CppConsUI
{

/**
 * The generic widget container class.
 *
 * It implements @ref moveFocus "moving focus" in different @ref
 * FocusDirection "directions".
 */
class Container
: public Widget
{
public:
  /**
   * Type to keep a tree of "focusable" widgets as leaves and Containers as
   * internal nodes.
   */
  typedef tree<Widget*> FocusChain;
  enum FocusCycleScope {
    /**
     * The focus doesn't cycle, it ends at the last widget from the focus
     * chain.
     */
    FOCUS_CYCLE_NONE,
    /**
     * The focus cycles only locally.
     */
    FOCUS_CYCLE_LOCAL,
    /**
     * The focus cycles also through the other containers windows.
     */
    FOCUS_CYCLE_GLOBAL
  };

  enum FocusDirection {
    FOCUS_PREVIOUS,
    FOCUS_NEXT,
    FOCUS_UP,
    FOCUS_DOWN,
    FOCUS_LEFT,
    FOCUS_RIGHT,
    FOCUS_PAGE_UP,
    FOCUS_PAGE_DOWN,
    FOCUS_BEGIN,
    FOCUS_END
  };

  Container(int w, int h);
  virtual ~Container();

  // Widget
  virtual void updateArea();
  virtual void draw();
  virtual Widget *getFocusWidget();
  virtual void cleanFocus();
  virtual bool restoreFocus();
  virtual bool grabFocus();
  virtual void ungrabFocus();
  virtual void setParent(Container& parent);

  /**
   * Adds a widget to the children list. The Container takes ownership of the
   * widget. It means that the widget will be deleted by the Container.
   */
  virtual void addWidget(Widget& widget, int x, int y);
  /**
   * Removes the widget from the children list and destroys it.
   */
  virtual void removeWidget(Widget& widget);

  /**
   * Changes logical position of the given widget to be before the position
   * widget. This affects focus cycling. Both passed widgets have to be
   * children of this Container.
   */
  virtual void moveWidgetBefore(Widget& widget, Widget& position);
  /**
   * Changes logical position of the given widget to be after the position
   * widget. This affects focus cycling. Both passed widgets have to be
   * children of this Container.
   */
  virtual void moveWidgetAfter(Widget& widget, Widget& position);

  /**
   * @todo Maybe inserting and moving of widgets should be extended. There
   * should be a way how to insert a widget at a specified position. Generally
   * insertWidget() should be made public, and MoveWidget(widget, size_t
   * position) should be added.
   */

  /**
   * Removes (and deletes) all children widgets.
   */
  virtual void clear();

  /**
   * Returns true if a widget is visible in current context.
   */
  virtual bool isWidgetVisible(const Widget& widget) const;

  /**
   * Resets the focus child by @ref cleanFocus "stealing" the focus from the
   * current chain and also ensures the focus goes also UP the chain to the
   * root widget (normally a Window).
   */
  virtual bool setFocusChild(Widget& child);
  virtual Widget *getFocusChild() const { return focus_child; }
  /**
   * Guilds a tree of the focus chain starting from this container and puts it
   * into the focus_chain tree as a subtree of @ref parent.
   */
  virtual void getFocusChain(FocusChain& focus_chain,
      FocusChain::iterator parent);
  /**
   * Gives this Container information that the cached focus chain has to be
   * updated. If this container has a parent then this information is
   * propageted to it.
   */
  virtual void updateFocusChain();
  /**
   * @todo Have a return value (to see if focus was moved successfully or
   * not)?
   */
  virtual void moveFocus(FocusDirection direction);

  virtual void setFocusCycle(FocusCycleScope scope)
    { focus_cycle_scope = scope; }
  virtual FocusCycleScope getFocusCycle() const { return focus_cycle_scope; }

  virtual void setPageFocus(bool enabled) { page_focus = enabled; }
  virtual bool canPageFocus() const { return page_focus; };

  virtual Point getRelativePosition(const Container& ref,
      const Widget& child) const;
  virtual Point getAbsolutePosition(const Widget& child) const;

  /**
   * Returns a subpad of current widget with given coordinates.
   */
  virtual Curses::Window *getSubPad(const Widget& child, int begin_x,
      int begin_y, int ncols, int nlines);

protected:
  /**
   * Structure to keep a child widget.
   */
  struct Child
  {
    Widget *widget;

    // signal connection to the widget
    sigc::connection sig_moveresize;
    sigc::connection sig_visible;

    Child(Widget& w) : widget(&w) {}
  };
  typedef std::vector<Child> Children;

  FocusCycleScope focus_cycle_scope;

  /**
   * Cached focus chain. Note: only the top container is caching the focus
   * chain.
   */
  FocusChain focus_chain;
  /**
   *
   */
  bool update_focus_chain;

  /*
   * Flag indicating if fast focus changing (paging) using PageUp/PageDown
   * keys is allowed or not.
   */
  bool page_focus;

  /**
   * This defines a chain of focus. Same as
   * dynamic_cast<Widget*>(input_child).
   */
  Widget *focus_child;

  Children children;

  /**
   * Searches children for a given widget.
   */
  virtual Children::iterator findWidget(const Widget& widget);

  /**
   * Inserts a widget in the children list at a given position. The Container
   * takes ownership of the widget. It means that the widget will be deleted
   * by the Container. This function is intended to be used by derived classes
   * that needs to keep child widgets in order (see ListBox and
   * HorizontalListBox).
   */
  virtual void insertWidget(size_t pos, Widget& widget, int x, int y);

  virtual void moveWidgetInternal(Widget& widget, Widget& position, bool after);

  virtual void onChildMoveResize(Widget& activator, const Rect& oldsize,
      const Rect& newsize);
  virtual void onChildWishSizeChange(Widget& activator, const Size& oldsize,
      const Size& newsize);
  virtual void onChildVisible(Widget& activator, bool visible);

private:
  Container(const Container&);
  Container& operator=(const Container&);

  void declareBindables();
};

} // namespace CppConsUI

#endif // __CONTAINER_H__

/* vim: set tabstop=2 shiftwidth=2 textwidth=78 expandtab : */
