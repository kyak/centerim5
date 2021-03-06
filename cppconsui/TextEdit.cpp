/*
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
 * TextEdit class implementation
 *
 * @ingroup cppconsui
 */

/*
 * Gap buffer implementation based on code from Hsin Tsao
 * (stsao@lazyhacker.com).
 */

#include "TextEdit.h"

#include <algorithm>
#include <string.h>

// gap expand size when the gap becomes filled
#define GAP_SIZE_EXPAND 4096

namespace CppConsUI
{

TextEdit::TextEdit(int w, int h, const char *text_, int flags_,
    bool single_line, bool accept_tabs_, bool masked_)
: Widget(w, h), flags(flags_), editable(true), overwrite_mode(false)
, single_line_mode(single_line), accept_tabs(accept_tabs_), masked(masked_)
, buffer(NULL), screen_lines_dirty(false)
{
  setText(text_);

  can_focus = true;
  declareBindables();
}

TextEdit::~TextEdit()
{
  g_free(buffer);
}

bool TextEdit::processInputText(const TermKeyKey &key)
{
  if (!editable)
    return false;

  if (single_line_mode && key.code.codepoint == '\n')
    return false;

  if (!accept_tabs && key.code.codepoint == '\t')
    return false;

  // filter out unwanted input
  if (flags) {
    if ((flags & FLAG_ALPHABETIC) && !g_unichar_isalpha(key.code.codepoint))
      return false;
    if ((flags & FLAG_NUMERIC) && !g_unichar_isdigit(key.code.codepoint))
      return false;
    if ((flags & FLAG_NOSPACE) && g_unichar_isspace(key.code.codepoint))
      return false;
    if ((flags & FLAG_NOPUNCTUATION) && g_unichar_ispunct(key.code.codepoint))
      return false;
  }

  insertTextAtCursor(key.utf8);
  return true;
}

void TextEdit::draw()
{
  int origw = area ? area->getmaxx() : 0;
  proceedUpdateArea();

  if (!area)
    return;

  if (origw != area->getmaxx()) {
    updateScreenLines();
    updateScreenCursor();
  }

  assertUpdatedScreenLines();

  area->erase();

  int attrs = getColorPair("textedit", "text");
  area->attron(attrs);

  int realh = area->getmaxy();
  ScreenLines::iterator i;
  int j;
  for (i = screen_lines.begin() + view_top, j = 0; i != screen_lines.end()
      && j < realh; i++, j++) {
    const char *p = i->start;
    int w = 0;
    for (size_t k = 0; k < i->length && *p != '\n'; k++) {
      if (masked)
        w += area->mvaddchar(w, j, '*');
      else {
        gunichar uc = g_utf8_get_char(p);
        if (uc == '\t') {
          int t = onScreenWidth(uc, w);
          for (int l = 0; l < t; l++)
            area->mvaddchar(w + l, j, ' ');
          w += t;
        }
        else
          w += area->mvaddchar(w, j, uc);
      }
      p = nextChar(p);
    }
  }

  area->attroff(attrs);

  if (has_focus) {
    const char *line = screen_lines[current_sc_line].start;
    int sc_x = width(line, current_sc_linepos);
    int sc_y = current_sc_line - view_top;
    area->mvchgat(sc_x, sc_y, 1, Curses::Attr::REVERSE, 0, NULL);
  }
}

void TextEdit::setText(const char *new_text)
{
  if (!new_text) {
    clear();
    return;
  }

  // XXX should the text be validated (FLAG_*)?
  size_t size = strlen(new_text);
  initBuffer(size + GAP_SIZE_EXPAND);
  insertTextAtCursor(new_text, size);
}

void TextEdit::clear()
{
  initBuffer(GAP_SIZE_EXPAND);
  redraw();
}

const char *TextEdit::getText() const
{
  g_assert(gapend > gapstart);

  screen_lines_dirty = true;

  // move gap to the end
  bool point_after_gap = point >= gapend;

  // '-1' so the last '\n' is still in the end of the buffer
  g_memmove(gapstart, gapend, bufend - gapend - 1);
  if (point_after_gap)
    point -= gapend - gapstart;
  gapstart += bufend - gapend - 1;
  gapend = bufend - 1;

  *gapstart = '\0';

  return buffer;
}

void TextEdit::setFlags(int new_flags, bool revalidate)
{
  if (new_flags == flags)
    return;

  flags = new_flags;

  if (flags && revalidate) {
    bool valid = true;
    const char *p = getTextStart();
    while (p < bufend - 1) {
      gunichar uc = g_utf8_get_char(p);
      if ((flags & FLAG_ALPHABETIC) && !g_unichar_isalpha(uc)) {
        valid = false;
        break;
      }
      if ((flags & FLAG_NUMERIC) && !g_unichar_isdigit(uc)) {
        valid = false;
        break;
      }
      if ((flags & FLAG_NOSPACE) && g_unichar_isspace(uc)) {
        valid = false;
        break;
      }
      if ((flags & FLAG_NOPUNCTUATION) && g_unichar_ispunct(uc)) {
        valid = false;
        break;
      }
      p = nextChar(p);
    }
    if (!valid)
      clear();
  }
}

void TextEdit::setSingleLineMode(bool new_single_line_mode)
{
  if (new_single_line_mode == single_line_mode)
    return;

  single_line_mode = new_single_line_mode;
}

void TextEdit::setAcceptTabs(bool new_accept_tabs)
{
  if (new_accept_tabs == accept_tabs)
    return;

  accept_tabs = new_accept_tabs;
}

void TextEdit::setMasked(bool new_masked)
{
  if (new_masked == masked)
    return;

  masked = new_masked;
  /* In the masked mode, the tab character and wide characters lose their
   * width property, thus screen lines and cursor have to be updated. */
  updateScreenLines();
  updateScreenCursor();
  redraw();
}

bool TextEdit::ScreenLine::operator==(const ScreenLine& other) const
{
  return start == other.start && end == other.end && length == other.length;
}

bool TextEdit::CmpScreenLineEnd::operator()(ScreenLine& sline,
    const char *tag)
{
  return sline.end < tag;
}

void TextEdit::initBuffer(size_t size)
{
  g_assert(size > 0);

  g_free(buffer);
  buffer = g_new(char, size);

  current_pos = 0;
  point = gapstart = buffer;

  bufend = gapend = buffer + size;
  gapend = bufend - 1;
  // insert empty line
  *gapend = '\n';

  text_length = 0;
  current_pos = 0;
  current_sc_line = 0;
  current_sc_linepos = 0;

  view_top = 0;

  updateScreenLines();
}

size_t TextEdit::getGapSize() const
{
  /* '-1' so '\0' character can be stored at the gapstart position in the
   * getText() method. */
  return gapend - gapstart - 1;
}

void TextEdit::expandGap(size_t size)
{
  if (size <= getGapSize())
    return;

  size += GAP_SIZE_EXPAND;

  const char *origbuffer = buffer;
  bool point_after_gap = point >= gapend;

  buffer = g_renew(char, buffer, (bufend - buffer) + size);

  point += buffer - origbuffer;
  bufend += buffer - origbuffer;
  gapstart += buffer - origbuffer;
  gapend += buffer - origbuffer;

  g_memmove(gapend + size, gapend, bufend - gapend);

  if (point_after_gap) {
    /* This should never happen because moveGapToCursor() is always called
     * before expandGap(). */
    point += size;
  }
  gapend += size;
  bufend += size;
}

void TextEdit::moveGapToCursor()
{
  if (point == gapstart)
    return;

  if (point == gapend) {
    point = gapstart;
    return;
  }

  // move gap towards the left
  if (point < gapstart) {
    // move the point over by gapsize
    g_memmove(point + (gapend - gapstart), point, gapstart - point);
    gapend -= gapstart - point;
    gapstart = point;
  }
  else {
    /* Since point is after the gap, find distance between gapend and
     * point and that's how much we move from gapend to gapstart. */
    g_memmove(gapstart, gapend, point - gapend);
    gapstart += point - gapend;
    gapend = point;
    point = gapstart;
  }
}

char *TextEdit::getTextStart() const
{
  if (buffer == gapstart)
    return const_cast<char*>(gapend);
  return const_cast<char*>(buffer);
}

char *TextEdit::prevChar(const char *p) const
{
  if (p >= gapend) {
    if ((p = g_utf8_find_prev_char(gapend, p)))
      return const_cast<char*>(p);
    else
      p = gapstart;
  }

  if ((p = g_utf8_find_prev_char(buffer, p)))
    return const_cast<char*>(p);
  else
    return const_cast<char*>(buffer);
}

char *TextEdit::nextChar(const char *p) const
{
  // this happens when point == gapstart
  if (p == gapstart)
    p = gapend;

  if (p < gapstart) {
    if ((p = g_utf8_find_next_char(p, gapstart)))
      return const_cast<char*>(p);
    else
      return const_cast<char*>(gapend);
  }

  if ((p = g_utf8_find_next_char(p, bufend)))
    return const_cast<char*>(p);
  else
    return const_cast<char*>(bufend);
}

int TextEdit::width(const char *start, size_t chars) const
{
  g_assert(start);

  int width = 0;

  while (chars--) {
    gunichar uc = g_utf8_get_char(start);
    width += onScreenWidth(uc, width);
    start = nextChar(start);
  }
  return width;
}

int TextEdit::onScreenWidth(gunichar uc, int w) const
{
  if (masked)
    return 1;
  return Curses::onscreen_width(uc, w);
}

char *TextEdit::getScreenLine(const char *text, int max_width,
    size_t *res_length) const
{
  g_assert(text);
  g_assert(text < bufend);
  g_assert(max_width > 0);
  g_assert(res_length);

  const char *cur = text;
  const char *res = text;
  int prev_width = 0;
  int cur_width = 0;
  size_t cur_length = 0;
  bool space = false;
  *res_length = 0;

  while (cur < bufend) {
    prev_width = cur_width;
    gunichar uc = g_utf8_get_char(cur);
    cur_width += onScreenWidth(uc, cur_width);
    cur_length++;

    if (prev_width > max_width)
      break;

    // possibly too long word
    if (cur_width > max_width && !*res_length) {
      *res_length = cur_length - 1;
      res = cur;
    }

    // end of line (paragraph on screen) found
    if (*cur == '\n') {
      *res_length = cur_length;
      return nextChar(cur);
    }

    if (g_unichar_isspace(uc))
      space = true;
    else if (space) {
      /* Found start of a word and everything before that can fit into
       * a screen line. */
      *res_length = cur_length - 1;
      res = cur;
      space = false;
    }

    cur = nextChar(cur);
  }

  /* Fix for very small max_width and characters wider that 1 cell. For
   * example, max_width = 1 and text = "W" where W is a wide character (2
   * cells width) (or simply for tabs). In that case we can not draw anything
   * but we want to skip to another character. */
  if (res == text) {
    *res_length = 1;
    res = nextChar(res);
  }

  return const_cast<char*>(res);
}

void TextEdit::updateScreenLines()
{
  screen_lines.clear();

  int realw;
  if (!area || (realw = area->getmaxx()) <= 1)
    return;

  const char *p = getTextStart();

  while (p < bufend) {
    const char *s = p;
    size_t length;
    // lower max width by one to make a room for the cursor
    p = getScreenLine(p, realw - 1, &length);
    screen_lines.push_back(ScreenLine(s, p, length));
  }
}

void TextEdit::updateScreenLines(const char *begin, const char *end)
{
  g_assert(begin);
  g_assert(end);

  int realw;
  if (!area || (realw = area->getmaxx()) <= 1)
    return;

  ScreenLines::iterator b, i;
  b = std::lower_bound(screen_lines.begin(), screen_lines.end(), begin,
      TextEdit::CmpScreenLineEnd());
  if (b != screen_lines.begin()) {
    /*
     *  Initial      Correct final
     * situation      situation
     * ---------      ---------
     * |aaaa   |  ->  |aaaa b |
     * |bcdddd |      |cdddd  |
     *
     * User inserts a space in front of the 'c' character. The 'b' string can
     * be moved on the previous line thus one more extra line before has to be
     * recalculated to handle the situation correctly.
     */
    b--;
  }
  i = b;

  ScreenLines new_screen_lines;

  const char *p = b->start;
  if (i == screen_lines.begin())
    p = getTextStart();

  while (p < bufend) {
    const char *s = p;
    size_t length;
    // lower max width by one to make a room for the cursor
    p = getScreenLine(p, realw - 1, &length);
    ScreenLine sline(s, p, length);
    new_screen_lines.push_back(sline);
    while (i != screen_lines.end() && (i->end <= end || i->start < s
          || i->end < p))
      i++;
    if (i != screen_lines.end() && sline == *i) {
      /* Screen lines are same thus it isn't necessary to recalculate more
       * screen lines. */
      break;
    }
  }
  if (i != screen_lines.end())
    i++;

  /*
  g_debug("UpdateScreenLines(), new_lines=%d, old_lines=%d",
      new_screen_lines.size(), i - b);
  */

  // replace old screen lines with new screen lines
  ScreenLines::iterator j;
  for (j = new_screen_lines.begin(); j != new_screen_lines.end() && b != i;
      j++, b++)
    *b = *j;

  if (j != new_screen_lines.end()) {
    // b == i
    screen_lines.insert(b, j, new_screen_lines.end());
  }
  else {
    // b != i
    screen_lines.erase(b, i);
  }
}

void TextEdit::assertUpdatedScreenLines()
{
  if (!screen_lines_dirty)
    return;

  updateScreenLines();
  screen_lines_dirty = false;
}

void TextEdit::updateScreenCursor()
{
  size_t acu_length = 0;
  current_sc_line = 0;
  current_sc_linepos = 0;

  if (!area)
    return;

  assertUpdatedScreenLines();

  for (ScreenLines::iterator i = screen_lines.begin();
      i != screen_lines.end(); i++) {
    size_t length = i->length;
    if (acu_length <= current_pos && current_pos < acu_length + length) {
      current_sc_linepos = current_pos - acu_length;
      break;
    }
    current_sc_line++;
    acu_length += length;
  }

  // fix cursor visibility
  int realh = area->getmaxy();
  if (view_top <= current_sc_line && current_sc_line < view_top + realh)
    return;
  while (view_top > current_sc_line)
    view_top--;
  while (view_top + realh <= current_sc_line)
    view_top++;
}

void TextEdit::insertTextAtCursor(const char *new_text, size_t new_text_bytes)
{
  g_assert(new_text);

  assertUpdatedScreenLines();

  // move the gap if the point isn't already at the start of the gap
  const char *min = gapstart;
  const char *max = gapend;
  moveGapToCursor();
  const char *min2 = gapstart;

  // check to make sure that the gap has room
  bool full_screen_lines_update = false;
  if (new_text_bytes > getGapSize()) {
    expandGap(new_text_bytes);
    full_screen_lines_update = true;
  }

  size_t n_chars = g_utf8_strlen(new_text, new_text_bytes);
  text_length += n_chars;
  current_pos += n_chars;

  while (new_text_bytes) {
    *gapstart++ = *new_text++;
    new_text_bytes--;
  }
  point = gapstart;

  if (full_screen_lines_update)
    updateScreenLines();
  else
    updateScreenLines(MIN(min, min2), MAX(max, gapend));
  updateScreenCursor();
  redraw();

  signal_text_change(*this);
}

void TextEdit::insertTextAtCursor(const char *new_text)
{
  g_assert(new_text);

  insertTextAtCursor(new_text, strlen(new_text));
}

void TextEdit::deleteFromCursor(DeleteType type, Direction dir)
{
  if (!editable)
    return;

  assertUpdatedScreenLines();

  int count = 0;

  switch (type) {
    case DELETE_CHARS:
      count = moveLogicallyFromCursor(dir) - current_pos;
      break;
    case DELETE_WORD_ENDS:
      count = moveWordFromCursor(dir, true) - current_pos;
      break;
    default:
      g_assert_not_reached();
  }

  if (count) {
    const char *min = gapstart;
    const char *max = gapend;
    moveGapToCursor();

    while (count > 0) {
      gapend = nextChar(gapend);
      text_length--;
      count--;
    }

    while (count < 0) {
      gapstart = prevChar(gapstart);
      current_pos--;
      text_length--;
      count++;
    }
    point = gapstart;

    updateScreenLines(MIN(min, gapstart), MAX(max, gapend));
    updateScreenCursor();
    redraw();

    signal_text_change(*this);
  }
}

void TextEdit::moveCursor(CursorMovement step, Direction dir)
{
  assertUpdatedScreenLines();

  size_t old_pos = current_pos;
  switch (step) {
    case MOVE_LOGICAL_POSITIONS:
      current_pos = moveLogicallyFromCursor(dir);
      break;
    case MOVE_WORDS:
      current_pos = moveWordFromCursor(dir, false);
      break;
    case MOVE_DISPLAY_LINES:
      if (dir == DIR_FORWARD) {
        if (current_sc_line + 1 < screen_lines.size()) {
          int oldw = width(screen_lines[current_sc_line].start,
              current_sc_linepos);
          // first move to end of current line
          current_pos += screen_lines[current_sc_line].length
              - current_sc_linepos;
          // find a character close to the original position
          const char *ch = screen_lines[current_sc_line + 1].start;
          size_t i = 0;
          int w = 0;
          while (w < oldw
              && i < screen_lines[current_sc_line + 1].length - 1) {
            gunichar uc = g_utf8_get_char(ch);
            w += onScreenWidth(uc, w);
            ch = nextChar(ch);
            i++;
          }
          current_pos += i;
        }
      }
      else { // DIR_BACK
        if (current_sc_line > 0) {
          int oldw = width(screen_lines[current_sc_line].start,
              current_sc_linepos);
          // first move to start of current line
          current_pos -= current_sc_linepos;
          // move to the start of the previous line
          current_pos -= screen_lines[current_sc_line - 1].length;
          // find a character close to the original position
          const char *ch = screen_lines[current_sc_line - 1].start;
          size_t i = 0;
          int w = 0;
          while (w < oldw
              && i < screen_lines[current_sc_line - 1].length - 1) {
            gunichar uc = g_utf8_get_char(ch);
            w += onScreenWidth(uc, w);
            ch = nextChar(ch);
            i++;
          }
          current_pos += i;
        }
      }
      break;
    case MOVE_DISPLAY_LINE_ENDS:
      if (dir == DIR_FORWARD)
        current_pos += screen_lines[current_sc_line].length
          - current_sc_linepos - 1;
      else // DIR_BACK
        current_pos -= current_sc_linepos;
      break;
    default:
      g_assert_not_reached();
  }

  // update point
  while (old_pos > current_pos) {
    point = prevChar(point);
    old_pos--;
  }
  while (old_pos < current_pos) {
    point = nextChar(point);
    old_pos++;
  }

  updateScreenCursor();
  redraw();
}

void TextEdit::toggleOverwrite()
{
  overwrite_mode = !overwrite_mode;
}

size_t TextEdit::moveLogicallyFromCursor(Direction dir) const
{
  if (dir == DIR_FORWARD && current_pos < text_length)
    return current_pos + 1;
  else if (dir == DIR_BACK && current_pos > 0)
    return current_pos - 1;
  return current_pos;
}

size_t TextEdit::moveWordFromCursor(Direction dir, bool word_end) const
{
  size_t new_pos = current_pos;
  const char *cur = point;
  if (cur == gapstart)
    cur = gapend;

  if (dir == DIR_FORWARD) {
    if (word_end) {
      // search for the first white character after nonwhite characters
      bool nonwhite = false;
      while (new_pos < text_length) {
        if (!g_unichar_isspace(g_utf8_get_char(cur)) && *cur != '\n')
          nonwhite = true;
        else if (nonwhite)
          break;
        cur = nextChar(cur);
        new_pos++;
      }
      return new_pos;
    }
    else {
      // search for the first nonwhite character after white characters
      bool white = false;
      while (new_pos < text_length) {
        if (g_unichar_isspace(g_utf8_get_char(cur)) || *cur == '\n')
          white = true;
        else if (white)
          break;
        cur = nextChar(cur);
        new_pos++;
      }
      return new_pos;
    }
  }
  else { // DIR_BACK
    if (new_pos == 0)
      return 0;

    // always move at least one character back
    cur = prevChar(cur);
    new_pos--;

    // search for the first white character before nonwhite characters
    bool nonwhite = false;
    while (new_pos != static_cast<size_t>(-1)) {
      if (!g_unichar_isspace(g_utf8_get_char(cur)) && *cur != '\n')
        nonwhite = true;
      else if (nonwhite)
        break;
      if (new_pos > 0)
        cur = prevChar(cur);
      new_pos--;
    }
    return ++new_pos;
  }
}

void TextEdit::actionMoveCursor(CursorMovement step, Direction dir)
{
  moveCursor(step, dir);
}

void TextEdit::actionDelete(DeleteType type, Direction dir)
{
  deleteFromCursor(type, dir);
}

void TextEdit::actionToggleOverwrite()
{
  toggleOverwrite();
}

void TextEdit::declareBindables()
{
  // cursor movement
  declareBindable("textentry", "cursor-right",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionMoveCursor),
        MOVE_LOGICAL_POSITIONS, DIR_FORWARD),
      InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "cursor-left",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionMoveCursor),
        MOVE_LOGICAL_POSITIONS, DIR_BACK), InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "cursor-down",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionMoveCursor),
        MOVE_DISPLAY_LINES, DIR_FORWARD), InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "cursor-up",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionMoveCursor),
        MOVE_DISPLAY_LINES, DIR_BACK), InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "cursor-right-word",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionMoveCursor), MOVE_WORDS,
        DIR_FORWARD), InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "cursor-left-word",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionMoveCursor), MOVE_WORDS,
        DIR_BACK), InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "cursor-end",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionMoveCursor),
        MOVE_DISPLAY_LINE_ENDS, DIR_FORWARD),
      InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "cursor-begin",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionMoveCursor),
        MOVE_DISPLAY_LINE_ENDS, DIR_BACK), InputProcessor::BINDABLE_NORMAL);

  // deleting text
  declareBindable("textentry", "delete-char",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionDelete),
        DELETE_CHARS, DIR_FORWARD), InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "backspace",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionDelete),
        DELETE_CHARS, DIR_BACK), InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "delete-word-end",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionDelete),
        DELETE_WORD_ENDS, DIR_FORWARD), InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "delete-word-begin",
      sigc::bind(sigc::mem_fun(this, &TextEdit::actionDelete),
        DELETE_WORD_ENDS, DIR_BACK), InputProcessor::BINDABLE_NORMAL);

  declareBindable("textentry", "newline",
      sigc::bind(sigc::mem_fun(this, static_cast<void (TextEdit::*)
          (const char*)>(&TextEdit::insertTextAtCursor)), "\n"),
      InputProcessor::BINDABLE_NORMAL);

  /*
  // overwrite
  declareBindable("textentry", "toggle-overwrite", sigc::mem_fun(this,
        &TextEdit::actionToggleOverwrite), InputProcessor::BINDABLE_NORMAL);
  */
}

} // namespace CppConsUI

/* vim: set tabstop=2 shiftwidth=2 textwidth=78 expandtab : */
