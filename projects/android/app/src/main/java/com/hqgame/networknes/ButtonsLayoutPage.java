package com.hqgame.networknes;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.view.MotionEventCompat;
import android.support.v7.widget.Toolbar;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

public class ButtonsLayoutPage extends BasePage implements View.OnTouchListener {
    public static final String ORIENTATION_KEY = "ORIENTATION_KEY";

    // normal buttons plus dpad
    private static final int NUM_BUTTONS = Settings.Button.NORMAL_BUTTONS + 1;

    private FrameLayout mRootView = null;
    private ViewGroup mGameViewFrameContainer = null;
    private FrameLayout mGameViewFrame = null;
    private GameSurfaceView mGameView = null;

    private boolean mGameViewReady = false;
    private boolean mForPortrait = false;

    private int mNumDownPointers = 0;
    private int m1stPointerDownId = -1;
    private int m2ndPointerDownId = -1;
    private float m1stPointerDownX, m1stPointerDownY;
    private float m2ndPointerDownX, m2ndPointerDownY;
    private double mTwoPointersDownDistance;
    private Rect mScaleInitialRect = new Rect();
    private float m1stPointerDownRelX, m1stPointerDownRelY; // position relative to current editing button's rect
    private ButtonBoudingView mCurrentTouchedBoundingView = null;

    private ButtonBoudingView[] mButtonsBoundingViews = new ButtonBoudingView[NUM_BUTTONS];

    @Override
    protected void onNavigatingTo(Activity activity) {
        System.out.println("ButtonsLayoutPage.onNavigatingTo()");

        //lock screen orientation
        int request_orientation = getExtras().getInt(ORIENTATION_KEY);

        switch (request_orientation) {
            case ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT:
            case ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT:
                mForPortrait = true;
                break;
            case ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE:
            case ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE:
                mForPortrait = false;
                break;
            default:
                throw new RuntimeException("unsupported orientation");
        }

        activity.setRequestedOrientation(request_orientation);

        // immersive mode
        Utils.hideSystemUI(activity);
    }

    @Override
    public void onCreate (Bundle savedInstanceState) {
        System.out.println("ButtonsLayoutPage.onCreate()");

        super.onCreate(savedInstanceState);

        setHasOptionsMenu(true);

        // initialize touch interceptor array
        for (int i = 0; i < mButtonsBoundingViews.length; ++i) {
            mButtonsBoundingViews[i] = null;
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mRootView = new FrameLayout(getContext());

        mRootView.addView(recreateContentView(inflater, mRootView));

        return mRootView;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        System.out.println("ButtonsLayoutPage.onWindowFocusChanged(" + hasFocus + ")");

        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            Utils.hideSystemUI(getActivity());
        }
    }

