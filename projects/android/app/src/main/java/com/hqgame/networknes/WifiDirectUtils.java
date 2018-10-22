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

import android.net.NetworkInfo;
import android.net.wifi.WpsInfo;
import android.net.wifi.p2p.WifiP2pConfig;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pGroup;
import android.net.wifi.p2p.WifiP2pInfo;
import android.net.wifi.p2p.WifiP2pManager;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceInfo;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceRequest;
import android.net.wifi.p2p.nsd.WifiP2pServiceInfo;
import android.net.wifi.p2p.nsd.WifiP2pServiceRequest;
import android.support.annotation.NonNull;

import java.net.InetAddress;
import java.util.HashMap;
import java.util.Map;

public class WifiDirectUtils {
    private static final String SERVICE_INSTANCE_NAME = "multiness";
    private static final String SERVICE_TYPE = "_nesgames._tcpudp";

    interface FailureCallback {
        void onFailure(int code);
    }

    interface ServerCreateCallback extends FailureCallback {
        void onServerCreatedSuccess(InetAddress ipAddress);
        void onServerPublishedSuccess();
    }

    interface ServersListingCallback extends FailureCallback {
        void onDnsSdTxtRecordAvailable(String publishedName, int publishedPort, WifiP2pDevice device);
        void onDnsSdServiceAvailable(WifiP2pDevice resourceType);
    }

    interface ServerConnectCallback extends FailureCallback {
        void onSuccess(InetAddress serverIpAddress);
    }

    interface ServerHandler extends AsyncOperation {
        void setAvailable(boolean e); // set server to be available for joining
    }

    interface ServerConnectHandler extends AsyncOperation {
        void detach(); // detach instead of cancel
    }

    public static abstract class CancelableActionListener implements AsyncOperation, WifiP2pManager.ActionListener {
        @Override
        final public synchronized void onSuccess() {
            if (mCanceled)
                return;
            System.out.println("CancelableActionListener.onSuccess()");
            onSuccessImpl();
        }


        @Override
        final public synchronized void onFailure(int reason) {
            if (mCanceled)
                return;
            System.out.println("CancelableActionListener.onFailure(" + reason + ")");
            onFailureImpl(reason);
        }

        @Override
        public synchronized void cancel() {
            mCanceled = true;
        }

        protected abstract void onSuccessImpl();
        protected abstract void onFailureImpl(int reason);

        protected volatile boolean mCanceled = false;
    }

    private static abstract class BaseHandler<T extends FailureCallback> extends CancelableActionListener {
        protected WifiP2pManager mManager;
        protected final WifiP2pManager.Channel mChannel;

        protected final T mCallback;

        protected BaseHandler(@NonNull WifiP2pManager manager, @NonNull WifiP2pManager.Channel channel, T callback) {
            mManager = manager;
            mChannel = channel;

            mCallback = callback;

            // try to disconnect from existing room first
            mManager.requestGroupInfo(mChannel, new WifiP2pManager.GroupInfoListener() {
                @Override
                public void onGroupInfoAvailable(WifiP2pGroup group) {
                    if (mCanceled)
                        return;

                    System.out.println("BaseHandler.onGroupInfoAvailable");

                    if (group != null) {
                        mManager.removeGroup(mChannel, new WifiP2pManager.ActionListener() {
                            @Override
                            public void onSuccess() {
                                if (mCanceled)
                                    return;
                                System.out.println("BaseHandler.removeGroup.onSuccess");
                                onReady();
                            }

                            @Override
                            public void onFailure(int reason) {
                                if (mCanceled)
                                    return;
                                System.out.println("BaseHandler.removeGroup.onFailure(" + reason + ")");
                                // TODO: right now, just proceed as usual
                                onReady();

                            }
                        });
                    } else {
                        onReady();
                    }
                }
            });
        }

        @Override
        final protected void onFailureImpl(int reason) {
            mCallback.onFailure(reason);
        }

        protected abstract void onReady();
    }

