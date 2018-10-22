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

import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pManager;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.net.InetAddress;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Timer;
import java.util.TimerTask;

public class WifiDirectServerChooserPage extends BaseInvitationsPage<WifiDirectServerChooserPage.ServerInfo> {
    private UniqueArray<ServerInfo> mServerInfos = new UniqueArray<>();
    // this map contains Txt Records for all services discovered, may not be the service we wants
    private HashMap<String, Pair<String, Integer>> mServiceTxtRecords = new HashMap<>();
    private HashSet<String> mValidServers = new HashSet<>();

    private WifiDirectUtils.ServerConnectHandler mConnectionOp = null;
    private AsyncOperation mServerListingTaskOp = null;

    private ServerInfo mPreRefreshSelectedServer = null;

    private Timer autoRefreshTimer = null;

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View v = super.onCreateViewWithDefaultToolbar(inflater, container, savedInstanceState);

        // change label
        setLabel(getString(R.string.title_activity_wifi_direct_server_chooser_page));
        // enable progress bar
        setProgressBarVisible(true);

        // we don't support delete
        View deleteBtn = v.findViewById(R.id.btnDeleteRoom);
        if (deleteBtn != null)
            deleteBtn.setVisibility(View.GONE);

        return v;
    }

    @Override
    public void onResume() {
        super.onResume();

        //auto refresh every 30s
        autoRefreshTimer = new Timer();
        autoRefreshTimer.scheduleAtFixedRate(new TimerTask() {
                                                 @Override
                                                 public void run() {
                                                     runOnUiThread(new Runnable() {
                                                         @Override
                                                         public void run() {
                                                             refresh();
                                                         }
                                                     });
                                                 }
                                             },
                30000,
                30000);
    }

    @Override
    public void onPause() {
        super.onPause();

        autoRefreshTimer.cancel();
        autoRefreshTimer = null;

        cancelAllTasks();
    }

    @Override
    protected void refresh() {
        // retain the selected item
        mPreRefreshSelectedServer = getSelectedInvitation();

        super.refresh();
    }

    @Override
    protected boolean invitationsAreEqual(ServerInfo lhs, ServerInfo rhs) {
        return lhs != null && lhs.equals(rhs);
    }

    @Override
    protected void deleteInvitation(@NonNull ServerInfo serverInfo, Runnable finishCallback) {
        // ignore
    }

    @Override
    protected void acceptedInvitation(@NonNull final ServerInfo serverInfo) {
        WifiP2pManager manager = getBaseActivity().getWifiDirectManager();
        WifiP2pManager.Channel channel = getBaseActivity().getWifiDirectChannel();
        if (manager == null || channel == null) {
            displayErrorDialog(R.string.wifi_direct_not_supported, null);
            return;
        }

        cancelConnectionTask();

        // show progress dialog
        showProgressDialog(getString(R.string.connecting_to_host), new Runnable() {
            @Override
            public void run() {
                cancelConnectionTask();
            }
        });

        // start connecting to wifi direct server, obtain its IP then start Game Page
        mConnectionOp = WifiDirectUtils.connectToServer(manager, channel, serverInfo.device, new WifiDirectUtils.ServerConnectCallback() {
            @Override
            public void onSuccess(InetAddress serverIpAddress) {
                startGame(serverInfo, serverIpAddress);
            }

            @Override
            public void onFailure(int code) {
                onWifiDirectError(code);
            }
        });
    }

    @Override
    protected AsyncOperation doFetchInvitations(AsyncQuery<Boolean> finishCallback) {
        WifiP2pManager manager = getBaseActivity().getWifiDirectManager();
        WifiP2pManager.Channel channel = getBaseActivity().getWifiDirectChannel();
        if (manager == null || channel == null) {
            displayErrorDialog(R.string.wifi_direct_not_supported, null);
            return null;
        }

        cancelAllTasks();

        mServerInfos.clear();
        mServiceTxtRecords.clear();
        mValidServers.clear();

        mServerListingTaskOp = WifiDirectUtils.listServers(manager, channel, new WifiDirectUtils.ServersListingCallback() {
            @Override
            public void onDnsSdTxtRecordAvailable(String publishedName, int publishedPort, WifiP2pDevice device) {
                updateIfExist(publishedName, publishedPort, device);
                mServiceTxtRecords.put(device.deviceAddress.toLowerCase(), new Pair<String, Integer>(publishedName, publishedPort));
            }

            @Override
            public void onDnsSdServiceAvailable(WifiP2pDevice resourceType) {
                String deviceAddress = resourceType.deviceAddress.toLowerCase();
                mValidServers.add(deviceAddress);

                Pair<String, Integer> record = mServiceTxtRecords.get(deviceAddress);
                if (record != null) {
                    addOrUpdate(record.first, record.second, resourceType);
                }
            }

            @Override
            public void onFailure(int reasonCode) {
                onWifiDirectError(reasonCode);
            }
        });

        // notify caller immediately without any data changes,
        // since we don't know when the discovery will complete
        if (finishCallback != null)
            finishCallback.run(false);

        return mServerListingTaskOp;
    }

    @Override
    protected ServerInfo getInvitation(int index) {
        try {
            return mServerInfos.get(index);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    @Override
    protected int getNumInvitations() {
        return mServerInfos.size();
    }

    @Override
    protected View createInvitationItemView(
            @NonNull ServerInfo serverInfo,
            LayoutInflater inflater,
            View convertView,
            ViewGroup parent,
            QueryCallback<Boolean> selectedItemChangedCallback) {
        View vi = convertView;
        if (vi == null) // we use the same item layout as LAN server chooser page
            vi = inflater.inflate(R.layout.lan_server_item_layout, parent, false);

        TextView descView = (TextView) vi.findViewById(R.id.lan_server_desc_txt_view);
        TextView addrView = (TextView) vi.findViewById(R.id.lan_server_address_txt_view);

        descView.setText(serverInfo.name);
        addrView.setText(serverInfo.toString());

        // this is the server that was selected before the refresh, so tell base class
        // to set it as current selected server
        if (serverInfo.equals(mPreRefreshSelectedServer) && selectedItemChangedCallback != null) {
            selectedItemChangedCallback.run(true);

            mPreRefreshSelectedServer = null;
        }

        return vi;
    }

    private void updateIfExist(String hostName, int port, WifiP2pDevice device) {
        if (!mValidServers.contains(device.deviceAddress.toLowerCase()))
            return;

        addOrUpdate(hostName, port, device);
    }

    private void addOrUpdate(String hostName, int port, WifiP2pDevice device) {
        ServerInfo info = new ServerInfo(hostName, port, device);
        int idx = mServerInfos.getIndex(info);
        if (idx < 0)
            mServerInfos.add(info);
        else {
            ServerInfo existingInfoRecord = mServerInfos.get(idx);
            existingInfoRecord.copy(info);
        }

        notifyDataSetChanged();
    }

    private void startGame(ServerInfo serverInfo, InetAddress serverIpAddr) {
        dismissProgressDialog();

        if (mConnectionOp != null)
            mConnectionOp.detach();

        String ipAddrString = serverIpAddr.getHostAddress();

        //construct game's settings
        Bundle settings = new Bundle();
        settings.putSerializable(Settings.GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY, Settings.RemoteControl.CONNECT_LAN_REMOTE_CONTROL);
        settings.putString(Settings.GAME_ACTIVITY_REMOTE_HOST_KEY, ipAddrString);
        settings.putInt(Settings.GAME_ACTIVITY_REMOTE_PORT_KEY, serverInfo.port);

        //start game activity directly
        BasePage intent = BasePage.create(GamePage.class);
        intent.setExtras(settings);

        goToPage(intent);
    }

    private void cancelConnectionTask() {
        if (mConnectionOp != null) {
            System.out.println("WifiDirectServerChooserPage.cancelConnectionTask()");

            mConnectionOp.cancel();
            mConnectionOp = null;
        }
    }

    private void cancelListingTask() {
        if (mServerListingTaskOp != null) {
            System.out.println("WifiDirectServerChooserPage.cancelListingTask()");

            mServerListingTaskOp.cancel();
            mServerListingTaskOp = null;
        }
    }

    private void cancelAllTasks() {
        cancelConnectionTask();
        cancelListingTask();
    }

    private void onWifiDirectError(int reasonCode) {
        dismissProgressDialog();

        cancelAllTasks();

        switch (reasonCode) {
            case WifiP2pManager.P2P_UNSUPPORTED:
                displayErrorDialog(R.string.wifi_direct_not_supported, new Runnable() {
                    @Override
                    public void run() {
                        finish();
                    }
                });
                break;
            case WifiP2pManager.ERROR:
                displayErrorDialog(R.string.wifi_direct_internal_err_msg, null);
                break;
            default:
                displayErrorDialog(String.format(getActivity().getString(R.string.wifi_direct_generic_err_msg), reasonCode),
                        null);
        }
    }

    public static class ServerInfo {
        public String name;
        WifiP2pDevice device;
        public int port;

        private ServerInfo(String name, int port, WifiP2pDevice device) {
            this.name = name;
            this.device = device;
            this.port = port;
        }

        private void copy(ServerInfo src) {
            this.name = src.name;
            this.device = src.device;
            this.port = src.port;
        }

        @Override
        public String toString() {
            return device.deviceAddress.toLowerCase() + ":" + port;
        }

        @Override
        public boolean equals(Object other) {
            if (other == null || other instanceof ServerInfo == false)
                return false;

            ServerInfo rhs = (ServerInfo) other;
            return rhs.port == port && rhs.device.deviceAddress.equalsIgnoreCase(device.deviceAddress);
        }

        @Override
        public int hashCode() {
            return toString().hashCode();
        }
    }
}
