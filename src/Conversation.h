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

#ifndef __CONVERSATION_H__
#define __CONVERSATION_H__

#include "Conf.h"
#include "Log.h"

#include <libpurple/conversation.h>

#include <cppconsui/HorizontalLine.h>
#include <cppconsui/LineStyle.h>
#include <cppconsui/TextEdit.h>
#include <cppconsui/TextView.h>
#include <cppconsui/Window.h>

class Conversation
: public Window
{
public:
	enum Status {
		STATUS_ACTIVE,
		STATUS_TRASH
	};

	Conversation(PurpleConversation *conv_);
	virtual ~Conversation();

	// FreeWindow
	virtual void Close();
	virtual void ScreenResized();
	virtual void Show();

	// Widget
	virtual void MoveResize(int newx, int newy, int neww, int newh);

	void Receive(const char *name, const char *alias, const char *message,
		PurpleMessageFlags flags, time_t mtime);

	Status GetStatus() const { return status; }

	PurpleConversation *GetPurpleConversation() const { return conv; };

protected:
	void SetPartitioning(unsigned percentage);

	virtual void LoadHistory() = 0;

	TextView *view;
	TextEdit *input;
	HorizontalLine *line;

	PurpleConversation *conv;

	char *filename;
	GIOChannel *logfile;

	Status status;

	guint destroy_id;

private:
	Conversation();
	Conversation(const Conversation&);
	Conversation& operator=(const Conversation&);

	static gboolean timeout_once_purple_conversation_destroy(gpointer data);

	void BuildLogFilename();

	virtual void Send() = 0;

	DECLARE_SIG_REGISTERKEYS();
	static bool RegisterKeys();
	void DeclareBindables();
};

class ConversationChat
: public Conversation
{
	public:
		ConversationChat(PurpleConversation *conv);
		virtual ~ConversationChat() {}

	protected:
		// Conversation
		virtual void LoadHistory();

	private:
		ConversationChat();
		ConversationChat(const ConversationChat&);
		ConversationChat& operator=(const ConversationChat&);

		// Conversation
		virtual void Send();

		PurpleConvChat* convchat;
};

class ConversationIm
: public Conversation
{
	public:
		ConversationIm(PurpleConversation *conv);
		virtual ~ConversationIm();

	protected:
		// Conversation
		virtual void LoadHistory();

	private:
		ConversationIm();
		ConversationIm(const ConversationIm&);
		ConversationIm& operator=(const ConversationIm&);

		// Conversation
		virtual void Send();

		PurpleConvIm* convim;
};


#endif /* __CONVERSATION_H__ */
