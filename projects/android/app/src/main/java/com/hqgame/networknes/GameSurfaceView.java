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
import android.app.ProgressDialog;
import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.net.wifi.p2p.WifiP2pManager;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Vibrator;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.view.MotionEventCompat;
import android.util.AttributeSet;
import android.util.Base64;
import android.view.HapticFeedbackConstants;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.widget.Toast;

import com.facebook.FacebookCallback;
import com.facebook.FacebookException;
import com.facebook.share.model.GameRequestContent;
import com.facebook.share.widget.GameRequestDialog;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.games.Games;
import com.google.android.gms.games.multiplayer.Multiplayer;
import com.google.android.gms.games.multiplayer.realtime.RoomConfig;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatch;
import com.google.android.gms.games.multiplayer.turnbased.TurnBasedMatchConfig;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;

import org.json.JSONObject;

import java.io.IOException;
import java.io.InputStream;
import java.lang.ref.WeakReference;
import java.net.InetAddress;
import java.util.ArrayList;
import java.util.LinkedList;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Created by lehoangquyen on 26/2/16.
 */
public class GameSurfaceView extends GLSurfaceView {
    private static final int FB_INVITE_CTX = 0;
    private static final int GOOGLE_INVITE_CTX = 1;
    private static final int NO_INVITE_CTX = 2;

    private static volatile long sNativeHandle = 0;
    private static ThreadLocal<GameSurfaceView> sJavaHandle = new ThreadLocal<>();

    // special task queue to be outlive even the GameSurfaceView itself
    private static LinkedList<Runnable> sPersitentTasksQueue = new LinkedList<>();

    private int mWidth = -1;
    private int mHeight = -1;

    private long mLastStartTime = 0;
    private long mLastStartTimeMultiplayer = 0;
    private long mPlayDuration = 0;
    private long mMulitplayerDuration = 0;

    private GLUtils.ContextFactory mCtxFactory = null;
    private Renderer mRenderer = null;

    private NesMachineEvent[] machineEventCodes = NesMachineEvent.values();

    private Toast mToast = null;
    private GameRequestDialog mFbInviteDialog = null;
    private ProgressDialog mProgressDialog = null;
    private Vibrator mVibrator = null;

    private volatile Runnable mLastLoadRemoteCommand = null;
    private GameChatDialog.ChatMessagesDatabaseViewAdapter mChatHistoryViewAdapter = null;

    // Google Multiplayer
    private GooglePlayGamesMatchHandler mGoogleMultiplayerMatch = null;
    private AsyncOperation mGoogleMultiplayerMatchWaitingToBeConnectedFallbackOp = null;
    private GoogleMatchCreatedCallback mGoogleMultiplayerMatchCreatedCallback = null;
    private boolean mWaitingForInviteResponse = false;

    // Wifi Direct Multiplayer
    private WifiDirectUtils.ServerHandler mWifiDirectServer = null;

    private WeakReference<GamePage> mParentPageRef = null;

    private final boolean mDrawButtonsOnly;
    private final float mDrawButtonOutlineSize;
    private final Runnable mGLContextReadyCallback;

    public GameSurfaceView(GamePage parentPage, AttributeSet attrs) {
        super(parentPage.getContext(), attrs);
        Context context = parentPage.getContext();

        mDrawButtonsOnly = false;
        mDrawButtonOutlineSize = 0;
        mGLContextReadyCallback = null;

        /*
        try {
            mVibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
        } catch (Exception e)
        {
            e.printStackTrace();
            mVibrator = null;
        }
        */

        mParentPageRef = new WeakReference<>(parentPage);

        setEGLConfigChooser(new GLUtils.ConfigChooser(5, 6, 5, 0, 0, 0));
        setEGLContextFactory((mCtxFactory = new GLUtils.ContextFactory(this.getHolder())));

        mRenderer = new Renderer();

        mChatHistoryViewAdapter = new GameChatDialog.ChatMessagesDatabaseViewAdapter(context, (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE));

        // Set the Renderer for drawing on the GLSurfaceView
        setRenderer(mRenderer);

        setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);