    private static class ServerHandlerAdapter extends BaseHandler<ServerCreateCallback>
            implements  ServerHandler,
                        WifiP2pManager.ConnectionInfoListener,
                        BaseActivity.WifiDirectStateUpdateCallback
    {
        private final CancelableActionListener mServerPublishCallbackAdapter;
        private final WifiP2pServiceInfo mServiceInfo;
        private volatile boolean mAvaialble = false;
        private boolean mCreated = false;

        private  ServerHandlerAdapter(@NonNull WifiP2pManager manager, @NonNull  WifiP2pManager.Channel channel, ServerCreateCallback callback,  WifiP2pServiceInfo serviceInfo) {
            super(manager, channel, callback);

            mServiceInfo = serviceInfo;

            // create callback adapter
            mServerPublishCallbackAdapter = new CancelableActionListener() {
                @Override
                protected void onSuccessImpl() {
                    mCallback.onServerPublishedSuccess();
                }

                @Override
                protected void onFailureImpl(int reason) {
                    mCallback.onFailure(reason);
                }
            };
        }

        @Override
        protected void onReady() {
            // register state change listener
            BaseActivity.getCurrentActivity().registerWifiDirectCallback(this);

            // create the room
            System.out.println("ServerHandlerAdapter.createGroup");

            // now create new room
            mManager.createGroup(mChannel, this);
        }

        // cancel all operations and remove the room
        @Override
        public synchronized void cancel() {
            if (mManager == null)
                return;
            System.out.println("ServerHandlerAdapter.cancel");

            super.cancel();

            mServerPublishCallbackAdapter.cancel();

            setAvailable(false);

            mManager.removeGroup(mChannel, null);

            BaseActivity.getCurrentActivity().unregisterWifiDirectCallback(this);

            mCreated = false;

            // invalidate this instance
            mManager = null;
        }

        @Override
        public synchronized void setAvailable(boolean available) {
            if (mManager == null)
                return;

            if (mAvaialble == available)
                return;
            System.out.println("ServerHandlerAdapter.setAvailable(" + available + ")");

            mAvaialble = available;

            if (!mCreated) // if server hasn't been created yet, wait for it and publish our service later
                return;

            if (available) {
                registerServer();
            } else {
                mManager.removeLocalService(mChannel, mServiceInfo, null);
            }
        }

        private void registerServer() {
            System.out.println("ServerHandlerAdapter.registerServer");
            // remove old one
            mManager.removeLocalService(mChannel, mServiceInfo, new WifiP2pManager.ActionListener() {
                @Override
                public void onSuccess() {
                    System.out.println("ServerHandlerAdapter.removeLocalService.onSuccess");
                    mManager.addLocalService(mChannel, mServiceInfo, mServerPublishCallbackAdapter);
                }

                @Override
                public void onFailure(int reason) {
                    System.out.println("ServerHandlerAdapter.removeLocalService.onFailure(" + reason + ")");
                    mManager.addLocalService(mChannel, mServiceInfo, mServerPublishCallbackAdapter);
                }
            });
        }

        // this will be invoked during server creation
        @Override
        protected void onSuccessImpl() {
            // do nothing, wait for connection state change
            System.out.println("ServerHandlerAdapter.onSuccessImpl");
        }

        // WifiDirectStateUpdateCallback
        @Override
        public void onDisabled() {

        }

        @Override
        public void onConnectionStateChanged(NetworkInfo info) {
            if (mCanceled)
                return;

            System.out.println("ServerHandlerAdapter.onConnectionStateChanged");
            if (info.isConnected()) {
                mManager.requestConnectionInfo(mChannel, this);
            }
        }

        // ConnectionInfoListener
        @Override
        public void onConnectionInfoAvailable(WifiP2pInfo info) {
            if (mCanceled)
                return;
            System.out.println("ServerHandlerAdapter.onConnectionInfoAvailable");

            if (info.isGroupOwner && !mCreated) {

                mCreated = true;

                mCallback.onServerCreatedSuccess(info.groupOwnerAddress);

                if (mAvaialble)
                    registerServer();
            }
        }
    }

