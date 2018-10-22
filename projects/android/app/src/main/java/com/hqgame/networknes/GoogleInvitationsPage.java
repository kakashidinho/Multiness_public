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

import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import com.google.android.gms.common.images.ImageManager;
import com.google.android.gms.games.AnnotatedData;
import com.google.android.gms.games.InvitationsClient;
import com.google.android.gms.games.TurnBasedMultiplayerClient;
import com.google.android.gms.games.multiplayer.Invitation;
import com.google.android.gms.games.multiplayer.InvitationBuffer;
import com.google.android.gms.games.multiplayer.InvitationCallback;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatch;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;

import java.text.DateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;
import java.util.Timer;
import java.util.TimerTask;

public class GoogleInvitationsPage extends BaseInvitationsPage<Invitation>
{
    private InvitationBuffer invitations = null;

    private ImageManager imageManager = null;

    private GoogleInvitationsUpdateCallback googleInvitationsUpdateCallback = null;

    private long lastRefreshTime = 0;

    private Timer delayedRefreshTimer = null;

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        imageManager = ImageManager.create(getContext());

        // register Google Play Games Invitations change callback
        googleInvitationsUpdateCallback = new GoogleInvitationsUpdateCallback();
    }

    @Override
    public void onResume() {
        super.onResume();

        InvitationsClient gclient = getBaseActivity().getGoogleInvitationsClient();
        if (gclient != null) {
            gclient.registerInvitationCallback(googleInvitationsUpdateCallback);
        }
    }

    @Override
    public void onPause() {
        super.onPause();

        // cancel delayed refresh task
        cancelDelayedRefreshTask();

        // stop listen to invitation changes
        InvitationsClient gclient = getBaseActivity().getGoogleInvitationsClient();
        if (gclient != null) {
            gclient.unregisterInvitationCallback(googleInvitationsUpdateCallback);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (invitations != null) {
            invitations.release();
            invitations = null;
        }
    }

    @Override
    protected void refresh() {
        this.lastRefreshTime = System.currentTimeMillis();

        if (invitations != null) {
            invitations.release();
            invitations = null;
        }

        super.refresh();
    }

    @Override
    protected boolean invitationsAreEqual(Invitation lhs, Invitation rhs) {
        return lhs != null && rhs != null && lhs.getInvitationId().equals(rhs.getInvitationId());
    }

    @Override
    protected void deleteInvitation(@NonNull Invitation invitation, final Runnable finishCallback) {
        TurnBasedMultiplayerClient client = getBaseActivity().getGoogleMultiplayerClient();
        if (client == null) {
            Utils.alertDialog(getContext(),
                    getString(R.string.generic_err_title),
                    getString(R.string.google_signedout_err_msg),
                    null);
            return;
        }

        client.declineInvitation(invitation.getInvitationId())
            .addOnCompleteListener(new OnCompleteListener<Void>() {
                @Override
                public void onComplete(@NonNull Task<Void> task) {
                    if (finishCallback != null)
                        finishCallback.run();
                }
            });
    }

    @Override
    protected void acceptedInvitation(@NonNull Invitation invitation) {
        TurnBasedMultiplayerClient gclient = getBaseActivity().getGoogleMultiplayerClient();
        if (gclient == null) {
            Utils.alertDialog(getContext(),
                    getString(R.string.generic_err_title),
                    getString(R.string.google_signedout_err_msg),
                    null);
            return;
        }

        final String gpgRoomCreatorId = invitation.getInviter().getPlayer().getPlayerId();
        if (Settings.DEBUG)
            System.out.println("GoogleInvitationsPage.acceptedInvitation(gpgRoomCreatorId=" + gpgRoomCreatorId + ")");

        // display progress
        showProgressDialog(getString(R.string.connecting_to_host), new Runnable() {
            @Override
            public void run() {
                cancelExistGoogleMatchJoiningAttempt();
            }
        });

        // accept invitation
        gclient.acceptInvitation(invitation.getInvitationId()).addOnCompleteListener(new OnCompleteListener<TurnBasedMatch>() {
            @Override
            public void onComplete(@NonNull Task<TurnBasedMatch> task) {
                dismissProgressDialog();

                if (task.isSuccessful()) {
                    final TurnBasedMatch match = task.getResult();

                    onGoogleMatchAccepted(match);
                } else {
                    displayErrorDialog(task.getException().getLocalizedMessage());
                }
            }
        });
    }

    @Override
    protected AsyncOperation doFetchInvitations(final AsyncQuery<Boolean> finishCallback) {
        if (invitations != null) {
            invitations.release();
            invitations = null;
        }

        InvitationsClient client = getBaseActivity().getGoogleInvitationsClient();
        if (client == null) {
            Utils.alertDialog(getContext(),
                    getString(R.string.generic_err_title),
                    getString(R.string.google_signedout_err_msg),
                    null);
            return null;
        }

        Task<AnnotatedData<InvitationBuffer>> task = client.loadInvitations().addOnCompleteListener(new OnCompleteListener<AnnotatedData<InvitationBuffer>>() {
            @Override
            public void onComplete(@NonNull Task<AnnotatedData<InvitationBuffer>> task) {
                if (task.isSuccessful()) {
                    invitations = task.getResult().get();
                }

                if (finishCallback != null)
                    finishCallback.run(true);
            }
        });

        return null;
    }

    @Override
    protected Invitation getInvitation(int index) {
        return invitations != null ? invitations.get(index) : null;
    }

    @Override
    protected int getNumInvitations() {
        return invitations != null ? invitations.getCount() : 0;
    }

    @Override
    protected View createInvitationItemView(
            @NonNull Invitation invitation,
            LayoutInflater inflater,
            View convertView,
            ViewGroup parent,
            QueryCallback<Boolean> selectedItemChangedCallback)
    {
        View vi = convertView;
        if (vi == null)
            vi = inflater.inflate(R.layout.invitation_item_layout, parent, false);

        ImageView profilePicView = (ImageView) vi.findViewById(R.id.profile_pic_view);
        TextView nameView = (TextView) vi.findViewById(R.id.profile_name_txt_view);
        TextView dateView = (TextView) vi.findViewById(R.id.invitation_date_txt_view);

        // profile pic
        try {
            Uri iconUri = invitation.getInviter().getIconImageUri();
            imageManager.loadImage(profilePicView, iconUri);
        } catch (Exception e) {
            profilePicView.setImageDrawable(null);
        }

        // name
        try {
            nameView.setText(invitation.getInviter().getDisplayName());
        } catch (Exception e) {
            e.printStackTrace();

            nameView.setText("Error");
        }

        // creation date
        try {
            DateFormat df = DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.MEDIUM, Locale.getDefault());
            df.setTimeZone(TimeZone.getDefault());
            dateView.setText(df.format(new Date(invitation.getCreationTimestamp())));
        } catch (Exception e) {
            dateView.setText("");
        }

        return vi;
    }

    @Override
    protected void onJoiningGoogleMatchError(String matchId, String errorMessage) {
        super.displayGoogleMatchJoiningErrorDialog(matchId, errorMessage, new Runnable() {
            @Override
            public void run() {
                refresh();
            }
        });
    }

    private void scheduleRefresh(long delayMs) {
        cancelDelayedRefreshTask();

        delayedRefreshTimer = new Timer();
        delayedRefreshTimer.schedule(new TimerTask() {
            @Override
            public void run() {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        refresh();
                    }
                });
            }
        }, delayMs);
    }

    private void cancelDelayedRefreshTask() {
        if (delayedRefreshTimer != null) {
            delayedRefreshTimer.cancel();
            delayedRefreshTimer = null;
        }
    }

    // ------------------ Google callbacks -------------------
    private class GoogleInvitationsUpdateCallback extends InvitationCallback {

        @Override
        public void onInvitationReceived(@NonNull Invitation invitation) {
            long timeSinceLastRefresh = System.currentTimeMillis() - lastRefreshTime;
            if (timeSinceLastRefresh > 10000) // avoid frequent refresh
                refresh();
            else {
                scheduleRefresh(10000 - timeSinceLastRefresh);
            }
        }

        @Override
        public void onInvitationRemoved(@NonNull String s) {
            long timeSinceLastRefresh = System.currentTimeMillis() - lastRefreshTime;
            if (timeSinceLastRefresh > 10000) // avoid frequent refresh
                refresh();
            else {
                scheduleRefresh(10000 - timeSinceLastRefresh);
            }
        }
    }
}
