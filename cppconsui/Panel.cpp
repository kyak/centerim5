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
 * Panel class implementation.
 *
 * @ingroup cppconsui
 */

#include "Panel.h"

Panel::Panel(int w, int h, const char *text, LineStyle::Type ltype)
: Widget(w, h)
, linestyle(ltype)
, title(NULL)
, title_width(0)
{
  SetTitle(text);
}

Panel::~Panel()
{
  if (title)
    g_free(title);
}

void Panel::Draw()
{
  RealUpdateArea();

  int realw, realh;

  if (!area || (realw = area->getmaxx()) == 0
      || (realh = area->getmaxy()) == 0)
    return;

  int attrs, i;

  // calc title width
  int draw_title_width = 0;
  if (realw > 4)
    draw_title_width = realw - 4;
  draw_title_width = MIN(draw_title_width, title_width);

  // calc horizontal line length (one segment width)
  int hline_len = 0;
  int extra = draw_title_width ? 4 : 2;
  if (realw > draw_title_width + extra)
    hline_len = (realw - draw_title_width - extra) / 2;

  if (draw_title_width) {
    // draw title
    attrs = GetColorPair("panel", "title");
    area->attron(attrs);
    area->mvaddstring(2 + hline_len, 0, draw_title_width, title);
    area->attroff(attrs);
  }

  // draw lines
  attrs = GetColorPair("panel", "line");
  area->attron(attrs);

  // draw top horizontal line
  for (i = 1; i < 1 + hline_len; i++)
    area->mvaddstring(i, 0, linestyle.H());
  for (i = 1 + hline_len + extra - 2 + draw_title_width; i < realw - 1; i++)
    area->mvaddstring(i, 0, linestyle.H());

  // draw bottom horizontal line
  for (i = 1; i < realw - 1; i++)
    area->mvaddstring(i, realh - 1, linestyle.H());

  // draw left and right vertical line
  for (i = 1; i < realh - 1; i++) {
    area->mvaddstring(0, i, linestyle.V());
    area->mvaddstring(realw - 1, i, linestyle.V());
  }

  // draw corners
  area->mvaddstring(0, 0, linestyle.CornerTL());
  area->mvaddstring(0, realh - 1, linestyle.CornerBL());
  area->mvaddstring(realw - 1, 0, linestyle.CornerTR());
  area->mvaddstring(realw - 1, realh - 1, linestyle.CornerBR());

  area->attroff(attrs);
}

void Panel::SetTitle(const char *text)
{
  if (title)
    g_free(title);

  if (text) {
    title = g_strdup(text);
    title_width = Curses::onscreen_width(title);
  }
  else {
    title = NULL;
    title_width = 0;
  }

  Redraw();
}

const char *Panel::GetTitle() const
{
  return title;
}

void Panel::SetBorderStyle(LineStyle::Type ltype)
{
  linestyle.SetStyle(ltype);
  Redraw();
}

LineStyle::Type Panel::GetBorderStyle() const
{
  return linestyle.GetStyle();
}