    private static class ServersListingHandlerAdapter extends BaseHandler<ServersListingCallback> implements WifiP2pManager.DnsSdTxtRecordListener, WifiP2pManager.DnsSdServiceResponseListener {
        private final WifiP2pServiceRequest mServiceRequest;

        ServersListingHandlerAdapter(@NonNull WifiP2pManager manager, @NonNull  WifiP2pManager.Channel channel, ServersListingCallback callback, WifiP2pServiceRequest request) {
            super(manager, channel, callback);

            mServiceRequest = request;
        }

        private void start() {
            // add service request
            mManager.addServiceRequest(mChannel, mServiceRequest, this);

            // start service discovery
            System.out.println("ServersListingHandlerAdapter.discoverServices");
            mManager.discoverServices(mChannel, this);
        }

        @Override
        protected void onReady() {

            // register discovery callbacks
            mManager.setDnsSdResponseListeners(mChannel, this, this);

            // remove old service request just to be sure
            mManager.removeServiceRequest(mChannel, mServiceRequest, new WifiP2pManager.ActionListener() {
                @Override
                public void onSuccess() {
                    if (mCanceled)
                        return;

                    System.out.println("ServersListingHandlerAdapter.removeServiceRequest.onSuccess");

                    start();
                }

                @Override
                public void onFailure(int reason) {
                    if (mCanceled)
                        return;

                    System.out.println("ServersListingHandlerAdapter.removeServiceRequest.onFailure(" + reason + ")");

                    start();
                }
            });
        }

        // DnsSdServiceResponseListener
        @Override
        public void onDnsSdServiceAvailable(String instanceName, String registrationType, WifiP2pDevice srcDevice) {
            if (mCanceled || !instanceName.equalsIgnoreCase(SERVICE_INSTANCE_NAME))
                return;

            System.out.println("ServersListingHandlerAdapter.onDnsSdServiceAvailable");

            mCallback.onDnsSdServiceAvailable(srcDevice);
        }

