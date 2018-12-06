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


import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.arch.lifecycle.MutableLiveData;
import android.arch.lifecycle.ViewModel;
import android.arch.lifecycle.ViewModelProviders;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.widget.Toolbar;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.Toast;

import java.io.File;
import java.io.FilenameFilter;
import java.util.Arrays;
import java.util.Comparator;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.google.android.gms.ads.AdListener;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.InterstitialAd;
import com.google.android.gms.games.TurnBasedMultiplayerClient;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatch;


public class GamePage extends BasePage implements GameChatDialog.Delegate {
    private static final int LOAD_SAVE_REQUEST = 0;
    private static final int SAVE_GAME_REQUEST = 1;

    private static final String CHAT_DIALOG_TAG = "CHAT_DIALOG";
    private static long sDurationMsSinceLastAdDisplay = 0;
    private static long sNumGamesSinceLastAdDisplay = 0;

    private FrameLayout mContentView = null;
    private ViewGroup mGameViewFrameContainer = null;
    private GameSurfaceView mGameView;

    private File mPrivateDataPath = null;
    private Settings.RemoteControl mRemoteCtlType = Settings.RemoteControl.NO_REMOTE_CONTROL;

    PersistenState mPersistentState = null;

    private InterstitialAd mInterstitialAd;

    @Override
    protected void onNavigatingTo(Activity activity) {
        System.out.println("GamePage.onNavigatingTo()");

        //lock screen orientation
        int request_orientation;
        switch (Settings.getPreferedOrientation()) {
            case LANDSCAPE:
                request_orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
                break;
            case PORTRAIT:
                request_orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
                break;
            case AUTO:
            default:
                request_orientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
                break;
        }

        activity.setRequestedOrientation(request_orientation);
    }

