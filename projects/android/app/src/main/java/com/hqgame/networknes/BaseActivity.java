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

import android.app.Activity;
import android.app.ProgressDialog;
import android.arch.lifecycle.ViewModel;
import android.arch.lifecycle.ViewModelProviders;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.net.NetworkInfo;
import android.net.wifi.p2p.WifiP2pManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.app.AppCompatActivity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;
import android.widget.Button;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.PurchaseHistoryResponseListener;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.appsflyer.AppsFlyerConversionListener;
import com.appsflyer.AppsFlyerLib;
import com.facebook.AccessToken;
import com.facebook.AccessTokenTracker;
import com.facebook.CallbackManager;
import com.facebook.FacebookCallback;
import com.facebook.FacebookException;
import com.facebook.FacebookSdk;
import com.facebook.GraphRequest;
import com.facebook.GraphRequestAsyncTask;
import com.facebook.GraphResponse;
import com.facebook.HttpMethod;
import com.facebook.appevents.AppEventsLogger;
import com.facebook.login.LoginManager;
import com.facebook.login.LoginResult;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.auth.api.Auth;
import com.google.android.gms.auth.api.signin.GoogleSignIn;
import com.google.android.gms.auth.api.signin.GoogleSignInAccount;
import com.google.android.gms.auth.api.signin.GoogleSignInClient;
import com.google.android.gms.auth.api.signin.GoogleSignInOptions;
import com.google.android.gms.auth.api.signin.GoogleSignInResult;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.games.Games;
import com.google.android.gms.games.GamesClient;
import com.google.android.gms.games.InvitationsClient;
import com.google.android.gms.games.Player;
import com.google.android.gms.games.PlayersClient;
import com.google.android.gms.games.TurnBasedMultiplayerClient;
import com.google.android.gms.games.multiplayer.realtime.RoomConfig;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatch;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatchConfig;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatchUpdateCallback;
import com.google.android.gms.tasks.OnCanceledListener;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.android.gms.tasks.Task;

import org.json.JSONObject;

import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

/**
 * Created by le on 3/21/2016.
 */
