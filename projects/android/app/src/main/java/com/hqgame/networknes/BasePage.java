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
import android.arch.lifecycle.LiveData;
import android.arch.lifecycle.MutableLiveData;
import android.arch.lifecycle.ViewModel;
import android.arch.lifecycle.ViewModelProviders;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v7.widget.Toolbar;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;

import com.google.android.gms.games.GamesClient;
import com.google.android.gms.games.TurnBasedMultiplayerClient;
import com.google.android.gms.games.multiplayer.Multiplayer;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatch;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;

import java.util.LinkedList;

/**
 * A simple {@link Fragment} subclass.
 */
public class BasePage extends Fragment {
  public static final int RESULT_OK = 1;
  public static final int RESULT_CANCELED = 0;

  private static final String REQUEST_CODE_KEY = "requestCode";
  private static final String OPTIONS_KEY = "options";

  private BaseViewModel mModel = null;

  private boolean mFinishing = false;
  private boolean mActive = false;
  private LinkedList<Runnable> mOnResumeTasks = new LinkedList<>();

  private String mLabel = null; // label to be displayed on default toolbar
  private boolean mProgressBarVisble = false;

  private FrameLayout mDefaultRootView = null ;// this is the full view containing default toolbar and the main view below it
  private ViewGroup mDefaultContainerLayout = null; // the container that need to be recreated duration rotation
  private View mDefaultMainView = null; // the content view below default toolbar

  // Google match handler for client
  private GooglePlayGamesMatchHandler.Client matchClientHandler = new GooglePlayGamesMatchHandler.Client();

  public static <T extends BasePage> BasePage create(Class<T> class_) {
    try {
      BasePage page = class_.newInstance();
      Bundle args = new Bundle();

      page.setArguments(args);

      return page;
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  public static <T extends BasePage> BasePage create(Class<T> class_, int requestCode) {
    return create(class_, requestCode, null);
  }

  public static <T extends BasePage> BasePage create(Class<T> class_, int requestCode, Bundle extras) {
    return create(class_, (Integer) requestCode, extras);
  }

  public static <T extends BasePage> BasePage create(Class<T> class_, Bundle extras) {
    return create(class_, null, extras);
  }

  public static <T extends BasePage> BasePage create(Class<T> class_, Integer requestCode, Bundle extras) {
    try {
      BasePage page = class_.newInstance();

      Bundle args = new Bundle();

      if (requestCode != null)
        args.putInt(REQUEST_CODE_KEY, requestCode);

      if (extras != null)
        args.putBundle(OPTIONS_KEY, extras);

      page.setArguments(args);

      return page;
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    System.out.println("BasePage.onCreate()");

    super.onCreate(savedInstanceState);
    mFinishing = false;
  }

  // subclass can override this method to create view without default toolbar
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    System.out.println("BasePage.onCreateView()");

    mDefaultMainView = onCreateViewWithDefaultToolbar(inflater, null, savedInstanceState);
    if (mDefaultMainView == null)
      return null;

    mDefaultRootView = new FrameLayout(getContext());

    createConfigDependentViewContainer(inflater, container);

    if (mDefaultContainerLayout == null)
      return null;

    // add all of them to the root view
    mDefaultRootView.addView(mDefaultContainerLayout);

    return mDefaultRootView;
  }

  /**
   * override this method to create a child view to be shown just below default toolbar
   *
   */
  protected View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    return null;
  }

  @Override
  public void onConfigurationChanged(Configuration newConfig) {
    super.onConfigurationChanged(newConfig);

    // this instance doesn't use default layout with toolbar, ignore
    if (mDefaultRootView == null)
      return;

    mDefaultRootView.removeAllViews();

    // re-create layout
    LayoutInflater inflater = getLayoutInflater();
    createConfigDependentViewContainer(inflater, null);

    mDefaultRootView.addView(mDefaultContainerLayout);

    // re apply toolbar style
    setDefaultToolbarStyle();

    // reset label
    setLabel(mLabel);

    // reset progress bar visible
    doSetProgressBarVisible(mProgressBarVisble);
  }

  @Override
  public void onActivityCreated(Bundle savedInstanceState) {
    System.out.println("BasePage.onActivityCreated()");

    super.onActivityCreated(savedInstanceState);

    // base toolbar style
    setDefaultToolbarStyle();

    // retrieve view model
    mModel = ViewModelProviders.of(this).get(BaseViewModel.class);
  }

  @Override
  public void onResume() {
    System.out.println("BasePage.onResume()");

    super.onResume();

    // set this page to active
    mActive = true;

    // set label
    setLabel(mLabel);

    // set progress bar visible
    doSetProgressBarVisible(mProgressBarVisble);

    /*-- run scheduled onresume tasks ---*/
    for (Runnable task: mOnResumeTasks){
      task.run();
    }

    mOnResumeTasks.clear();
  }

  @Override
  public void onPause() {
    System.out.println("BasePage.onPause()");

    super.onPause();

    mActive = false;
  }

  @Override
  public void onDestroyView() {
    super.onDestroyView();

    mDefaultMainView = null;
    mDefaultContainerLayout = null;
    mDefaultRootView = null;
  }

  public BaseActivity getBaseActivity() {
    return BaseActivity.getCurrentActivity();
  }

  private void setDefaultToolbarStyle() {
    if (mDefaultRootView != null) {
      // use embedded toolbar if it exists in view's layout
      Toolbar toolbar = mDefaultRootView.findViewById(R.id.default_toolbar_id);
      if (toolbar != null) {
        getBaseActivity().setSupportActionBar(toolbar);
        getBaseActivity().getSupportActionBar().setDisplayShowTitleEnabled(true);
        getBaseActivity().getSupportActionBar().setDisplayShowHomeEnabled(true);
        getBaseActivity().getSupportActionBar().setDisplayUseLogoEnabled(true);
        getBaseActivity().getSupportActionBar().setIcon(R.mipmap.ic_launcher);
      }
    }
  }

  private void createConfigDependentViewContainer(LayoutInflater inflater, ViewGroup container) {
    if (mDefaultMainView == null)
      return; // this instance doesn't use default toolbar layout, so ignore

    // cleanup old created container
    if (mDefaultContainerLayout != null)
      mDefaultContainerLayout.removeAllViews();

    // ------------ create new container ----------*/
    // use default container with toolbar on top
    LinearLayout layout = (LinearLayout) inflater.inflate(R.layout.page_default_container, container, false);

    // add child view just below the toolbar
    layout.addView(mDefaultMainView,  new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1.f));

    mDefaultContainerLayout = layout;
  }