    @Override
    public void onCreate (Bundle savedInstanceState) {
        System.out.println("GamePage.onCreate()");

        super.onCreate(savedInstanceState);

        setHasOptionsMenu(true);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        System.out.println("GamePage.onCreateView()");

        // cache private data path
        mPrivateDataPath = getContext().getFilesDir();

        // create content view
        mContentView = new FrameLayout(getContext());
        mContentView.addView(recreateGameView(inflater, container));

        mInterstitialAd = new InterstitialAd(getContext());
        mInterstitialAd.setAdUnitId(getString(R.string.admob_fullscreen_ad_id));

        mInterstitialAd.setAdListener(new AdListener() {
            @Override
            public void onAdClosed() {
                requestNewInterstitialAd();

                // HACK: cannot finish() immediately, only can do it
                // in next view cycle after onResume()
                queueOnResumeTask(new Runnable() {
                    @Override
                    public void run() {
                        new Handler().post((new Runnable() {
                            @Override
                            public void run() {
                                finish();
                            }
                        }));
                    }
                });
            }
        });

        requestNewInterstitialAd();

        //notify view that activity has been created
        mGameView.onParentPageViewCreated(this);

        // restore the states if the page has been recreated
        ensurePersistentStateAvailable();
        boolean initialized = mPersistentState.isInitialized();

        if (initialized) {
            // restore remote controlling type's value
            mRemoteCtlType = mPersistentState.getRemoteCtlType();

            // skip the initialization step
            System.out.println("--> GamePage.skipInitialize");
            return mContentView;
        }

        // ------ Initialize game --------------
        Bundle settings = getExtras();
        String game = null;
        mRemoteCtlType = Settings.RemoteControl.NO_REMOTE_CONTROL;
        if (settings != null)
        {
            game = settings.getString(Settings.GAME_ACTIVITY_PATH_KEY);
            mRemoteCtlType = (Settings.RemoteControl)settings.getSerializable(Settings.GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY);
            if (mRemoteCtlType == null)
                mRemoteCtlType = Settings.RemoteControl.NO_REMOTE_CONTROL;
        }
        else {
            //debugging
            File sdPath = getContext().getExternalFilesDir(null);
            game = new File(sdPath,
                    "Battletoads & Double Dragon - The Ultimate Team.zip"
                    //"Battletoads (USA).zip"
                    //"Battle City (Japan).zip"
            ).getAbsolutePath();
        }

        try {
            switch (mRemoteCtlType) {
                case NO_REMOTE_CONTROL:
                    mGameView.disableRemoteController();
                    break;
                case CONNECT_LAN_REMOTE_CONTROL: {
                    game = null;//ignore game, since we will connect and play game hosted by remote side
                    String host = settings.getString(Settings.GAME_ACTIVITY_REMOTE_HOST_KEY);
                    int port = settings.getInt(Settings.GAME_ACTIVITY_REMOTE_PORT_KEY);

                    mGameView.loadRemote(host, port, getBaseActivity().getLanPlayerName());
                }
                break;
                case ENABLE_LAN_REMOTE_CONTROL: {
                    int port = settings.getInt(Settings.GAME_ACTIVITY_REMOTE_PORT_KEY);
                    mGameView.enableRemoteController(port, getBaseActivity().getLanPlayerName());
                }
                break;
                case ENABLE_WIFI_DIRECT_REMOTE_CONTROL: {
                    final int port = settings.getInt(Settings.GAME_ACTIVITY_REMOTE_PORT_KEY, Settings.DEFAULT_NETWORK_PORT);
                    final String roomName = settings.getString(Settings.GAME_ACTIVITY_REMOTE_HOST_KEY, getBaseActivity().getDefaultHostRoomName());

                    mGameView.enableRemoteControllerWifiDirect(port, roomName);
                }
                break;
                case ENABLE_INTERNET_REMOTE_CONTROL_FB: {
                    mGameView.enableRemoteControllerInternetFb(BaseActivity.getFbPlayerId(), BaseActivity.getFbPlayerName());
                }
                break;
                case ENABLE_INTERNET_REMOTE_CONTROL_GOOGLE: {
                    mGameView.enableRemoteControllerInternetGoogle(BaseActivity.tryGetGoogleAccountId(), BaseActivity.tryGetGooglePlayerName());
                }
                break;
                case ENABLE_INTERNET_REMOTE_CONTROL_PUBLIC: {
                    mGameView.enableRemoteControllerInternetPublic(getBaseActivity().getDefaultHostGUID(), getNameForPublicMultiplayer(settings));
                }
                break;
                case QUICKJOIN_INTERNET_REMOTE_CONTROL_GOOGLE:
                    mRemoteCtlType = Settings.RemoteControl.JOIN_INTERNET_REMOTE_CONTROL_GOOGLE;
                case JOIN_INTERNET_REMOTE_CONTROL_FB:
                case JOIN_INTERNET_REMOTE_CONTROL_GOOGLE:
                case JOIN_INTERNET_REMOTE_CONTROL_PUBLIC:
                {
                    game = null;//ignore game, since we will connect and play game hosted by remote side
                    String host_invitation_data = settings.getString(Settings.GAME_ACTIVITY_REMOTE_INVITATION_DATA_KEY);

                    if (mRemoteCtlType == Settings.RemoteControl.JOIN_INTERNET_REMOTE_CONTROL_FB) {
                        String host_invitation_id = settings.getString(Settings.GAME_ACTIVITY_REMOTE_FB_INVITATION_ID_KEY);
                        mGameView.loadRemoteInternetFb(host_invitation_id, host_invitation_data, BaseActivity.getFbPlayerId(), BaseActivity.getFbPlayerName());
                    } else if (mRemoteCtlType == Settings.RemoteControl.JOIN_INTERNET_REMOTE_CONTROL_GOOGLE) {
                        String match_id = settings.getString(Settings.GAME_ACTIVITY_REMOTE_GOOGLE_MATCH_ID, null);
                        mGameView.loadRemoteInternetGoogle(match_id, host_invitation_data, BaseActivity.tryGetGoogleAccountId(), BaseActivity.tryGetGooglePlayerName());
                    } else {
                        mGameView.loadRemoteInternetPublic( host_invitation_data, getBaseActivity().getDefaultHostGUID(), getNameForPublicMultiplayer(settings));
                    }
                }
                break;
                default:
                    //TODO
                    break;
            }//switch (remoteCtlType)
        } catch (Exception e)
        {
            e.printStackTrace();
            Utils.alertDialog(getContext(), "Error", getString(R.string.err_network_create_failed), new Runnable() {
                @Override
                public void run() {
                    finish();
                }
            });
        }
        //load game if any
        if (game != null) {
            //save last chosen game
            SharedPreferences pref = getContext().getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
            SharedPreferences.Editor edit = pref.edit();
            edit.putString(Settings.LAST_PLAYED_GAME_KEY, game);
            edit.commit();

            //load and start game
            mGameView.loadAndStartGame(game);
        }

        // check for recording permission
        if (mRemoteCtlType != Settings.RemoteControl.NO_REMOTE_CONTROL)
            Utils.checkPermission(getActivity(), Manifest.permission.RECORD_AUDIO, getString(R.string.record_permission_msg), 0);

        // done
        initialized = true;

        // save state persistently
        mPersistentState.setInitialized(initialized);
        mPersistentState.setRemoteCtlType(mRemoteCtlType);

        return mContentView;
    }

    private String getNameForPublicMultiplayer(Bundle settings) {
        String name = getBaseActivity().getDefaultHostRoomName();
        if (name == null)
            name = getString(R.string.a_player);
        String countryCode = settings.getString(Settings.GAME_ACTIVITY_COUNTRY_CODE_KEY, null);
        if (countryCode == null)
            countryCode = LobbyPage.getCountryCode();

        if (countryCode != null && countryCode.length() <= 4)
            name = "[" + countryCode + "] " + name;

        return name;
    }

    private boolean isGameOwner() {
        switch (mRemoteCtlType){
            case CONNECT_LAN_REMOTE_CONTROL:
            case JOIN_INTERNET_REMOTE_CONTROL_FB:
            case JOIN_INTERNET_REMOTE_CONTROL_GOOGLE:
            case JOIN_INTERNET_REMOTE_CONTROL_PUBLIC:
            case QUICKJOIN_INTERNET_REMOTE_CONTROL_GOOGLE:
                return false;
            default:
                return true;
        }
    }

