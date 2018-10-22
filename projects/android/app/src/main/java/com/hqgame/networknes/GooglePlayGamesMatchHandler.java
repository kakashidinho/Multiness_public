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

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.google.android.gms.games.TurnBasedMultiplayerClient;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatch;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatchUpdateCallback;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;

import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Random;


public abstract class GooglePlayGamesMatchHandler {
    private String mMatchId = null;
    private TurnBasedMultiplayerClient mMultiplayerClient = null;

    private GooglePlayGamesMatchData mOldMatchData = null;

    private AsyncOperation mWaitOp = null;

    private long mFirstTurnWaitTimeMs = 0;
    private int mTurnWaitMaxDeviationMs = 0;

    private boolean mShouldDismissAfterDone = false;

    private TurnBasedMatchUpdateCallback mPostMatchmakingMonitor = null;

    private Callback mCallbackAdapter = null;
    private Callback mCallback = null;

    private Random mRandom = new Random();

    // constructor
    protected GooglePlayGamesMatchHandler() {

    }

    protected GooglePlayGamesMatchHandler(long firstTurnWaitTime) {
        mFirstTurnWaitTimeMs = firstTurnWaitTime;
    }

    public GooglePlayGamesMatchHandler setFirstTurnWaitTimeout(long timeoutMs) {
        mFirstTurnWaitTimeMs = timeoutMs;
        return this;
    }

    public GooglePlayGamesMatchHandler setTurnWaitTimeoutTimeMaxDeviation(int diffMs) {
        mTurnWaitMaxDeviationMs = diffMs;
        return this;
    }

    protected void setShouldDismissAfterDone(boolean e) {
        mShouldDismissAfterDone = e;
    }

    protected boolean doStart(BaseActivity activity, @NonNull TurnBasedMatch match, final Callback callback) {
        stop();

        if (activity == null || activity.getGoogleMultiplayerClient() == null)
            return false;

        wrap(activity, match.getMatchId());

        mCallback = callback;

        mCallbackAdapter = new Callback() {
            @Override
            public void onMatchInvalid(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId) {
                System.out.println("GooglePlayGamesMatchHandler.onMatchInvalid(matchId=" + matchId + ")");
                if (mCallback != null)
                    mCallback.onMatchInvalid(handler, activity, matchId);

                stop();
            }

            @Override
            public void onFatalError(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId, Exception exception) {
                System.out.println("GooglePlayGamesMatchHandler.onFatalError(matchId=" + matchId + ", exception=" + exception.getLocalizedMessage() + ")");
                if (mCallback != null)
                    mCallback.onFatalError(handler, activity, matchId, exception);

                stop();
            }

            @Override
            public void onTimeout(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId) {
                System.out.println("GooglePlayGamesMatchHandler.onTimeout(matchId=" + matchId + ")");
                if (mCallback != null)
                    mCallback.onTimeout(handler, activity, matchId);

                stop();
            }

            @Override
            public void onMatchRemoved(GooglePlayGamesMatchHandler handler, BaseActivity activity, @NonNull String matchId) {
                System.out.println("GooglePlayGamesMatchHandler.onMatchRemoved(matchId=" + matchId + ")");
                if (mCallback != null)
                    mCallback.onMatchRemoved(handler, activity, matchId);
                if (matchId.equals(mMatchId))
                    mMatchId = null;
                stop();
            }
        };

        if (match.getTurnStatus() != TurnBasedMatch.MATCH_TURN_STATUS_MY_TURN)
            waitForMyTurn(activity, mFirstTurnWaitTimeMs);
        else
            myTurn(activity, match);

        return true;
    }

    // after calling this method, can call stop() to cancel the wrapped match
    public GooglePlayGamesMatchHandler wrap(BaseActivity activity, String matchId) {
        if (activity == null || activity.getGoogleMultiplayerClient() == null)
            return this;

        mMultiplayerClient = activity.getGoogleMultiplayerClient();
        mMatchId = matchId;

        System.out.println("GooglePlayGamesMatchHandler.wrap(matchId=" + matchId + ")");

        return this;
    }