        // DnsSdTxtRecordListener
        @Override
        public void onDnsSdTxtRecordAvailable(String fullDomainName, Map<String, String> txtRecordMap, WifiP2pDevice srcDevice) {
            if (mCanceled || txtRecordMap == null)
                return;

            System.out.println("ServersListingHandlerAdapter.onDnsSdTxtRecordAvailable");

            try {
                String name = txtRecordMap.get(Settings.GAME_ACTIVITY_REMOTE_HOST_KEY);
                int port = Integer.parseInt(txtRecordMap.get(Settings.GAME_ACTIVITY_REMOTE_PORT_KEY));

                if (name == null)
                    return;

                mCallback.onDnsSdTxtRecordAvailable(name, port, srcDevice);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        // CancelableActionListener
        @Override
        protected void onSuccessImpl() {
            // do nothing
            System.out.println("ServersListingHandlerAdapter.onSuccessImpl");
        }

        @Override
        public synchronized void cancel() {
            if (mManager == null || mCanceled)
                return;

            System.out.println("ServersListingHandlerAdapter.cancel");

            super.cancel();

            mManager.removeServiceRequest(mChannel, mServiceRequest, null);

            mManager = null;
        }
    }

    private static class ServerConnectHandlerAdapter extends BaseHandler<ServerConnectCallback>
            implements ServerConnectHandler,
                       WifiP2pManager.ConnectionInfoListener,
                       BaseActivity.WifiDirectStateUpdateCallback
    {
        private final WifiP2pDevice mDevice;

        public ServerConnectHandlerAdapter(@NonNull WifiP2pManager manager,
                                           @NonNull  WifiP2pManager.Channel channel,
                                           WifiP2pDevice device,
                                           ServerConnectCallback callback) {
            super(manager, channel, callback);

            mDevice = device;
        }

        @Override
        protected void onReady() {

            WifiP2pConfig config = new WifiP2pConfig();
            config.deviceAddress = mDevice.deviceAddress;
            config.wps.setup = WpsInfo.PBC;

            // register callback to receive connection status
            BaseActivity.getCurrentActivity().registerWifiDirectCallback(this);

            // now connect to wifi direct server
            if (Settings.DEBUG)
                System.out.println("ServerConnectHandlerAdapter.connecting(" + mDevice.deviceAddress + ")");
            else
                System.out.println("ServerConnectHandlerAdapter.connecting");
            mManager.connect(mChannel, config, this);
        }

        // ConnectionInfoListener
        @Override
        public synchronized void onConnectionInfoAvailable(WifiP2pInfo info) {
            if (mCanceled)
                return;

            System.out.println("ServerConnectHandlerAdapter.onConnectionInfoAvailable(" + info.groupOwnerAddress + ")");
            mCallback.onSuccess(info.groupOwnerAddress);
        }

        // CancelableActionListener
        @Override
        protected void onSuccessImpl() {
            System.out.println("ServerConnectHandlerAdapter.onSuccessImpl");

            // do nothing. onConnectionStateChanged() should do the job
        }

        @Override
        public synchronized void cancel() {
            if (mManager == null)
                return;

            System.out.println("ServerConnectHandlerAdapter.cancel");

            mManager.cancelConnect(mChannel, null);
            mManager.removeGroup(mChannel, null);

            detach();
        }

        // ServerConnectHandler
        @Override
        public void detach() {
            if (mManager == null)
                return;

            System.out.println("ServerConnectHandlerAdapter.detach");

            super.cancel();

            // unregister broadcast receiver callback
            BaseActivity.getCurrentActivity().unregisterWifiDirectCallback(this);

            mManager = null;
        }

        // WifiDirectStateUpdateCallback
        @Override
        public synchronized void onDisabled() {
            if (mCanceled)
                return;
            // TODO
        }

        @Override
        public void onConnectionStateChanged(NetworkInfo info) {
            if (mCanceled)
                return;
            System.out.println("ServerConnectHandlerAdapter.onConnectionStateChanged");

            if (info.isConnected()) {
                // request server's ip address
                mManager.requestConnectionInfo(mChannel, this);
            }
        }
    }

    public static ServerHandler createServer(@NonNull WifiP2pManager manager, @NonNull  WifiP2pManager.Channel channel, @NonNull String hostName, int port, @NonNull final ServerCreateCallback callback) {
        //  Create a string map containing information about your service.
        HashMap<String, String> record = new HashMap<>();
        record.put(Settings.GAME_ACTIVITY_REMOTE_PORT_KEY, String.valueOf(port));
        record.put(Settings.GAME_ACTIVITY_REMOTE_HOST_KEY, hostName);

        // Service information.  Pass it an instance name, service type
        // _protocol._transportlayer , and the map containing
        // information other devices will want once they connect to this one.
        WifiP2pDnsSdServiceInfo serviceInfo =
                WifiP2pDnsSdServiceInfo.newInstance(SERVICE_INSTANCE_NAME, SERVICE_TYPE, record);

        return new ServerHandlerAdapter(manager, channel, callback, serviceInfo);
    }

    public static AsyncOperation listServers(@NonNull WifiP2pManager manager, @NonNull  WifiP2pManager.Channel channel, ServersListingCallback callback) {
        WifiP2pDnsSdServiceRequest serviceRequest =
                // WifiP2pDnsSdServiceRequest.newInstance(SERVICE_INSTANCE_NAME, SERVICE_TYPE);
                WifiP2pDnsSdServiceRequest.newInstance();

        return new ServersListingHandlerAdapter(manager, channel, callback, serviceRequest);
    }

    public static ServerConnectHandler connectToServer(@NonNull WifiP2pManager manager, @NonNull  WifiP2pManager.Channel channel, WifiP2pDevice device, ServerConnectCallback callback) {
        return new ServerConnectHandlerAdapter(manager, channel, device, callback);
    }
}
