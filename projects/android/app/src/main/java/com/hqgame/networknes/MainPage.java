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
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.text.InputType;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.PurchasesUpdatedListener;

import java.util.List;


public class MainPage extends BaseMenuPage implements PurchasesUpdatedListener {
    private static final int LOCATION_REQUEST_CODE = 100;

    private static final long USER_RATING_PROMPT_INTERVAL_MS = 1800000;//half an hour
    private static final long USER_PLAYTIME_RATE_THRESHOLD_MS = 3600000;//one hour
    private static final long USER_MULTIPLAYER_TIME_RATE_THRESHOLD_MS = 15 * 60 * 1000;//15'

    FrameLayout mContentView = null;

    public MainPage() {
        // Required empty public constructor
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        BaseActivity.getCurrentActivity().registerIAPPurchaseUpdatedCallback(this);
    }

    @Override
    protected View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
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

        setToolBarStyle();

        // check donation button's status again since the view was recreated
        checkDonationButtonState();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        setToolBarStyle();
    }

    @Override
    public void onResume()
    {
        super.onResume();

        registerGoogleLoginButtn(getView());

        // try prompting user to rate the app
        tryPromptUserToRate();

        // check donation button's status
        checkDonationButtonState();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        BaseActivity.getCurrentActivity().unregisterIAPPurchaseUpdatedCallback(this);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String permissions[], int[] grantResults) {
        if (requestCode != LOCATION_REQUEST_CODE)
            return;
        for (int i = 0; i < permissions.length; ++i) {
            String permission = permissions[i];
            if (permission.equals(Manifest.permission.ACCESS_COARSE_LOCATION)) {
                if (grantResults[i] == PackageManager.PERMISSION_GRANTED) {
                    selectWifiDirectPlayerDialog();
                }
            }
        }
    }

    private void setToolBarStyle() {
        // disable title from action bar
        if (getBaseActivity().getSupportActionBar() != null) {
            getBaseActivity().getSupportActionBar().setDisplayShowTitleEnabled(false);
        }
    }

    private View createConfigDependentView(LayoutInflater inflater, ViewGroup container) {
        View view = inflater.inflate(R.layout.page_main, container, false);

        //initialize buttons' listener
        View.OnClickListener buttonClickListener = new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onButtonClicked(view.getId());
            }
        };

        Button singlePlayBtn = (Button)view.findViewById(R.id.btnSinglePlayer);
        Button multiPlayLanBtn = (Button)view.findViewById(R.id.btnLanMultiplayer);
        Button multiPlayInternetBtn = (Button)view.findViewById(R.id.btnInternet);
        Button wifiDirectBtn = (Button)view.findViewById(R.id.btnWifiDirect);
        View donateBtn = view.findViewById(R.id.btnDonate);
        singlePlayBtn.setOnClickListener(buttonClickListener);
        multiPlayLanBtn.setOnClickListener(buttonClickListener);
        multiPlayInternetBtn.setOnClickListener(buttonClickListener);
        wifiDirectBtn.setOnClickListener(buttonClickListener);
        if (donateBtn != null)
            donateBtn.setOnClickListener(buttonClickListener);

        // google login button
        registerGoogleLoginButtn(view);

        //facebook login button
        com.facebook.login.widget.LoginButton fbLoginButton =
                (com.facebook.login.widget.LoginButton) view.findViewById(R.id.fb_login_button);
        if (fbLoginButton != null) {
            fbLoginButton.setReadPermissions(Settings.FACEBOOK_PERMISSIONS);

            if (BuildConfig.FLAVOR.equals("gpg")) // disable fb button
            {
                fbLoginButton.setVisibility(View.GONE);
            }
        }

        return view;
    }

    private void registerGoogleLoginButtn(View container) {
        if (container != null) {
            // register google login button's click listener
            Button googleLoginBtn
                    = (Button) container.findViewById(R.id.google_login_button);

            if (googleLoginBtn != null) {
                getBaseActivity().registerGoogleSigninBtn(googleLoginBtn);
            }
        }
    }

    private void onButtonClicked(int id)
    {
        switch (id)
        {
            case R.id.btnSinglePlayer:
            {
                goToPage(BasePage.create(GameChooserPage.class));
            }
            break;
            case R.id.btnLanMultiplayer:
            {
                selectLanPlayerDialog();
            }
            break;
            case R.id.btnWifiDirect:
            {
                selectWifiDirectPlayerDialog();
            }
            break;
            case R.id.btnInternet:
            {
                selectInternetModeDialog();
            }
            break;
            case R.id.btnDonate:
            {
                Utils.dialog(getActivity(), null, getString(R.string.donate_msg), null,
                        new Runnable() {
                            @Override
                            public void run() {
                                // buy no ads iap
                                startPurchaseNoAds();
                            }
                        }, new Runnable() {
                            @Override
                            public void run() {

                            }
                        });
            }
            break;
        }
    }

    /*---------- Donation/IAP ----------------------------*/
    private void checkDonationButtonState() {
        try {
            if (Settings.isAdsDisabled()) {
                System.out.println("MainPage.checkDonationButtonState() ads was disabled according to Settings");
                enableDonationButton(false, false);
                return;
            }

            enableDonationButton(false, true); // hide it until we find out the state of Donation made by user

            getBaseActivity().queryIAPPurchase(getString(R.string.no_ads_iap_id), new AsyncQuery<Purchase>() {
                @Override
                public void run(Purchase result) {
                    if (result == null) // no ads was not purchased?
                        enableDonationButton(true, true); // reenable donation button
                    else
                        enableDonationButton(false, false);
                }
            });
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void enableDonationButton(boolean enable, boolean visible) {
        try {
            View donationBtn = getView().findViewById(R.id.btnDonate);

            if (visible) {
                donationBtn.setVisibility(View.VISIBLE);
            }
            else {
                donationBtn.setVisibility(View.GONE);
            }

            donationBtn.setEnabled(enable);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void startPurchaseNoAds() {
        enableDonationButton(false, true);
        getBaseActivity().startIAPPurchaseFlow(getString(R.string.no_ads_iap_id), new AsyncQuery<Integer>() {
            @Override
            public void run(Integer result) {
                if (!result.equals(BillingClient.BillingResponse.OK))
                    handlePurchaseError(result);
            }
        });
    }

    private void handlePurchaseError(int resultCode) {
        switch (resultCode) {
            case BillingClient.BillingResponse.ITEM_ALREADY_OWNED:
                Settings.enableAds(false);
                enableDonationButton(false, false);
                Utils.alertDialog(getContext(), null, getString(R.string.iap_already_owned_msg), new Runnable() {
                    @Override
                    public void run() {

                    }
                });
                break;
            case BillingClient.BillingResponse.USER_CANCELED:
                if (!Settings.isAdsDisabled())
                    enableDonationButton(true, true);
                else
                    enableDonationButton(false, false);
                break;
            default:
                Utils.errorDialog(getContext(), String.format(getString(R.string.iap_generic_err_msg), resultCode), new Runnable() {
                    @Override
                    public void run() {
                        if (!Settings.isAdsDisabled())
                            enableDonationButton(true, true);
                    }
                });
        }
    }

    /**
     * PurchasesUpdatedListener
     * Implement this method to get notifications for purchases updates. Both purchases initiated by
     * your app and the ones initiated by Play Store will be reported here.
     *
     * @param responseCode Response code of the update.
     * @param purchases    List of updated purchases if present.
     */
    @Override
    public void onPurchasesUpdated(int responseCode, @Nullable List<Purchase> purchases) {
        if (responseCode != (BillingClient.BillingResponse.OK))
            handlePurchaseError(responseCode);
        else if (purchases != null) {
            for (Purchase purchase: purchases) {
                if (purchase.getSku().equals(getString(R.string.no_ads_iap_id)))
                    enableDonationButton(false, false);
            }
        } else {
            if (!Settings.isAdsDisabled())
                enableDonationButton(true, true);
        }
    }

    /*---------- try to prompt user to rate the app ------*/
    private boolean tryPromptUserToRate() {
        final Context context = getContext();
        final SharedPreferences preferences = context.getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
        if (preferences == null)
        {
            return false;
        }

        long currentPlayTimeMs = preferences.getLong(Settings.USER_TOTAL_PLAY_TIME_KEY, 0);
        long currentMultiPlayTimeMs = preferences.getLong(Settings.USER_TOTAL_MULTIPLAYER_TIME_KEY, 0);

        if ((currentMultiPlayTimeMs < USER_MULTIPLAYER_TIME_RATE_THRESHOLD_MS && currentPlayTimeMs < USER_PLAYTIME_RATE_THRESHOLD_MS) ||
                (BaseActivity.getTotalPlayTimeMsAtTheStart() == currentPlayTimeMs && BaseActivity.getTotalMultiPlayerTimeMsAtTheStart() == currentMultiPlayTimeMs))
            return false;//wait for them to play a little more

        boolean skip = preferences.getBoolean(Settings.SKIP_USER_RATING_KEY, false);
        if (skip)
            return false;

        //should only prompt for user's rating every specified time interval
        long lastRatePromptTime = preferences.getLong(Settings.USER_RATING_PROMPT_DATE_KEY, 0);

        long now = System.currentTimeMillis();
        long elapsedTime = now - lastRatePromptTime;
        if (elapsedTime < USER_RATING_PROMPT_INTERVAL_MS)
            return false;//wait a little bit

        //save last prompt time
        SharedPreferences.Editor prefEditor = preferences.edit();
        prefEditor.putLong(Settings.USER_RATING_PROMPT_DATE_KEY, now);
        prefEditor.commit();
        prefEditor = null;

        //open dialog
        final String rateTitle = getString(R.string.rate_title);
        final String rateMsg = getString(R.string.rate_msg);
        final String rateBtnTitle = getString(R.string.rate_btn);
        final String noRateBtnTitle = getString(R.string.no_rate_btn);
        final String rateLaterTitle = getString(R.string.rate_later_btn);

        new AlertDialog.Builder(context)
                .setTitle(rateTitle)
                .setMessage(rateMsg)
                .setPositiveButton(rateBtnTitle, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        //prevent this dialog from opening ever again
                        SharedPreferences.Editor editor = preferences.edit();
                        editor.putBoolean(Settings.SKIP_USER_RATING_KEY, true);
                        editor.commit();

                        //open Google Play's app page
                        final String appPackageName = context.getPackageName();
                        Intent intent = createAppPageInStoreRedirection(appPackageName);

                        startActivity(intent);
                    }
                })
                .setNeutralButton(rateLaterTitle, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        //DO NOTHING
                    }
                })
                .setNegativeButton(noRateBtnTitle, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        //prevent this dialog from opening ever again
                        SharedPreferences.Editor editor = preferences.edit();
                        editor.putBoolean(Settings.SKIP_USER_RATING_KEY, true);
                        editor.commit();
                    }
                })
                .setIcon(R.mipmap.ic_launcher)
                .show();

        return true;
    }

    private static Intent createAppPageInStoreRedirection(final String appPackageName)
    {
        Intent intent;
        try {
            intent = new Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=" + appPackageName));
        } catch (Exception e) {
            intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://play.google.com/store/apps/details?id=" + appPackageName));
        }

        return intent;
    }

    private void lanPlayerSelected(int playerIdx)
    {
        switch (playerIdx)
        {
            case 0:
                hostLanGame();
                break;
            case 1:
                connectLanGame();
                break;
        }
    }

    private void hostLanGame()
    {
        final Context context = getContext();
        String ip = Utils.getHostIPAddress(getActivity());
        if (ip == null)
        {
            Utils.alertDialog(context, "Error", getString(R.string.err_no_network), null);
        }//if (ip == null)
        else {
            //load previous port
            SharedPreferences pref = context.getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
            int lastPort = pref.getInt(Settings.LAST_TYPED_PORT_KEY, Settings.DEFAULT_NETWORK_PORT);

            //create dialog
            LinearLayout layout = new LinearLayout(context);
            layout.setOrientation(LinearLayout.VERTICAL);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT);

            layout.setLayoutParams(lp);
            layout.setPadding(
                    (int) getResources().getDimension(R.dimen.activity_horizontal_margin),
                    0,
                    (int) getResources().getDimension(R.dimen.activity_horizontal_margin),
                    0);

            //IP
            final TextView ipTextLblView = new TextView(context);
            Utils.setTexAppearance(ipTextLblView, context, android.R.style.TextAppearance_Medium);
            ipTextLblView.setText("IP:");
            layout.addView(ipTextLblView);

            final EditText ipText = new EditText(context);
            ipText.setText(ip);
            ipText.setFocusable(false);
            ipText.setEnabled(false);
            ipText.setCursorVisible(false);
            ipText.setKeyListener(null);
            ipText.setInputType(InputType.TYPE_NULL);
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
                    context, getString(R.string.host_game), getString(R.string.host_lan_game_msg), layout,
                    new Runnable() {
                        @Override
                        public void run() {
                            int port = 0;
                            try {
                                port = Integer.parseInt(portText.getText().toString());
                            } catch (Exception e)
                            {
                                Utils.alertDialog(context, "Error", getString(R.string.err_invalid_port), null);
                                return;
                            }

                            //save the typed port so that we can reuse later for creating the dialog
                            SharedPreferences pref = context.getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
                            SharedPreferences.Editor editor = pref.edit();
                            editor.putInt(Settings.LAST_TYPED_PORT_KEY, port);
                            editor.commit();

                            //construct game's settings
                            Bundle settings = new Bundle();
                            settings.putSerializable(Settings.GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY, Settings.RemoteControl.ENABLE_LAN_REMOTE_CONTROL);
                            settings.putInt(Settings.GAME_ACTIVITY_REMOTE_PORT_KEY, port);

                            //start game chooser activity
                            BasePage intent = BasePage.create(GameChooserPage.class);
                            intent.setExtras(settings);

                            goToPage(intent);
                        }
                    },
                    null);
        }//if (ip == null)
    }

    private void connectLanGame()
    {
        goToPage(BasePage.create(LanServerChooserPage.class));
    }

    private void selectLanPlayerDialog()
    {
        CharSequence choices[] = new CharSequence[]{ getString(R.string.host), getString(R.string.client) };
        AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
        builder.setTitle(getString(R.string.select_player));

        builder.setSingleChoiceItems(choices, -1, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                dialogInterface.dismiss();
                lanPlayerSelected(i);
            }
        });

        AlertDialog dialog = builder.create();
        dialog.show();
    }

    private void wifiDirectPlayerSelected(int playerIdx)
    {
        switch (playerIdx)
        {
            case 0:
                hostWifiDirectGame();
                break;
            case 1:
                connectWifiDirectGame();
                break;
        }
    }

    private void hostWifiDirectGame()
    {
        final BaseActivity context = getBaseActivity();

        //load previous port and name
        SharedPreferences pref = context.getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
        int lastPort = pref.getInt(Settings.LAST_TYPED_PORT_KEY, Settings.DEFAULT_NETWORK_PORT);
        String lastRoomName = pref.getString(Settings.LAST_TYPED_ROOM_NAME_KEY, context.getDefaultHostRoomName());

        //create dialog
        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);

        layout.setLayoutParams(lp);
        layout.setPadding(
                (int) getResources().getDimension(R.dimen.activity_horizontal_margin),
                0,
                (int) getResources().getDimension(R.dimen.activity_horizontal_margin),
                0);

        //name
        final TextView nameTextLblView = new TextView(context);
        Utils.setTexAppearance(nameTextLblView, context, android.R.style.TextAppearance_Medium);
        nameTextLblView.setText(getString(R.string.name_field));
        layout.addView(nameTextLblView);

        final EditText nameText = new EditText(context);
        nameText.setText(lastRoomName);
        layout.addView(nameText);

        //port
        final TextView portTextLblView = new TextView(context);
        Utils.setTexAppearance(portTextLblView, context, android.R.style.TextAppearance_Medium);
        portTextLblView.setText("Port:");
        layout.addView(portTextLblView);

        final EditText portText = new EditText(context);
        portText.setText(Integer.toString(lastPort));
        layout.addView(portText);

        Utils.dialog(
                context, getString(R.string.host_game), getString(R.string.host_wifi_direct_game_msg), layout,
                new Runnable() {
                    @Override
                    public void run() {
                        int port = 0;
                        String name;
                        try {
                            port = Integer.parseInt(portText.getText().toString());
                        } catch (Exception e)
                        {
                            Utils.errorDialog(context, getString(R.string.err_invalid_port), null);
                            return;
                        }

                        name = nameText.getText().toString();
                        if (name.length() <= 0) {
                            Utils.errorDialog(context, getString(R.string.err_invalid_room_name), null);
                            return;
                        }


                        //save the typed port & room name so that we can reuse later for creating the dialog
                        SharedPreferences pref = context.getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
                        SharedPreferences.Editor editor = pref.edit();
                        editor.putInt(Settings.LAST_TYPED_PORT_KEY, port);
                        editor.putString(Settings.LAST_TYPED_ROOM_NAME_KEY, name);
                        editor.commit();

                        //construct game's settings
                        Bundle settings = new Bundle();
                        settings.putSerializable(Settings.GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY, Settings.RemoteControl.ENABLE_WIFI_DIRECT_REMOTE_CONTROL);
                        settings.putInt(Settings.GAME_ACTIVITY_REMOTE_PORT_KEY, port);
                        settings.putString(Settings.GAME_ACTIVITY_REMOTE_HOST_KEY, name);

                        //start game chooser activity
                        BasePage intent = BasePage.create(GameChooserPage.class);
                        intent.setExtras(settings);

                        goToPage(intent);
                    }
                },
                null);
    }

    private void connectWifiDirectGame()
    {
        goToPage(BasePage.create(WifiDirectServerChooserPage.class));
    }

    private void selectWifiDirectPlayerDialog() {
        if (!Utils.checkPermission(getBaseActivity(),
                Manifest.permission.ACCESS_COARSE_LOCATION,
                getString(R.string.location_permission_msg),
                LOCATION_REQUEST_CODE,
                true)) {
            return;
        }

        CharSequence choices[] = new CharSequence[]{ getString(R.string.host), getString(R.string.client) };
        AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
        builder.setTitle(getString(R.string.select_player));

        builder.setSingleChoiceItems(choices, -1, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                dialogInterface.dismiss();
                wifiDirectPlayerSelected(i);
            }
        });

        AlertDialog dialog = builder.create();
        dialog.show();
    }

    private void selectInternetModeDialog()
    {
        goToPage(BasePage.create(InternetMultiplayerPage.class));
    }
}