    private View recreateContentView(LayoutInflater inflater, ViewGroup container) {
        if (mGameViewFrame == null) {
            // use game view frame to hold the game view and buttons' bounding boxes
            mGameViewFrame = new FrameLayout(getContext());

            // use game view to draw buttons only, make it child of mGameViewFrame
            mGameView = new GameSurfaceView(
                    getContext(),
                    null,
                    Utils.convertDpToPixel(1, getContext()),
                    false,
                    new Runnable() {
                        @Override
                        public void run() {
                            onGameSurfaceViewReady();
                        }
                    });
            mGameViewFrame.addView(mGameView);
        }
        else if (mGameViewFrameContainer != null)
            mGameViewFrameContainer.removeAllViews(); // remove the game frame from the old container

        ViewGroup contentView = (ViewGroup) inflater.inflate(R.layout.page_game, container, false);

        mGameViewFrameContainer = (ViewGroup) contentView.findViewById(R.id.game_content_frame);
        Toolbar toolbar = (Toolbar) contentView.findViewById(R.id.game_toolbar);

        if (mGameViewFrameContainer instanceof RelativeLayout)//this should be landscape layout
        {
            // re-insert our frame
            mGameViewFrameContainer.addView(mGameViewFrame, new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));

            // need to bring toolbar to front since our frame is added after it
            toolbar.bringToFront();

            ImageButton inviteBnt = (ImageButton) contentView.findViewById(R.id.btnInviteFriendIngame);
            if (inviteBnt != null) {
                inviteBnt.bringToFront();
            }
        }
        else {
            // re-insert the frame to fill the remaining space besides the toolbar
            mGameViewFrameContainer.addView(mGameViewFrame, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1.f));
        }

        // before lolipop this will completely hide the GL game view
        // without any transparency blending, so avoid it
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // add help text overlay on top of GL surface
            mGameViewFrame.addView(inflater.inflate(R.layout.button_editor_help_overlay, null, false));
        }
        // install on touch listener
        mGameViewFrame.setOnTouchListener(this);

        //set the Toolbar to act as the ActionBar for this Activity window.
        getBaseActivity().setSupportActionBar(toolbar);

        //disable actionbar's title, icon and home
        getBaseActivity().getSupportActionBar().setDisplayShowTitleEnabled(false);
        getBaseActivity().getSupportActionBar().setDisplayUseLogoEnabled(false);
        getBaseActivity().getSupportActionBar().setDisplayShowHomeEnabled(false);

        return contentView;
    }

    @Override
    public void onConfigurationChanged (Configuration newConfig) {
        System.out.println("GamePage.onConfigurationChanged()");

        super.onConfigurationChanged(newConfig);

        if (mRootView != null)
            mRootView.removeAllViews();
        else {
            mRootView = new FrameLayout(getContext());
        }

        mRootView.addView(recreateContentView(getLayoutInflater(), mRootView));
    }

    @Override
    public void onResume() {
        super.onResume();

        mGameView.onResume();
    }

    @Override
    public void onPause() {
        super.onPause();

        mGameView.onPause();

        //save settings
        Settings.saveGlobalSettings(getContext());
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        System.out.println("ButtonsLayoutPage.onCreateOptionsMenu()");

        inflater.inflate(R.menu.menu_edit_buttons, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        switch (id)
        {
            case R.id.action_reset_buttons_layout:
                resetButtonsLayout();
                return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public  boolean onTouch(View view, MotionEvent event) {

        int index = MotionEventCompat.getActionIndex(event);
        int id = MotionEventCompat.getPointerId(event, index);
        float x = MotionEventCompat.getX(event, index);
        float y = MotionEventCompat.getY(event, index);

        final int action = MotionEventCompat.getActionMasked(event);

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                if (mNumDownPointers == 0) {
                    m1stPointerDownId = id;

                    m1stPointerDownX = x;
                    m1stPointerDownY = y;

                    for (ButtonBoudingView boudingView: mButtonsBoundingViews) {
                        if (boudingView != null && boudingView.rect.contains((int)x, (int)y)) {
                            mCurrentTouchedBoundingView = boudingView;

                            m1stPointerDownRelX = x - mCurrentTouchedBoundingView.rect.left;
                            m1stPointerDownRelY = y - mCurrentTouchedBoundingView.rect.top;

                            break;
                        }
                    }
                } else if (mNumDownPointers >= 1 && m2ndPointerDownId == -1 && mCurrentTouchedBoundingView != null) {
                    double distance = Utils.distance(m1stPointerDownX, m1stPointerDownY, x, y);

                    if (distance >= 1.f) {
                        m2ndPointerDownId = id;
                        m2ndPointerDownX = x;
                        m2ndPointerDownY = y;
                        mTwoPointersDownDistance = distance;

                        mScaleInitialRect.set(mCurrentTouchedBoundingView.rect);
                    }
                }
                mNumDownPointers ++;
                break;
            case MotionEvent.ACTION_MOVE:
            {
                if (mCurrentTouchedBoundingView == null)
                    break;
                //this is special event, it may contain multiple touches
                final int pointerCount = event.getPointerCount();

                //process current position
                for (int p = 0; p < pointerCount; p++) {
                    id = event.getPointerId(p);
                    x = event.getX(p);
                    y = event.getY(p);

                    Rect rect = mCurrentTouchedBoundingView.rect;

                    if (id == m1stPointerDownId) {
                        if (mNumDownPointers != 1)
                            continue;

                        // handling buttons moving

                        int newLeft = (int) (x - m1stPointerDownRelX);
                        int newTop = (int) (y - m1stPointerDownRelY);

                        int width = rect.width();
                        int height = rect.height();

                        // move this button
                        rect.left = newLeft;
                        rect.top = newTop;
                        rect.right = rect.left + width;
                        rect.bottom = rect.top + height;

                        setButtonRect(mCurrentTouchedBoundingView.button, rect, true);
                    } else if (id == m2ndPointerDownId) {
                        // handle button scaling
                        double currentDistance = Utils.distance(m1stPointerDownX, m1stPointerDownY, x, y);

                        double ratio = currentDistance / mTwoPointersDownDistance;

                        // scale the rect
                        int centerX = mScaleInitialRect.centerX();
                        int centerY = mScaleInitialRect.centerY();
                        int newWidth = (int)(mScaleInitialRect.width() * ratio);
                        int newHeight = (int)(mScaleInitialRect.height() * ratio);

                        rect.left = centerX - newWidth / 2;
                        rect.top = centerY - newHeight / 2;
                        rect.right = rect.left + newWidth;
                        rect.bottom = rect.top + newHeight;

                        setButtonRect(mCurrentTouchedBoundingView.button, rect, true);
                    }
                } // for (pointerCount)
            }
            break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_POINTER_UP:
                if (mNumDownPointers > 0)
                    mNumDownPointers --;

                if (m1stPointerDownId == id) {
                    mCurrentTouchedBoundingView = null;
                    m1stPointerDownId = -1;
                }

                if (m2ndPointerDownId == id) {
                    m2ndPointerDownId = -1;
                }

                if (mNumDownPointers <= 0) {
                    mNumDownPointers = 0;
                    m1stPointerDownId = -1;
                    m2ndPointerDownId = -1;
                }
                break;
        }//switch (action)

        return true;
    }

    public static Rect getDPadRectFromSettings(boolean forPortrait) {
        return Settings.getUIButtonRect(Settings.Button.values()[Settings.Button.NORMAL_BUTTONS], forPortrait);
    }

    private void onGameSurfaceViewReady() {
        mGameViewReady = true;

        for (int i = 0; i < NUM_BUTTONS; ++i) {
            try {
                // see if the button's position and size was saved before
                final Settings.Button button = Settings.Button.values()[i];
                Rect savedRect = Settings.getUIButtonRect(button, mForPortrait);
                if (savedRect == null)
                {
                    // if not get the default one
                    resetButtonRect(button, true);
                } else {
                    // if existing, apply the saved settings
                    setButtonRect(button, savedRect, true);
                }

            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private void resetButtonsLayout() {
        Settings.resetUIButtonsRects(mForPortrait);

        if (mGameViewReady) {
            for (int i = 0; i < NUM_BUTTONS; ++i) {
                final Settings.Button button = Settings.Button.values()[i];

                resetButtonRect(button, true);
            }
        }
    }

    private void resetButtonRect(final Settings.Button button, final boolean saveSetting) {
        if (button.ordinal() == Settings.Button.NORMAL_BUTTONS) // this represents dpad
        {
            mGameView.getDPadDefaultRect(new AsyncQuery<Rect>() {
                @Override
                public void run(Rect result) {
                    if (result == null)
                        return;
                    setButtonRect(button, result, saveSetting);
                }
            });
        } else {
            mGameView.getUIButtonDefaultRect(button, new AsyncQuery<Rect>() {
                @Override
                public void run(Rect result) {
                    if (result != null) {
                        setButtonRect(button, result, saveSetting);
                    }
                }
            });
        }
    }

    private void setButtonRect(final Settings.Button button, final Rect rect, final boolean saveSetting) {
        if (saveSetting)
            Settings.setUIButtonRect(button, rect, mForPortrait);

        if (button.ordinal() == Settings.Button.NORMAL_BUTTONS) // this represents dpad
            mGameView.setDPadRect(rect.left, rect.top, rect.width(), new AsyncQuery<Boolean>() {
                @Override
                public void run(Boolean result) {
                    if (result != null && result)
                        moveButtonBoundingView(button, rect);
                }
            });
        else
            mGameView.setUIButtonRect(button, rect, new AsyncQuery<Boolean>() {
                @Override
                public void run(Boolean result) {
                    if (result != null && result)
                        moveButtonBoundingView(button, rect);
                }
            });
    }

    private void moveButtonBoundingView(final Settings.Button button, final Rect rect) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ButtonBoudingView view = mButtonsBoundingViews[button.ordinal()];

                if (view == null) {
                    view = mButtonsBoundingViews[button.ordinal()] = new ButtonBoudingView(button, rect);
                } else {
                    view.rect = rect;
                }
            }
        });
    }

    private class ButtonBoudingView {
        Rect rect;
        final Settings.Button button;


        public ButtonBoudingView(Settings.Button _button, Rect _rect) {
            this.rect = _rect;
            this.button = _button;
        }
    }
}