    private View recreateGameView(LayoutInflater inflater, ViewGroup container) {
        ensurePersistentStateAvailable();

        View contentView = null;
        try {
            if (mGameView == null)
                mGameView = new GameSurfaceView(this, null);
            else {
                // we will reuse the game view, but need to remove from its old container first
                if (mGameViewFrameContainer != null) {
                    mGameViewFrameContainer.removeView(mGameView);
                    mGameViewFrameContainer = null;
                }
            }

            mGameView.setSpeed(mPersistentState.getGameSpeed());

            /*----- config screen's layout ---------*/
            contentView = inflater.inflate(R.layout.page_game, container, false);

            mGameViewFrameContainer = (ViewGroup) contentView.findViewById(R.id.game_content_frame);
            Toolbar toolbar = (Toolbar) contentView.findViewById(R.id.game_toolbar);

            if (mGameViewFrameContainer instanceof RelativeLayout)//this should be landscape layout
            {
                //re-insert GameSurfaceView
                mGameViewFrameContainer.addView(mGameView, new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));

                //need to bring toolbar to front since GameSurfaceView is added after it
                toolbar.bringToFront();

                ImageButton inviteBnt = (ImageButton)contentView.findViewById(R.id.btnInviteFriendIngame);
                if (inviteBnt != null) {
                    inviteBnt.bringToFront();
                }
            }
            else {
                //re-insert GameSurfaceView to fill the remaining space besides the toolbar
                mGameViewFrameContainer.addView(mGameView, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1.f));
            }

            //set the Toolbar to act as the ActionBar for this Activity window.
            getBaseActivity().setSupportActionBar(toolbar);

            //disable actionbar's title, icon and home
            getBaseActivity().getSupportActionBar().setDisplayShowTitleEnabled(false);
            getBaseActivity().getSupportActionBar().setDisplayUseLogoEnabled(false);
            getBaseActivity().getSupportActionBar().setDisplayShowHomeEnabled(false);

            //register toolbar's buttons' listener
            ImageButton quickSaveBtn = (ImageButton)toolbar.findViewById(R.id.btnQuickSave);
            ImageButton quickLoadBtn = (ImageButton)toolbar.findViewById(R.id.btnQuickLoad);
            ImageButton chatBtn = (ImageButton)toolbar.findViewById(R.id.btnChatRoom);
            ImageButton inviteBtn = (ImageButton)contentView.findViewById(R.id.btnInviteFriendIngame);