  // set label to be displayed on default toolbar
  protected void setLabel(String label) {
    if (mActive && getBaseActivity() != null && getBaseActivity().getSupportActionBar() != null)
    {
      if (label != null)
        getBaseActivity().getSupportActionBar().setTitle(label);
      else
        getBaseActivity().getSupportActionBar().setTitle("");
    }

    mLabel = label;
  }

  protected void setProgressBarVisible(boolean visible) {
    doSetProgressBarVisible(visible);

    mProgressBarVisble = visible;
  }

  private void doSetProgressBarVisible(boolean visible) {
    if (mActive && getBaseActivity() != null && getView() != null && getBaseActivity().getSupportActionBar() != null) {
      ProgressBar bar = (ProgressBar)getView().findViewById(R.id.default_toolbar_progress_bar_id);
      if (bar != null) {
        if (visible) {
          bar.setVisibility(View.VISIBLE);
          bar.setIndeterminate(true);
        } else {
          bar.setVisibility(View.GONE);
        }
      }
    }
  }

  // called when the page is about to be navigated to
  protected void onNavigatingTo(Activity activity) {
    // subclass should implement
    // default orientation
    activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
  }

  /* ---- emulates Activity's methods ------*/
  public void onBackPressed() {
    //sub-class should implement if it want to overide default back key pressed event
    finish();
  }

  public void finish() {
    mFinishing = true;
    BaseActivity activity = getBaseActivity();

    FragmentManager fm = activity.getSupportFragmentManager();
    int pagesCount = fm.getBackStackEntryCount();
    if (pagesCount <= 1) {
      activity.finish();
      return;
    }

    BasePage top = activity.getTopFragmentAsPage();
    BasePage _2ndTop = activity.get2ndTopFragmentAsPage();

    // pop current page
    if (top == null) {
      activity.finish();
      return;
    }

    Integer requestCode = top.getRequestCode();
    int resultCode = top.getResult();
    Bundle data = top.getResultData();

    // notify next page
    if (_2ndTop != null)
      _2ndTop.onNavigatingTo(activity);

    if (!fm.popBackStackImmediate()) {
      activity.finish();
      return;
    }

    // notify new top page about the result of previous top page
    // need to obtain the page instance again since Android may recreate a new instance of Fragment
    _2ndTop = activity.getTopFragmentAsPage();
    if (requestCode != null && _2ndTop != null) {
      _2ndTop.onPageResult(requestCode, resultCode, data);
    }
  }

