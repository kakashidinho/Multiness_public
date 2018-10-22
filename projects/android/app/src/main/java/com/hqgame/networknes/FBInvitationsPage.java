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

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.json.JSONObject;

import java.text.DateFormat;
import java.util.Locale;
import java.util.TimeZone;
import java.util.TreeSet;

public class FBInvitationsPage extends BaseInvitationsPage<FBUtils.InvitationInfo> {
    private static final int EXPECTED_MAX_INVITATIONS_TO_FETCH = 25;

    private FBUtils.InvitationInfo[] invitations = null;

    @Override
    protected void deleteInvitation(@NonNull FBUtils.InvitationInfo invitationInfo, final Runnable finishCallback) {
        FBUtils.deleteGraphObj(invitationInfo.graphObject, finishCallback);
    }

    @Override
    protected void acceptedInvitation(@NonNull FBUtils.InvitationInfo info) {
        Bundle existSettings = getExtras();
        final Bundle settings = existSettings != null ? existSettings : new Bundle();

        try {
            settings.putString(Settings.GAME_ACTIVITY_REMOTE_FB_INVITATION_ID_KEY, info.graphObject.getString("id"));
            settings.putString(Settings.GAME_ACTIVITY_REMOTE_INVITATION_DATA_KEY, info.graphObject.getString("data"));

            BasePage intent = BasePage.create(GamePage.class);
            intent.setExtras(settings);

            goToPage(intent);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    protected AsyncOperation doFetchInvitations(final AsyncQuery<Boolean> finishCallback) {
        //invalidate current invitations
        invitations = null;

        // fetch FB invitations
        return FBUtils.fetchInvitations(EXPECTED_MAX_INVITATIONS_TO_FETCH, new AsyncQuery<TreeSet<FBUtils.InvitationInfo>>() {
            @Override
            public void run(TreeSet<FBUtils.InvitationInfo> result) {
                invitations = result.toArray(new FBUtils.InvitationInfo[0]);

                if (finishCallback != null)
                    finishCallback.run(true);
            }
        });
    }

    @Override
    protected FBUtils.InvitationInfo getInvitation(int index) {
        return invitations != null ? invitations[index] : null;
    }

    @Override
    protected int getNumInvitations() {
        return invitations != null ? invitations.length : 0;
    }

    @Override
    protected View createInvitationItemView(
            @NonNull FBUtils.InvitationInfo info,
            LayoutInflater inflater,
            View convertView,
            ViewGroup parent,
            QueryCallback<Boolean> selectedItemChangedCallback) {
        View vi = convertView;
        if (vi == null)
            vi = inflater.inflate(R.layout.invitation_item_layout, parent, false);

        ImageView profilePicView = (ImageView) vi.findViewById(R.id.profile_pic_view);
        TextView nameView = (TextView) vi.findViewById(R.id.profile_name_txt_view);
        TextView dateView = (TextView) vi.findViewById(R.id.invitation_date_txt_view);

        try {
            JSONObject fromObj = info.graphObject.getJSONObject("from");

            FBUtils.downloadProfilePic(getResources(), fromObj.getString("id"), profilePicView);
            nameView.setText(fromObj.getString("name"));

            DateFormat df = DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.MEDIUM, Locale.getDefault());
            df.setTimeZone(TimeZone.getDefault());
            dateView.setText(df.format(info.creationTime));

        } catch (Exception e) {
            e.printStackTrace();

            nameView.setText("Error");
            dateView.setText("Error");
        }
        return vi;
    }
}