    public GooglePlayGamesMatchHandler stop() {
        System.out.println("GooglePlayGamesMatchHandler.stop()");

        if (mMatchId != null && mMultiplayerClient != null)
        {
            final TurnBasedMultiplayerClient client = mMultiplayerClient;
            final String matchId = mMatchId;
            mMultiplayerClient.cancelMatch(mMatchId).addOnCompleteListener(new OnCompleteListener<String>() {
                @Override
                public void onComplete(@NonNull Task<String> task) {
                    if (task.isSuccessful())
                        client.dismissMatch(task.getResult());
                    else
                        client.dismissMatch(matchId);
                }
            });
        }

        return detach();
    }

    // detach from the match without canceling it
    public GooglePlayGamesMatchHandler detach() {
        System.out.println("GooglePlayGamesMatchHandler.detach()");

        if (mWaitOp != null) {
            mWaitOp.cancel();
            mWaitOp = null;
        }

        invalidatePostMatchMakingMonitor(BaseActivity.getCurrentActivity());

        mCallbackAdapter = null;
        mMultiplayerClient = null;
        mMatchId = null;
        mOldMatchData = null;

        return this;
    }

    private void invalidatePostMatchMakingMonitor(BaseActivity activity) {
        if (activity == null)
            return;

        if (mPostMatchmakingMonitor != null) {
            activity.unregisterGoogleTurnBasedMatchUpdateCallback(mPostMatchmakingMonitor);
            mPostMatchmakingMonitor = null;
        }
    }

    // we continue monitor the match state after the final turn completes
    private void startPostMatchMakingMonitor(final BaseActivity activity) {
        final GooglePlayGamesMatchHandler thiz = this;

        invalidatePostMatchMakingMonitor(activity);

        mPostMatchmakingMonitor = new TurnBasedMatchUpdateCallback() {
            @Override
            public void onTurnBasedMatchReceived(@NonNull TurnBasedMatch match) {
                if (!match.getMatchId().equals(mMatchId))
                    return;
                switch (match.getStatus())
                {
                    case TurnBasedMatch.MATCH_STATUS_CANCELED:
                    case TurnBasedMatch.MATCH_STATUS_COMPLETE:
                    case TurnBasedMatch.MATCH_STATUS_EXPIRED:
                        if (mCallbackAdapter != null)
                            mCallbackAdapter.onMatchRemoved(thiz, activity, match.getMatchId());
                        break;
                }
            }

            @Override
            public void onTurnBasedMatchRemoved(@NonNull String s) {
                if (!s.equals(mMatchId))
                    return;

                if (mCallbackAdapter != null)
                    mCallbackAdapter.onMatchRemoved(thiz, activity, s);
            }
        };

        activity.registerGoogleTurnBasedMatchUpdateCallback(mPostMatchmakingMonitor);
    }

    private void waitForMyTurn(BaseActivity activity, long timeout) {
        if (mWaitOp != null) {
            mWaitOp.cancel();
            mWaitOp = null;
        }

        final GooglePlayGamesMatchHandler thiz = GooglePlayGamesMatchHandler.this;

        mWaitOp = activity.waitForGooglePlayGamesTurn(mMatchId, new BaseActivity.GoogleTurnedBaseMatchWaitForMyTurnCallback() {
            @Override
            public void onMyTurn(final BaseActivity activity, @NonNull TurnBasedMatch match) {
                myTurn(activity, match);
            }

            @Override
            public void onMatchRemoved(BaseActivity activity, @NonNull String matchId) {
                mCallbackAdapter.onMatchRemoved(thiz, activity, matchId);
            }

            @Override
            public void onTimeout(BaseActivity activity, @NonNull String matchId) {
                mCallbackAdapter.onTimeout(thiz, activity, matchId);
            }
        }, timeout);
    }

