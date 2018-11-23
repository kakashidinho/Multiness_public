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
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.Button;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.Spinner;

import java.io.InputStream;


/**
 * A simple {@link Activity} subclass.
 */
public class SettingsPage extends BasePage {

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.action_settings));

        View v = inflater.inflate(R.layout.page_settings, container, false);

        // no need to load saved settings, it should be done when app starts
        // Settings.loadGlobalSettings(getContext());

        SeekBar audioBar = (SeekBar)v.findViewById(R.id.sound_settings_bar);
        CheckBox voiceCheckBox = (CheckBox)v.findViewById(R.id.voice_settings_checkbox);
        CheckBox vibrationCheckBox = (CheckBox)v.findViewById(R.id.vibration_settings_checkbox);
        CheckBox uiButtonsCheckBox = (CheckBox)v.findViewById(R.id.ui_controls_settings_checkbox);
        CheckBox fullScreenCheckBox = (CheckBox)v.findViewById(R.id.fullscreen_settings_checkbox);
        CheckBox enableAutoSearchOnResumeCheckBox = (CheckBox)v.findViewById(R.id.enable_auto_search_on_resume_checkbox);
        Button controllerMapBtn = (Button)v.findViewById(R.id.controller_mapping_btn);
        Button uiButtonLayoutEdit = (Button) v.findViewById(R.id.ui_controls_layout_edit_btn);

        //screen orientation settings
        Spinner orientationList = (Spinner)v.findViewById(R.id.screen_orientation_list);
        Settings.Orientation[] ori_items = Settings.Orientation.values();
        ArrayAdapter<Settings.Orientation> ori_adapter = new ArrayAdapter<Settings.Orientation>(getContext(), android.R.layout.simple_spinner_dropdown_item, ori_items);
        orientationList.setAdapter(ori_adapter);

        //apply saved settings
        audioBar.setProgress((int)(Settings.getAudioVolume() * audioBar.getMax()));
        vibrationCheckBox.setChecked(Settings.isButtonsVibrationEnabled());
        voiceCheckBox.setChecked(Settings.isVoiceChatEnabled());
        uiButtonsCheckBox.setChecked(Settings.isUIButtonsEnbled());
        fullScreenCheckBox.setChecked(Settings.isFullscreenEnabled());
        enableAutoSearchOnResumeCheckBox.setChecked(Settings.isAutoSearchGamesOnResumeEnabled());
        orientationList.setSelection(Settings.getPreferedOrientation().ordinal());

        //register callbacks
        SeekBar.OnSeekBarChangeListener seekBarListener = (new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                onSettingBarChanged(seekBar, progress, fromUser);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });

        audioBar.setOnSeekBarChangeListener(seekBarListener);

        voiceCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                onSettingsCheckboxChanged(buttonView, isChecked);
            }
        });

        vibrationCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                onSettingsCheckboxChanged(buttonView, isChecked);
            }
        });

        uiButtonsCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                onSettingsCheckboxChanged(buttonView, isChecked);
            }
        });

        fullScreenCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                onSettingsCheckboxChanged(buttonView, isChecked);
            }
        });

        enableAutoSearchOnResumeCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                onSettingsCheckboxChanged(buttonView, isChecked);
            }
        });

        orientationList.setOnItemSelectedListener(new OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                onListItemSelected(parent, view, position, id);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
                Settings.setPreferedOrientation(Settings.Orientation.AUTO);
            }
        });

        controllerMapBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onButtonClicked(v);
            }
        });
        uiButtonLayoutEdit.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onButtonClicked(v);
            }
        });

        return v;
    }

    @Override
    public void onPause() {
        super.onPause();

        //save settings
        Settings.saveGlobalSettings(getContext());
    }

    private void onSettingBarChanged(SeekBar seekBar, int value, boolean fromUser)
    {
        if (!fromUser)
            return;

        float percent = (float) value / seekBar.getMax();

        switch (seekBar.getId()) {
            case R.id.sound_settings_bar:
                Settings.setAudioVolume(percent);
                break;
        }

        //TODO: auto save settings?
    }

    private void onSettingsCheckboxChanged(CompoundButton view, boolean checked) {
        switch (view.getId()) {
            case R.id.voice_settings_checkbox:
                Settings.enableVoiceChat(checked);
                break;
            case R.id.vibration_settings_checkbox:
                Settings.enableButtonsVibration(checked);
                break;
            case R.id.ui_controls_settings_checkbox:
                Settings.enableUIButtons(checked);
                break;
            case R.id.fullscreen_settings_checkbox:
                Settings.enableFullscreen(checked);
                break;
            case R.id.enable_auto_search_on_resume_checkbox:
                Settings.enableAutoSearchGamesOnResume(checked);
                break;
        }
    }

    private void onListItemSelected(AdapterView<?> parent, View view, int position, long id) {
        switch (parent.getId()) {
            case R.id.screen_orientation_list:
            {
                Settings.Orientation orientation = (Settings.Orientation)parent.getItemAtPosition(position);
                Settings.setPreferedOrientation(orientation);
            }
                break;
        }
    }

    private void onButtonClicked(View v) {
        switch (v.getId())
        {
            case R.id.controller_mapping_btn:
                goToPage(BasePage.create(ControllerMappingPage.class));
                break;
            case R.id.ui_controls_layout_edit_btn:
            {
                // TODO: localizaton
                final int orientations[] = { ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE,  ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT};
                final CharSequence choices[] = new CharSequence[]{ "Landscape", "Portrait" };
                AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
                builder.setTitle(getString(R.string.select_orientation));

                builder.setSingleChoiceItems(choices, -1, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        dialogInterface.dismiss();

                        BasePage page = BasePage.create(ButtonsLayoutPage.class);
                        Bundle settings = new Bundle();
                        settings.putInt(ButtonsLayoutPage.ORIENTATION_KEY, orientations[i]);

                        page.setExtras(settings);

                        goToPage(page);
                    }
                });

                AlertDialog dialog = builder.create();
                dialog.show();
            }
            break;
        }
    }

    private float dpad_ui_2_scaled_percent(float ui_percent) {
        return ui_percent * 1.15f + (1.f - ui_percent) * 0.5f;
    }

    private float dpad_scaled_2_ui_percent(float scaled_percent) {
        float re = (scaled_percent - 0.5f) / (1.15f - 0.5f);

        if (re > 1.f)
            return 1.f;

        if (re < 0.f)
            return 0.f;

        return re;
    }
}