        this.setHapticFeedbackEnabled(true);
    }

    // this is for rendering button layout only.
    // Note: glContextReadyCallback will be invoked on ui thread
    public GameSurfaceView(Context context, AttributeSet attrs, float buttonOutlineSize, boolean hasAlpha, Runnable glContextReadyCallback) {
        super(context, attrs);

        mDrawButtonsOnly = true;
        mDrawButtonOutlineSize = buttonOutlineSize;
        mGLContextReadyCallback = glContextReadyCallback;

        if (hasAlpha) {
            setEGLConfigChooser(new GLUtils.ConfigChooser(8, 8, 8, 8, 0, 0));
            getHolder().setFormat(PixelFormat.TRANSLUCENT);
        } else {
            setEGLConfigChooser(new GLUtils.ConfigChooser(5, 6, 5, 0, 0, 0));
        }
        setEGLContextFactory((mCtxFactory = new GLUtils.ContextFactory(this.getHolder())));

        mRenderer = new Renderer();

        // Set the Renderer for drawing on the GLSurfaceView
        setRenderer(mRenderer);

        setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
    }

    private class Renderer implements GLSurfaceView.Renderer {
        private long mThreadId = -1;

        public long getThreadId()
        {
            return mThreadId;
        }

        public void onSurfaceCreated(GL10 unused, EGLConfig config) {
            initGameNativeIfNeeded();
            if (mWidth != -1 && mHeight != -1) {
                resetNativeView(mWidth, mHeight, true);
            }
        }

        public void onDrawFrame(GL10 unused) {
            mThreadId = Thread.currentThread().getId();
            sJavaHandle.set(GameSurfaceView.this);

            initGameNativeIfNeeded();

            // execute any queued persistent tasks
            synchronized (sPersitentTasksQueue) {
                for (Runnable task: sPersitentTasksQueue)
                    task.run();
                sPersitentTasksQueue.clear();
            }

            GLES20.glViewport(0, 0, mWidth, mHeight);
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

            renderGameViewNative(sNativeHandle, mDrawButtonsOnly, mDrawButtonOutlineSize);

            sJavaHandle.set(null);
        }

        public void onSurfaceChanged(GL10 unused, final int width, final int height) {
            initGameNativeIfNeeded();
            final boolean contextFirstCreate = mWidth == -1 || mHeight == -1;

            resetNativeView(width, height, contextFirstCreate);

            mWidth = width;
            mHeight = height;

            if (mGLContextReadyCallback != null) {
                post(new Runnable() {
                    @Override
                    public void run() {
                        mGLContextReadyCallback.run();
                    }
                });
            }
        }

        private void resetNativeView(int width, int height, boolean contextRecreated) {
            //showProgressDialog(R.string.loading_msg, null);
            resetGameViewNative(getContext().getAssets(), sNativeHandle, mCtxFactory.getESVersionMajor(), width, height, contextRecreated);
            //dismissProgressDialog();
        }
    }

    public void onParentPageViewCreated(GamePage parentPage)
    {
        System.out.println("GameSurfaceView.onParentPageViewCreated()");

        sJavaHandle.set(this);

        // store reference to parent page
        mParentPageRef = new WeakReference<>(parentPage);

        //create invite dialog
        mFbInviteDialog = new GameRequestDialog(this.getActivity());
        mFbInviteDialog.registerCallback(getActivity().getFbCallbackManager(), new FacebookCallback<GameRequestDialog.Result>() {
            @Override
            public void onSuccess(GameRequestDialog.Result result) {
                //DO NOTHING
            }

            @Override
            public void onCancel() {
                finishGamePage();
            }

            @Override
            public void onError(FacebookException e) {
                fatalError(e.getMessage());
            }
        });
    }

    public void onParentPageDestroyed() {
        System.out.println("GameSurfaceView.onParentPageDestroyed()");

        sJavaHandle.set(null);
    }

    @Override
    public void onPause() {
        System.out.println("GameSurfaceView.onPause()");

        onPauseOnUIThreadNative(sNativeHandle);

        super.onPause();
    }

    @Override
    public void onResume() {
        System.out.println("GameSurfaceView.onResume()");

        super.onResume();

        final boolean hasRecordPermission = Utils.hasPermission(getActivity(), Manifest.permission.RECORD_AUDIO);

        queuePersistentEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                onResumeNative(sNativeHandle);

                setAudioRecordPermissionChangedNative(sNativeHandle, hasRecordPermission);
                setAudioVolumeNative(sNativeHandle, Settings.getAudioVolume());
                enableAudioInput(sNativeHandle, Settings.isVoiceChatEnabled());
                enableUIButtonsNative(sNativeHandle, Settings.isUIButtonsEnbled());
                enableFullScreenNative(sNativeHandle, Settings.isFullscreenEnabled());
                switchABTurboModeNative(sNativeHandle, Settings.isBtnATurbo(), Settings.isBtnBTurbo());
                applyFilteringShader();

                // apply buttons layout settings
                boolean portrait = mWidth < mHeight;

                if (Settings.getNumAssignedUIButtonsRects(portrait) > 0) {
                    for (int i = 0; i < Settings.Button.NORMAL_BUTTONS; ++i) {
                        Settings.Button button = Settings.Button.values()[i];
                        Rect rect = Settings.getUIButtonRect(button, portrait);
                        if (rect != null)
                            setUIButtonRect(button, rect, null);
                    }
                }

                Rect dpadRect = ButtonsLayoutPage.getDPadRectFromSettings(portrait);
                if (dpadRect != null)
                    setDPadRect(dpadRect.left, dpadRect.top, dpadRect.width(), null);
            }
        });
    }

    @Override
    protected void finalize() throws Throwable
    {
    }

    @Override
    public void queueEvent(final Runnable task) {
        super.queueEvent(new Runnable() {
            @Override
            public void run() {
                sJavaHandle.set(GameSurfaceView.this);
                task.run();
                sJavaHandle.set(null);
            }
        });
    }

    public static void queuePersistentEvent(final Runnable task) {
        synchronized (sPersitentTasksQueue) {
            sPersitentTasksQueue.add(task);
        }
    }

    public static boolean verifyGame(Context context, String path) throws IllegalStateException
    {
        initGameNativeIfNeeded(context);
        return verifyGameNative(sNativeHandle, path);
    }

    public static void enableLANServerDiscovery(Context context, boolean enable) {
        initGameNativeIfNeeded(context);

        enableLANServerDiscoveryNative(sNativeHandle, enable);
    }

    public static void findLANServers(Context context, long request_id) {
        initGameNativeIfNeeded(context);

        findLANServersNative(sNativeHandle, request_id);
    }

    // callback will be invoked on ui thread
    public static AsyncOperation testRemoteInternetConnectivity(
            String invite_data,
            String clientGUID,
            final AsyncQuery<Boolean> callback) {

        TestInternetConnectivityQueryAdapter callbackAdapter = new TestInternetConnectivityQueryAdapter(callback);

        final long connHandlerPtr = createRemoteInternetConnectivityTestNative(null, invite_data, clientGUID, callbackAdapter);
        if (connHandlerPtr == 0)
            return null;

        callbackAdapter.setConnectionHandlerPtr(connHandlerPtr);

        // now start
        if (!startRemoteInternetConnectivityTestNative(connHandlerPtr))
            return null;

        return callbackAdapter;
    }

    public void loadedGame(final AsyncQuery<String> resultCallback) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                String loadedGame = loadedGameNative(sNativeHandle);
                resultCallback.run(loadedGame);
            }
        });
    }

    public void loadAndStartGame(final String path) {
        mLastLoadRemoteCommand = null;

        aggregatePlayTimeAndResetTimer(System.nanoTime(), false);

        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                loadAndStartGameNative(sNativeHandle, path);
            }
        });
    }

    public void loadRemote(final String ip, final int port, final String clientInfo) {
        cancelExistingAsyncOps();

        aggregatePlayTimeAndResetTimer(System.nanoTime(), false);

        queueEvent(mLastLoadRemoteCommand = new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                showProgressDialog(R.string.connecting_to_host, new Runnable() {
                    @Override
                    public void run() {
                        shutdownGameAndClose();
                    }
                });

                // close any existing connection
                disableRemoteControllerNative(sNativeHandle);
                shutdownGameNative(sNativeHandle);

                boolean re = loadRemoteNative(sNativeHandle, ip, port, clientInfo);
                if (!re) {
                    dismissProgressDialog();
                    fatalError("Failed to establish remote connection");
                } else {
                    System.out.println("loadRemote successfully");
                }//if (!re)
            }
        });
    }

    public void loadRemoteInternetFb(final String invite_id, final String invite_data, final String clientGUID, final String clientName) {
        cancelExistingAsyncOps();

        loadRemoteInternet(invite_id, invite_data, clientGUID, clientName, FB_INVITE_CTX);
    }

    public void loadRemoteInternetGoogle(final String google_match_id, final String invite_data, final String clientGUID, final String clientName) {
        cancelExistingAsyncOps();

        mGoogleMultiplayerMatch = new GooglePlayGamesMatchHandler.StopOnlyHandler(getActivity(), google_match_id);

        loadRemoteInternet(google_match_id, invite_data, clientGUID, clientName, GOOGLE_INVITE_CTX);
    }

    public void loadRemoteInternetPublic(final String invite_data, final String clientGUID, final String clientName) {
        cancelExistingAsyncOps();

        loadRemoteInternet("", invite_data, clientGUID, clientName, NO_INVITE_CTX);
    }

    private void loadRemoteInternet(final String invite_id, final String invite_data, final String clientGUID, final String clientName, final int context) {
        aggregatePlayTimeAndResetTimer(System.nanoTime(), false);

        queueEvent(mLastLoadRemoteCommand = new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                showProgressDialog(R.string.connecting_to_host, new Runnable() {
                    @Override
                    public void run() {
                        shutdownGameAndClose();
                    }
                });

                // close any existing connection
                disableRemoteControllerNative(sNativeHandle);
                shutdownGameNative(sNativeHandle);

                boolean re = loadRemoteInternetNative(sNativeHandle, invite_id, invite_data, clientGUID, clientName, context);
                if (!re) {
                    dismissProgressDialog();
                    fatalError("Failed to establish remote connection");
                }
                else {
                    System.out.println("loadRemote successfully");
                }//if (!re)
            }
        });
    }

    public static String getLobbyServerAddress() {
        return getLobbyServerAddressNative();
    }

    public static String getLobbyAppId() {
        return getLobbyAppIdNative();
    }

    public void resetGame() {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                resetGameNative(sNativeHandle);
            }
        });
    }

    public void shutdownGame(final boolean closePage, final Runnable shutdownFinishCallback)
    {
        aggregatePlayTimeAndResetTimer(System.nanoTime(), true);

        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                disableRemoteControllerNative(sNativeHandle);
                shutdownGameNative(sNativeHandle);
                finishGamePage(closePage, shutdownFinishCallback);
            }
        });
    }

    public void shutdownGameAndClose()
    {
        aggregatePlayTimeAndResetTimer(System.nanoTime(), true);

        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                disableRemoteControllerNative(sNativeHandle);
                shutdownGameNative(sNativeHandle);
                finishGamePage();
            }
        });
    }

    // This must be called on ui thread
    public void shutdownUrgent(boolean closePage) {
        aggregatePlayTimeAndResetTimer(System.nanoTime(), true);

        queuePersistentEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                disableRemoteControllerNative(sNativeHandle);
                shutdownGameNative(sNativeHandle);
            }
        });

        finalizeGamePageOnUiThread(closePage);
    }

    public void enableRemoteController(final int port, final String hostName) {
        cancelExistingAsyncOps();

        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                boolean re = enableRemoteControllerNative(sNativeHandle, port, hostName, null);
                if (!re)
                    fatalError("Failed to establish remote connection");
            }
        });
    }

    public void enableRemoteControllerWifiDirect(final int port, final String hostName) {
        cancelExistingAsyncOps();

        WifiP2pManager manager = getActivity().getWifiDirectManager();
        WifiP2pManager.Channel channel = getActivity().getWifiDirectChannel();
        if (manager == null || channel == null) {
            fatalError(R.string.wifi_direct_not_supported);
            return;
        }

        showProgressDialog(R.string.wifi_direct_creating, new Runnable() {
            @Override
            public void run() {
                finishGamePage();
            }
        });

        // now create the server
        mWifiDirectServer = WifiDirectUtils.createServer(manager, channel, hostName, port, new WifiDirectUtils.ServerCreateCallback() {
            @Override
            public void onServerPublishedSuccess() {
                // TODO
                System.out.println("GameSurfaceView.WifiDirectUtils.ServerCreateCallback.onServerPublishedSuccess()");
            }

            @Override
            public void onServerCreatedSuccess(final InetAddress address) {
                System.out.println("GameSurfaceView.WifiDirectUtils.ServerCreateCallback.onServerCreatedSuccess(" + address + ")");

                dismissProgressDialog();
                // now just enable the remote controller as in LAN case
                queueEvent(new Runnable() {
                    @Override
                    public void run() {
                        initGameNativeIfNeeded();
                        boolean re = enableRemoteControllerNative(sNativeHandle, port, hostName, address.getHostAddress());
                        if (!re)
                            fatalError("Failed to establish remote connection");
                    }
                });
            }

            @Override
            public void onFailure(int reason) {
                System.out.println("GameSurfaceView.WifiDirectUtils.ServerCreateCallback.onFailure(" + reason + ")");

                switch (reason) {
                    case WifiP2pManager.P2P_UNSUPPORTED:
                        fatalError(R.string.wifi_direct_not_supported);
                        break;
                    case WifiP2pManager.ERROR:
                        fatalError(R.string.wifi_direct_internal_err_msg);
                        break;
                    default:
                        fatalError(String.format(getActivity().getString(R.string.wifi_direct_generic_err_msg), reason));
                }
            }
        });
    }

    public void enableRemoteControllerInternetFb(final String hostGUID, final String hostName) {
        enableRemoteControllerInternet(hostGUID, hostName, FB_INVITE_CTX, false);
    }

    public void enableRemoteControllerInternetGoogle(final String hostGUID, final String hostName) {


        enableRemoteControllerInternet(hostGUID, hostName, GOOGLE_INVITE_CTX, false);
    }

    public void enableRemoteControllerInternetPublic(final String hostGUID, final String hostName) {
        enableRemoteControllerInternet(hostGUID, hostName, NO_INVITE_CTX, true);
    }

    private void enableRemoteControllerInternet(final String hostGUID, final String hostName, final int context, final boolean isPublic) {
        cancelExistingAsyncOps();

        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                showProgressDialog(R.string.connecting_to_cserver, new Runnable() {
                    @Override
                    public void run() {
                        finishGamePage();
                    }
                });

                boolean re = enableRemoteControllerInternetNative(sNativeHandle, hostGUID, hostName, context, isPublic);
                if (!re) {
                    dismissProgressDialog();
                    fatalError("Failed to establish remote connection");
                }
            }
        });
    }

    public void reinviteNewFriendForRemoteControllerInternet() {
        cancelExistingAsyncOps();

        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                showProgressDialog(R.string.connecting_to_cserver, new Runnable() {
                    @Override
                    public void run() {
                        finishGamePage();
                    }
                });

                boolean re = reinviteNewFriendForRemoteControllerInternetNative(sNativeHandle);
                if (!re) {
                    dismissProgressDialog();
                    fatalError("Failed to establish remote connection");
                }
            }
        });
    }

    public void disableRemoteController() {
        cancelExistingAsyncOps();

        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                disableRemoteControllerNative(sNativeHandle);
            }
        });
    }
    public void isRemoteControllerConnected(final AsyncQuery<Boolean> resultCallback)
    {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                Boolean re = isRemoteControllerConnectedNative(sNativeHandle);
                resultCallback.run(re);
            }
        });
    }

    public void loadState(final String file) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                int re = loadStateNative(sNativeHandle, file);
                if (re == NesMachineResult.RESULT_ERR_INVALID_CRC)
                    errorMessage("This is not the correct save file. Did you try to load a save file for another game?");//TODO: localize
                else {
                    machineResultHandle(re, false);

                    if (NesMachineResult.succeeded(re))
                    {
                        post(new Runnable() {
                            @Override
                            public void run() {
                                showToast(R.string.save_file_load_succeeded_msg, Toast.LENGTH_SHORT);
                            }
                        });
                    }
                }
            }
        });
    }

    public void saveState(final String file) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                int re = saveStateNative(sNativeHandle, file);
                machineResultHandle(re, false);

                if (NesMachineResult.succeeded(re)) {
                    post(new Runnable() {
                        @Override
                        public void run() {
                            showToast(R.string.save_succeeded_msg, Toast.LENGTH_SHORT);
                        }
                    });
                }
            }
        });
    }

    public void switchABTurboMode(final boolean enableATurbo, final boolean enableBTurbo) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                switchABTurboModeNative(sNativeHandle, enableATurbo, enableBTurbo);
            }
        });
    }

    // pass negative number to slowdown the execution by |speed| times
    public void setSpeed(final int speed) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                setSpeedNative(sNativeHandle, speed);
            }
        });
    }

    private void applyFilteringShader() {
        Settings.DisplayFilterMode filterMode = Settings.getDisplayFilterMode();

        Activity activity = getActivity();
        if (activity == null)
            return;

        String shaderFileName = filterMode.toString();
        String vshader = null, fshader = null;
        float scale = 1;
        boolean linearSampling = false;

        switch (filterMode) {
            case CRT_CGWG_FAST:
            case CRT_HYLLIAN:
            case SCANLINE:
                scale = 1;
                linearSampling = false;
                break;
            case HQ2X: case _2XBR:
                scale = 2;
                break;
            case HQ4X: case _4XBR:
                scale = 4;
                break;
            case XBR_LV2_2X: case XBR_LV2_4X:
                shaderFileName = "xbr lv2";
                if (filterMode == Settings.DisplayFilterMode.XBR_LV2_2X)
                    scale = 2;
                else
                    scale = 4;
                break;
            case XBR_LV2_FAST_2X: case XBR_LV2_FAST_4X:
                shaderFileName = "xbr lv2 fast";
                if (filterMode == Settings.DisplayFilterMode.XBR_LV2_FAST_2X)
                    scale = 2;
                else
                    scale = 4;
                break;
        }

        if (filterMode != Settings.DisplayFilterMode.NONE) {
            vshader = Utils.readTextAsset(activity, "shaders", shaderFileName + ".glslv");
            fshader = Utils.readTextAsset(activity, "shaders", shaderFileName + ".glslf");

            if (vshader != null && vshader.length() == 0)
                vshader = null;
            if (fshader != null && fshader.length() == 0)
                fshader = null;
        }

        setFilteringShaderNative(sNativeHandle, vshader, fshader, scale, linearSampling);
    }

    /*- hardware input ----*/
    public void onJoystickMoved(final float x, final float y) // x, y must be in range [-1, 1]
    {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                onJoystickMovedNative(sNativeHandle, x, y);
            }
        });
    }
    public void onUserHardwareButtonDown(final Settings.Button button) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                onUserHardwareButtonDownNative(sNativeHandle, button.ordinal());

                //System.out.println("onUserHardwareButtonDownNative(" + button.toString() + ")");
            }
        });
    }

    public void onUserHardwareButtonUp(final Settings.Button button) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                onUserHardwareButtonUpNative(sNativeHandle, button.ordinal());

                //System.out.println("onUserHardwareButtonUpNative(" + button.toString() + ")");
            }
        });
    }

    /*---- chat handler ----*/
    public GameChatDialog.ChatMessagesDatabaseViewAdapter getChatDatabaseViewAdapter() {
        return mChatHistoryViewAdapter;
    }

    public void sendMessageToRemote(final long id, final String message, final AsyncQuery<Boolean> resultCallback) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                int re = sendMessageToRemoteNative(sNativeHandle, id, message);

                switch (re) {
                    case NesMachineResult.RESULT_ERR_BUFFER_TOO_BIG:
                        errorMessage(getContext().getString(R.string.chat_message_too_big_err));
                        resultCallback.run(false);
                        break;
                    case NesMachineResult.RESULT_ERR_NOT_READY:
                        errorMessage(getContext().getString(R.string.chat_not_ready_err));
                        resultCallback.run(false);
                        break;
                    default: {
                        if (NesMachineResult.succeeded(re)) {
                            GameChatDialog.ChatMessage newMsgEntry = new GameChatDialog.ChatMessage(message);
                            if (NesMachineResult.failed(re))
                                newMsgEntry.setStatus(GameChatDialog.ChatMessage.Status.FAILED_TO_SEND);

                            mChatHistoryViewAdapter.addMessage(id, newMsgEntry);

                            notifyChatHistoryUpdated();

                            resultCallback.run(true);
                        }
                        else {
                            machineResultHandle(re, false);
                            resultCallback.run(false);
                        }
                    }
                }
            }
        });
    }

    //get name of connected remote side
    public void getRemoteEndName(final AsyncQuery<String> resultCallback) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                String re = getRemoteNameNative(sNativeHandle);
                resultCallback.run(re);
            }
        });
    }

    // ----------------- UI buttons editor --------------------------
    public void setUIButtonRect(final Settings.Button button, final Rect rect, final AsyncQuery<Boolean> finishCallback) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                if (mWidth == -1 || mHeight == -1)
                {
                    if (finishCallback != null)
                        finishCallback.run(false);
                    return;
                }

                initGameNativeIfNeeded();

                setUIButtonRectNative(sNativeHandle, button.ordinal(), rect.left, mHeight - rect.bottom, rect.width(), rect.height());

                if (finishCallback != null)
                    finishCallback.run(true);
            }
        });
    }

    public void setDPadRect(final float left, final float top, final float size, final AsyncQuery<Boolean> finishCallback) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                if (mWidth == -1 || mHeight == -1)
                {
                    if (finishCallback != null)
                        finishCallback.run(false);
                    return;
                }

                initGameNativeIfNeeded();

                setDPadRectNative(sNativeHandle, left, mHeight - top - size, size);

                if (finishCallback != null)
                    finishCallback.run(true);
            }
        });
    }

    public void getUIButtonDefaultRect(final Settings.Button button, final AsyncQuery<Rect> resultCallback) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                if (mWidth == -1 || mHeight == -1)
                {
                    return;
                }

                initGameNativeIfNeeded();

                float [] values = new float[4];

                getUIButtonDefaultRectNative(sNativeHandle, button.ordinal(), values);

                int width = (int)values[2];
                int height = (int)values[3];

                Rect rect = new Rect();
                rect.left = (int)values[0];
                rect.bottom = (int)(mHeight - values[1]);
                rect.right = (int)(rect.left + width);
                rect.top = (int)(rect.bottom - height);

                resultCallback.run(rect);
            }
        });
    }

    public void getDPadDefaultRect(final AsyncQuery<Rect> resultCallback) {
        queueEvent(new Runnable() {
            @Override
            public void run() {
                if (mWidth == -1 || mHeight == -1)
                {
                    return;
                }

                initGameNativeIfNeeded();

                float [] values = new float[3];

                getDPadDefaultRectNative(sNativeHandle, values);

                int sizeI = (int) values[2];

                Rect rect = new Rect();
                rect.left = (int)values[0];
                rect.bottom = (int)(mHeight - values[1]);
                rect.right = (int)(rect.left + sizeI);
                rect.top = (int)(rect.bottom - sizeI);

                resultCallback.run(rect);
            }
        });
    }

    /*--------------------- */
    private void initGameNativeIfNeeded() {
        initGameNativeIfNeeded(this.getContext());
    }

    private static void initGameNativeIfNeeded(Context context)
    {
        if (sNativeHandle == 0)
        {
            byte[] dbData = null;

            //load database
            try {
                InputStream inputStream = context.getAssets().open("NesDatabase.xml");

                dbData = new byte[inputStream.available()];
                inputStream.read(dbData);

                inputStream.close();

            } catch (IOException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }

            //create native handle
            sNativeHandle = initGameNative(dbData);
        }
     }

    @Override
    public boolean onTouchEvent(MotionEvent _event)
    {
        if (mDrawButtonsOnly)
            return false;

        final MotionEvent event = MotionEvent.obtainNoHistory(_event);//TODO: do we need history to handle precise touch movement?

        final GLSurfaceView view = this;

        this.queueEvent(new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();

                int index = MotionEventCompat.getActionIndex(event);
                int id = MotionEventCompat.getPointerId(event, index);
                float x = MotionEventCompat.getX(event, index);
                //touch coordinate is upside down, while OpenGL coordinate is bottom up
                float y = mHeight - MotionEventCompat.getY(event, index);

                final int action = MotionEventCompat.getActionMasked(event);

                boolean shouldVibrate = false;

                switch (action) {
                    case MotionEvent.ACTION_DOWN:
                    case MotionEvent.ACTION_POINTER_DOWN:
                        shouldVibrate = onTouchBeganNative(sNativeHandle, id, x, y);
                        break;
                    case MotionEvent.ACTION_MOVE:
                    {
                        //this is special event, it may contain multiple touches
                        final int historySize = event.getHistorySize();
                        final int pointerCount = event.getPointerCount();

                        //process historical movement first
                        for (int h = 0; h < historySize; h++) {
                            for (int p = 0; p < pointerCount; p++) {
                                id = event.getPointerId(p);
                                x = event.getHistoricalX(p, h);
                                y = mHeight - event.getHistoricalY(p, h);

                                shouldVibrate = onTouchMovedNative(sNativeHandle, id, x, y) || shouldVibrate;
                            }
                        }

                        //process current position
                        for (int p = 0; p < pointerCount; p++) {
                            id = event.getPointerId(p);
                            x = event.getX(p);
                            y = mHeight - event.getY(p);

                            shouldVibrate = onTouchMovedNative(sNativeHandle, id, x, y) || shouldVibrate;
                        }
                    }
                        break;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                    case MotionEvent.ACTION_POINTER_UP:
                        onTouchEndedNative(sNativeHandle, id, x, y);
                        break;
                }//switch (action)

                if (shouldVibrate && Settings.isButtonsVibrationEnabled()) {
                    //vibration
                    view.post(new Runnable() {
                        @Override
                        public void run() {
                            //if (mVibrator != null)
                            //    mVibrator.vibrate(200);
                            view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
                        }
                    });
                }

                event.recycle();//mark this event to be reused by system
            }
        });

        return true;
    }

    private static void machineEventCallback(final int event, int value)
    {
        final GameSurfaceView javaHandle = sJavaHandle.get();
        if (javaHandle == null || event < 0 || javaHandle.machineEventCodes.length <= event)
            return;
        
        NesMachineEvent eventCode = javaHandle.machineEventCodes[event];

        //TODO: localize error message
        switch (eventCode)
        {
            case EVENT_LOAD:
                machineResultHandle(value);
                break;
            case EVENT_UNLOAD:
                break;
            case EVENT_LOAD_REMOTE:
                break;
            case EVENT_REMOTE_CONTROLLER_ENABLED:
            {
                System.out.println("Remote controller (" + value + ") enabled");

                if (value >= 0) {
                    // allow another client to connect
                    Utils.enableMulticast(javaHandle.getContext(), true);
                    if (javaHandle.mWifiDirectServer != null)
                        javaHandle.mWifiDirectServer.setAvailable(true);
                }
            }
                break;
            case EVENT_REMOTE_CONTROLLER_DISABLED:
            {
                System.out.println("Remote controller (" + value + ") disabled");

                // disallow another client to connect
                Utils.enableMulticast(javaHandle.getContext(), false);
                if (javaHandle.mWifiDirectServer != null)
                    javaHandle.mWifiDirectServer.setAvailable(false);
            }
                break;
            case EVENT_REMOTE_DISCONNECTED: {//disconnected from server
                javaHandle.aggregateMultiplayerTimeAndResetTimer(System.nanoTime(), true);//calculate multiplayer duration and reset timer

                Runnable retryCallback = null;

                if (javaHandle.mLastLoadRemoteCommand != null)
                    retryCallback = new Runnable() {
                            @Override
                            public void run() {
                                //retry
                                javaHandle.queueEvent(javaHandle.mLastLoadRemoteCommand);
                            }
                        };

                errorMessage("Remote connection timeout or disconnected",
                        retryCallback,
                        new Runnable() {
                            @Override
                            public void run() {
                                javaHandle.finishGamePage();
                            }
                        });
            }
                break;
            case EVENT_REMOTE_DATA_RATE:
                //TODO
                break;
            default:
                break;
        }

        if (Settings.DEBUG)
            System.out.println(eventCode.toString() + " handled");
    }

    private static void machineEventCallback(final int event, float value)
    {
        GameSurfaceView javaHandle = sJavaHandle.get();
        if (javaHandle == null || event < 0 || javaHandle.machineEventCodes.length <= event)
            return;

        NesMachineEvent eventCode = javaHandle.machineEventCodes[event];

        switch (eventCode)
        {
            case EVENT_REMOTE_DATA_RATE:
                //TODO
                break;
            default:
                break;
        }

        if (Settings.DEBUG)
            System.out.println(eventCode.toString() + " handled");
    }

    private static void machineEventCallback(final int event, long value) {
        GameSurfaceView javaHandle = sJavaHandle.get();
        if (javaHandle == null || event < 0 || javaHandle.machineEventCodes.length <= event)
            return;

        NesMachineEvent eventCode = javaHandle.machineEventCodes[event];

        switch (eventCode)
        {
            case EVENT_REMOTE_MESSAGE_ACK://message received by remote side
            {
                GameChatDialog.ChatMessage msgEntry = javaHandle.mChatHistoryViewAdapter.getMessage(value);
                if (msgEntry != null)
                {
                    msgEntry.setStatus(GameChatDialog.ChatMessage.Status.RECEIVED_BY_THEM);
                    javaHandle.notifyChatHistoryUpdated();
                }
            }
                break;
            default:
                break;
        }

        if (Settings.DEBUG)
            System.out.println(eventCode.toString() + " handled");
    }

    private static void machineEventCallback(final int event, final byte[] valueBytes)
    {
        final GameSurfaceView javaHandle = sJavaHandle.get();
        if (javaHandle == null || event < 0 || javaHandle.machineEventCodes.length <= event)
            return;

        NesMachineEvent eventCode = javaHandle.machineEventCodes[event];

        String valueString = null;

        if (valueBytes != null) {
            try {
                valueString = new String(valueBytes, "UTF-8");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        final String value = valueString;

        switch (eventCode)
        {
            case EVENT_REMOTE_CONNECTED://Connected to server, can have optional "value"
            {
                final boolean connectedViaProxy = javaHandle.isRemoteConntectionViaProxyNative(sNativeHandle);

                javaHandle.aggregateMultiplayerTimeAndResetTimer(System.nanoTime(), false);//aggregate multiplayer duration

                if (value == null) // server's player name is mandatory
                    break;

                javaHandle.dismissProgressDialog();
                javaHandle.post(new Runnable() {
                    @Override
                    public void run() {
                        javaHandle.showToast(javaHandle.getContext().getString(R.string.connected_to) + " " + value, Toast.LENGTH_LONG);
                    }
                });

                if (connectedViaProxy) {
                    javaHandle.post(new Runnable() {
                        @Override
                        public void run() {
                            // warn user that we are connected via proxy server, so expect low bandwidth
                            Activity activity = javaHandle.getActivity();
                            if (activity != null)
                                Utils.alertDialog(activity, null, activity.getString(R.string.connected_via_proxy),
                                        new Runnable() {
                                            @Override
                                            public void run() {

                                            }
                                        }
                                );
                        }
                    });
                }
            }
                break;
            case EVENT_REMOTE_CONNECTION_INTERNAL_ERROR:
                javaHandle.aggregateMultiplayerTimeAndResetTimer(System.nanoTime(), true);//calculate multiplayer duration and reset timer

                if (value.equals("remote_cancel"))//user simply cancelled the operation, so don't display error message
                {
                    javaHandle.finishGamePage();
                }
                else
                {
                    //TODO: localize error code
                    Runnable retryCallback = null;

                    if (javaHandle.mLastLoadRemoteCommand != null)
                        retryCallback = new Runnable() {
                            @Override
                            public void run() {
                                //retry
                                javaHandle.queueEvent(javaHandle.mLastLoadRemoteCommand);
                            }
                        };

                    String errorMsg = value;
                    if (value.equals("guid_exist"))
                        errorMsg = "One of your devices is already playing using this account. You cannot play multiplayer using the same account on multiple devices.";//TODO: localize

                    errorMessage(errorMsg,
                            retryCallback,
                            new Runnable() {
                                @Override
                                public void run() {
                                    javaHandle.finishGamePage();
                                }
                            });
                }
                break;
            case EVENT_CLIENT_CONNECTED://client connected
                // finally done
                if (javaHandle.mWaitingForInviteResponse) {
                    javaHandle.mWaitingForInviteResponse = false;
                    javaHandle.dismissProgressDialog();//dismiss progress dialog
                }

                javaHandle.cancelExistingMatchGoogle(); // no more need for the GPG matchmaking anymore

                javaHandle.aggregateMultiplayerTimeAndResetTimer(System.nanoTime(), false);//aggregate multiplayer duration

                javaHandle.post(new Runnable() {
                    @Override
                    public void run() {
                        javaHandle.showToast(value + " " + javaHandle.getContext().getString(R.string.has_connected), Toast.LENGTH_LONG);
                    }
                });

                // disallow another client to connect
                Utils.enableMulticast(javaHandle.getContext(), false);
                if (javaHandle.mWifiDirectServer != null)
                    javaHandle.mWifiDirectServer.setAvailable(false);

                break;
            case EVENT_CLIENT_DISCONNECTED://client disconnected
                javaHandle.aggregateMultiplayerTimeAndResetTimer(System.nanoTime(), true);//calculate multiplayer duration and reset timer

                javaHandle.post(new Runnable() {
                    @Override
                    public void run() {
                        javaHandle.showToast(value + " " + javaHandle.getContext().getString(R.string.has_disconnected), Toast.LENGTH_LONG);
                    }
                });

                // allow another client to connect
                Utils.enableMulticast(javaHandle.getContext(), true);
                if (javaHandle.mWifiDirectServer != null)
                    javaHandle.mWifiDirectServer.setAvailable(true);
                break;
            case EVENT_REMOTE_MESSAGE:
            {
                //we received a message from remote side
                String currentConnectedName = javaHandle.getRemoteNameNative(sNativeHandle);
                if (currentConnectedName == null)
                    currentConnectedName = "Unknown";

                GameChatDialog.ChatMessage newMsgEntry = new GameChatDialog.ChatMessage(currentConnectedName, value, GameChatDialog.ChatMessage.Status.RECEIVED_BY_US);
                javaHandle.mChatHistoryViewAdapter.addMessage(Utils.generateId(), newMsgEntry);
                javaHandle.notifyChatHistoryUpdated();

                GamePage parentPage = null;
                if (javaHandle.mParentPageRef != null && (parentPage = javaHandle.mParentPageRef.get()) != null)
                    parentPage.onNewMessageArrived();
            }
                break;
            default:
                break;
        }

        if (Settings.DEBUG)
            System.out.println(eventCode.toString() + " handled");
    }

    private void showProgressDialog(final int messageId, final Runnable cancelCallback) {
        Activity activity = getActivity();
        if (activity == null)
            return;

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mProgressDialog != null)
                    mProgressDialog.dismiss();

                mProgressDialog = Utils.showProgressDialog(getContext(), getContext().getString(messageId), cancelCallback);
            }
        });
    }

    private void dismissProgressDialog()
    {
        Activity activity = getActivity();
        if (activity == null)
            return;

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mProgressDialog != null)
                    mProgressDialog.dismiss();

                mProgressDialog = null;
            }
        });
    }

    private void showToast(int msgResId, int duration) {
        showToast(getContext().getString(msgResId), duration);
    }

    public void showToast(String msg, int duration)
    {
        if (mToast != null)
            mToast.cancel();

        mToast = Toast.makeText(getContext(), msg, duration);
        mToast.show();
    }

    private BaseActivity getActivity()
    {
        return BaseActivity.getCurrentActivity();
    }

    private void finishGamePage(){
        finishGamePage(true, null);
    }

    private void finishGamePage(final boolean closePage, final Runnable finishedGameCallback){
        Runnable finishTask = new Runnable() {
            @Override
            public void run() {
                initGameNativeIfNeeded();
                disableRemoteControllerNative(sNativeHandle);//stop remote controller

                final BaseActivity activity = BaseActivity.getCurrentActivity();
                if (activity != null) {
                    activity.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            finalizeGamePageOnUiThread(closePage);

                            // run the callback
                            if (finishedGameCallback != null)
                                finishedGameCallback.run();
                        }
                    });
                }
                else {
                    // no activity? run the callback immediately
                    if (finishedGameCallback != null)
                        finishedGameCallback.run();
                }
            }
        };

        if (mRenderer != null && mRenderer.getThreadId() == Thread.currentThread().getId())
        {
            //execute the finishing task immediately
            finishTask.run();
        }
        else
        {
            //queue the finishing task on GL thread
            queueEvent(finishTask);
        }
    }

    private void finalizeGamePageOnUiThread(boolean closePage) {
        if (Looper.myLooper() != Looper.getMainLooper())
            throw new RuntimeException("GameSurfaceView.finalizeGamePageOnUiThread() must be called on UI thread");

        dismissProgressDialog();//dismiss progress dialog

        cancelExistingAsyncOps();// cancel any existing google play games match

        // calculate total play time
        long currentTime = System.nanoTime();

        aggregatePlayTimeAndResetTimer(currentTime, true);

        aggregateMultiplayerTimeAndResetTimer(currentTime, true);

        GamePage parentPage = null;
        if (mParentPageRef != null && (parentPage = mParentPageRef.get()) != null)
            parentPage.finishWithStats(mPlayDuration, mMulitplayerDuration, closePage);
    }

    private synchronized void aggregatePlayTimeAndResetTimer(long currentTime, boolean resetToZero) {
        if (mLastStartTime != 0) {
            mPlayDuration += currentTime - mLastStartTime;
        }

        mLastStartTime = resetToZero ? 0 : currentTime;
    }

    private synchronized void aggregateMultiplayerTimeAndResetTimer(long currentTime, boolean resetToZero) {
        if (mLastStartTimeMultiplayer != 0)
        {
            mMulitplayerDuration += currentTime - mLastStartTimeMultiplayer;
        }

        mLastStartTimeMultiplayer = resetToZero ? 0 : currentTime;
    }

    /*---- internet invitation -*/
    private static class TestInternetConnectivityQueryAdapter extends AsyncQuery.Cancelable<Boolean> {
        private long mConnHandlerPtr;
        private AsyncQuery<Boolean> mCallback;

        public TestInternetConnectivityQueryAdapter(AsyncQuery<Boolean> callback) {
            mCallback = callback;
        }

        public synchronized void setConnectionHandlerPtr(long connHandlerPtr) {
            mConnHandlerPtr = connHandlerPtr;
        }

        @Override
        protected synchronized void cancelImpl() {
            AsyncTask.execute(new Runnable() {
                @Override
                protected void runImpl() {
                    cleanupConnectionHandler();
                }
            });
        }

        @Override
        protected void runImpl(final Boolean result) {
            // run the callback on main thread
            new Handler(Looper.getMainLooper()).post(new java.lang.Runnable() {
                @Override
                public void run() {
                    // invoke supplied callback
                    if (mCallback != null)
                        mCallback.run(result);

                    // stop the handler
                    cancelImpl();
                }
            });
        }

        private synchronized void cleanupConnectionHandler() {
            stopRemoteInternetConnectivityTestNative(mConnHandlerPtr);
            mConnHandlerPtr = 0;
        }
    };

    private class GoogleMatchCreatedCallback implements OnCompleteListener<TurnBasedMatch>, AsyncOperation {
        private final Bundle mMatchSettings;
        private final String mInviteData;

        public GoogleMatchCreatedCallback(Bundle matchSettings, String inviteData) {
            mMatchSettings = matchSettings;
            mInviteData = inviteData;
        }

        @Override
        public synchronized void onComplete(@NonNull Task<TurnBasedMatch> task) {
            if (mCanceled)
                return;
            if (task.isSuccessful()) {
                handleMatchCreatedGoogle(BaseActivity.getCurrentActivity(), task.getResult(), mMatchSettings, mInviteData);
            } else {
                commonErrorHandlingGoogle(task.getException());
            }
        }

        @Override
        public synchronized void cancel() {
            mCanceled = true;
        }

        private volatile boolean mCanceled = false;
    }

    private static String createPublicServerMetaData(final String publicName, final String inviteData, final int context) {
        try {
            JSONObject jsonObject = new JSONObject();
            String gameName = loadedGameNameNative(sNativeHandle);
            int dotIndex = gameName.lastIndexOf('.');
            if (dotIndex > 0) {
                gameName = gameName.substring(0, dotIndex);
            }
            jsonObject.put(Settings.PUBLIC_SERVER_ROM_NAME_KEY, gameName);
            jsonObject.put(Settings.PUBLIC_SERVER_INVITE_DATA_KEY, inviteData);
            jsonObject.put(Settings.PUBLIC_SERVER_NAME_KEY, publicName);

            String re = Base64.encodeToString(jsonObject.toString().getBytes("UTF-8"), Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP);
            return re;
        } catch (Exception e) {
            e.printStackTrace();

            return null;
        }
    }

    private static void displayInviteDialog(final String inviteData, final int context) {
        if (Settings.DEBUG)
            System.out.println("displaying invite dialog with invite_data=" + inviteData);

        final Activity activity = BaseActivity.getCurrentActivity();
        if (activity != null) {
            activity.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    GameSurfaceView javaHandle = sJavaHandle.get();
                    if (javaHandle != null)
                    {
                        javaHandle.dismissProgressDialog();

                        switch (context) {
                            case FB_INVITE_CTX:
                                javaHandle.displayInviteDialogFb(inviteData);
                                break;
                            case GOOGLE_INVITE_CTX:
                                javaHandle.displayInviteDialogGoogle(inviteData);
                                break;
                        }
                    }//if (javaHandle != null)
                }
            });
        }//if (activity != null)
    }

    private static void clientAboutToConnectToRemote(final String invite_id, final String remoteGUID, final int context) {
        switch (context) {
            case FB_INVITE_CTX:
                FBUtils.deleteGraphObj(invite_id);
                break;
            case GOOGLE_INVITE_CTX:
                break;
        }
    }

    private static void clientConnectivityTestCallback(
            final String invite_id,
            final String remoteGUID,
            final Object callback,
            boolean connectOk) {
        if (callback != null && callback instanceof AsyncQuery) {
            ((AsyncQuery) callback).run(connectOk);
        }
    }

    private void displayInviteDialogFb(final String inviteData) {
        GameRequestContent content = new GameRequestContent.Builder()
                .setMessage("Come play with me")
                .setTitle("Multiplayer invitation")
                .setData(inviteData)
                .build();
        mFbInviteDialog.show(content);
    }

    private void displayInviteDialogGoogle(final String inviteData) {
        BaseActivity activity = getActivity();
        if (activity == null)
            return;

        // display Google Play Games Invite dialog
        activity.displayGooglePlayGamesInviteUIWithoutRoom(
                1,
                false,
                new BaseActivity.GooglePlayGamesMatchCreateCallback<Bundle>() {
                    @Override
                    public void onComplete(BaseActivity activity, Bundle result) {
                        handleInviteDialogResultGoogle(activity, result, inviteData);
                    }

                    @Override
                    public void onActivityResultError(BaseActivity activity, int resultCode) {
                        fatalError(String.format(activity.getString(R.string.google_invite_ui_generic_err_msg), resultCode));
                    }

                    @Override
                    public void onError(BaseActivity activity, Exception task) {
                        commonErrorHandlingGoogle(task);
                    }

                    @Override
                    public void onCancel(BaseActivity activity) {
                        finishGamePage();
                    }
                });
    }

    private void handleInviteDialogResultGoogle(final BaseActivity activity, final Bundle settings, final String inviteData) {
        hostNewMatchGoogle(activity, settings, inviteData);
    }

    private void hostNewMatchGoogle(final BaseActivity activity, final Bundle matchSettings, final String inviteData) {
        cancelExistingAsyncOps();

        final Bundle matchSettingsCopy = new Bundle(matchSettings);

        ArrayList<String> invitees = matchSettingsCopy.getStringArrayList(Games.EXTRA_PLAYER_IDS);

        // Get automatch criteria
        int minAutoPlayers = matchSettingsCopy.getInt(Multiplayer.EXTRA_MIN_AUTOMATCH_PLAYERS, 0);
        int maxAutoPlayers = matchSettingsCopy.getInt(Multiplayer.EXTRA_MAX_AUTOMATCH_PLAYERS, 0);

        TurnBasedMatchConfig.Builder builder = TurnBasedMatchConfig.builder();
        if (invitees != null) {
            if (invitees.size() > 0) {
                // display waiting dialog until the invited player connected to us
                showProgressDialog(R.string.google_match_invite_postprocessing, new Runnable() {
                    @Override
                    public void run() {
                        finishGamePage();
                    }
                });

                mWaitingForInviteResponse = true;
            }

            builder.addInvitedPlayers(invitees);
        }
        if (minAutoPlayers > 0) {
            builder.setAutoMatchCriteria(
                    RoomConfig.createAutoMatchCriteria(minAutoPlayers, maxAutoPlayers, Settings.GOOGLE_MATCH_P1_MASK));
        }

        activity.getGoogleMultiplayerClient().createMatch(builder.build())
                .addOnCompleteListener(mGoogleMultiplayerMatchCreatedCallback = new GoogleMatchCreatedCallback(matchSettingsCopy, inviteData));
    }

    private void handleMatchCreatedGoogle(final BaseActivity activity, TurnBasedMatch match, final Bundle settings, final String inviteData) {
        GooglePlayGamesMatchHandler.Host hostHandler = new GooglePlayGamesMatchHandler.Host();
        mGoogleMultiplayerMatch = hostHandler;

        final boolean isAutoMatch = settings.getInt(Multiplayer.EXTRA_MIN_AUTOMATCH_PLAYERS, 0) > 0;
        if (isAutoMatch) {
            hostHandler.setTurnWaitTimeoutTimeMaxDeviation(Settings.GOOGLE_AUTO_MATCH_WAIT_FOR_TURN_TIMEOUT_RAND_MS);
            hostHandler.setDefaultTurnWaitTimeout(Settings.GOOGLE_AUTO_MATCH_WAIT_FOR_TURN_TIMEOUT_MS);
        }

        hostHandler.start(activity, match, inviteData, new GooglePlayGamesMatchHandler.Host.ServerCallback() {
            @Override
            public void onComplete(GooglePlayGamesMatchHandler.Host handler, final BaseActivity activity, @NonNull String matchId) {
                if (isAutoMatch)
                {
                    // we will wait until client connected to us through p2p connection within a specified time.
                    // if timeout happens, we will look for another match
                    mGoogleMultiplayerMatchWaitingToBeConnectedFallbackOp = activity.runOnUiThreadDelayed(new Runnable() {
                        @Override
                        public void run() {
                            hostNewMatchGoogle(BaseActivity.getCurrentActivity(), settings, inviteData);
                        }
                    }, Settings.GOOGLE_AUTO_MATCH_WAIT_TO_BE_CONNECTED_TIMEOUT_MS);
                }
            }

            @Override
            public void onMatchInvalid(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId) {
                // create new auto match
                if (isAutoMatch)
                    hostNewMatchGoogle(activity, settings, inviteData);
                else
                    fatalError(activity.getString(R.string.google_match_handshake_invalid_msg));
            }

            @Override
            public void onFatalError(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId, Exception exception) {
                if (isAutoMatch) {
                    if (activity.isGoogleSignedIn()) // still connected, then ignore it and host new match
                    {
                        exception.printStackTrace();
                        hostNewMatchGoogle(activity, settings, inviteData);
                        return;
                    }
                }
                fatalError(exception.getLocalizedMessage());
            }

            @Override
            public void onTimeout(GooglePlayGamesMatchHandler handler, BaseActivity activity, String matchId) {
                // create new auto match
                if (isAutoMatch)
                    hostNewMatchGoogle(activity, settings, inviteData);
                else
                    errorMessage(activity.getString(R.string.google_match_not_respond_msg),
                            new Runnable() {
                                @Override
                                public void run() {
                                    reinviteNewFriendForRemoteControllerInternet();
                                }
                            }, new Runnable() {
                                @Override
                                public void run() {
                                    finishGamePage();
                                }
                            });
            }

            @Override
            public void onMatchRemoved(GooglePlayGamesMatchHandler handler, BaseActivity activity, @NonNull String matchId) {
                // create new auto match
                if (isAutoMatch)
                    hostNewMatchGoogle(activity, settings, inviteData);
                else {
                    if (mWaitingForInviteResponse) {
                        errorMessage(activity.getString(R.string.google_match_canceled_msg),
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        reinviteNewFriendForRemoteControllerInternet();
                                    }
                                }, new Runnable() {
                                    @Override
                                    public void run() {
                                        finishGamePage();
                                    }
                                });
                    }
                }
            }
        });
    }

    private void commonErrorHandlingGoogle(Exception exception) {
        if (exception == null) {
            fatalError("INTERNAL_GPG_ERRORR");
            return;
        }
        // There was an error. Show the error.
        if (exception instanceof ApiException) {
            ApiException apiException = (ApiException) exception;
            // TODO
        }
        fatalError(exception.getLocalizedMessage());
    }

    /*------------------------------------- */

    private synchronized void cancelExistingMatchGoogle() {
        BaseActivity activity = BaseActivity.getCurrentActivity();
        if (activity == null)
            return;

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    System.out.println("GameSurfaceView.cancelExistingMatchGoogle()");

                    // cancel match on Google Play Game Services
                    if (mGoogleMultiplayerMatch != null) {
                        mGoogleMultiplayerMatch.stop();
                    }
                    if (mGoogleMultiplayerMatchWaitingToBeConnectedFallbackOp != null) {
                        mGoogleMultiplayerMatchWaitingToBeConnectedFallbackOp.cancel();
                        mGoogleMultiplayerMatchWaitingToBeConnectedFallbackOp = null;
                    }

                    if (mGoogleMultiplayerMatchCreatedCallback != null) {
                        mGoogleMultiplayerMatchCreatedCallback.cancel();
                        mGoogleMultiplayerMatchCreatedCallback = null;
                    }

                    mGoogleMultiplayerMatch = null;
                    mWaitingForInviteResponse = false;

                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });
    }

    private synchronized void cancelExistingAsyncOps() {
        BaseActivity activity = BaseActivity.getCurrentActivity();
        if (activity == null)
            return;

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    System.out.println("GameSurfaceView.cancelExistingAsyncOps()");

                    cancelExistingMatchGoogle();

                    if (mWifiDirectServer != null) {
                        mWifiDirectServer.cancel();
                        mWifiDirectServer = null;
                    }

                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });
    }

    /*---- chat handler ----*/
    private void notifyChatHistoryUpdated() {
        post(new Runnable() {
            @Override
            public void run() {
                mChatHistoryViewAdapter.notifyDataSetChanged();
            }
        });
    }

    /*---- machine result handler ----*/
    private static void machineResultHandle(int resultValue) {
        machineResultHandle(resultValue, true);
    }

    private static void machineResultHandle(int resultValue, boolean errorIsFatal) {
        if (NesMachineResult.failed(resultValue))
        {
            String error = "";
            switch (resultValue)
            {
                case NesMachineResult.RESULT_ERR_INVALID_FILE:
                    error = "Error: Invalid file ";
                    break;

                case NesMachineResult.RESULT_ERR_OUT_OF_MEMORY:
                    error = "Error: Out of Memory";
                    break;

                case NesMachineResult.RESULT_ERR_CORRUPT_FILE:
                    error = "Error: Corrupt or Missing File ";
                    break;

                case NesMachineResult.RESULT_ERR_UNSUPPORTED_MAPPER:
                    error = "Error: Unsupported Mapper";
                    break;

                case NesMachineResult.RESULT_ERR_MISSING_BIOS:
                    error = "Error: Missing Fds BIOS";
                    break;

                default:
                    error = "Error: " + resultValue;
                    break;
            }

            if (errorIsFatal)
                fatalError(error);
            else
                errorMessage(error);
        }//if (NesMachineResult.failed(resultValue)
    }

    /*--- error handler ---*/
    private void fatalError(int messageResId) {
        String message = getActivity().getString(messageResId);
        fatalError(message);
    }

    private static void fatalError(final String message) {
        final GameSurfaceView javaHandle = sJavaHandle.get();
        if (javaHandle == null)
            return;

        errorMessage(message, new Runnable() {
            @Override
            public void run() {
                javaHandle.finishGamePage();
            }
        });
    }

    private static void errorMessage(final String message) {
        errorMessage(message, null);
    }

    private static void errorMessage(final String message, final Runnable closeCallback) {
        errorMessage(message, null, closeCallback);
    }

    private static void errorMessage(final String message, final Runnable retryCallback, final Runnable closeCallback) {
        GameSurfaceView javaHandle = sJavaHandle.get();
        if (javaHandle == null)
            return;

        final Activity activity = javaHandle.getActivity();
        if (activity == null)
            return;

        activity.runOnUiThread(new Runnable() {

            @Override
            public void run() {
                if (retryCallback != null) {
                    Utils.alertDialog(activity, "Error", message, R.string.retry, new Runnable() {
                        @Override
                        public void run() {
                            retryCallback.run();
                        }
                    }, new Runnable() {
                        @Override
                        public void run() {
                            if (closeCallback != null)
                                closeCallback.run();
                        }
                    });
                } else {
                    Utils.alertDialog(activity, "Error", message, new Runnable() {
                        @Override
                        public void run() {
                            if (closeCallback != null)
                                closeCallback.run();
                        }
                    });
                }
            }
        });
    }

    /*---- misc ---*/
    private static void runOnGameThread(GLSurfaceView javaHandle, final long nativeFunctionId, final long nativeFunctionArg) {
        if (javaHandle == null)
            return;
        javaHandle.queueEvent(new Runnable() {
            @Override
            public void run() {
                invokeNativeFunction(sNativeHandle, nativeFunctionId, nativeFunctionArg);
            }
        });
    }

    static {
        initNative();
    }

    private static native void initNative();

    private static native long initGameNative(byte[] databaseData);
    private static native void destroyGameNative(long nativeHandle);
    private static native boolean verifyGameNative(long nativeHandle, String path);
    private static native boolean isGameLoadedNative(long nativeHandle);

    private static native String getLobbyServerAddressNative();
    private static native String getLobbyAppIdNative();

    private static native String loadedGameNative(long nativeHandle);
    private static native String loadedGameNameNative(long nativeHandle);

    private static native void invokeNativeFunction(long nativeHandle, long nativeFunctionId, long nativeFunctionArg);

    private native boolean loadAndStartGameNative(long nativeHandle, String path);
    private native boolean loadRemoteNative(long nativeHandle, String ip, int port, String clientName);
    private native boolean loadRemoteInternetNative(long nativeHandle, String invite_id, String invite_data, String clientGUID, String clientName, int context);
    private native boolean isRemoteConntectionViaProxyNative(long nativeHandle);
    private native void resetGameNative(long nativeHandle);
    private native void shutdownGameNative(long nativeHandle);
    private native boolean enableRemoteControllerNative(long nativeHandle, int port, String hostName, @Nullable String hostIpBound);
    private native boolean enableRemoteControllerInternetNative(long nativeHandle, String hostGUID, String hostName, int context, boolean isPublic);
    private native boolean reinviteNewFriendForRemoteControllerInternetNative(long nativeHandle);
    private native void disableRemoteControllerNative(long nativeHandle);
    private native boolean isRemoteControllerEnabledNative(long nativeHandle);//is remote controller enabled
    private native boolean isRemoteControllerConnectedNative(long nativeHandle);//is remote controller connected by client
    private native int loadStateNative(long nativeHandle, String file);
    private native int saveStateNative(long nativeHandle, String file);

    private native void setAudioVolumeNative(long nativeHandle, float gain);
    private native void setAudioRecordPermissionChangedNative(long nativeHandle, boolean hasPermission);
    private native void enableAudioInput(long nativeHandle, boolean enable);
    private native void enableUIButtonsNative(long nativeHandle, boolean enable);
    private native void enableFullScreenNative(long nativeHandle, boolean enable);
    private native void switchABTurboModeNative(long nativeHandle, boolean enableATurbo, boolean enableBTurbo); // if set, the normal A/B will become auto A/B buttons
    private native boolean setFilteringShaderNative(long nativeHandle, String vshader, String fshader, float scale, boolean videoLinearSampling);
    private native void setSpeedNative(long nativeHandle, int speed);

    // UI buttons editor
    private native void setUIButtonRectNative(long nativeHandle, int buttonCode, float x, float y, float width, float height);
    private native void setDPadRectNative(long nativeHandle, float x, float y, float size);
    private native void getUIButtonDefaultRectNative(long nativeHandle, int buttonCode, float[] values);
    private native void getDPadDefaultRectNative(long nativeHandle, float [] values);

    private native int sendMessageToRemoteNative(long nativeHandle, long id, String message);
    private native String getRemoteNameNative(long nativeHandle);

    private native void resetGameViewNative(AssetManager assetManager, long nativeHandle, int esVersionMajor, int width, int height, boolean contextRecreate);
    private native void cleanupGameViewNative(long nativeHandle);//TODO: for now, we don't explicitly call this method, instead let Android system free up the resources automatically for us
    private native void renderGameViewNative(long nativeHandle, boolean drawButtonsOnly, float buttonBoundingBoxOutlineSize);

    private native void onResumeNative(long nativeHandle);//this is called in game thread
    private native void onPauseOnUIThreadNative(long nativeHandle);//this is called in main thread

    private static native void enableLANServerDiscoveryNative(long nativeHandle, boolean enable);
    private static native void findLANServersNative(long nativeHandle, long request_id);

    private static native long createRemoteInternetConnectivityTestNative(String invite_id, String invite_data, String clientGUID, AsyncQuery<Boolean> callback);
    private static native boolean startRemoteInternetConnectivityTestNative(long clientTestHandlerPtr);
    private static native void stopRemoteInternetConnectivityTestNative(long clientTestHandlerPtr);

    private native void onJoystickMovedNative(long nativeHandle, float x, float y); // x, y must be in range [-1, 1]
    private native void onUserHardwareButtonDownNative(long nativeHandle, int buttonCode);
    private native void onUserHardwareButtonUpNative(long nativeHandle, int buttonCode);

    private native boolean onTouchBeganNative(long nativeHandle, int id, float x, float y);
    private native boolean onTouchMovedNative(long nativeHandle, int id, float x, float y);
    private native boolean onTouchEndedNative(long nativeHandle, int id, float x, float y);
}