public final class BaseActivity extends AppCompatActivity
        implements GameChatDialog.Delegate, PurchasesUpdatedListener
{
    private static final int GOOGLE_SERVICE_AVAIL_ERR_REQUEST_CODE = 0;
    private static final int GOOGLE_SIGN_IN_REQUEST_CODE = 1;
    private static final int GOOGLE_TURN_BASED_MATCH_INVITE_REQUEST_CODE = 2;
    private static final int GOOGLE_TURN_BASED_MATCH_INBOX_REQUEST_CODE = 3;
    private static final int GOOGLE_PLAYER_SEARCH_REQUEST_CODE = 4;
    private static final String FIRST_START_KEY = "firstStart";

    private static BaseActivity sCurrentActivity = null;

    // FB
    private static boolean sIsFbLoadingProfile = false;
    private static GraphRequestAsyncTask sFbProfileRequest = null;
    private static boolean sFbLoggedIn = false;
    private static JSONObject sProfileData = null;
    private static AccessTokenTracker sAccessTokenTracker = null;

    private LinkedList<Runnable> mFbLoadingProfileFinishedTasks = new LinkedList<>(); // callbacks to be invoked when fb logged in
    private CallbackManager mFbCallbackManager = null;

    //
    private static HashSet<LanServerDiscoveryCallback> sLanDiscoveryCallbacks = new HashSet<>();

    private static Long sTotalPlayTimeMsAtTheStart = null;
    private static Long sTotalMultiPlayerTimeMsAtTheStart = null;

    // Google
    private boolean mIsGoogleSigningInOut = false;
    private boolean mGoogleSignedIn = false;
    private String mGoogleAccountId = null;
    private String mGooglePlayerId = null;
    private String mGooglePlayerName = null;
    private Button mGoogleSignInButton = null;
    private TurnBasedMultiplayerClient mGoogleMultiplayerClient = null;
    private GamesClient mGoogleGamesClient = null;
    private PlayersClient mGooglePlayersClient = null;
    private InvitationsClient mGoogleInvitationsClient = null;
    private TurnBasedMatchUpdateCallback mGoogleMultiplayerMatchUpdateCallback = null;

    // Wifi Direct
    private WifiP2pManager.Channel mWifiP2pChannel = null;
    private WifiP2pManager mWifiP2pManager = null;
    private BroadcastReceiver mWifiP2pBroadcaseReceiver = null;
    private IntentFilter mWifiP2pIntentFilter = new IntentFilter();
    private boolean mWifiDirectEnabled = true;

    private HashSet<WifiDirectStateUpdateCallback> mWifiP2pCallbacks = new HashSet<>();

    // billing
    private static enum BillingClientSate {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
    }

    private BillingClientSate mBillingClientState = BillingClientSate.DISCONNECTED;
    private BillingClient mBillingClient = null;
    private LinkedList<AsyncQuery<Integer>> mBillingClientReadyTasks = new LinkedList<>();
    private HashSet<PurchasesUpdatedListener> mPurchaseCallbacks = new HashSet<>();

    //
    private ProgressDialog mProgressDialog = null;

    private boolean mFirstCreate = true;
    private boolean mActive = false;

    private LinkedList<Runnable> mOnResumeTasks = new LinkedList<>();
    private PersistentData mPersistentData = null; // will survive through Activity recreation

    private GameChatDialog.Delegate mChatDialogDelegate = null;

    public static BaseActivity getCurrentActivity() {
        return sCurrentActivity;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState){
        super.onCreate(savedInstanceState);

        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);

        setContentView(R.layout.activity_base);

        sCurrentActivity = this;

        //initialize AdMob
        MobileAds.initialize(this, getString(R.string.admob_app_id));

        //tracking install
        initAppsFlyer();

        // initialize Google Services
        initGoogleServices();

        //initialize Facebook SDK
        initFacebookSdk();

        //load saved settings
        Settings.loadGlobalSettings(this);

        // retrieve view model
        ensurePersistentDataAvailable();

        //read the player stats once
        readStatsOnce();

        // init billing
        initIAP();

        //notify native side
        cacheJVMNative();
        onCreatedNative(savedInstanceState);

        // open main menu
        if (savedInstanceState != null) {
            mFirstCreate = savedInstanceState.getBoolean(FIRST_START_KEY, true);
        }

        if (mFirstCreate || getTopFragment() == null)
        {
            System.out.println("BaseActivity.onCreate() first time created, go to main page");

            goToNextFragment(BasePage.create(MainPage.class));

            mFirstCreate = false;
        }
    }

    @Override
    protected void onStart()
    {
        super.onStart();

        sCurrentActivity = this;
        onStartedNative();
    }

    @Override
    protected void onResume()
    {
        super.onResume();

        sCurrentActivity = this;
        mActive = true;

        signInGoogleSilently();

        // register wifi direct broadcast receiver
        if (mWifiP2pBroadcaseReceiver != null)
            registerReceiver(mWifiP2pBroadcaseReceiver, mWifiP2pIntentFilter);

        // billing check
        checkIAP();

        onResumedNative();

        /*-- run scheduled onresume tasks ---*/
        for (Runnable task: mOnResumeTasks){
            task.run();
        }

        mOnResumeTasks.clear();
    }

    @Override
    protected void onPause()
    {
        super.onPause();
        mActive = false;

        // unregister wifi direct broadcast receiver
        if (mWifiP2pBroadcaseReceiver != null)
            unregisterReceiver(mWifiP2pBroadcaseReceiver);

        onPausedNative();
    }

    @Override
    protected void onStop()
    {
        super.onStop();

        onStoppedNative();
    }

    @Override
    protected void onDestroy()
    {
        super.onDestroy();

        onDestroyedNative();

        // wifi direct
        deinitWifiDirect();

        //facebook
        cancelFbProfileRequest();
        sAccessTokenTracker.stopTracking();

        // google
        if (mGoogleMultiplayerClient != null)
            mGoogleMultiplayerClient.unregisterTurnBasedMatchUpdateCallback(mGoogleMultiplayerMatchUpdateCallback);
        mGoogleMultiplayerClient = null;
        mGoogleGamesClient = null;
        mGooglePlayersClient = null;
        mGoogleInvitationsClient = null;

        // IAP
        deinitIAP();

        //invalidate global activity reference
        if (sCurrentActivity == this)
            sCurrentActivity = null;
    }

    @Override
    protected void onSaveInstanceState(Bundle state)
    {
        state.putBoolean(FIRST_START_KEY, mFirstCreate);

        onSaveInstanceStateNative(state);

        super.onSaveInstanceState(state);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        super.onActivityResult(requestCode, resultCode, data);

        onActivityResultNative(requestCode, resultCode, data);

        mFbCallbackManager.onActivityResult(requestCode, resultCode, data);

        if (requestCode == GOOGLE_SIGN_IN_REQUEST_CODE) {
            System.out.println("BaseActivity.onActivityResult(GOOGLE_SIGN_IN_REQUEST_CODE)");

            GoogleSignInResult result = Auth.GoogleSignInApi.getSignInResultFromIntent(data);
            if (result.isSuccess()) {
                // The signed in account is stored in the result.
                GoogleSignInAccount signedInAccount = result.getSignInAccount();
                onGoogleSignedIn(signedInAccount);
            } else {
                System.out.println("BaseActivity.onActivityResult(GOOGLE_SIGN_IN_REQUEST_CODE), status=" + result.getStatus());

                String message = result.getStatus().getStatusMessage();
                if (message == null)
                    message =  CommonStatusCodes.getStatusCodeString(result.getStatus().getStatusCode());

                onGoogleSignedIn(null);

                if (message != null && !message.isEmpty()) {
                    Utils.alertDialog(this, null, message, new Runnable() {
                        @Override
                        public void run() {
                        }
                    });
                }
            }
        } else if (requestCode == GOOGLE_SERVICE_AVAIL_ERR_REQUEST_CODE) {
            System.out.println("BaseActivity.onActivityResult(GOOGLE_SERVICE_AVAIL_ERR_REQUEST_CODE) restarting ...");

            queueOnResumeTask(new Runnable() {
                @Override
                public void run() {
                    cancelFbProfileRequest();

                    // restart
                    Intent intent = getIntent();
                    finish();
                    startActivity(intent);
                }
            });
        }

        // now forward to delegates if there are any registered
        forwardActivityResultToRegisteredHandlers(requestCode, resultCode, data);

    }

    /* ---- permission request callback -----*/
    @Override
    public void onRequestPermissionsResult(int requestCode, String permissions[], int[] grantResults) {
        BasePage page = getTopFragmentAsPage();
        if (page != null)
          page.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    /* ----- input events -------*/
    @Override
    public void onBackPressed() {
        BasePage page = getTopFragmentAsPage();
        if (page != null)
        {
            page.onBackPressed();
        }
        else
            super.onBackPressed();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        BasePage top = getTopFragmentAsPage();
        if (top != null && top.dispatchKeyEvent(event))
            return true;

        return super.dispatchKeyEvent(event);//use default handler
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        BasePage top = getTopFragmentAsPage();
        if (top != null && top.dispatchGenericMotionEvent(event))
            return true;

        return super.dispatchGenericMotionEvent(event);
    }

    /* ---------------------------*/
    @Override
    public void finish() {
        super.finish();
    }

    /*-----------------------------*/
    public AsyncOperation runOnUiThreadDelayed(final Runnable runnable, long delayedMs) {
        Handler handler = new Handler(Looper.getMainLooper());
        AsyncOperation.Runnable asyncOperationAdapter;
        handler.postDelayed(asyncOperationAdapter = new AsyncOperation.Runnable() {
            @Override
            protected void runImpl() {
                if (runnable != null)
                    runnable.run();
            }
        }, delayedMs);

        return asyncOperationAdapter;
    }

    /*----- onActivityResult delegation ----*/
    public void requestStartActivityForResult(Intent intent, int requestCode, ActivityResultHandler resultHandler) {
        if (resultHandler == null)
            return;

        ensurePersistentDataAvailable();

        // register handler to map table
        ActivityResultHandlerMap delegatesMap = mPersistentData.mOnActivityResultDelegates;

        delegatesMap.register(requestCode, true, resultHandler);

        // now start activity
        startActivityForResult(intent, requestCode);
    }

    public void unregisterActivityResultHandler(int requestCode, ActivityResultHandler resultHandler) {
        ensurePersistentDataAvailable();

        ActivityResultHandlerMap delegatesMap = mPersistentData.mOnActivityResultDelegates;

        delegatesMap.uregister(requestCode, resultHandler);

    }

    private void forwardActivityResultToRegisteredHandlers(int requestCode, int resultCode, Intent data) {
        ensurePersistentDataAvailable();

        ActivityResultHandlerMap delegatesMap = mPersistentData.mOnActivityResultDelegates;

        delegatesMap.handle(this, requestCode, resultCode, data);
    }

    /* ------ fragment stack -------*/
    public void goToNextFragment(Fragment next) {
        FragmentTransaction ft = getSupportFragmentManager().beginTransaction();

        ft.setCustomAnimations(R.anim.page_enter, R.anim.page_exit, R.anim.page_pop_enter, R.anim.page_pop_exit);

        ft.replace(R.id.main_fragment_container, next, Integer.toString(getSupportFragmentManager().getBackStackEntryCount()));
        ft.addToBackStack(null);
        ft.commit();
    }

    public Fragment getTopFragment() {
        FragmentManager fm = getSupportFragmentManager();
        int backStackCount = fm.getBackStackEntryCount();
        if (backStackCount > 0) {
            Fragment f = fm.findFragmentByTag(Integer.toString(backStackCount - 1));
            return f;
        }

        return null;
    }

    public Fragment get2ndTopFragment() {
      FragmentManager fm = getSupportFragmentManager();
      int backStackCount = fm.getBackStackEntryCount();
      if (backStackCount > 1) {
        Fragment f = fm.findFragmentByTag(Integer.toString(backStackCount - 2));
        return f;
      }

      return null;
    }

    public BasePage getTopFragmentAsPage() {
        Fragment f = getTopFragment();
        if (f != null && f instanceof BasePage)
            return (BasePage)f;

        return null;
    }

  public BasePage get2ndTopFragmentAsPage() {
    Fragment f = get2ndTopFragment();
    if (f != null && f instanceof BasePage)
      return (BasePage)f;

    return null;
  }

    /* ------------------------------*/
    protected void queueOnResumeTask(Runnable task) {
        if (mActive)
            task.run();
        else
            mOnResumeTasks.add(task);
    }

    private static Object getAssetsManagerObject() {
        if (sCurrentActivity != null)
            return sCurrentActivity.getAssets();
        return null;
    }

    private static Class getSettingsClass() {
        return Settings.class;
    }

    private static String getHostIPAddress() {
        if (sCurrentActivity == null)
            return null;
        return Utils.getHostIPAddress(sCurrentActivity);
    }

    private PersistentData ensurePersistentDataAvailable() {
        if (mPersistentData == null)
            mPersistentData = ViewModelProviders.of(this).get(PersistentData.class);

        return mPersistentData;
    }

    private static void runOnMainThread(final long nativeFunctionId, final long nativeFunctionArg) {
        if (sCurrentActivity != null) {
            sCurrentActivity.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    invokeNativeFunction(nativeFunctionId, nativeFunctionArg);
                }
            });
        }
    }

    public void showProgressDialog(int messageResId, Runnable cancelCallback) {
        showProgressDialog(getString(messageResId), cancelCallback);
    }

    public void showProgressDialog(String message, Runnable cancelCallback) {
        dismissProgressDialog();
        mProgressDialog = Utils.showProgressDialog(this, message, cancelCallback);
    }

    public void dismissProgressDialog() {
        if (mProgressDialog != null && mProgressDialog.isShowing()) {
            mProgressDialog.dismiss();
        }

        mProgressDialog = null;
    }

    /*---------- AppsFlyer ------*/
    private void initAppsFlyer() {
        //tracking install
        AppsFlyerConversionListener conversionDataListener = new AppsFlyerConversionListener() {
            @Override
            public void onInstallConversionDataLoaded(Map<String, String> map) {

            }

            @Override
            public void onInstallConversionFailure(String s) {

            }

            @Override
            public void onAppOpenAttribution(Map<String, String> map) {

            }

            @Override
            public void onAttributionFailure(String s) {

            }
        };

        AppsFlyerLib.getInstance().init(
                getString(R.string.appsflyer_dev_key),
                conversionDataListener,
                getApplicationContext());

        AppsFlyerLib.getInstance().startTracking(getApplication());
    }

    /*---------- google services -----*/
    private void initGoogleServices() {

    }

    private GoogleSignInClient createGoogleSigninClient() {
        GoogleSignInOptions gso =
                new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_GAMES_SIGN_IN)
                    .requestId()
                    .build();

        return GoogleSignIn.getClient(this, gso);
    }

    private void setGoogleIsSigningIn(boolean inProgress) {
        if (inProgress) {
            showGoogleSigningInDialog();
        } else {
            dismissProgressDialog();
            if (sIsFbLoadingProfile)
                showFbLoadingProfileDialog();
        }
        mIsGoogleSigningInOut = inProgress;

        if (mGoogleSignInButton != null)
            mGoogleSignInButton.setEnabled(!inProgress);
    }

    private void signInGoogleSilently() {
        if (mIsGoogleSigningInOut)
            return;

        System.out.println("BaseActivity.signInGoogleSilently()");

        setGoogleIsSigningIn(true);

        GoogleSignInAccount currentAccount = GoogleSignIn.getLastSignedInAccount(this);
        if (currentAccount != null) {
            onGoogleSignedIn(currentAccount);
            return;
        }
        // disable this code for now, GoogleSignIn.getLastSignedInAccount() can return valid account
        // when user already signed in the app previously on the same device
        else if (false) {

            if (mGoogleSignInButton != null) {
                // change button appearance
                if (currentAccount != null && mGooglePlayerId != null)
                    mGoogleSignInButton.setText(R.string.google_signout);
                else
                    mGoogleSignInButton.setText(R.string.google_signin);
            }

            GoogleSignInClient signInClient = createGoogleSigninClient();
            signInClient.silentSignIn().addOnCompleteListener(new OnCompleteListener<GoogleSignInAccount>() {
                @Override
                public void onComplete(@NonNull Task<GoogleSignInAccount> task) {
                    if (task.isSuccessful()) {
                        // The signed in account is stored in the task's result.
                        GoogleSignInAccount signedInAccount = task.getResult();
                        onGoogleSignedIn(signedInAccount);
                    } else
                        onGoogleSignedIn(null);
                }
            });
        } else {
            onGoogleSignedIn(null);
        }
    }

    private void onHasGoogleSignedInPlayerInfo(final GoogleSignInAccount signedInAccount, final Player player) {

        System.out.println("BaseActivity.onHasGoogleSignedInPlayerInfo()");

        // retrieve and cache relevant objects
        getGoogleGamesClient();
        getGoogleMultiplayerClient();
        getGooglePlayersClient();
        getGoogleInvitationsClient();

        mGooglePlayerName = player.getDisplayName();
        mGooglePlayerId = player.getPlayerId(); // getPlayerId is not globally unique
        mGoogleAccountId = signedInAccount.getId();

        if (mGoogleAccountId == null) {
            Utils.alertDialog(this, getString(R.string.generic_err_title),
                    getString(R.string.google_signin_no_permission_msg), new Runnable() {
                        @Override
                        public void run() {
                            doGoogleSigningOut();
                        }
                    });

            return;
        }

        if (Settings.DEBUG) {
            System.out.println("BaseActivity.onHasGoogleSignedInPlayerInfo: playerId=" + mGooglePlayerId + ", accountId=" + mGoogleAccountId);
        }

        if (!mGoogleSignedIn) {
            mGoogleSignedIn = true;
            setGoogleIsSigningIn(false);

            // show toast if this is the 1st time logging in
            String acc = mGooglePlayerName;
            //show welcome back toast
            if (acc != null)
                Toast.makeText(BaseActivity.this, getString(R.string.welcome_back) + ", " + acc, Toast.LENGTH_LONG).show();
            else
                Toast.makeText(BaseActivity.this, getString(R.string.google_signed_in_msg), Toast.LENGTH_LONG).show();
        }

        // change button appearance
        if (mGoogleSignInButton != null)
            mGoogleSignInButton.setText(R.string.google_signout);
        setGoogleIsSigningIn(false);

        Runnable notifyTopPageTask = new Runnable() {
            @Override
            public void run() {
                // notify top fragment
                BasePage page = getTopFragmentAsPage();
                if (page != null && !page.isFinishing()) {
                    page.onGoogleSignedIn(true);
                }
            }
        };

        if (sIsFbLoadingProfile)
            mFbLoadingProfileFinishedTasks.add(notifyTopPageTask);
        else
            notifyTopPageTask.run();
    }

    private void onGoogleSignedIn(final GoogleSignInAccount signedInAccount) {

        System.out.println("BaseActivity.onGoogleSignedIn(" + signedInAccount + ")");

        if (signedInAccount != null) {
            // start collecting player's info
            PlayersClient client = Games.getPlayersClient(this, signedInAccount);
            client.getCurrentPlayer().addOnCompleteListener(new OnCompleteListener<Player>() {
                @Override
                public void onComplete(@NonNull Task<Player> task) {
                    if (task.isSuccessful()) {
                        onHasGoogleSignedInPlayerInfo(GoogleSignIn.getLastSignedInAccount(BaseActivity.this), task.getResult());
                    } else {
                        Exception exception = task.getException();
                        if (exception instanceof ApiException) {
                            /* int status = ((ApiException)exception).getStatusCode();
                            if (status == CommonStatusCodes.SIGN_IN_REQUIRED) {
                                startGoogleSigningIn();
                                return;
                            } */
                        }

                        Utils.alertDialog(BaseActivity.this, getString(R.string.generic_err_title), exception.getLocalizedMessage(), new Runnable() {
                            @Override
                            public void run() {
                                onGoogleSignedIn(null);
                            }
                        });
                    }
                }
            });
        } else {
            mGoogleSignedIn = false;

            if (mGoogleMultiplayerClient != null)
                mGoogleMultiplayerClient.unregisterTurnBasedMatchUpdateCallback(mGoogleMultiplayerMatchUpdateCallback);
            mGoogleMultiplayerClient = null;
            mGoogleGamesClient = null;
            mGooglePlayersClient = null;
            mGoogleInvitationsClient = null;
            mGooglePlayerId = null;
            mGoogleAccountId = null;
            mGooglePlayerName = null;

            setGoogleIsSigningIn(false);

            // change button appearance
            if (mGoogleSignInButton != null)
                mGoogleSignInButton.setText(R.string.google_signin);
        }
    }

    public boolean isGoogleSignedIn() {
        return mGoogleSignedIn && GoogleSignIn.getLastSignedInAccount(this) != null;
    }

    private boolean checkGooglePlayServices() {
        try {
            GoogleApiAvailability apiAvailability = GoogleApiAvailability.getInstance();
            int errorCode = apiAvailability.isGooglePlayServicesAvailable(this);
            if (errorCode == ConnectionResult.SUCCESS)
                return true;
            if (apiAvailability.isUserResolvableError(errorCode)) {
                apiAvailability.getErrorDialog(this, errorCode, GOOGLE_SERVICE_AVAIL_ERR_REQUEST_CODE).show();
            }

            return false;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }

    public void startGoogleSigningIn() {
        if (isGoogleSignedIn())
            return;

        System.out.println("BaseActivity.startGoogleSigningIn()");

        // check for play servrices availability
        if (!checkGooglePlayServices())
            return;

        setGoogleIsSigningIn(true);

        GoogleSignInClient signInClient = createGoogleSigninClient();

        Intent signInIntent = signInClient.getSignInIntent();
        startActivityForResult(signInIntent, GOOGLE_SIGN_IN_REQUEST_CODE);
    }

    private void doGoogleSigningOut() {
        GoogleSignInAccount currentAccount = GoogleSignIn.getLastSignedInAccount(this);
        if (currentAccount == null) {
            onGoogleSignedIn(null);
            return;
        }


        System.out.println("BaseActivity.doGoogleSigningOut()");

        setGoogleIsSigningIn(true);

        GoogleSignInClient signInClient = createGoogleSigninClient();
        signInClient.signOut().addOnCompleteListener(new OnCompleteListener<Void>() {
            @Override
            public void onComplete(@NonNull Task<Void> task) {
                onGoogleSignedIn(GoogleSignIn.getLastSignedInAccount(BaseActivity.this));
            }
        });
    }

    public void startGoogleSigningOut() {
        GoogleSignInAccount currentAccount = GoogleSignIn.getLastSignedInAccount(this);
        if (currentAccount == null)
            return;

        System.out.println("BaseActivity.startGoogleSigningOut()");

        Utils.dialog(this,
                getString(R.string.google_signout),
                Utils.getSpannedText(String.format(getString(R.string.google_signout_msg), getGooglePlayerName())),
                null,
                new Runnable() {
                    @Override
                    public void run() {
                        doGoogleSigningOut();
                    }
                },
                new Runnable() {
                    @Override
                    public void run() {
                        if (mGoogleSignInButton != null)
                            setGoogleIsSigningIn(false);
                    }
                }
                );
    }

    public void registerGoogleSigninBtn(Button button) {
        // invalidate old button
        if (mGoogleSignInButton != null)
            mGoogleSignInButton.setOnClickListener(null);

        mGoogleSignInButton = button;
        if (mGoogleSignInButton != null)
        {
            // change appearance
            if (isGoogleSignedIn())
                mGoogleSignInButton.setText(R.string.google_signout);
            else
                mGoogleSignInButton.setText(R.string.google_signin);

            // disable this button if we are still in the process of signing in/out
            mGoogleSignInButton.setEnabled(!mIsGoogleSigningInOut);

            // register on click listener
            mGoogleSignInButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (v != mGoogleSignInButton)
                        return;

                    if (isGoogleSignedIn())
                        startGoogleSigningOut();
                    else
                        startGoogleSigningIn();
                }
            });
        }
    }

    public String getGoogleAccountId() {
        return mGoogleAccountId;
    }

    public String getGooglePlayerId() {
        return mGooglePlayerId;
    }

    public static String tryGetGoogleAccountId() {
        if (sCurrentActivity != null)
        {
            return sCurrentActivity.getGoogleAccountId();
        }

        return null;
    }

    public static String tryGetGooglePlayerId() {
        if (sCurrentActivity != null)
        {
            return sCurrentActivity.getGooglePlayerId();
        }

        return null;
    }

    public String getGooglePlayerName() {
        String name;
        try {
            if ((name = mGooglePlayerName) == null)
                name = getString(R.string.a_google_player);
        } catch (Exception e) {
            e.printStackTrace();

            return "A google player";
        }

        return name;
    }

    public static String tryGetGooglePlayerName() {
        if (sCurrentActivity != null)
        {
            return sCurrentActivity.getGooglePlayerName();
        }
        return null;
    }

    private void onGoogleTurnBasedMatchReceived(@NonNull TurnBasedMatch turnBasedMatch) {
        System.out.println("BaseActivity.onGoogleTurnBasedMatchReceived(matchId=" + turnBasedMatch.getMatchId() + ")");

        ensurePersistentDataAvailable();

        // check for pending unregistration requests
        cleanupUnregisteredGoogleTurnBasedMatchUpdateCallbacks();

        // ---------- forward to registered delegates. ----------
        // Lock to prevent the callback from calling
        // unregister and modifying the registered callbacks set
        mPersistentData.mRegisteredGoogleMatchUpdateCallbacksLocked = true;

        for (TurnBasedMatchUpdateCallback callback: mPersistentData.mRegisteredGoogleMatchUpdateCallbacks) {
            callback.onTurnBasedMatchReceived(turnBasedMatch);
        }

        mPersistentData.mRegisteredGoogleMatchUpdateCallbacksLocked = false;

        // check for pending unregistration requests again
        cleanupUnregisteredGoogleTurnBasedMatchUpdateCallbacks();

    }

    private void onGoogleTurnBasedMatchRemoved(@NonNull String matchId) {
        System.out.println("BaseActivity.onGoogleTurnBasedMatchRemoved(matchId=" + matchId + ")");

        ensurePersistentDataAvailable();

        // check for pending unregistration requests
        cleanupUnregisteredGoogleTurnBasedMatchUpdateCallbacks();

        // ---------- forward to registered delegates. ----------
        // Lock to prevent the callback from calling
        // unregister and modifying the registered callbacks set
        mPersistentData.mRegisteredGoogleMatchUpdateCallbacksLocked = true;

        for (TurnBasedMatchUpdateCallback callback: mPersistentData.mRegisteredGoogleMatchUpdateCallbacks) {
            callback.onTurnBasedMatchRemoved(matchId);
        }

        mPersistentData.mRegisteredGoogleMatchUpdateCallbacksLocked = false;

        // check for pending unregistration requests again
        cleanupUnregisteredGoogleTurnBasedMatchUpdateCallbacks();
    }

    private void cleanupUnregisteredGoogleTurnBasedMatchUpdateCallbacks() {
        // check for pending unregistration requests
        for (TurnBasedMatchUpdateCallback callback: mPersistentData.mPendingUneregisteredGoogleMatchUpdateCallbacks) {
            mPersistentData.mRegisteredGoogleMatchUpdateCallbacks.remove(callback);
        }
        mPersistentData.mPendingUneregisteredGoogleMatchUpdateCallbacks.clear();
    }

    public TurnBasedMultiplayerClient getGoogleMultiplayerClient() {
        if (mGoogleMultiplayerClient == null) {
            GoogleSignInAccount account = GoogleSignIn.getLastSignedInAccount(this);
            if (account == null)
                return null;

            mGoogleMultiplayerClient = Games.getTurnBasedMultiplayerClient(this, account);

            // register match update callback
            mGoogleMultiplayerMatchUpdateCallback = new TurnBasedMatchUpdateCallback() {
                @Override
                public void onTurnBasedMatchReceived(@NonNull TurnBasedMatch turnBasedMatch) {
                    BaseActivity.this.onGoogleTurnBasedMatchReceived(turnBasedMatch);
                }

                @Override
                public void onTurnBasedMatchRemoved(@NonNull String matchId) {
                    BaseActivity.this.onGoogleTurnBasedMatchRemoved(matchId);
                }
            };

            mGoogleMultiplayerClient.registerTurnBasedMatchUpdateCallback(mGoogleMultiplayerMatchUpdateCallback);
        }
        return mGoogleMultiplayerClient;
    }

    public GamesClient getGoogleGamesClient() {
        if (mGoogleGamesClient == null) {
            GoogleSignInAccount account = GoogleSignIn.getLastSignedInAccount(this);
            if (account == null)
                return null;

            mGoogleGamesClient = Games.getGamesClient(this, account);
        }

        return mGoogleGamesClient;
    }

    public PlayersClient getGooglePlayersClient() {
        if (mGooglePlayersClient == null) {
            GoogleSignInAccount account = GoogleSignIn.getLastSignedInAccount(this);
            if (account == null)
                return null;
            mGooglePlayersClient = Games.getPlayersClient(this, account);
        }

        return mGooglePlayersClient;
    }

    public InvitationsClient getGoogleInvitationsClient() {
        if (mGoogleInvitationsClient == null) {
            GoogleSignInAccount account = GoogleSignIn.getLastSignedInAccount(this);
            if (account == null)
                return null;
            mGoogleInvitationsClient = Games.getInvitationsClient(this, account);
        }

        return mGoogleInvitationsClient;
    }

    public void registerGoogleTurnBasedMatchUpdateCallback(final TurnBasedMatchUpdateCallback callback) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {

                ensurePersistentDataAvailable();

                mPersistentData.mRegisteredGoogleMatchUpdateCallbacks.add(callback);
            }
        });
    }

    public void unregisterGoogleTurnBasedMatchUpdateCallback(final TurnBasedMatchUpdateCallback callback) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {

                ensurePersistentDataAvailable();

                if (mPersistentData.mRegisteredGoogleMatchUpdateCallbacks.contains(callback)) {
                    if (callback instanceof GoogleTurnedBaseMatchWaitForMyTurnOnceCallbackAdapter) {
                        GoogleTurnedBaseMatchWaitForMyTurnOnceCallbackAdapter callbackAdapter
                                = (GoogleTurnedBaseMatchWaitForMyTurnOnceCallbackAdapter)callback;
                        System.out.println("BaseActivity.unregisterGoogleTurnBasedMatchUpdateCallback(matchId=" + callbackAdapter.getMatchId() + ")");
                    }

                    if (mPersistentData.mRegisteredGoogleMatchUpdateCallbacksLocked)
                        mPersistentData.mPendingUneregisteredGoogleMatchUpdateCallbacks.add(callback);
                    else
                        mPersistentData.mRegisteredGoogleMatchUpdateCallbacks.remove(callback);
                }
            }
        });
    }

    // wait (asynchronously) until it's current player's turn then invoke callback
    public AsyncOperation waitForGooglePlayGamesTurn(
            final String matchId,
            final GoogleTurnedBaseMatchWaitForMyTurnCallback callback,
            long timeoutMs,
            ProgressDialog progressDialog) {
        final GoogleTurnedBaseMatchWaitForMyTurnOnceCallbackAdapter callbackAdapter
                = new GoogleTurnedBaseMatchWaitForMyTurnOnceCallbackAdapter(callback, matchId, progressDialog);

        registerGoogleTurnBasedMatchUpdateCallback(callbackAdapter);

        if (timeoutMs > 0) {
            Handler handler = new Handler(Looper.getMainLooper());
            handler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    if (!callbackAdapter.isCanceled())
                        callback.onTimeout(BaseActivity.this, matchId);
                    unregisterGoogleTurnBasedMatchUpdateCallback(callbackAdapter);
                }
            }, timeoutMs);
        }

        return callbackAdapter;
    }

    public AsyncOperation waitForGooglePlayGamesTurn(
            String matchId,
            GoogleTurnedBaseMatchWaitForMyTurnCallback callback,
            long timeout) {
        return waitForGooglePlayGamesTurn(matchId, callback, timeout, null);
    }

    public void displayGooglePlayGamesInviteUIWithoutRoom(
            int players,
            boolean allowAutoMatch,
            final GooglePlayGamesMatchCreateCallback<Bundle> delegate) {
        if (!isGoogleSignedIn())
        {
            delegate.onError(this, new Exception(getString(R.string.google_signedout_err_msg)));
            return;
        }

        getGoogleMultiplayerClient()
                .getSelectOpponentsIntent(players, players, allowAutoMatch)
                .addOnSuccessListener(new OnSuccessListener<Intent>() {
                    @Override
                    public void onSuccess(Intent intent) {
                        requestStartActivityForResult(intent,
                                GOOGLE_TURN_BASED_MATCH_INVITE_REQUEST_CODE,
                                new ActivityResultHandler() {
                                    @Override
                                    public void handle(BaseActivity activity, int resultCode, Intent data) {
                                        if (resultCode != Activity.RESULT_OK) {
                                            if (resultCode == Activity.RESULT_CANCELED)
                                                delegate.onCancel(BaseActivity.this);
                                            else
                                                delegate.onActivityResultError(BaseActivity.this, resultCode);
                                            return;
                                        }
                                        delegate.onComplete(BaseActivity.this, data.getExtras());
                                    }
                                });
                    }
                })
                .addOnCanceledListener(new OnCanceledListener() {
                    @Override
                    public void onCanceled() {
                        delegate.onCancel(BaseActivity.this);
                    }
                })
                .addOnFailureListener(new OnFailureListener() {
                    @Override
                    public void onFailure(@NonNull Exception e) {
                        delegate.onError(BaseActivity.this, e);
                    }
                });
    }

    public AsyncOperation autoJoinGooglePlayGames(int minAutoPlayers,
                                        int maxAutoPlayers,
                                        final int autoMatchExclusiveBits,
                                        final GooglePlayGamesMatchCreateCallback<TurnBasedMatch> delegate) {
        if (!isGoogleSignedIn())
        {
            delegate.onError(this, new Exception(getString(R.string.google_signedout_err_msg)));
            return null;
        }

        TurnBasedMatchConfig.Builder builder = TurnBasedMatchConfig.builder();
        if (minAutoPlayers > 0) {
            builder.setAutoMatchCriteria(
                    RoomConfig.createAutoMatchCriteria(minAutoPlayers, maxAutoPlayers, autoMatchExclusiveBits));
        }

        final GooglePlayGamesMatchCreateCallbackAsyncAdapter<TurnBasedMatch> adapter =
                new GooglePlayGamesMatchCreateCallbackAsyncAdapter<>(delegate);

        // create room
        final TurnBasedMultiplayerClient client = getGoogleMultiplayerClient();
        client.createMatch(builder.build()).addOnCompleteListener(new OnCompleteListener<TurnBasedMatch>() {
            @Override
            public void onComplete(@NonNull Task<TurnBasedMatch> task) {
                if (task.isSuccessful())
                    adapter.onComplete(BaseActivity.this, task.getResult());
                else {
                    adapter.onError(BaseActivity.this, task.getException());
                }
            }
        });

        return adapter;
    }

    interface GooglePlayGamesMatchCreateMininalCallback {
        public void onActivityResultError(BaseActivity activity, int resultCode);
        public void onError(BaseActivity activity, Exception task);
        public void onCancel(BaseActivity activity);
    }

    interface GooglePlayGamesMatchCreateCallback<T> extends GooglePlayGamesMatchCreateMininalCallback {
        public void onComplete(BaseActivity activity, T result);
    }

    interface GoogleTurnedBaseMatchWaitForMyTurnCallback {
        void onMyTurn(BaseActivity activity, @NonNull TurnBasedMatch match);
        void onMatchRemoved(BaseActivity activity, @NonNull String matchId);
        void onTimeout(BaseActivity activity, @NonNull String matchId);
    }

    private static class GooglePlayGamesMatchCreateCallbackAsyncAdapter<T> implements GooglePlayGamesMatchCreateCallback<T>, AsyncOperation {

        private final GooglePlayGamesMatchCreateCallback<T> mDelegate;
        private volatile boolean mCanceled = false;

        public GooglePlayGamesMatchCreateCallbackAsyncAdapter(GooglePlayGamesMatchCreateCallback<T> delegate) {
            mDelegate = delegate;
        }

        @Override
        public synchronized void cancel() {
            mCanceled = true;
        }

        @Override
        public synchronized void onActivityResultError(BaseActivity activity, int resultCode) {
            if (mCanceled)
                return;
            mDelegate.onActivityResultError(activity, resultCode);
        }

        @Override
        public synchronized void onError(BaseActivity activity, Exception task) {
            if (mCanceled)
                return;
            mDelegate.onError(activity, task);
        }

        @Override
        public synchronized void onCancel(BaseActivity activity) {
            if (mCanceled)
                return;
            mDelegate.onCancel(activity);
        }

        @Override
        public synchronized void onComplete(BaseActivity activity, T result) {
            if (mCanceled)
                return;
            mDelegate.onComplete(activity, result);
        }
    }

    // this class will wait for turn based match update from Google Play Games
    // then invoke onMyTurn() method if current turn is current player's turn,
    // afterwards it will not listen to turn based match update anymore
    private static class GoogleTurnedBaseMatchWaitForMyTurnOnceCallbackAdapter
            extends TurnBasedMatchUpdateCallback
            implements AsyncOperation
    {
        public GoogleTurnedBaseMatchWaitForMyTurnOnceCallbackAdapter(
                GoogleTurnedBaseMatchWaitForMyTurnCallback callback,
                String matchId,
                ProgressDialog progressDialog) {
            mCallback = callback;
            mMatchId = matchId;
            mProgressDialog = progressDialog;
            if (mProgressDialog != null && !mProgressDialog.isShowing())
                mProgressDialog.show();
        }

        public String getMatchId() {
            return mMatchId;
        }

        public boolean isCanceled() {
            return mCanceled;
        }

        @Override
        public synchronized void cancel() {
            mCanceled = true;

            unregisterSelf();
        }

        @Override
        public synchronized void onTurnBasedMatchReceived(@NonNull TurnBasedMatch match) {
            if (mCanceled) {
                unregisterSelf();
                return;
            }

            System.out.println("GoogleTurnedBaseMatchWaitForMyTurnOnceCallbackAdapter.onTurnBasedMatchReceived(matchId=" + match.getMatchId() + ")");

            if (!mMatchId.equals(match.getMatchId()))
                return;


            if (match.getTurnStatus() == TurnBasedMatch.MATCH_TURN_STATUS_MY_TURN)
            {
                if (mCallback != null)
                    mCallback.onMyTurn(getCurrentActivity(), match);

                // self unregister the callback, we won't listen to the match update events anymore
                unregisterSelf();
            }

            switch (match.getStatus())
            {
                case TurnBasedMatch.MATCH_STATUS_CANCELED:
                case TurnBasedMatch.MATCH_STATUS_COMPLETE:
                case TurnBasedMatch.MATCH_STATUS_EXPIRED:
                    if (mCallback != null)
                        mCallback.onMatchRemoved(getCurrentActivity(), match.getMatchId());

                    // self unregister the callback, we won't listen to the match update events anymore
                    unregisterSelf();
                break;
            }
        }

        @Override
        public synchronized void onTurnBasedMatchRemoved(@NonNull String s) {
            if (mCanceled) {
                unregisterSelf();
                return;
            }
            if (mMatchId.equals(s)) {
                unregisterSelf();

                if (mCallback != null)
                    mCallback.onMatchRemoved(getCurrentActivity(), s);
            }
        }


        private void unregisterSelf() {
            mCanceled = true;

            BaseActivity activity = BaseActivity.getCurrentActivity();
            activity.unregisterGoogleTurnBasedMatchUpdateCallback(this);

            if (mProgressDialog != null) {
                try {
                    mProgressDialog.dismiss();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }

        private final GoogleTurnedBaseMatchWaitForMyTurnCallback mCallback;
        private final String mMatchId;
        private volatile boolean mCanceled = false;
        private final ProgressDialog mProgressDialog;
    }

    private void showGoogleSigningInDialog() {
        showProgressDialog(getString(R.string.google_signing_in), new Runnable() {
            @Override
            public void run() {
                doGoogleSigningOut();
            }
        });
    }

    /*---------- Facebook SDK ---*/
    private void initFacebookSdk() {
        if (!FacebookSdk.isInitialized()) {
            FacebookSdk.sdkInitialize(getApplicationContext());
            AppEventsLogger.activateApp(getApplication());
        }

        mFbCallbackManager = CallbackManager.Factory.create();

        //handle login state changes during runtime
        LoginManager.getInstance().registerCallback(mFbCallbackManager,
                new FacebookCallback<LoginResult>() {
                    @Override
                    public void onSuccess(LoginResult loginResult) {
                        onFbSignedIn(loginResult.getAccessToken() != null);
                    }

                    @Override
                    public void onCancel() {
                        // App code
                    }

                    @Override
                    public void onError(FacebookException exception) {
                        Utils.alertDialog(BaseActivity.this, getString(R.string.generic_err_title), exception.getLocalizedMessage(), null);
                    }
                });

        //handle access token changes
        sAccessTokenTracker = new AccessTokenTracker() {
            @Override
            protected void onCurrentAccessTokenChanged(
                    AccessToken oldAccessToken,
                    AccessToken currentAccessToken) {
                onFbSignedIn(currentAccessToken != null);
            }
        };

        //handle auto login
        onFbSignedIn(AccessToken.getCurrentAccessToken() != null);
    }

    public boolean isFbSignedIn() {
        if (!sFbLoggedIn && AccessToken.getCurrentAccessToken() != null)//this may be caused by auto login, the callback was not fired
            this.onFbSignedIn(true);

        return sFbLoggedIn;
    }

    public void startFbSigningIn() {
        LoginManager.getInstance().logInWithReadPermissions(this, Settings.FACEBOOK_PERMISSIONS);
    }

    private void onFbSignedIn(final boolean signedIn) {
        queueOnResumeTask(new Runnable() {
            @Override
            public void run() {
                if (signedIn == sFbLoggedIn)
                    return;//already updated
                sFbLoggedIn = signedIn;

                System.out.println("BaseActivity.onFbSignedIn(" + signedIn + ")");

                //invoke native function
                onSignedInNative(signedIn);

                if (signedIn) {
                    System.out.println("BaseActivity.onFbSignedIn(" + signedIn + ") loading profile ...");

                    sIsFbLoadingProfile = true;
                    showFbLoadingProfileDialog();

                    //fetch profile data
                    Bundle parameters = new Bundle();
                    parameters.putString("fields", "id,name");

                    sFbProfileRequest = new GraphRequest(
                            AccessToken.getCurrentAccessToken(),
                            "/me",
                            parameters,
                            HttpMethod.GET,
                            new GraphRequest.Callback() {
                                public void onCompleted(GraphResponse response) {
                                    System.out.println("BaseActivity.onFbSignedIn(" + signedIn + ") loading profile completed");

                            /* handle the result */
                                    dismissProgressDialog();
                                    sIsFbLoadingProfile = false;
                                    sFbProfileRequest = null;

                                    if (isFinishing()) {
                                        System.out.println("BaseActivity.onFbSignedIn(" + signedIn + ") loading profile completed and callback was called with finished activity");
                                        return;
                                    }

                                    if (mIsGoogleSigningInOut)
                                        showGoogleSigningInDialog();

                                    if (response.getError() != null) {
                                        sProfileData = null;

                                        Utils.errorDialog(BaseActivity.this, response.getError().getErrorMessage(), new Runnable() {
                                            @Override
                                            public void run() {
                                                runFbProfileLoadingFinishedTasks();
                                            }
                                        });
                                    } else {
                                        try {
                                            sProfileData = response.getJSONObject();

                                            System.out.println("Got facebook profile name = " + sProfileData);

                                            //show welcome back toast
                                            Toast.makeText(BaseActivity.this, getString(R.string.welcome_back) + ", " + sProfileData.getString("name"), Toast.LENGTH_LONG).show();

                                            runFbProfileLoadingFinishedTasks();
                                        } catch (Exception e) {
                                            e.printStackTrace();

                                            Utils.errorDialog(BaseActivity.this, getString(R.string.loading_profile_err), new Runnable() {
                                                @Override
                                                public void run() {
                                                    runFbProfileLoadingFinishedTasks();
                                                }
                                            });

                                            LoginManager.getInstance().logOut();
                                        }
                                    }
                                }
                            }
                    ).executeAsync();


                    System.out.println("BaseActivity.onFbSignedIn(" + signedIn + ") loading profile --> executeAsync() returned ...");

                    //cleanup out-of-date invitations
                    FBUtils.resumePendingInvitationsCleanupTask();
                }//if (signedIn)
                else {
                    sProfileData = null;
                }
            }
        });
    }

    private void runFbProfileLoadingFinishedTasks() {
        // execute scheduled tasks
        for (Runnable task: mFbLoadingProfileFinishedTasks) {
            task.run();
        }
        mFbLoadingProfileFinishedTasks.clear();
    }

    private void cancelFbProfileRequest() {
        System.out.println("BaseActivity.cancelFbProfileRequest()");

        if (sFbProfileRequest != null) {
            dismissProgressDialog();
            sFbProfileRequest.cancel(true);
            sFbProfileRequest = null;
        }
        sIsFbLoadingProfile = false;
    }

    public String getLanPlayerName() {
        String name = null;
        if (isFbSignedIn())
            name = getFbPlayerName();
        if (name == null && isGoogleSignedIn())
            name = getGooglePlayerName();
        if (name == null)
            name = Utils.getHostIPAddress(this);

        return name;
    }

    public static String getFbPlayerName() {
        if (!sFbLoggedIn)
            return null;

        try {
            return sProfileData != null ? sProfileData.getString("name") :
                                          (sCurrentActivity != null ? sCurrentActivity.getString(R.string.a_fb_player) : null);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public static String getFbPlayerId() {
        if (!sFbLoggedIn)
            return null;

        try {
            return sProfileData != null ? sProfileData.getString("id") : null;
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    public CallbackManager getFbCallbackManager() {
        return mFbCallbackManager;
    }

    private void showFbLoadingProfileDialog() {
        showProgressDialog(getString(R.string.loading_fb_profile_msg), new Runnable() {
            @Override
            public void run() {
                LoginManager.getInstance().logOut();
            }
        });
    }

    // ---------------------------------------
    public String getDefaultHostRoomName() {
        String name = getFbPlayerName();
        if (name == null)
            name = getGooglePlayerName();

        if (name != null)
            return name;

        return getString(R.string.a_player);
    }

    public String getDefaultHostGUID() {
        String id = getFbPlayerId();
        if (id == null)
            id = getGoogleAccountId();

        return id;
    }

    // ------------ Wifi Direct ---------------------
    public WifiP2pManager getWifiDirectManager() {
        initWifiDirectIfNeeded();

        return mWifiP2pManager;
    }

    public WifiP2pManager.Channel getWifiDirectChannel() {
        initWifiDirectIfNeeded();

        return mWifiP2pChannel;
    }

    public boolean isWifiDirectEnabled() {
        return mWifiDirectEnabled;
    }

    public synchronized void registerWifiDirectCallback(WifiDirectStateUpdateCallback callback) {
        mWifiP2pCallbacks.add(callback);
    }

    public synchronized void unregisterWifiDirectCallback(WifiDirectStateUpdateCallback callback) {
        mWifiP2pCallbacks.remove(callback);
    }

    private void initWifiDirectIfNeeded() {
        if (mWifiP2pManager == null) {
            mWifiP2pManager = (WifiP2pManager) getSystemService(Context.WIFI_P2P_SERVICE);
            if (mWifiP2pManager == null)
                return;

            initWifiDirectChannel();

            // Indicates a change in the Wi-Fi P2P status.
            mWifiP2pIntentFilter.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION);

            // Indicates a change in the list of available peers.
            mWifiP2pIntentFilter.addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION);

            // Indicates the state of Wi-Fi P2P connectivity has changed.
            mWifiP2pIntentFilter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);

            // Indicates this device's details have changed.
            mWifiP2pIntentFilter.addAction(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION);

            mWifiP2pBroadcaseReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    onReceiveWifiDirectAction(context, intent);
                }
            };

            if (mActive)
                registerReceiver(mWifiP2pBroadcaseReceiver, mWifiP2pIntentFilter);
        }
    }

    private void initWifiDirectChannel() {
        if (mWifiP2pManager == null)
            return;

        mWifiP2pChannel = mWifiP2pManager.initialize(this, Looper.getMainLooper(), new WifiP2pManager.ChannelListener() {
            @Override
            public void onChannelDisconnected() {
                initWifiDirectChannel();
            }
        });
    }

    private void deinitWifiDirect() {
        mWifiP2pManager = null;
        mWifiP2pChannel = null;
    }

    private void onReceiveWifiDirectAction(Context context, Intent intent) {
        if (mWifiP2pManager == null) {
            return;
        }

        String action = intent.getAction();
        switch (action) {
            case WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION: {

                int state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, -1);
                if (state == WifiP2pManager.WIFI_P2P_STATE_ENABLED)
                    mWifiDirectEnabled = true;
                else {
                    mWifiDirectEnabled = false;

                    // notify registered callback
                    iterateWifiDirectCallbacks(new AsyncQuery<WifiDirectStateUpdateCallback>() {
                        @Override
                        public void run(WifiDirectStateUpdateCallback callback) {
                            callback.onDisabled();
                        }
                    });
                }
            }
            break;
            case WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION: {
                final NetworkInfo networkInfo = (NetworkInfo) intent
                        .getParcelableExtra(WifiP2pManager.EXTRA_NETWORK_INFO);

                // notify registered callback
                iterateWifiDirectCallbacks(new AsyncQuery<WifiDirectStateUpdateCallback>() {
                    @Override
                    public void run(WifiDirectStateUpdateCallback callback) {
                        callback.onConnectionStateChanged(networkInfo);
                    }
                });
            }
            break;
        }
    }

    private synchronized void iterateWifiDirectCallbacks(AsyncQuery<WifiDirectStateUpdateCallback> handler) {
        for (WifiDirectStateUpdateCallback registeredCallback: mWifiP2pCallbacks) {
            handler.run(registeredCallback);
        }
    }

    public interface WifiDirectStateUpdateCallback {
        void onDisabled();
        void onConnectionStateChanged(NetworkInfo info);
    }

    // ---------------- billing ---------------
    private void initIAP() {
        // creat iap client
        mBillingClient = BillingClient.newBuilder(this).setListener(this).build();
        mBillingClientState = BillingClientSate.DISCONNECTED;
    }

    private void deinitIAP() {
        mBillingClient.endConnection();
    }

    private void checkIAP() {
        // check whether user has purchased no ads
        queryIAPPurchase(getString(R.string.no_ads_iap_id), null);
    }

    private void onBillingClientReady(final String skuToCheck, final AsyncQuery<Purchase> callback) {
        // check the purchase in cache first
        Purchase.PurchasesResult cachedResult = mBillingClient.queryPurchases(BillingClient.SkuType.INAPP);

        Purchase purchase = null;

        if (cachedResult != null) {
            purchase = findPurchase(cachedResult.getPurchasesList(), skuToCheck);
        }

        // if the purchase is not found in the cache, ignore it for now,
        // as start purchasing again could return "item already owned"
        // if user already bought it elsewhere
        if (callback != null)
            callback.run(purchase);
    }

    private Purchase findPurchase(List<Purchase> purchases, String sku) {
        if (purchases == null)
        {
            return null;
        }

        for (Purchase purchase: purchases) {
            if (purchase.getSku().equals(sku)) {
                if (handlePurchase(purchase))
                    return purchase;
            }
        }

        return null;
    }

    // return false if purchase is invalid
    private boolean handlePurchase(Purchase purchase) {
        // secury check
        if (!Security.verifyPurchase(purchase.getOriginalJson(), purchase.getSignature()))
            return false;

        // update ads removal status
        if (purchase.getSku().equals(getString(R.string.no_ads_iap_id)))
            Settings.enableAds(false);

        return true;
    }

    private void getIAPClient(@NonNull final AsyncQuery<Integer> clientReadyCallback) {
        if (mBillingClient.isReady() && mBillingClientState == BillingClientSate.CONNECTED) {
            clientReadyCallback.run(BillingClient.BillingResponse.OK);
        } else {
            // queue the callback to be executed when client is ready
            mBillingClientReadyTasks.add(clientReadyCallback);

            if (mBillingClientState != BillingClientSate.CONNECTING) {

                // avoid multiple startConnection() calls
                mBillingClientState = BillingClientSate.CONNECTING;

                mBillingClient.startConnection(new BillingClientStateListener() {
                    @Override
                    public void onBillingSetupFinished(int responseCode) {
                        System.out.println("BaseActivity.onBillingSetupFinished(" + responseCode + ")");

                        if (responseCode == BillingClient.BillingResponse.OK)
                            mBillingClientState = BillingClientSate.CONNECTED;
                        else
                            mBillingClientState = BillingClientSate.DISCONNECTED;

                        // execute pending callbacks
                        for (AsyncQuery<Integer> callback: mBillingClientReadyTasks)
                            callback.run(responseCode);
                        mBillingClientReadyTasks.clear();
                    }

                    @Override
                    public void onBillingServiceDisconnected() {
                        System.out.println("BaseActivity.onBillingServiceDisconnected()");

                        mBillingClientState = BillingClientSate.DISCONNECTED;
                    }
                });
            }
        }
    }

    public synchronized void registerIAPPurchaseUpdatedCallback(PurchasesUpdatedListener callback) {
        mPurchaseCallbacks.add(callback);
    }

    public synchronized void unregisterIAPPurchaseUpdatedCallback(PurchasesUpdatedListener callback) {
        mPurchaseCallbacks.remove(callback);
    }

    public void queryIAPPurchase(final String skuToCheck, final AsyncQuery<Purchase> callback) {
        getIAPClient(new AsyncQuery<Integer>() {
            @Override
            public void run(Integer responseCode) {
                if (responseCode.equals(BillingClient.BillingResponse.OK)) {
                    onBillingClientReady(skuToCheck, callback);
                } else {
                    if (callback != null)
                        callback.run(null);
                }
            }
        });
    }

    public void startIAPPurchaseFlow(final String sku, final AsyncQuery<Integer> callback) {
        getIAPClient(new AsyncQuery<Integer>() {
            @Override
            public void run(Integer responseCode) {
                if (responseCode.equals(BillingClient.BillingResponse.OK)) {
                    BillingFlowParams flowParams = BillingFlowParams.newBuilder()
                            .setSku(sku)
                            .setType(BillingClient.SkuType.INAPP)
                            .build();
                    int re = mBillingClient.launchBillingFlow(BaseActivity.this, flowParams);

                    if (callback != null)
                        callback.run(re);
                } else {
                    if (callback != null)
                        callback.run(responseCode);
                }
            }
        });
    }

    /**
     * Implement this method to get notifications for purchases updates. Both purchases initiated by
     * your app and the ones initiated by Play Store will be reported here.
     *
     * @param responseCode Response code of the update.
     * @param purchases    List of updated purchases if present.
     */
    @Override
    public void onPurchasesUpdated(int responseCode, @Nullable List<Purchase> purchases) {
        LinkedList<Purchase> filteredPurchases = new LinkedList<>();
        if (responseCode == BillingClient.BillingResponse.OK
                && purchases != null) {
            for (Purchase purchase: purchases) {
                if (handlePurchase(purchase))
                    filteredPurchases.add(purchase);
            }
        }

        synchronized (this) {
            for (PurchasesUpdatedListener callback: mPurchaseCallbacks) {
                callback.onPurchasesUpdated(responseCode, filteredPurchases);
            }
        }
    }

    // ---------------------------------------

    // game rom file verification
    public static synchronized boolean verifyGame(Context ctx, String path) {
        return GameSurfaceView.verifyGame(ctx, path);
    }

    // ------------lan server discovery related -----------------
    public static synchronized void enableLANServerDiscovery(Context context, boolean enable) {
        GameSurfaceView.enableLANServerDiscovery(context, enable);
    }

    public static synchronized void findLANServers(Context context, long request_id) {
        GameSurfaceView.findLANServers(context, request_id);
    }

    private static synchronized void onLanServerDiscovered(long request_id, String address, int port, String description) {
        for (LanServerDiscoveryCallback callback: sLanDiscoveryCallbacks) {
            callback.onLanServerDiscovered(request_id, address, port, description);
        }
    }

    public static synchronized void registerLanServerDiscoveryCallback(LanServerDiscoveryCallback callback) {
        sLanDiscoveryCallbacks.add(callback);
    }

    public static synchronized void unregisterLanServerDiscoveryCallback(LanServerDiscoveryCallback callback) {
        sLanDiscoveryCallbacks.remove(callback);
    }

    static interface LanServerDiscoveryCallback {
        public void onLanServerDiscovered(long request_id, String address, int port, String description);
    }

    /*---------- player's stats ----*/
    public static Long getTotalPlayTimeMsAtTheStart() {
        return sTotalPlayTimeMsAtTheStart;
    }

    public static Long getTotalMultiPlayerTimeMsAtTheStart() {
        return sTotalMultiPlayerTimeMsAtTheStart;
    }

    //read player's stats once
    private void readStatsOnce() {
        final SharedPreferences preferences = getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
        if (preferences == null)
        {
            return;
        }

        if (sTotalPlayTimeMsAtTheStart == null)
            sTotalPlayTimeMsAtTheStart = preferences.getLong(Settings.USER_TOTAL_PLAY_TIME_KEY, 0);
        if (sTotalMultiPlayerTimeMsAtTheStart == null)
            sTotalMultiPlayerTimeMsAtTheStart = preferences.getLong(Settings.USER_TOTAL_MULTIPLAYER_TIME_KEY, 0);
    }

    /*--- implements GameChatDialog.Delegate ---*/
    // this is just a wrapper, GameChatDialog can only retrieve current activity but not fragment,
    // hence better making activity implement the GameChatDialog.Delegate interface.
    // Inside implementation, just forward to current active GameChatDialog.Delegate object.
    @Override
    public GameChatDialog.ChatMessagesDatabaseViewAdapter getChatDatabaseViewAdapter() {
        synchronized (this) {
            if (mChatDialogDelegate != null)
                return mChatDialogDelegate.getChatDatabaseViewAdapter();

            return null;
        }
    }

    @Override
    public void onSendButtonClicked(String message) {
        synchronized (this) {
            if (mChatDialogDelegate != null)
                mChatDialogDelegate.onSendButtonClicked(message);
        }
    }

    public void setChatDialogDelegate(GameChatDialog.Delegate delete) {
        synchronized (this) {
            mChatDialogDelegate = delete;
        }
    }

    //

    // ------------ onActivityResult delegates -------------
    public static interface ActivityResultHandler {
        public void handle(BaseActivity activity, int resultCode, Intent data);
    }

    private static class ActivityResultHandlerGroup extends HashMap<ActivityResultHandler, Boolean> {
    }

    private static class ActivityResultHandlerMap  {
        public void register(int requestCode, boolean oneTime, ActivityResultHandler handler) {
            ActivityResultHandlerGroup requestCodeGroup = mDelegateMap.get(requestCode);
            if (requestCodeGroup == null) {
                requestCodeGroup = new ActivityResultHandlerGroup();
                mDelegateMap.put(requestCode, requestCodeGroup);
            }

            requestCodeGroup.put(handler, oneTime);
        }

        public void uregister(int requestCode, ActivityResultHandler handler) {
            ActivityResultHandlerGroup requestCodeGroup = mDelegateMap.get(requestCode);
            if (requestCodeGroup == null) {
                return;
            }

            requestCodeGroup.remove(handler);
        }

        public void handle(BaseActivity activity, int requestCode, int resultCode, Intent data ){
            ActivityResultHandlerGroup requestCodeGroup = mDelegateMap.get(requestCode);
            if (requestCodeGroup == null) {
                return;
            }

            LinkedList<ActivityResultHandler> handlersToRemove = new LinkedList<>();

            for (ActivityResultHandlerGroup.Entry<ActivityResultHandler, Boolean> entry : requestCodeGroup.entrySet()) {
                if (entry.getKey() != null) {
                    entry.getKey().handle(activity, resultCode, data);

                    if (entry.getValue() == true) // one time only
                        handlersToRemove.add(entry.getKey());
                }
            }

            // now remove all one time handler
            for (ActivityResultHandler handler: handlersToRemove) {
                requestCodeGroup.remove(handler);
            }
        }

        private HashMap<Integer, ActivityResultHandlerGroup> mDelegateMap = new HashMap<>();
    }

    // ------------ ViewModel -------------
    public static class PersistentData extends ViewModel {
        public ActivityResultHandlerMap mOnActivityResultDelegates = new ActivityResultHandlerMap();

        public HashSet<TurnBasedMatchUpdateCallback> mRegisteredGoogleMatchUpdateCallbacks = new HashSet<>();
        public HashSet<TurnBasedMatchUpdateCallback> mPendingUneregisteredGoogleMatchUpdateCallbacks = new HashSet<>();
        public volatile boolean mRegisteredGoogleMatchUpdateCallbacksLocked = false;
    }

    //native
    private native void cacheJVMNative();
    private native void onSignedInNative(boolean signedIn);
    private native void onCreatedNative(Bundle savedInstanceState);
    private native void onDestroyedNative();
    private native void onResumedNative();
    private native void onPausedNative();
    private native void onStartedNative();
    private native void onStoppedNative();
    private native void onSaveInstanceStateNative(Bundle state);
    private native void onActivityResultNative(int requestCode, int resultCode, Intent data);

    private static native void invokeNativeFunction(long nativeFunctionId, long nativeFunctionArg);

    static {
        //System.loadLibrary("gnustl_shared");
        System.loadLibrary("c++_shared");
        System.loadLibrary("RemoteController");
        System.loadLibrary("nes");
    }
}