            quickSaveBtn.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    handleCommonMenuOrToolbarAction(v, R.id.action_quick_save);
                }
            });

            quickLoadBtn.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    handleCommonMenuOrToolbarAction(v, R.id.action_quick_load);
                }
            });

            chatBtn.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    handleCommonMenuOrToolbarAction(v, R.id.action_chat);
                }
            });

            inviteBtn.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    handleCommonMenuOrToolbarAction(v, R.id.action_invite_ingame);
                }
            });
        } catch (Exception e) {
            e.printStackTrace();
        }

        return contentView;
    }

    @Override
    public void onSaveInstanceState (Bundle outState) {
        System.out.println("GamePage.onSaveInstanceState()");

        super.onSaveInstanceState(outState);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        System.out.println("GamePage.onCreateOptionsMenu()");

        boolean isInvitingInternetHost =
                mRemoteCtlType == Settings.RemoteControl.ENABLE_INTERNET_REMOTE_CONTROL_FB
                || mRemoteCtlType == Settings.RemoteControl.ENABLE_INTERNET_REMOTE_CONTROL_GOOGLE;
        // Inflate the menu; this adds items to the action bar if it is present.

        inflater.inflate(R.menu.menu_game, menu);

        menu.setGroupVisible(R.id.menu_save_load_group, isGameOwner());
        menu.setGroupVisible(R.id.menu_game_power_control_group, isGameOwner());
        menu.setGroupVisible(R.id.menu_remote_control_group, mRemoteCtlType != Settings.RemoteControl.NO_REMOTE_CONTROL);
        menu.setGroupVisible(R.id.menu_remote_control_internet_host_group, isInvitingInternetHost);

        final MenuItem a_turbo_checkbox_item = menu.findItem(R.id.action_a_turbo);
        final MenuItem b_turbo_checkbox_item = menu.findItem(R.id.action_b_turbo);

        a_turbo_checkbox_item.setChecked(Settings.isBtnATurbo());
        b_turbo_checkbox_item.setChecked(Settings.isBtnBTurbo());

        MenuItem.OnActionExpandListener preventCollapse = new MenuItem.OnActionExpandListener() {
            @Override
            public boolean onMenuItemActionExpand(MenuItem item) {
                return false;
            }

            @Override
            public boolean onMenuItemActionCollapse(MenuItem item) {
                return false;
            }
        };

        a_turbo_checkbox_item.setOnActionExpandListener(preventCollapse);
        b_turbo_checkbox_item.setOnActionExpandListener(preventCollapse);

        // initialize game speed settings submenu
        initGameSpeedMenu(menu);

        // ---------- set visibility of toolbar items ---------------
        // note: must do here after mRemoteCtlType is known
        Toolbar toolbar = (Toolbar)mContentView.findViewById(R.id.game_toolbar);

        ImageButton quickSaveBtn = (ImageButton)toolbar.findViewById(R.id.btnQuickSave);
        ImageButton quickLoadBtn = (ImageButton)toolbar.findViewById(R.id.btnQuickLoad);
        ImageButton chatBtn = (ImageButton)toolbar.findViewById(R.id.btnChatRoom);
        ImageButton inviteBtn = (ImageButton)mContentView.findViewById(R.id.btnInviteFriendIngame);

        if (!isGameOwner()) {
            //if we are not game owner, then no save/load is allowed
            quickSaveBtn.setVisibility(View.INVISIBLE);
            quickLoadBtn.setVisibility(View.INVISIBLE);
        }

        if (mRemoteCtlType == Settings.RemoteControl.NO_REMOTE_CONTROL)//if not in remote control mode, disable chat feature
            chatBtn.setVisibility(View.INVISIBLE);

        if (!isInvitingInternetHost)//if we are not internet host, disable invite button
            inviteBtn.setVisibility(View.INVISIBLE);

    }

    private void initGameSpeedMenu(Menu menu) {
        ensurePersistentStateAvailable();

        int subMenuItemId;
        switch (mPersistentState.getGameSpeed()) {
            case -4:
                subMenuItemId = R.id.action_speed_quarter;
                break;
            case -2:
                subMenuItemId = R.id.action_speed_half;
                break;
            case 2:
                subMenuItemId = R.id.action_speed_2x;
                break;
            case 4:
                subMenuItemId = R.id.action_speed_4x;
                break;
            case 8:
                subMenuItemId = R.id.action_speed_8x;
                break;
            default:
                subMenuItemId = R.id.action_speed_default;
                break;
        }

        try {
            menu.findItem(subMenuItemId).setChecked(true);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        // special menu actions
        switch (id) {
            case R.id.action_a_turbo:
                item.setChecked(!item.isChecked());
                Settings.enableBtnATurbo(item.isChecked());
                if (mGameView != null)
                    mGameView.switchABTurboMode(Settings.isBtnATurbo(), Settings.isBtnBTurbo());
                Settings.saveGlobalSettings(getContext());
                return false;
            case R.id.action_b_turbo:
                item.setChecked(!item.isChecked());
                Settings.enableBtnBTurbo(item.isChecked());
                if (mGameView != null)
                    mGameView.switchABTurboMode(Settings.isBtnATurbo(), Settings.isBtnBTurbo());
                Settings.saveGlobalSettings(getContext());
                return false;
            case R.id.action_speed_default:
            case R.id.action_speed_quarter:
            case R.id.action_speed_half:
            case R.id.action_speed_2x:
            case R.id.action_speed_4x:
            case R.id.action_speed_8x:
            {
                item.setChecked(true);

                if (mGameView == null)
                    return true;

                ensurePersistentStateAvailable();

                int speed = 1;
                switch (id) {
                    case R.id.action_speed_default:
                        speed = 1;
                        break;
                    case R.id.action_speed_quarter:
                        speed = -4;
                        break;
                    case R.id.action_speed_half:
                        speed = -2;
                        break;
                    case R.id.action_speed_2x:
                        speed = 2;
                        break;
                    case R.id.action_speed_4x:
                        speed = 4;
                        break;
                    case R.id.action_speed_8x:
                        speed = 8;
                        break;
                }

                mGameView.setSpeed(speed);
                mPersistentState.setGameSpeed(speed);
            }
                return true;
        }

        // common actions shared between toolbar and menu
        if (handleCommonMenuOrToolbarAction(item.getActionView(), id))
            return true;

        return super.onOptionsItemSelected(item);
    }

    private boolean handleCommonMenuOrToolbarAction(final View actionView, int actionId) {
        boolean handled = true;
        //noinspection SimplifiableIfStatement
        switch (actionId)
        {
            case R.id.action_settings:
            {
                goToPage(BasePage.create(SettingsPage.class));
            }
            break;
            case R.id.action_reset:
                if (mGameView != null)
                    mGameView.resetGame();
                break;
            case R.id.action_load_save:
            {
                goToPage(BasePage.create(LoadSaveFilePage.class, LOAD_SAVE_REQUEST));
            }
            break;
            case R.id.action_save_game:
            {
                goToPage(BasePage.create(SaveGamePage.class, SAVE_GAME_REQUEST));
            }
            break;
            case R.id.action_quick_load:
            {
                //vibrate to notify user that button will do something
                if (actionView != null)
                    actionView.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
                else if (mGameView != null)
                    mGameView.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);

                if (mGameView != null) {
                    mGameView.loadedGame(new AsyncQuery<String>() {
                        @Override
                        public void run(String result) {
                            if (result != null) {
                                performQuickLoad(result);
                            }
                        }
                    });
                }//if (mGameView != null)
            }
            break;
            case R.id.action_quick_save:
            {
                //vibrate to notify user that button will do something
                if (actionView != null)
                    actionView.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
                else if (mGameView != null)
                    mGameView.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);

                if (mGameView != null) {
                    mGameView.loadedGame(new AsyncQuery<String>() {
                        @Override
                        public void run(String result) {
                            if (result != null)
                                performQuickSave(result);
                        }
                    });
                }//if (mGameView != null)
            }
            break;
            case R.id.action_chat:
                showChatDialog();
                break;
            case R.id.action_invite_ingame:
                if (mGameView != null) {
                    mGameView.isRemoteControllerConnected(new AsyncQuery<Boolean>() {
                        @Override
                        public void run(final Boolean connected) {
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    int messageResId;
                                    if (connected)
                                    {
                                        messageResId = R.string.kick_and_new_invite_msg;
                                    }
                                    else {
                                        messageResId = R.string.new_invite_ingame_msg;
                                    }

                                    Utils.dialog(
                                            GamePage.this.getContext(),
                                            getString(R.string.new_invite_ingame_title),
                                            getString(messageResId),
                                            null,
                                            new Runnable() {
                                                @Override
                                                public void run() {
                                                    if (mGameView != null)
                                                        mGameView.reinviteNewFriendForRemoteControllerInternet();
                                                }
                                            },
                                            new Runnable() {
                                                @Override
                                                public void run() {

                                                }
                                            });
                                }
                            });
                        }
                    });
                }
                break;
            case R.id.action_cheats:
            {
                if (mGameView != null) {
                    mGameView.loadedGameName(new AsyncQuery<String>() {
                        @Override
                        public void run(String result) {
                            if (result != null && result.length() > 0)
                                openCheatsPage(result);
                        }
                    });
                }//if (mGameView != null)
            }
                break;
            default:
                handled = false;
                break;
        }//switch (actionId)

        return handled;
    }

    @Override
    public void onConfigurationChanged (Configuration newConfig) {
        System.out.println("GamePage.onConfigurationChanged()");

        super.onConfigurationChanged(newConfig);

        // re-create layout
        LayoutInflater inflater = (LayoutInflater)getActivity().getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        mContentView.removeAllViews();

        mContentView.addView(recreateGameView(inflater, null));

        mGameView.onResume();
    }

    @Override
    public void onBackPressed() {
        new AlertDialog.Builder(getContext())
                .setTitle(getString(R.string.exit))
                .setMessage(getString(R.string.exit_msg))
                .setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        if (mGameView == null)//already exited?
                            return;
                        mGameView.shutdownGameAndClose();
                    }
                })
                .setNegativeButton(android.R.string.no, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        //DO nothing
                    }
                })
                .show();
    }

    @Override
    protected void onPageResult(int requestCode, int resultCode, Bundle data) {
        System.out.println("GamePage.onPageResult()");

        super.onPageResult(requestCode, resultCode, data);

        switch (requestCode) {
            case LOAD_SAVE_REQUEST:
            {
                if (resultCode == RESULT_OK && data != null) {
                    final String savePath = data.getString(Settings.SAVE_PATH_KEY);
                    if (savePath != null)
                    {
                        queueOnResumeTask(new Runnable() {
                            @Override
                            public void run() {
                                if (mGameView != null)
                                    mGameView.loadState(savePath);
                            }
                        });
                    }
                }//if (resultCode == RESULT_OK && data != null)
            }//LOAD_SAVE_REQUEST
                break;
            case SAVE_GAME_REQUEST:
            {
                if (resultCode == RESULT_OK && data != null) {
                    final String savePath = data.getString(Settings.SAVE_PATH_KEY);
                    if (savePath != null)
                    {
                        queueOnResumeTask(new Runnable() {
                            @Override
                            public void run() {
                                if (mGameView != null)
                                    mGameView.saveState(savePath);
                            }
                        });
                    }
                }//if (resultCode == RESULT_OK && data != null)
            }//LOAD_SAVE_REQUEST
            break;
        }
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        System.out.println("GamePage.onActivityCreated()");

        super.onActivityCreated(savedInstanceState);

        getBaseActivity().setChatDialogDelegate(this);
    }

    @Override
    public void onDestroyView() {
        System.out.println("GamePage.onDestroyView()");

        super.onDestroyView();

        // clear action bar
        getBaseActivity().setSupportActionBar(null);

        // deactivate chat dialog's delegate
        getBaseActivity().setChatDialogDelegate(null);
    }

    @Override
    public void onDestroy()
    {
        System.out.println("GamePage.onDestroy()");

        super.onDestroy();

        mGameView.onParentPageDestroyed();
    }

    @Override
    public void onResume()
    {
        System.out.println("GamePage.onResume()");

        super.onResume();

        // resume view
        mGameView.onResume();

        mContentView.setFocusable(true);
        mContentView.setFocusableInTouchMode(true);
        mContentView.requestFocus();
    }

    @Override
    public void onPause()
    {
        System.out.println("GamePage.onPause()");

        mGameView.onPause();

        super.onPause();

        //save settings
        Settings.saveGlobalSettings(getContext());
    }

    /*------- hardware input's handler ---*/

    private boolean onKeyDown(final Iterable<Settings.Button> mappedNesButtons, KeyEvent event) {
        try {
            if (event.getRepeatCount() != 0)
                return mappedNesButtons.iterator().hasNext();//we ignore this but won't let default handler handle this event
        } catch (Exception e)
        {
            e.printStackTrace();
            return false;
        }

        boolean handled = false;

        if (mappedNesButtons != null) {
            for (Settings.Button button: mappedNesButtons) {
                if (mGameView != null)
                    mGameView.onUserHardwareButtonDown(button);

                handled = true;
            }//for (Settings.Button button: mappedButtons)
        }//if (mappedButtons != null)

        return handled;
    }

    private boolean onKeyUp(final Iterable<Settings.Button> mappedNesButtons, KeyEvent event) {
        boolean handled = false;
        if (mappedNesButtons != null) {
            for (Settings.Button button: mappedNesButtons) {
                switch (button)
                {
                    case QUICK_SAVE:
                        handleCommonMenuOrToolbarAction(null, R.id.action_quick_save);
                        break;
                    case QUICK_LOAD:
                        handleCommonMenuOrToolbarAction(null, R.id.action_quick_load);
                        break;
                    default:
                        if (mGameView != null)
                            mGameView.onUserHardwareButtonUp(button);
                }

                handled = true;
            }//for (Settings.Button button: mappedButtons)
        }//if (mappedButtons != null)

        return handled;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        if (keyCode == KeyEvent.KEYCODE_UNKNOWN)
            return super.dispatchKeyEvent(event);

        boolean handled = false;
        final Iterable<Settings.Button> mappedNesButtons = Settings.getMappedButton(keyCode);
        if (mappedNesButtons != null)
        {
            switch (event.getAction())
            {
                case KeyEvent.ACTION_DOWN:
                case KeyEvent.ACTION_MULTIPLE:
                    handled = onKeyDown(mappedNesButtons, event);
                    break;
                case KeyEvent.ACTION_UP:
                    handled = onKeyUp(mappedNesButtons, event);
                    break;
            }
        }

        if (handled)
            return true;//stop default handler
        return super.dispatchKeyEvent(event);//use default handler
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (mGameView == null)
            return super.dispatchGenericMotionEvent(event);

        if (((event.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK)
                || (event.getSource() & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD)
        {
            // Process all historical movement samples in the batch
            final int historySize = event.getHistorySize();

            // Process the movements starting from the
            // earliest historical position in the batch
            for (int i = 0; i < historySize; i++) {
                // Process the event at historical position i
                processJoystickInput(event, i);
            }

            // Process the current movement sample in the batch
            processJoystickInput(event, -1);

            return true;//stop default handler
        }
        return super.dispatchGenericMotionEvent(event);
    }

    private float getJoystickInputAxis(MotionEvent event, InputDevice device, int axis, int historyPos) {
        if (device == null)
            return 0;

        final InputDevice.MotionRange range =
                device.getMotionRange(axis, event.getSource());

        // A joystick at rest does not always report an absolute position of
        // (0,0). Use the getFlat() method to determine the range of values
        // bounding the joystick axis center.
        if (range != null) {
            final float flat = range.getFlat();
            final float value =
                    historyPos < 0 ? event.getAxisValue(axis):
                            event.getHistoricalAxisValue(axis, historyPos);

            // Ignore axis values that are within the 'flat' region of the
            // joystick axis center.
            if (Math.abs(value) > flat) {
                return value;
            }
        }
        return 0;
    }

    private void processJoystickInput(MotionEvent event, int historicalPos) {
        InputDevice inputDevice = event.getDevice();

        // Calculate the horizontal distance to move by
        // using the input value from one of these physical controls:
        // the left control stick, hat axis, or the right control stick.
        float x = getJoystickInputAxis(event, inputDevice, MotionEvent.AXIS_X, historicalPos);
        if (x == 0) {
            x = getJoystickInputAxis(event, inputDevice, MotionEvent.AXIS_HAT_X, historicalPos);
        }
        if (x == 0) {
            x = getJoystickInputAxis(event, inputDevice, MotionEvent.AXIS_Z, historicalPos);
        }

        // Calculate the vertical distance to move by
        // using the input value from one of these physical controls:
        // the left control stick, hat switch, or the right control stick.
        float y = getJoystickInputAxis(event, inputDevice, MotionEvent.AXIS_Y, historicalPos);
        if (y == 0) {
            y = getJoystickInputAxis(event, inputDevice, MotionEvent.AXIS_HAT_Y, historicalPos);
        }
        if (y == 0) {
            y = getJoystickInputAxis(event, inputDevice, MotionEvent.AXIS_RZ, historicalPos);
        }

        mGameView.onJoystickMoved(x, -y);
    }

    /*-------------- handle the case when user accepted invitation on notificaton bar -------*/
    // and enters this page in middle of an existing game
    @Override
    protected void onGoogleMatchAccepted(@NonNull final TurnBasedMultiplayerClient gclient, @NonNull final TurnBasedMatch match) {
        if (mGameView != null) {
            mGameView.shutdownGame(true, new Runnable() {
                @Override
                public void run() {
                    GamePage.super.onGoogleMatchAccepted(gclient, match);
                }
            });
        }
    }

    /*--- on Game Exit ---*/
    // WARNING: finish() and finishWithStats are supposed to be called by GameSurfaceView only
    @Override
    public void finish() {
        // invalidate persistent state
        ensurePersistentStateAvailable();

        // reset settings
        mPersistentState.setInitialized(false);
        mPersistentState.setGameSpeed(1);

        // exit
        super.finish();
    }

    public void finishWithStats(long playTimeNs, long multiplayerTimeNs, boolean closePage) {
        if (isFinishing())
            return;

        System.out.println("Game finished with stats: duration=" + playTimeNs / 1e9 + " seconds, multiplayer duration=" + multiplayerTimeNs / 1e9);

        long playTimeMs = playTimeNs / 1000000;
        long multiplayerTimeMs = multiplayerTimeNs / 1000000;

        final SharedPreferences preferences = getContext().getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
        if (preferences != null)
        {
            SharedPreferences.Editor prefEditor = preferences.edit();

            long currentPlayTimeMs = preferences.getLong(Settings.USER_TOTAL_PLAY_TIME_KEY, 0);
            long currentMultiPlayTimeMs = preferences.getLong(Settings.USER_TOTAL_MULTIPLAYER_TIME_KEY, 0);

            currentPlayTimeMs += playTimeMs;
            currentMultiPlayTimeMs += multiplayerTimeMs;

            System.out.println("Lifetime stats: duration=" + currentPlayTimeMs / 1e3 + " seconds, multiplayer duration=" + currentMultiPlayTimeMs / 1e3);

            //save total play time stats
            prefEditor.putLong(Settings.USER_TOTAL_PLAY_TIME_KEY, currentPlayTimeMs);
            prefEditor.putLong(Settings.USER_TOTAL_MULTIPLAYER_TIME_KEY, currentMultiPlayTimeMs);
            prefEditor.commit();
        }//if (preferences != null)

        sDurationMsSinceLastAdDisplay += playTimeMs;
        sNumGamesSinceLastAdDisplay++;

        if (closePage) {
            //determine whether displaying ads or not
            boolean adshowing = false;

            if (sDurationMsSinceLastAdDisplay > 30 * 60 * 1000 ||
                    (sDurationMsSinceLastAdDisplay > 15 * 60 * 1000 && sNumGamesSinceLastAdDisplay > 1) ||
                    (sDurationMsSinceLastAdDisplay > 5 * 60 * 1000 && sNumGamesSinceLastAdDisplay >= 5)) {
                adshowing = displayInterstitialAd();
            }

            if (adshowing) {
                //reset counters
                sDurationMsSinceLastAdDisplay = 0;
                sNumGamesSinceLastAdDisplay = 0;
            } else
                finish();
        } // if (closePage)
    }

    /*----- chat dialog ----*/
    private void showChatDialog() {
        if (mContentView == null)
            return;
        // DialogFragment.show() will take care of adding the fragment
        // in a transaction.  We also want to remove any currently showing
        // dialog, so make our own transaction and take care of that here.
        FragmentTransaction ft = getBaseActivity().getSupportFragmentManager().beginTransaction();
        Fragment prev = getBaseActivity().getSupportFragmentManager().findFragmentByTag(CHAT_DIALOG_TAG);
        if (prev != null) {
            ft.remove(prev);
        }
        ft.addToBackStack(null);

        //show new dialog
        GameChatDialog dialog = new GameChatDialog();
        dialog.show(ft, CHAT_DIALOG_TAG);

        //mark chat button as read
        ImageView chatBtnView = (ImageView)mContentView.findViewById(R.id.btnChatRoom);
        if (chatBtnView != null)
            chatBtnView.setImageResource(R.drawable.ic_action_email_white);
    }

    public void onNewMessageArrived() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mContentView == null)
                    return;
                Fragment currentDialog = getBaseActivity().getSupportFragmentManager().findFragmentByTag(CHAT_DIALOG_TAG);
                if (currentDialog == null || !currentDialog.isVisible() || currentDialog.isRemoving()) {
                    //mark chat button as unread
                    ImageView chatBtnView = (ImageView) mContentView.findViewById(R.id.btnChatRoom);
                    if (chatBtnView != null)
                        chatBtnView.setImageResource(R.drawable.ic_action_new_email_white);

                    if (mGameView != null) {
                        mGameView.showToast(getString(R.string.new_msg_toast_msg), Toast.LENGTH_SHORT);
                    }
                }
            }
        });
    }

    /*--- implements GameChatDialog.Delegate ---*/
    @Override
    public GameChatDialog.ChatMessagesDatabaseViewAdapter getChatDatabaseViewAdapter() {
        return mGameView.getChatDatabaseViewAdapter();
    }

    @Override
    public void onSendButtonClicked(String message) {
        long id = Utils.generateId();

        mGameView.sendMessageToRemote(id, message, new AsyncQuery<Boolean>() {
            @Override
            public void run(final Boolean result) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (!result)
                            return;
                        GameChatDialog dialog = (GameChatDialog)getBaseActivity().getSupportFragmentManager().findFragmentByTag(CHAT_DIALOG_TAG);
                        if (dialog != null)
                            dialog.clearTypingText();
                    }
                });
            }
        });
    }

    /*---------- AdMob -----*/
    private void requestNewInterstitialAd() {
        AdRequest adRequest = new AdRequest.Builder()
                .addTestDevice("3E59E296DBD138C632FC38A6CAFEE271")
                .addTestDevice("F03057FE9683E1FBCAA50DF3A52DBA66")
                .addTestDevice("921A8020A9448FBC638FE9F600FCC6BD")
                .addTestDevice("F07D9F860948B0D957AEDA110D4A3EF4")
                .build();

        mInterstitialAd.loadAd(adRequest);
    }

    private boolean displayInterstitialAd() {
        if (!Settings.isAdsDisabled() && mInterstitialAd.isLoaded())
        {
            mInterstitialAd.show();
            return true;
        }

        return false;
    }

    /*----- load cheats ---*/
    private void openCheatsPage(String gameName) {
        Bundle settings = new Bundle();
        settings.putString(CheatsPage.GAME_NAME_KEY, gameName);

        BasePage cheatsPage = BasePage.create(CheatsPage.class);
        cheatsPage.setExtras(settings);
        goToPage(cheatsPage);
    }

    /*----- save/load ----*/
    private void performQuickSave(final String currentGame) {
        if (mPrivateDataPath == null || mGameView == null)
            return;
        try {
            String currentGameName = (new File(currentGame)).getName();

            File[] savedFiles = listQuickSavedFiles(currentGameName);

            int slot;
            if (savedFiles != null && savedFiles.length > 0) {
                slot = parseQuickSaveSlot(currentGameName, savedFiles[0].getName());//get slot index of latest saved file
                //increase slot index
                slot = (slot + 1) % Settings.MAX_QUICK_SAVES_PER_GAME;
            }
            else
                slot = 0;

            //save state
            mGameView.saveState(constructQuickSavePath(currentGameName, slot));

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void performQuickLoad(final String currentGame) {
        if (mPrivateDataPath == null || mGameView == null)
            return;

        try {
            String currentGameName = (new File(currentGame)).getName();

            //get current quick saved files for this game
            File[] savedFiles = listQuickSavedFiles(currentGameName);

            //load newest file
            if (savedFiles != null && savedFiles.length > 0)
                mGameView.loadState(savedFiles[0].getAbsolutePath());
            else {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Utils.alertDialog(getContext(), "Error", getString(R.string.save_file_not_available_msg), null);
                    }
                });
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private File[] listQuickSavedFiles(String currentGameName) {
        //get current quick saved files for this game
        File[] savedFiles = mPrivateDataPath.listFiles(new QuickSaveFileFilter(currentGameName));
        if (savedFiles != null)
            Arrays.sort(savedFiles, new QuickSaveFileComparator());
        return savedFiles;
    }

    private String constructQuickSavePath(String currentGameName, int slot) {
        String name = Settings.QUICK_SAVE_PREFIX + slot + "." + currentGameName + ".ns";
        return (new File(mPrivateDataPath, name)).getAbsolutePath();
    }

    private static int parseQuickSaveSlot(String currentGameName, String fileName){
        String patternStr = "^" + Settings.QUICK_SAVE_PREFIX + "([0-9]+)\\." + Pattern.quote(currentGameName) + "\\.ns$";
        Pattern pattern = Pattern.compile(patternStr);
        Matcher matcher = pattern.matcher(fileName);
        if (!matcher.find())
            return -1;

        if (matcher.groupCount() != 1)
            return -1;

        String slotStr = matcher.group(1);
        int slot = Integer.parseInt(slotStr);
        if (slot >= Settings.MAX_QUICK_SAVES_PER_GAME)
            return -1;
        return slot;
    }

    /*----------- QuickSaveFileFilter ---------*/
    private static class QuickSaveFileFilter implements FilenameFilter {
        private final String mCurrentGameName;

        public QuickSaveFileFilter(String gameName) {
            mCurrentGameName = gameName;
        }

        @Override
        public boolean accept(File dir, String name) {
            int slot = parseQuickSaveSlot(mCurrentGameName, name);
            return slot != -1;
        }
    }

    /*------------- QuickSaveFileComparator -------*/
    private static class QuickSaveFileComparator implements Comparator<File> {
        @Override
        public int compare(File lhs, File rhs) {
            if (lhs.lastModified() > rhs.lastModified())
                return -1;
            if (lhs.lastModified() < rhs.lastModified())
                return 1;
            return 0;
        }
    }

    /*-------- persistent state -----------------*/
    PersistenState ensurePersistentStateAvailable() {
        if (mPersistentState == null)
            mPersistentState = ViewModelProviders.of(getActivity()).get(PersistenState.class);

        return mPersistentState;
    }

    public static class PersistenState extends ViewModel {
        private MutableLiveData<Boolean> mInitialized = new MutableLiveData<Boolean>();
        private MutableLiveData<Settings.RemoteControl> mRemoteCtlType = new MutableLiveData<>();
        private MutableLiveData<Integer> mGameSpeed = new MutableLiveData<>();

        public PersistenState() {
            mInitialized.setValue(false);
            mRemoteCtlType.setValue(Settings.RemoteControl.NO_REMOTE_CONTROL);
            mGameSpeed.setValue(1);
        }

        public boolean isInitialized() {
            return mInitialized.getValue();
        }

        public void setInitialized(boolean b) {
            mInitialized.setValue(b);
        }

        public Settings.RemoteControl getRemoteCtlType() {
            return mRemoteCtlType.getValue();
        }

        public void setRemoteCtlType(Settings.RemoteControl type) {
            mRemoteCtlType.setValue(type);
        }

        public void setGameSpeed(int speed) { mGameSpeed.setValue(speed); }
        public int getGameSpeed() { return mGameSpeed.getValue(); }
    }
}
