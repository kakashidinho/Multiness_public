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


import android.app.ProgressDialog;
import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ListView;

import java.lang.ref.WeakReference;


/**
 * A simple {@link BaseActivity} subclass.
 */
public abstract class BaseInvitationsPage<InvitationObj> extends BasePage {
    private AsyncOperation invitationsFetchOp = null;

    private LayoutInflater listItemInflater = null;

    private InvitationItemAdapter invitationListAdapter = new InvitationItemAdapter();
    private WeakReference<View> selectedInvitationViewRef = new WeakReference(null);
    private InvitationObj selectedInvitation = null;

    private ProgressDialog progressDialog = null;


    private Button joinBtn = null;
    private Button deleteBtn = null;
    private ImageButton refreshBtn = null;


    protected abstract void deleteInvitation(@NonNull InvitationObj obj, final Runnable finishCallback);
    protected abstract void acceptedInvitation(@NonNull InvitationObj obj);
    protected abstract AsyncOperation doFetchInvitations(AsyncQuery<Boolean> finishCallback);
    protected abstract InvitationObj getInvitation(int index);
    protected abstract int getNumInvitations();
    protected abstract View createInvitationItemView(
            @NonNull InvitationObj obj,
            LayoutInflater inflater,
            View convertView,
            ViewGroup parent,
            QueryCallback<Boolean> selectedItemChangedCallback);