    private void myTurn(final BaseActivity activity, @NonNull TurnBasedMatch match) {
        final GooglePlayGamesMatchHandler thiz = this;
        final GooglePlayGamesMatchData data = new GooglePlayGamesMatchData(match);

        System.out.println("GooglePlayGamesMatchHandler.myTurn(matchData begin state=" + data.getState() + ")");

        if (data.getState() == Settings.GOOGLE_MATCH_STATE_INVALID
                || !isMatchStateValid(data, mOldMatchData))
        {
            mCallbackAdapter.onMatchInvalid(this, activity, mMatchId);
            return;
        }

        // call implementation
        long nextTimeout = myTurnImpl(activity, match, data, mOldMatchData);

        // adjusted the next turn's timeout
        long timeoutShift = 0;
        if (nextTimeout > 0 && mTurnWaitMaxDeviationMs > 0) {
            timeoutShift = mRandom.nextInt(mTurnWaitMaxDeviationMs * 2) - mTurnWaitMaxDeviationMs;

            if (timeoutShift <= -nextTimeout)
                timeoutShift = 0;
            nextTimeout = nextTimeout + timeoutShift;
        }

        System.out.println("---> GooglePlayGamesMatchHandler.myTurn(matchData end state=" + data.getState() + ", next timeout=" + nextTimeout + " shifted=" + timeoutShift + " )");

        if (mMultiplayerClient == null) // subclass called cancel()?
            return;

        mOldMatchData = data;
        // take turn
        final WeakReference<Callback> callbackAdapterRef = new WeakReference<>(mCallbackAdapter);
        final WeakReference<TurnBasedMultiplayerClient> clientRef = new WeakReference<>(mMultiplayerClient);
        // for some reasons dismiss the match will prevent other players from joining it.
        // So we should only dismiss the match if we are client
        final boolean shouldDismiss = mShouldDismissAfterDone && nextTimeout < 0;
        final String matchId = mMatchId;
        mMultiplayerClient
                .takeTurn(match.getMatchId(), data.toBytes(), getNextParticipantId(activity, match))
                .addOnCompleteListener(new OnCompleteListener<TurnBasedMatch>() {
                    @Override
                    public void onComplete(@NonNull Task<TurnBasedMatch> task) {
                        if (task.isSuccessful()) {
                            TurnBasedMultiplayerClient client = clientRef.get();
                            if (client != null && shouldDismiss) {
                                System.out.println("GooglePlayGamesMatchHandler: dismissing the match");

                                client.dismissMatch(matchId); // we no longer need this match, dismiss it
                            }
                        } else {
                            Callback callbackAdapter = callbackAdapterRef.get();
                            if (callbackAdapter != null)
                                callbackAdapter.onFatalError(thiz, activity, matchId, task.getException());
                        }
                    }
                });

        // wait for next turn or finish?
        if (nextTimeout >= 0)
            waitForMyTurn(activity, nextTimeout);
        else if (!shouldDismiss)
            startPostMatchMakingMonitor(activity);
    }

    protected String getNextParticipantId(BaseActivity activity, @NonNull TurnBasedMatch match) {
        String myParticipantId = match.getParticipantId(activity.getGooglePlayerId());
        ArrayList<String> participantIds = match.getParticipantIds();
        if (participantIds.size() == 1) // only we are in the room, we must chose auto join mode
        {
            System.out.println("-----> GooglePlayGamesMatchHandler.getNextParticipantId() returns null");
            return null;
        } else {
            for (int i = 0; i < participantIds.size(); ++i) {
                if (!participantIds.get(i).equals(myParticipantId)) {
                    System.out.println("-----> GooglePlayGamesMatchHandler.getNextParticipantId() returns " + participantIds.get(i));
                    return participantIds.get(i);
                }
            }
        }

        return null;
    }

    protected boolean isMatchStateValid(@NonNull GooglePlayGamesMatchData data, GooglePlayGamesMatchData oldData) {
        if (mOldMatchData != null && data.getState() != mOldMatchData.getState() + 1)
            return false;
        return true;
    }

    protected @NonNull Callback getCallbackAdapter() {
        return mCallbackAdapter;
    }

    protected TurnBasedMultiplayerClient getMultiplayerClient() {
        return mMultiplayerClient;
    }