  public boolean isFinishing() {
    return mFinishing;
  }

  public boolean dispatchKeyEvent(KeyEvent event) {
    //sub-class should implement if it want to overide default event handler
    return false;
  }

  public boolean dispatchGenericMotionEvent(MotionEvent event) {
    //sub-class should implement if it want to overide default event handler
    return false;
  }

  protected void onPageResult(int requestCode, int resultCode, Bundle data) {
    //sub-class should implement
  }

  public void onRequestPermissionsResult(int requestCode, String permissions[], int[] grantResults) {
    //sub-class should implement
  }

  public void runOnUiThread(Runnable r) {
    Activity activity = BaseActivity.getCurrentActivity();
    if (activity != null)
      activity.runOnUiThread(r);
  }

  public void goToPage(BasePage page) {
    page.onNavigatingTo(getActivity());

    getBaseActivity().goToNextFragment(page);
  }

  public @Nullable Integer getRequestCode() {
    if (getArguments() == null)
      return null;
    if (getArguments().containsKey(REQUEST_CODE_KEY))
      return getArguments().getInt(REQUEST_CODE_KEY);
    return null;
  }
  public Bundle getExtras() {
    if (getArguments() == null)
      return null;
    if (getArguments().containsKey(OPTIONS_KEY))
      return getArguments().getBundle(OPTIONS_KEY);
    return null;
  }

  public void setExtras(Bundle extras) {
    Bundle args = getArguments();
    if (args == null)
    {
      args = new Bundle();
    }

    args.putBundle(OPTIONS_KEY, extras);

    setArguments(args);
  }

  public int getResult() {
    return mModel.getResultCode();
  }

  public @Nullable Bundle getResultData() {
    return mModel.getResultData();
  }

  public void setResult(int resultCode) {
    mModel.setResultCode(resultCode);
  }

  public void setResult(int resultCode, Bundle data) {
    mModel.setResultCode(resultCode);
    mModel.setResultData(data);
  }

  /* ----- task queue to be executed during onResume() ----*/
  protected void queueOnResumeTask(Runnable task) {
    if (mActive)
      task.run();
    else
      mOnResumeTasks.add(task);
  }

  /* ----- signin status ------*/
  public boolean isFbSignedIn() {
    return getBaseActivity().isFbSignedIn();
  }

  public boolean isGoogleSignedIn() {
    return getBaseActivity().isGoogleSignedIn();
  }

  public void onGoogleSignedIn(final boolean signedIn) {
    queueOnResumeTask(new Runnable() {
      @Override
      public void run() {
        handleOnGoogleSignedIn(signedIn);
      }
    });
  }

  private void handleOnGoogleSignedIn(boolean signedIn) {
    if (signedIn) {
      // check if user has accepted any invitation on notification bar
      GamesClient gamesClient = getBaseActivity().getGoogleGamesClient();
      final TurnBasedMultiplayerClient multiplayerClient = getBaseActivity().getGoogleMultiplayerClient();

      if (gamesClient != null && multiplayerClient != null) {
        gamesClient.getActivationHint().addOnCompleteListener(new OnCompleteListener<Bundle>() {
          @Override
          public void onComplete(@NonNull Task<Bundle> task) {
            if (task.isSuccessful()) {
              if (task.getResult() != null) {
                Bundle data = task.getResult();
                TurnBasedMatch match = data.getParcelable(Multiplayer.EXTRA_TURN_BASED_MATCH);
                if (match != null) {
                  onGoogleMatchAccepted(multiplayerClient, match);
                }
              }
            } // if (task.isSuccessful())
          }
        });
      } // if (gamesClient != null && multiplayerClient != null)
    }
  }

  protected void onGoogleMatchAccepted(@NonNull TurnBasedMatch match) {
    TurnBasedMultiplayerClient client = getBaseActivity().getGoogleMultiplayerClient();
    if (client == null) {
      Utils.errorDialog(R.string.google_signedout_err_msg, null);

      return;
    }

    onGoogleMatchAccepted(client, match);
  }

