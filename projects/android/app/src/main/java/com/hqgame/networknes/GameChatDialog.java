////////////////////////////////////////////////////////////////////////////////////////
//
// Multiness - NES/Famicom emulator written in C++
// Based on Nestopia emulator
//
// Copyright (C) 2016-2018 Le Hoang Quyen
//
// This file is part of Multiness.
//
// Multiness is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Multiness is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Multiness; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

package com.hqgame.networknes;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.text.style.UnderlineSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import java.util.TreeMap;

/**
 * Created by le on 6/17/2016.
 */
public class GameChatDialog extends DialogFragment {

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        //set title
        getDialog().setTitle(R.string.chat_room_lbl);

        //fill the entire screen
        int width = ViewGroup.LayoutParams.MATCH_PARENT;
        int height = ViewGroup.LayoutParams.MATCH_PARENT;
        getDialog().getWindow().setLayout(width, height);

        //create view
        View v = inflater.inflate(R.layout.chat_dialog, container, false);

        ListView chatDatabaseView = (ListView) v.findViewById(R.id.chatHistoryList);

        final EditText typingMessageView = (EditText)v.findViewById(R.id.typingChatMessageTxtView);
        Button sendBtn = (Button)v.findViewById(R.id.btnSendMessage);

        if (getActivity() == null || getActivity() instanceof Delegate == false)
            throw new IllegalStateException("Activity of GameChatDialog must implement Delegate interface");

        Delegate delegate = (Delegate)getActivity();

        //register list view adapter
        chatDatabaseView.setAdapter(delegate.getChatDatabaseViewAdapter());

        //register button's click listener
        sendBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                String message = typingMessageView.getText().toString();
                if (message.length() > 0)
                    ((Delegate)getActivity()).onSendButtonClicked(message);
            }
        });

        return v;
    }

    public void clearTypingText() {
        EditText typingMessageView = (EditText)getView().findViewById(R.id.typingChatMessageTxtView);
        typingMessageView.setText("");
    }

    /*---------- ChatMessagesDatabaseViewAdapter -------------*/
    public static class ChatMessagesDatabaseViewAdapter extends BaseAdapter {
        private static final int MAX_MESSAGES_TO_KEEP = 10;

        private TreeMap<Long, ChatMessage> messages = new TreeMap<>();
        private ChatMessage[] messagesArray = null;

        private final Context context;
        private final LayoutInflater layoutInflater;

        public ChatMessagesDatabaseViewAdapter(Context context, LayoutInflater layoutInflater) {
            this.context = context;
            this.layoutInflater = layoutInflater;
        }

        public void addMessage(long id, ChatMessage message) {
            if (messages.size() >= MAX_MESSAGES_TO_KEEP)
            {
                messages.remove(messages.firstKey());
            }

            messages.put(id, message);
        }

        public ChatMessage getMessage(long id) {
            return messages.get(id);
        }

        @Override
        public void notifyDataSetChanged() {
            messagesArray = messages.values().toArray(new ChatMessage[0]);

            super.notifyDataSetChanged();
        }

        @Override
        public int getCount() {
            return messagesArray != null ? messagesArray.length : 0;
        }

        @Override
        public Object getItem(int position) {
            return messagesArray != null ? messagesArray[position] : null;
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View vi = convertView;
            if (vi == null)
                vi = layoutInflater.inflate(R.layout.chat_message_item_layout, parent, false);

            ChatMessage message = (ChatMessage)getItem(position);

            TextView messageView = (TextView) vi.findViewById(R.id.chatMessageTxtView);
            TextView messageStatusView = (TextView) vi.findViewById(R.id.chatMessageStatusView);

            String sender = message.sender;
            if (message.sender == null) {
                if (message.isOurs())
                {
                    sender = context.getString(R.string.me);
                }
            }

            if (sender == null) {
                messageView.setText(message.message);
            }
            else
            {
                SpannableString ss = new SpannableString(sender + ": " + message.message);
                ss.setSpan(new StyleSpan(Typeface.BOLD), 0, sender.length() + 1, 0);
                ss.setSpan(new UnderlineSpan(), 0, sender.length() + 1, 0);
                messageView.setText(ss);
            }

            if (message.getStatus() == ChatMessage.Status.RECEIVED_BY_US)//this is message from remote side
                messageView.setTextColor(Color.BLUE);
            else
                messageView.setTextColor(Color.BLACK);

            switch (message.getStatus())
            {
                case SENT_BY_US:
                    messageStatusView.setText(R.string.pending);
                    break;
                case FAILED_TO_SEND:
                    messageStatusView.setText(R.string.failed);
                    break;
                case RECEIVED_BY_THEM:
                    messageStatusView.setText(R.string.received);
                    break;
                case RECEIVED_BY_US:
                    messageStatusView.setText("");
                    break;
            }

            return vi;
        }
    }

    /*---------- ChatMessage -----*/
    public static class ChatMessage {
        public static enum Status {
            SENT_BY_US,
            FAILED_TO_SEND,
            RECEIVED_BY_THEM,
            RECEIVED_BY_US,
        }

        public final String sender;
        public final String message;

        private Status status;

        public ChatMessage(String message) {
            this(message, Status.SENT_BY_US);
        }

        public ChatMessage(String message, Status status) {
            this(null, message, status);
        }

        public ChatMessage(String sender, String message, Status status) {
            this.sender = sender;
            this.message = message;
            this.status = status;
        }

        public boolean isOurs() {
            return status == Status.SENT_BY_US ||
                    status == Status.FAILED_TO_SEND ||
                    status == Status.RECEIVED_BY_THEM;
        }

        public Status getStatus() { return status; }
        public void setStatus(Status status) {
            this.status = status;
        }
    }

    /*---- Delegate ---*/
    public static interface Delegate {
        public ChatMessagesDatabaseViewAdapter getChatDatabaseViewAdapter();
        public void onSendButtonClicked(String message);
    }
}