    protected long getFirstTurnWaitTimeout() {
        return mFirstTurnWaitTimeMs;
    }

    protected String getMatchId() {
        return mMatchId;
    }

    // return next turn waiting timeout, negative if don't want to wait. zero will wait forever
    protected abstract long myTurnImpl(BaseActivity activity, @NonNull TurnBasedMatch match, @NonNull GooglePlayGamesMatchData data, GooglePlayGamesMatchData oldData);

    interface Callback {
        void onMatchInvalid(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId);
        void onFatalError(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId, Exception exception);
        void onTimeout(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId);
        void onMatchRemoved(GooglePlayGamesMatchHandler handler, BaseActivity activity, @NonNull String matchId);
    }

    /*------------------------ StopOnlyHandler --------------------------*/
    public static class StopOnlyHandler extends GooglePlayGamesMatchHandler {
        public StopOnlyHandler(BaseActivity activity, String matchId) {
            super(-1);
            wrap(activity, matchId);
        }

        @Override
        protected long myTurnImpl(BaseActivity activity, @NonNull TurnBasedMatch match, @NonNull GooglePlayGamesMatchData data, GooglePlayGamesMatchData oldData) {
            return -1;
        }
    }

    /*------------------------- client --------------------------*/
    public static class Client extends GooglePlayGamesMatchHandler {
        private String mInviteDataReceived = null;
        private long mDefaultTurnWaitTime = Settings.GOOGLE_MATCH_WAIT_FOR_TURN_TIMEOUT_MS;

        protected ClientCallback mClientCallback;

        public Client(long firstTurnWaitTimeout) {
            super(firstTurnWaitTimeout);

            setShouldDismissAfterDone(true);
        }

        public Client() {
            this(Settings.GOOGLE_MATCH_WAIT_FOR_TURN_TIMEOUT_MS);
        }

        public boolean start(BaseActivity activity, @NonNull TurnBasedMatch match, ClientCallback callback) {
            mClientCallback = callback;
            mInviteDataReceived = null;
            return super.doStart(activity, match, callback);
        }

        // exclude the first and last turn wait
        public Client setDefaultTurnWaitTimeout(long timeoutMs) {
            mDefaultTurnWaitTime = timeoutMs;
            return this;
        }

        @Override
        protected long myTurnImpl(final BaseActivity activity, @NonNull TurnBasedMatch match, @NonNull GooglePlayGamesMatchData data, GooglePlayGamesMatchData oldData) {
            final Client thiz = this;
            switch (data.getState()) {
                case Settings.GOOGLE_MATCH_STATE_UNINITIALIZED:
                    // ignore
                    break;
                case Settings.GOOGLE_MATCH_STATE_HAS_INVITE_DATA: {
                    String inviteData = data.getInviteData();

                    if (inviteData == null)
                    {
                        getCallbackAdapter().onMatchInvalid(this, activity, match.getMatchId());
                    }
                    else {
                        mInviteDataReceived = inviteData;

                        // by default notify the callback
                        onHasInviteData(activity, match, mInviteDataReceived);
                    }

                    return -1; // done, don't wait for next turn
                }
                default:
                    getCallbackAdapter().onMatchInvalid(this, activity, match.getMatchId());
            }

            return mDefaultTurnWaitTime;
        }

        protected void onHasInviteData(BaseActivity activity, @NonNull TurnBasedMatch match, @NonNull String inviteData) {
            if (mClientCallback != null)
                mClientCallback.onComplete(this, activity, match.getMatchId(), mInviteDataReceived);
        }

        interface ClientCallback extends GooglePlayGamesMatchHandler.Callback {
            void onComplete(Client handler, BaseActivity activity, @NonNull String matchId, @NonNull String inviteData);
        }
    }

    public static class AutoMatchClient extends Client {
        private AsyncOperation mP2pConnectivityTestOp = null;
        private AsyncOperation mP2pConnectivityTestTimeoutOp = null;
        private long mP2pTestTimeout = Settings.GOOGLE_AUTO_MATCH_WAIT_TO_BE_CONNECTED_TIMEOUT_MS;