  protected void onGoogleMatchAccepted(@NonNull TurnBasedMultiplayerClient gclient, @NonNull TurnBasedMatch match) {
    // stop old handler
    matchClientHandler.stop();

    // subclass can override
    onJoiningGoogleMatchBegin(match.getMatchId());

    // accept invitation
    matchClientHandler.start(getBaseActivity(), match, new GooglePlayGamesMatchHandler.Client.ClientCallback() {
        @Override
        public void onComplete(GooglePlayGamesMatchHandler.Client handler, BaseActivity activity, @NonNull String matchId, @NonNull String inviteData) {
          onJoiningGoogleMatchFinish(matchId, inviteData);
        }

        @Override
        public void onMatchInvalid(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId) {
          onJoiningGoogleMatchError(matchId, R.string.google_match_invalid);
        }

        @Override
        public void onFatalError(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId, Exception exception) {
          onJoiningGoogleMatchError(matchId, exception.getLocalizedMessage());
        }

        @Override
        public void onTimeout(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId) {
          onJoiningGoogleMatchError(matchId, R.string.google_match_not_respond_msg);
        }

        @Override
        public void onMatchRemoved(GooglePlayGamesMatchHandler handler, BaseActivity activity, @NonNull String matchId) {
          onJoiningGoogleMatchError(matchId, R.string.google_match_canceled_msg);
        }
      });
  }

  protected void onJoiningGoogleMatchBegin(String matchId) {
    BaseActivity activity = getBaseActivity();

    // display progress
    activity.showProgressDialog(getString(R.string.connecting_to_host), new Runnable() {
      @Override
      public void run() {
        matchClientHandler.stop();
      }
    });
  }

  protected void onJoiningGoogleMatchFinish(String matchId, String inviteData) {
    BaseActivity activity = getBaseActivity();

    activity.dismissProgressDialog();

    matchClientHandler.detach();

    // go to GamePage
    final Bundle settings = new Bundle();

    settings.putSerializable(Settings.GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY, Settings.RemoteControl.JOIN_INTERNET_REMOTE_CONTROL_GOOGLE);

    settings.putString(Settings.GAME_ACTIVITY_REMOTE_GOOGLE_MATCH_ID, matchId);
    settings.putString(Settings.GAME_ACTIVITY_REMOTE_INVITATION_DATA_KEY, inviteData);

    BasePage intent = BasePage.create(GamePage.class);
    intent.setExtras(settings);

    goToPage(intent);
  }

  protected void onJoiningGoogleMatchError(String matchId, int errorMessageResId) {
    onJoiningGoogleMatchError(matchId, getString(errorMessageResId));
  }

  protected void onJoiningGoogleMatchError(String matchId, String errorMessage) {
    displayGoogleMatchJoiningErrorDialog(matchId, errorMessage, null);
  }

  protected void displayGoogleMatchJoiningErrorDialog(String matchId, String errorMessage, final Runnable errorDialogCloseCallback) {
    BaseActivity activity = getBaseActivity();

    activity.dismissProgressDialog();
    Utils.errorDialog(activity, errorMessage, errorDialogCloseCallback);
  }

  protected void cancelExistGoogleMatchJoiningAttempt() {
    matchClientHandler.stop();
  }

  /* ------ View Model -----------*/
  public static class BaseViewModel extends ViewModel {
    private MutableLiveData<Integer> resultCode = null;
    private MutableLiveData<Bundle> resultData = null;

    private LiveData<Integer> createOrGetResultCode() {
      if (resultCode == null) {
        resultCode = new MutableLiveData<>();
        resultCode.setValue(BasePage.RESULT_CANCELED);
      }
      return resultCode;
    }

    public int getResultCode() {
      createOrGetResultCode();

      return resultCode.getValue();
    }

    public void setResultCode(int code) {
      createOrGetResultCode();
      resultCode.setValue(code);
    }

    public Bundle getResultData() {
      if (resultData == null)
        return null;
      return resultData.getValue();
    }

    public void setResultData(Bundle data) {
      if (resultData == null) {
        resultData = new MutableLiveData<>();
      }
      resultData.setValue(data);
    }
  }

}