    protected boolean invitationsAreEqual(InvitationObj lhs, InvitationObj rhs) {
        return lhs != null && lhs == rhs;
    }

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.title_invitations));

        View v = inflater.inflate(R.layout.page_invitations, container, false);

        //create inflater for list view's items
        listItemInflater = (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        ListView invitationListView = (ListView)v.findViewById(R.id.ui_invitations_list);

        invitationListView.setAdapter(invitationListAdapter);
        invitationListView.setOnItemClickListener(new InvitationItemClickListener());

        //register buttons' handler
        joinBtn = (Button)v.findViewById(R.id.btnJoinRoom);
        deleteBtn = (Button)v.findViewById(R.id.btnDeleteRoom);
        refreshBtn = (ImageButton)v.findViewById(R.id.btnRoomRefresh);

        View.OnClickListener clickListener = new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                BaseInvitationsPage.this.onClick(v);
            }
        };

        joinBtn.setOnClickListener(clickListener);
        deleteBtn.setOnClickListener(clickListener);
        refreshBtn.setOnClickListener(clickListener);

        return v;
    }

    @Override
    public void onResume() {
        super.onResume();

        //fetch invitations
        fetchInvitations();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        cancelInvitationsFetching();

        //resume cleaning up task
        FBUtils.resumePendingInvitationsCleanupTask();
    }

    // if user enter the page by accepting the invitation on notification bar, then
    // cancel existing fetching task
    @Override
    protected void onJoiningGoogleMatchBegin(String matchId) {
        cancelInvitationsFetching();
        super.onJoiningGoogleMatchBegin(matchId);
    }

    private void onClick(View view) {
        switch (view.getId()) {
            case R.id.btnJoinRoom:
                acceptInvitation();
                break;
            case R.id.btnDeleteRoom:
            {
                Utils.dialog(
                        getContext(),
                        getString(R.string.delete),
                        getString(R.string.delete_invitation_msg),
                        null,
                        new Runnable() {
                            @Override
                            public void run() {
                                showProgressDialog(getString(R.string.loading_msg), new Runnable() {
                                    @Override
                                    public void run() {
                                        refresh();
                                    }
                                });
                                if (selectedInvitation != null)
                                {
                                    deleteInvitation(selectedInvitation, new Runnable() {
                                        @Override
                                        public void run() {
                                            dismissProgressDialog();
                                            refresh();
                                        }
                                    });
                                }
                            }
                        },
                        new Runnable() {
                            @Override
                            public void run() {
                                //TODO
                            }
                        });
            }
                break;
            case R.id.btnRoomRefresh:
                refresh();
                break;
        }
    }

    private void acceptInvitation() {
        if (selectedInvitation == null)
            return;

        acceptedInvitation(selectedInvitation);
    }

    private void cancelInvitationsFetching() {
        dismissProgressDialog();

        if (invitationsFetchOp != null) {
            invitationsFetchOp.cancel();
            invitationsFetchOp = null;
        }
    }

    protected void refresh() {
        // this method may do more than just fetching in future
        fetchInvitations();
    }

    private void fetchInvitations() {
        invalidateSelectedInvitation();

        //cancel ongoing fetch
        cancelInvitationsFetching();

        //invalidate current invitations list
        invitationListAdapter.notifyDataSetChanged();

        //show progress dialog
        showProgressDialog(getString(R.string.loading_msg), new Runnable() {
            @Override
            public void run() {
                cancelInvitationsFetching();
            }
        });


        //start fetching
        invitationsFetchOp = doFetchInvitations(new AsyncQuery<Boolean>() {
            @Override
            public void run(final Boolean hasDataChanged) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        dismissProgressDialog();

                        if (hasDataChanged)
                            invitationListAdapter.notifyDataSetChanged();
                    }
                });
            }
        });
    }

    private void invalidateSelectedInvitation() {
        View selectedInvitationView = selectedInvitationViewRef.get();
        if (selectedInvitationView != null) {
            //stop highlighting the previously selected item
            selectedInvitationView.setBackgroundResource(android.R.color.transparent);
            selectedInvitationViewRef = new WeakReference<View>(null);
        }

        selectedInvitation = null;

        joinBtn.setEnabled(false);
        deleteBtn.setEnabled(false);
    }

    protected void dismissProgressDialog() {
        if (this.progressDialog != null) {
            this.progressDialog.dismiss();
            this.progressDialog = null;
        }
    }

    protected void notifyDataSetChanged() {
        invitationListAdapter.notifyDataSetChanged();
    }

    protected InvitationObj getSelectedInvitation() {
        return selectedInvitation;
    }

    protected void showProgressDialog(String message, Runnable cancelCallback) {
        dismissProgressDialog();

        this.progressDialog = Utils.showProgressDialog(getContext(), message, cancelCallback);
    }

    protected void displayErrorDialog(int messageResId) {
        displayErrorDialog(getString(messageResId));
    }

    protected void displayErrorDialog(String message) {
        displayErrorDialog(message, new Runnable() {
            @Override
            public void run() {
                refresh();
            }
        });
    }

    protected void displayErrorDialog(int messageResId, final Runnable closeCallback) {
        displayErrorDialog(getString(messageResId), closeCallback);
    }

    protected void displayErrorDialog(String message, final Runnable closeCallback) {
        Utils.errorDialog(getContext(), message, new Runnable() {
            @Override
            public void run() {
                if (closeCallback != null)
                    closeCallback.run();
            }
        });
    }

    /*----------- InvitationItemAdapter --------*/
    private class InvitationItemAdapter extends BaseAdapter {
        @Override
        public int getCount() {
            return getNumInvitations();
        }

        @Override
        public Object getItem(int position) {
            return getInvitation(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final InvitationObj info = getInvitation(position);

            View v = createInvitationItemView(info, listItemInflater, convertView, parent, new QueryCallback<Boolean>() {
                @Override
                public void run(Boolean result) {
                    if (result != null && result) {
                        invalidateSelectedInvitation();

                        selectedInvitation = info;
                    }
                }
            });

            if (invitationsAreEqual(getInvitation(position), selectedInvitation)) {
                v.setBackgroundResource(android.R.color.darker_gray);//highlight selected item
                selectedInvitationViewRef = new WeakReference<View>(v);

                joinBtn.setEnabled(true);
                deleteBtn.setEnabled(true);
            }
            else
                v.setBackgroundResource(android.R.color.transparent);

            return v;
        }
    }

    /*------------- InvitationItemClickListener ----*/
    private class InvitationItemClickListener implements AdapterView.OnItemClickListener {
        @Override
        public void onItemClick(AdapterView<?> adapterView, View view, int position, long id) {
            invalidateSelectedInvitation();

            //highlight the selected item
            view.setBackgroundResource(android.R.color.darker_gray);
            selectedInvitationViewRef = new WeakReference<View>(view);

            selectedInvitation = getInvitation(position);

            joinBtn.setEnabled(true);
            deleteBtn.setEnabled(true);
        }
    }

}