        public AutoMatchClient(long firstTurnWaitTimeoutMs) {
            super(firstTurnWaitTimeoutMs);

            setTurnWaitTimeoutTimeMaxDeviation(Settings.GOOGLE_AUTO_MATCH_WAIT_FOR_TURN_TIMEOUT_RAND_MS);
            setDefaultTurnWaitTimeout(Settings.GOOGLE_AUTO_MATCH_WAIT_FOR_TURN_TIMEOUT_MS);
        }

        public AutoMatchClient setP2pTestTimeout(long timeoutMs) {
            mP2pTestTimeout = timeoutMs;
            return this;
        }

        @Override
        public GooglePlayGamesMatchHandler detach() {
            cancelP2pTestTimeout();
            cancelP2pTest();

            return super.detach();
        }

        @Override
        protected void onHasInviteData(final BaseActivity activity, @NonNull TurnBasedMatch match, @NonNull final String inviteData) {
            final Client thiz = this;
            final String matchId = match.getMatchId();
            // need to check whether the match is valid or not
            mP2pConnectivityTestOp = GameSurfaceView.testRemoteInternetConnectivity(inviteData, Long.toString(System.currentTimeMillis()), new AsyncQuery<Boolean>() {
                @Override
                public void run(Boolean result) {
                    // the timeout timer should be stopped, we have finished the test
                    cancelP2pTestTimeout();

                    if (result)
                    {
                        if (mClientCallback != null)
                            mClientCallback.onComplete(thiz, activity, matchId, inviteData);
                    } else {
                        getCallbackAdapter().onMatchInvalid(thiz, activity, matchId);
                    }
                }
            });

            // only test within a finite duration
            mP2pConnectivityTestTimeoutOp = activity.runOnUiThreadDelayed(new Runnable() {
                @Override
                public void run() {
                    cancelP2pTest();
                    getCallbackAdapter().onMatchInvalid(thiz, activity, matchId);
                }
            }, mP2pTestTimeout);
        }

        private void cancelP2pTest() {
            if (mP2pConnectivityTestOp != null) {
                mP2pConnectivityTestOp.cancel();
                mP2pConnectivityTestOp = null;
            }
        }

        private void cancelP2pTestTimeout() {
            if (mP2pConnectivityTestTimeoutOp != null) {
                mP2pConnectivityTestTimeoutOp.cancel();
                mP2pConnectivityTestTimeoutOp = null;
            }
        }
    }

    /*-------------------- server ----------------------*/
    public static class Host extends GooglePlayGamesMatchHandler{
        private long mDefaultTurnWaitTime = Settings.GOOGLE_MATCH_WAIT_FOR_TURN_TIMEOUT_MS;
        private String mInviteData;
        private ServerCallback mServerCallback = null;

        public Host() {
            super(Settings.GOOGLE_MATCH_WAIT_FOR_TURN_TIMEOUT_MS);
        }

        public boolean start(BaseActivity activity, @NonNull TurnBasedMatch match, @NonNull String inviteData, ServerCallback callback) {
            mInviteData = inviteData;
            mServerCallback = callback;
            return super.doStart(activity, match, callback);
        }

        // exclude the first turn wait
        public Host setDefaultTurnWaitTimeout(long timeoutMs) {
            mDefaultTurnWaitTime = timeoutMs;
            return this;
        }

        @Override
        protected long myTurnImpl(BaseActivity activity, @NonNull TurnBasedMatch match, @NonNull GooglePlayGamesMatchData data, GooglePlayGamesMatchData oldData) {
            switch (data.getState()) {
                case Settings.GOOGLE_MATCH_STATE_UNINITIALIZED:
                    if (match.getData() != null) {
                        // for some reasons, host cannot pass turn to client if client is the creator of the room
                        // so marking this room as invalid and create another one
                        getCallbackAdapter().onMatchInvalid(this, activity, match.getMatchId());

                        return -1;
                    }

                    data.setState(Settings.GOOGLE_MATCH_STATE_HAS_INVITE_DATA, mInviteData);

                    // our job finished, now invoke registerd callback
                    if (mServerCallback != null)
                        mServerCallback.onComplete(this, activity, match.getMatchId());

                    return -1; // don't wait
                default:
                    getCallbackAdapter().onMatchInvalid(this, activity, match.getMatchId());
            }
            return mDefaultTurnWaitTime;
        }

