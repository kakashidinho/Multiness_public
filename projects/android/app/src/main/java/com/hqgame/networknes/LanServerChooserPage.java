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

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import java.lang.ref.WeakReference;
import java.util.Timer;
import java.util.TimerTask;

public class LanServerChooserPage extends BasePage implements BaseActivity.LanServerDiscoveryCallback, View.OnClickListener {

    private volatile long currentDiscoveryRequestId = -1;

    private LayoutInflater listItemInflater = null;

    private LanServersListViewAdapter serverListViewAdapter = new LanServersListViewAdapter();
    private UniqueArray<LanServer> availableServersList = new UniqueArray<>();

    private WeakReference<View> selectedServerListItemViewRef = new WeakReference<View>(null);
    private LanServer selectedServer = null;

    private Button joinBtn = null;

	private Timer autoRefreshTimer = null;

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.title_activity_lan_server_chooser_page));
        setProgressBarVisible(true);

        View v = inflater.inflate(R.layout.page_lan_server_chooser, container, false);

        //create inflater for list view's items
        listItemInflater = (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        //register adapter for lan servers list view
        ListView invitationListView = (ListView)v.findViewById(R.id.ui_lan_servers_list);

        invitationListView.setAdapter(serverListViewAdapter);
        invitationListView.setOnItemClickListener(new LanServersListViewClickListener());

        //register buttons' handler
        joinBtn = (Button)v.findViewById(R.id.btnJoinLanServer);
        Button manualBtn = (Button)v.findViewById(R.id.btnManualTypeLanServer);
        ImageButton refreshBtn = (ImageButton)v.findViewById(R.id.btnLanServerRefresh);

        joinBtn.setOnClickListener(this);
        manualBtn.setOnClickListener(this);
        refreshBtn.setOnClickListener(this);

        return v;
    }

    @Override
    public void onResume() {
        super.onResume();

        BaseActivity.registerLanServerDiscoveryCallback(this);
        BaseActivity.enableLANServerDiscovery(getContext(), true);

        //auto refresh every 10s
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
                0,
                10000);
    }

    @Override
    public void onPause() {
        super.onPause();

        autoRefreshTimer.cancel();
        autoRefreshTimer = null;
        BaseActivity.unregisterLanServerDiscoveryCallback(this);
        BaseActivity.enableLANServerDiscovery(getContext(), false);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId())
        {
            case R.id.btnJoinLanServer:
                if (selectedServer != null) {
                    startGame(selectedServer.address, selectedServer.port);
                }
                break;
            case R.id.btnManualTypeLanServer:
                manualConnectLanServer();
                break;
            case R.id.btnLanServerRefresh:
                refresh();
                break;
        }
    }

    @Override
    public void onLanServerDiscovered(final long request_id, final String address, final int port, final String description) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                //if (currentDiscoveryRequestId != request_id)
                //    return;//ignore
                if (availableServersList.add(new LanServer(description, address, port))) {
                    serverListViewAdapter.notifyDataSetChanged();
                }
            }
        });
    }

    private void refresh() {
        invalidateSelectedServerListItemView();

        availableServersList.clear();
        serverListViewAdapter.notifyDataSetChanged();

        currentDiscoveryRequestId = System.nanoTime();
        BaseActivity.findLANServers(getContext(), currentDiscoveryRequestId);
    }

    private void manualConnectLanServer()
    {
        final Context context = getContext();
        //load previous ip and port
        SharedPreferences pref = context.getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
        String lastIp = pref.getString(Settings.LAST_TYPED_HOST_KEY, "");
        int lastPort = pref.getInt(Settings.LAST_TYPED_PORT_KEY, Settings.DEFAULT_NETWORK_PORT);

        //create dialog
        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        layout.setLayoutParams(lp);
        layout.setPadding(
                (int)getResources().getDimension(R.dimen.activity_horizontal_margin),
                0,
                (int)getResources().getDimension(R.dimen.activity_horizontal_margin),
                0);

        //IP
        final TextView ipTextLblView = new TextView(context);
        Utils.setTexAppearance(ipTextLblView, context, android.R.style.TextAppearance_Medium);
        ipTextLblView.setText("IP:");
        layout.addView(ipTextLblView);

        final EditText ipText = new EditText(context);
        ipText.setText(lastIp);
        layout.addView(ipText);

        //port
        final TextView portTextLblView = new TextView(context);
        Utils.setTexAppearance(portTextLblView, context, android.R.style.TextAppearance_Medium);
        portTextLblView.setText("Port:");
        layout.addView(portTextLblView);

        final EditText portText = new EditText(context);
        portText.setText(Integer.toString(lastPort));
        layout.addView(portText);

        Utils.dialog(
                context, getString(R.string.connect_game), getString(R.string.connect_game_msg), layout,
                new Runnable() {
                    @Override
                    public void run() {
                        Context context2 = LanServerChooserPage.this.getContext();
                        int port = 0;
                        String host = ipText.getText().toString();
                        if (host.equals(""))
                        {
                            Utils.alertDialog(context2, "Error", getString(R.string.err_invalid_ip), null);
                            return;
                        }

                        try {
                            port = Integer.parseInt(portText.getText().toString());
                        } catch (Exception e)
                        {
                            Utils.alertDialog(context2, "Error", getString(R.string.err_invalid_port), null);
                            return;
                        }

                        //save the typed host & port so that we can reuse later for creating the dialog
                        SharedPreferences pref = context2.getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
                        SharedPreferences.Editor editor = pref.edit();
                        editor.putString(Settings.LAST_TYPED_HOST_KEY, host);
                        editor.putInt(Settings.LAST_TYPED_PORT_KEY, port);
                        editor.commit();

                        //start game
                        startGame(host, port);
                    }
                },
                null);
    }

    private void startGame(String address, int port) {
        //construct game's settings
        Bundle settings = new Bundle();
        settings.putSerializable(Settings.GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY, Settings.RemoteControl.CONNECT_LAN_REMOTE_CONTROL);
        settings.putString(Settings.GAME_ACTIVITY_REMOTE_HOST_KEY, address);
        settings.putInt(Settings.GAME_ACTIVITY_REMOTE_PORT_KEY, port);

        //start game activity directly
        BasePage intent = BasePage.create(GamePage.class);
        intent.setExtras(settings);

        goToPage(intent);
    }

    private void invalidateSelectedServerListItemView() {
        View selectedServerListItemView = selectedServerListItemViewRef.get();
        if (selectedServerListItemView != null) {
            selectedServerListItemView.setBackgroundResource(android.R.color.transparent);
        }
        selectedServerListItemViewRef = new WeakReference<View>(null);

        joinBtn.setEnabled(false);
    }

    /*------------ LanServer -------------*/
    private static class LanServer implements Comparable<LanServer>{
        public final String desc;
        public final String address;
        public final int port;

        public final String address_port;

        public LanServer(String desc, String address, int port) {
            this.desc = desc;
            this.address = address;
            this.port = port;

            this.address_port = this.address + ":" + this.port;
        }

        @Override
        public String toString() {
            return desc + "[" + address_port + "]";
        }

        @Override
        public int hashCode() {
            return address_port.hashCode();
        }

        @Override
        public boolean equals(Object rhs) {
            if (rhs == null || rhs instanceof LanServer == false)
                return false;

            LanServer rhss = (LanServer)rhs;

            return rhss.address_port.equals(address_port);
        }

        @Override
        public int compareTo(LanServer another) {
            return address_port.compareTo(another != null ? another.address_port : null);
        }
    }

    /*----------- LanServersListViewAdapter --------*/
    private class LanServersListViewAdapter extends BaseAdapter {
        @Override
        public int getCount() {
            return availableServersList.size();
        }

        @Override
        public Object getItem(int position) {
            return availableServersList.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View vi = convertView;
            if (vi == null)
                vi = listItemInflater.inflate(R.layout.lan_server_item_layout, parent, false);

            TextView descView = (TextView) vi.findViewById(R.id.lan_server_desc_txt_view);
            TextView addrView = (TextView) vi.findViewById(R.id.lan_server_address_txt_view);

            LanServer info = (LanServer) this.getItem(position);
            if (info.equals(selectedServer)) {
                vi.setBackgroundResource(android.R.color.darker_gray);//highlight selected item
                selectedServerListItemViewRef = new WeakReference<View>(vi);

                joinBtn.setEnabled(true);
            }
            else
                vi.setBackgroundResource(android.R.color.transparent);

            descView.setText(info.desc);
            addrView.setText(info.address_port);

            return vi;
        }
    }

    /*------------- LanServersListViewClickListener ----*/
    private class LanServersListViewClickListener implements AdapterView.OnItemClickListener {
        @Override
        public void onItemClick(AdapterView<?> adapterView, View view, int position, long id) {
            invalidateSelectedServerListItemView();

            //highlight the selected item
            view.setBackgroundResource(android.R.color.darker_gray);
            selectedServerListItemViewRef = new WeakReference<View>(view);

            selectedServer = (LanServer)serverListViewAdapter.getItem(position);

            joinBtn.setEnabled(true);
        }
    }
}
