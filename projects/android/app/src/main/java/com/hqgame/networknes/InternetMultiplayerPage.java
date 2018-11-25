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
import android.content.res.Configuration;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatch;

public class InternetMultiplayerPage extends BaseMenuPage {
    ProgressDialog mProgressDialog = null;
    GooglePlayGamesMatchHandler.AutoMatchClient mGoogleAutoMatchClientHandler
            = new GooglePlayGamesMatchHandler.AutoMatchClient(Settings.GOOGLE_AUTO_MATCH_WAIT_FOR_TURN_TIMEOUT_MS);
    FrameLayout mContentView = null;

    private AsyncOperation mGoogleJoiningAutoMatchOp = null;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    protected View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.title_activity_multiplayer));
        //
        mContentView = new FrameLayout(getContext());
        mContentView.addView(createConfigDependentView(inflater, container));

        return mContentView;
    }

    @Override
    public void onConfigurationChanged (Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // re-create layout
        LayoutInflater inflater = (LayoutInflater)getActivity().getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        mContentView.removeAllViews();

        mContentView.addView(createConfigDependentView(inflater, null));
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
    }

    @Override
    public void onPause()
    {
        super.onPause();

        cancelAutoJoinGooglePlayGamesTask();
    }

    @Override
    public void onResume()
    {
        super.onResume();

        registerGoogleLoginButtn(getView());
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        cancelAutoJoinGooglePlayGamesTask();
    }

    // if user enter the page by accepting the invitation on notification bar, then
    // cancel existing auto joining task
    @Override
    protected void onJoiningGoogleMatchBegin(String matchId) {
        cancelAutoJoinGooglePlayGamesTask();
        super.onJoiningGoogleMatchBegin(matchId);
    }

    private View createConfigDependentView(LayoutInflater inflater, ViewGroup container) {
        View view = inflater.inflate(R.layout.page_internet_multiplayer, container, false);

        //initialize buttons' listener
        View.OnClickListener buttonClickListener = new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onButtonClicked(view.getId());
            }
        };

        Button hostGpgBtn = (Button)view.findViewById(R.id.btnHostGpg);
        Button joinGpgBtn = (Button)view.findViewById(R.id.btnJoinGpg);
        Button quickJoinGpgBtn = (Button)view.findViewById(R.id.btnQuickJoinGpg);

        // disable random join button now as we have lobby system
        quickJoinGpgBtn.setVisibility(View.GONE);

        Button hostFbBtn = (Button)view.findViewById(R.id.btnHostFb);
        Button joinFbBtn = (Button)view.findViewById(R.id.btnJoinFb);
        hostGpgBtn.setOnClickListener(buttonClickListener);
        joinGpgBtn.setOnClickListener(buttonClickListener);
        quickJoinGpgBtn.setOnClickListener(buttonClickListener);
        if (hostFbBtn != null)
            hostFbBtn.setOnClickListener(buttonClickListener);
        if (joinFbBtn != null)
            joinFbBtn.setOnClickListener(buttonClickListener);

        // google login button
        registerGoogleLoginButtn(view);

        //facebook login button
        com.facebook.login.widget.LoginButton fbLoginButton =
                (com.facebook.login.widget.LoginButton) view.findViewById(R.id.fb_login_button);
        if (fbLoginButton != null)
            fbLoginButton.setReadPermissions(Settings.FACEBOOK_PERMISSIONS);

        return view;
    }

    private void onButtonClicked(int id)
    {
        Settings.RemoteControl mode;
        switch (id)
        {
            case R.id.btnHostGpg:
            {
                mode = Settings.RemoteControl.ENABLE_INTERNET_REMOTE_CONTROL_GOOGLE;
            }
            break;
            case R.id.btnJoinGpg:
            {
                mode = Settings.RemoteControl.JOIN_INTERNET_REMOTE_CONTROL_GOOGLE;
            }
            break;
            case R.id.btnQuickJoinGpg:
            {
                mode = Settings.RemoteControl.QUICKJOIN_INTERNET_REMOTE_CONTROL_GOOGLE;
            }
            break;
            case R.id.btnHostFb:
            {
                mode = Settings.RemoteControl.ENABLE_INTERNET_REMOTE_CONTROL_FB;
            }
            break;
            case R.id.btnJoinFb:
            {
                mode = Settings.RemoteControl.JOIN_INTERNET_REMOTE_CONTROL_FB;
            }
            break;
            default:
                return;
        }

        internetModeSelected(mode);
    }

    private void registerGoogleLoginButtn(View container) {
        if (container != null) {
            // register google login button's click listener
            Button googleLoginBtn
                    = (Button) container.findViewById(R.id.google_login_button);

            if (googleLoginBtn != null)
                getBaseActivity().registerGoogleSigninBtn(googleLoginBtn);
        }
    }

    private void internetModeSelected(final Settings.RemoteControl mode){
        if (mode == Settings.RemoteControl.ENABLE_INTERNET_REMOTE_CONTROL_FB
                || mode == Settings.RemoteControl.JOIN_INTERNET_REMOTE_CONTROL_FB) {
            //ask user to sign in before using internet mode
            if (!isFbSignedIn())
            {
                displaySignInRequiredDialog(R.string.must_signin_fb_title_msg);

                return;
            }
        } else if (mode == Settings.RemoteControl.ENABLE_INTERNET_REMOTE_CONTROL_GOOGLE
                || mode == Settings.RemoteControl.JOIN_INTERNET_REMOTE_CONTROL_GOOGLE
                || mode == Settings.RemoteControl.QUICKJOIN_INTERNET_REMOTE_CONTROL_GOOGLE) {
            //ask user to sign in before using internet mode
            if (!isGoogleSignedIn())
            {
                displaySignInRequiredDialog(R.string.must_signin_google_title_msg);

                return;
            }
        }

        //construct game's settings
        Bundle settings = new Bundle();
        settings.putSerializable(Settings.GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY, mode);

        BasePage page = null;
        switch (mode) {
            case ENABLE_INTERNET_REMOTE_CONTROL_FB:
            case ENABLE_INTERNET_REMOTE_CONTROL_GOOGLE:
                //start game chooser activity
                page = BasePage.create(GameChooserPage.class);
                break;
            case JOIN_INTERNET_REMOTE_CONTROL_FB:
                page = BasePage.create(FBInvitationsPage.class);
                break;
            case JOIN_INTERNET_REMOTE_CONTROL_GOOGLE:
            {
                page = BasePage.create(GoogleInvitationsPage.class);
            }
                break;
            case QUICKJOIN_INTERNET_REMOTE_CONTROL_GOOGLE:
            {
                cancelAutoJoinGooglePlayGamesTask();

                showProgressDialog(getString(R.string.google_auto_search_games), new Runnable() {
                    @Override
                    public void run() {
                        cancelAutoJoinGooglePlayGamesTask();
                    }
                });

                autoJoinGooglePlayGames(getBaseActivity(), mode);
            }
            return;
            default:
                //start game client directly
                page = BasePage.create(GamePage.class);
                break;
        }


        if (page != null) {
            page.setExtras(settings);

            goToPage(page);
        }
    }

    private void autoJoinGooglePlayGames(BaseActivity activity, final Settings.RemoteControl mode) {
        if (mGoogleJoiningAutoMatchOp != null) {
            mGoogleJoiningAutoMatchOp.cancel();
        }
        mGoogleJoiningAutoMatchOp = activity.autoJoinGooglePlayGames(1, 1, Settings.GOOGLE_MATCH_P2_MASK, new GooglePlayGamesMatchAutoJoinHandler(mode));
    }

    private void onGooglePlayGamesMatchJoined(BaseActivity activity, TurnBasedMatch result,final  Settings.RemoteControl mode) {
        final String joinedMatchId = result.getMatchId();

        mGoogleAutoMatchClientHandler.start(activity, result, new GooglePlayGamesMatchHandler.Client.ClientCallback() {
            @Override
            public void onComplete(GooglePlayGamesMatchHandler.Client handler, BaseActivity activity, @NonNull String matchId, @NonNull String inviteData) {
                // finally done
                dismissProgressDialog();

                cancelAutoJoinGooglePlayGamesTask(false);

                Bundle existSettings = getExtras();
                final Bundle settings = existSettings != null ? existSettings : new Bundle();

                // --------- Open Game Page --------------
                // treat as normal join when launching Game Page
                settings.putSerializable(Settings.GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY, Settings.RemoteControl.JOIN_INTERNET_REMOTE_CONTROL_GOOGLE);

                settings.putString(Settings.GAME_ACTIVITY_REMOTE_GOOGLE_MATCH_ID, matchId);
                settings.putString(Settings.GAME_ACTIVITY_REMOTE_INVITATION_DATA_KEY, inviteData);

                BasePage intent = BasePage.create(GamePage.class);
                intent.setExtras(settings);

                goToPage(intent);
            }

            @Override
            public void onMatchInvalid(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId) {
                // join a new match
                autoJoinGooglePlayGames(activity, mode);
            }

            @Override
            public void onFatalError(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId, Exception exception) {
                if (!activity.isGoogleSignedIn()) // maybe disconnected?
                    displayErrorDialog(exception);
                else {
                    // ignore and join a new match
                    exception.printStackTrace();
                    autoJoinGooglePlayGames(activity, mode);
                }
            }

            @Override
            public void onTimeout(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId) {
                // join a new match
                autoJoinGooglePlayGames(activity, mode);
            }

            @Override
            public void onMatchRemoved(GooglePlayGamesMatchHandler handler, BaseActivity activity, @NonNull String matchId) {
                if (matchId.equals(joinedMatchId)) {
                    // join new match
                    autoJoinGooglePlayGames(activity, mode);
                }
            }
        });
    }

    private void cancelAutoJoinGooglePlayGamesTask() {
        cancelAutoJoinGooglePlayGamesTask(true);
    }

    private void cancelAutoJoinGooglePlayGamesTask(boolean stopHandler) {
        dismissProgressDialog();

        if (mGoogleJoiningAutoMatchOp != null) {
            mGoogleJoiningAutoMatchOp.cancel();
            mGoogleJoiningAutoMatchOp = null;
        }

        if (stopHandler)
            mGoogleAutoMatchClientHandler.stop();
        else
            mGoogleAutoMatchClientHandler.detach();
    }

    private void displaySignInRequiredDialog(int dialogMessageResId) {
        dismissProgressDialog();

        Utils.alertDialog(
                getContext(),
                getString(R.string.must_signin_internet_title),
                getString(dialogMessageResId),
                null);
    }

    private void displayErrorDialog(Exception e) {
        displayErrorDialog(e.getLocalizedMessage());
    }

    private void displayErrorDialog(String msg) {
        dismissProgressDialog();

        Utils.alertDialog(
                getActivity(),
                getString(R.string.generic_err_title),
                msg,
                null);
    }

    private void showProgressDialog(String msg, Runnable cancelListener) {
        dismissProgressDialog();

        mProgressDialog = Utils.showProgressDialog(getContext(), msg, cancelListener);
    }

    private void dismissProgressDialog() {
        if (mProgressDialog != null && mProgressDialog.isShowing()) {
            try {
                mProgressDialog.dismiss();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private class GenericGooglePlayGamesMatchCreateHandler implements BaseActivity.GooglePlayGamesMatchCreateMininalCallback {
        public GenericGooglePlayGamesMatchCreateHandler(Settings.RemoteControl mode) {
            mRemoteControlMode = mode;
        }

        @Override
        public void onActivityResultError(BaseActivity activity, int resultCode) {
            Utils.alertDialog(
                    activity,
                    getString(R.string.generic_err_title),
                    String.format(getString(R.string.google_invite_ui_generic_err_msg), resultCode),
                    null);
        }
        @Override
        public void onCancel(BaseActivity activity) {
            dismissProgressDialog();
        }
        @Override
        public void onError(BaseActivity activity, Exception exception) {
            // There was an error. Show the error.
            if (exception instanceof ApiException) {
                ApiException apiException = (ApiException) exception;
            }
            displayErrorDialog(exception);;
        }

        protected final Settings.RemoteControl mRemoteControlMode;
    }

    private class GooglePlayGamesMatchAutoJoinHandler extends GenericGooglePlayGamesMatchCreateHandler implements BaseActivity.GooglePlayGamesMatchCreateCallback<TurnBasedMatch>
    {
        public GooglePlayGamesMatchAutoJoinHandler(Settings.RemoteControl mode) {
            super(mode);
        }

        @Override
        public void onComplete(BaseActivity activity, TurnBasedMatch result) {
            onGooglePlayGamesMatchJoined(activity, result, mRemoteControlMode);
        }
    }
}