        interface ServerCallback extends GooglePlayGamesMatchHandler.Callback {
            void onComplete(Host handler, BaseActivity activity, @NonNull String matchId);
        }
    }

    /* ---------------- match data ----------------*/
    private static class GooglePlayGamesMatchData {
        public GooglePlayGamesMatchData(TurnBasedMatch match) {
            byte[] data = match.getData();

            this.mState = getGooglePlayGamesMatchState(data);

            if (this.mState == Settings.GOOGLE_MATCH_STATE_HAS_INVITE_DATA) {
                parseInviteData(data, Settings.GOOGLE_MATCH_DATA_MAGIC.length + 4);
            }
        }

        public byte[] toBytes() {
            // format: magic bytes | state | invite data's magic string | invite data
            int length = Settings.GOOGLE_MATCH_DATA_MAGIC.length + 4;
            byte[] inviteDataWrappedBytes = null;

            if (mState == Settings.GOOGLE_MATCH_STATE_HAS_INVITE_DATA) {
                if (mInviteData == null) {
                    mState = Settings.GOOGLE_MATCH_STATE_INVALID;
                }
                else {
                    try {
                        inviteDataWrappedBytes = (Settings.GOOGLE_MATCH_INVITE_DATA_MAGIC_STR + this.mInviteData).getBytes("UTF-8");
                        length += inviteDataWrappedBytes.length;
                    } catch (Exception e) {
                        mState = Settings.GOOGLE_MATCH_STATE_INVALID;
                    }
                }
            }

            byte[] data = new byte[length];
            ByteBuffer buffer = ByteBuffer.wrap(data);
            buffer.order(ByteOrder.BIG_ENDIAN);

            buffer.put(Settings.GOOGLE_MATCH_DATA_MAGIC);
            buffer.putInt(mState);

            if (inviteDataWrappedBytes != null)
                buffer.put(inviteDataWrappedBytes);

            return data;
        }

        private void parseInviteData(byte[] data, int offset) {
            try {
                String inviteDataWrapped = new String(data, offset, data.length - offset, "UTF-8");

                if (!inviteDataWrapped.startsWith(Settings.GOOGLE_MATCH_INVITE_DATA_MAGIC_STR)) {
                    mState = Settings.GOOGLE_MATCH_STATE_INVALID;
                    return;
                }

                mInviteData = inviteDataWrapped.substring(Settings.GOOGLE_MATCH_INVITE_DATA_MAGIC_STR.length());
            } catch (Exception e) {
                e.printStackTrace();
                this.mState = Settings.GOOGLE_MATCH_STATE_INVALID;
            }
        }

        private int getGooglePlayGamesMatchState(byte[] data) {
            if (data == null)
                return Settings.GOOGLE_MATCH_STATE_UNINITIALIZED;

            if (data.length < Settings.GOOGLE_MATCH_DATA_MAGIC.length)
                return Settings.GOOGLE_MATCH_STATE_INVALID;

            for (int i = 0; i < Settings.GOOGLE_MATCH_DATA_MAGIC.length; ++i) {
                if (data[i] != Settings.GOOGLE_MATCH_DATA_MAGIC[i])
                    return Settings.GOOGLE_MATCH_STATE_INVALID;
            }

            ByteBuffer wrappedBuffer = ByteBuffer.wrap(data, Settings.GOOGLE_MATCH_DATA_MAGIC.length, 4);
            wrappedBuffer.order(ByteOrder.BIG_ENDIAN);
            return wrappedBuffer.getInt();
        }

        public void setState(int state, @Nullable String inviteData) {
            this.mState = state;
            this.mInviteData = inviteData;
        }

        public int getState() {
            return mState;
        }

        public String getInviteData() {
            return mInviteData;
        }

        private int mState;
        private String mInviteData;
    }
}
