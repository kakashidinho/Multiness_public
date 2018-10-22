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

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Map;
import java.util.Set;

public class ControllerMappingPage extends BasePage {
    private MappingListViewAdapter mappingListViewAdapter = new MappingListViewAdapter();
    private ArrayList<MappingEntry> mappingEntries = new ArrayList<>();

    private LayoutInflater listItemInflater = null;


    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.controller_mapping_lbl));

        View v = inflater.inflate(R.layout.page_controller_mapping, container, false);

        //reset button
        Button resetBtn = (Button) v.findViewById(R.id.btnResetControllerMapping);
        resetBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //reset button mappings
                Settings.resetMappedButtonSetting();
                reloadMappingEntries();
            }
        });

        //create inflater for list view's items
        listItemInflater = (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        /*------ populous mapping list -----*/
        ListView mappingListView = (ListView)v.findViewById(R.id.controller_mapping_list);

        mappingListView.setAdapter(this.mappingListViewAdapter);

        mappingListView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                onMappingEntryClick(view, position, id);
            }
        });

        reloadMappingEntries();

        return v;
    }

    @Override
    public void onPause() {
        super.onPause();

        //save mapping settings
        Settings.saveGlobalSettings(getContext());
    }

    //reload mapping entries from saved settings
    private void reloadMappingEntries() {
        final Iterable<Map.Entry<Settings.Button, Integer>> savedMappedEntries = Settings.getMappedButtons();

        int i = 0;
        for (Map.Entry<Settings.Button, Integer> savedEntry : savedMappedEntries) {
            MappingEntry newUIEntry = new MappingEntry(savedEntry.getKey(), savedEntry.getValue());

            if (i < this.mappingEntries.size())
                this.mappingEntries.set(i, newUIEntry);
            else
                this.mappingEntries.add(newUIEntry);

            ++i;
        }

        //refresh listview adapter
        mappingListViewAdapter.notifyDataSetChanged();
    }

    private void onMappingEntryClick(View view, final int position, long id) {
        final MappingEntry entry = (MappingEntry) mappingListViewAdapter.getItem(position);
        if (entry != null) {
            /*------ display dialog to prompt user to press any key ------*/
            final MappingKeyPromptDialog keyPromptDialog = new MappingKeyPromptDialog(entry);
            keyPromptDialog.show();
        }
    }

    private String getKeyLabel(int keycode) {
        switch (keycode) {
            case KeyEvent.KEYCODE_DPAD_LEFT:
                return "Dpad LEFT";
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                return "Dpad RIGHT";
            case KeyEvent.KEYCODE_DPAD_UP:
                return "Dpad UP";
            case KeyEvent.KEYCODE_DPAD_DOWN:
                return "Dpad DOWN";
            case KeyEvent.KEYCODE_BUTTON_A:
                return "Gamepad A";
            case KeyEvent.KEYCODE_BUTTON_B:
                return "Gamepad B";
            case KeyEvent.KEYCODE_BUTTON_X:
                return "Gamepad X";
            case KeyEvent.KEYCODE_BUTTON_Y:
                return "Gamepad Y";
            case KeyEvent.KEYCODE_BUTTON_SELECT:
                return "Gamepad SELECT";
            case KeyEvent.KEYCODE_BUTTON_START:
                return "Gamepad START";
            case KeyEvent.KEYCODE_BUTTON_R1:
                return "Gamepad R1";
            case KeyEvent.KEYCODE_BUTTON_L1:
                return "Gamepad L1";
            case KeyEvent.KEYCODE_BUTTON_R2:
                return "Gamepad R2";
            case KeyEvent.KEYCODE_BUTTON_L2:
                return "Gamepad L2";
            case KeyEvent.KEYCODE_BUTTON_THUMBL:
                return "Gamepad THUMBL";
            case KeyEvent.KEYCODE_BUTTON_THUMBR:
                return "Gamepad THUMBR";
            case KeyEvent.KEYCODE_UNKNOWN:
                return "";
        }

        return Integer.toString(keycode);
    }

    /*------- MappingKeyPromptDialog --------*/
    private class MappingKeyPromptDialog extends AlertDialog {
        final MappingEntry mappingEntry;

        public MappingKeyPromptDialog(MappingEntry mappingEntry) {
            super(ControllerMappingPage.this.getContext());

            this.mappingEntry = mappingEntry;


            this.setTitle(mappingEntry.realBtn.toString());
            this.setMessage(getString(R.string.controller_mapping_prompt_msg));
            this.setButton(DialogInterface.BUTTON_POSITIVE, getString(R.string.clear), new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int which) {
                    //clear key mapping
                    onKeyUp(KeyEvent.KEYCODE_UNKNOWN, null);
                }
            });
            this.setButton(DialogInterface.BUTTON_NEGATIVE, getString(R.string.cancel), new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int which) {
                }
            });
        }

        @Override
        public boolean onKeyUp(int keyCode, KeyEvent event) {
            onKeyPressedOnPromptDialog(keyCode, this.mappingEntry);

            return true;
        }

        private void onKeyPressedOnPromptDialog(int keyCode, final MappingEntry mappingEntry) {
            switch (mappingEntry.realBtn)
            {
                case LEFT:
                    if (isDpadButton(keyCode) && keyCode != KeyEvent.KEYCODE_DPAD_LEFT)
                        return;//don't allow mapping to different dpad key. ignore
                    break;
                case RIGHT:
                    if (isDpadButton(keyCode) && keyCode != KeyEvent.KEYCODE_DPAD_RIGHT)
                        return;//don't allow mapping to different dpad key. ignore
                    break;
                case UP:
                    if (isDpadButton(keyCode) && keyCode != KeyEvent.KEYCODE_DPAD_UP)
                        return;//don't allow mapping to different dpad key. ignore
                    break;
                case DOWN:
                    if (isDpadButton(keyCode) && keyCode != KeyEvent.KEYCODE_DPAD_DOWN)
                        return;//don't allow mapping to different dpad key. ignore
                    break;
            }

            this.dismiss();//close dialog

            //save new mapping entry
            Settings.mapKeyToButton(keyCode, mappingEntry.realBtn);

            //reload ui
            reloadMappingEntries();
        }

        private boolean isDpadButton(int keycode) {
            return keycode == KeyEvent.KEYCODE_DPAD_RIGHT
                    || keycode == KeyEvent.KEYCODE_DPAD_LEFT
                    || keycode == KeyEvent.KEYCODE_DPAD_UP
                    || keycode == KeyEvent.KEYCODE_DPAD_DOWN;
        }
    }

    /*------------ MappingListViewAdapter ---------------*/
    private class MappingListViewAdapter extends BaseAdapter {
        @Override
        public int getCount() {
            return mappingEntries.size();
        }

        @Override
        public Object getItem(int position) {
            return mappingEntries.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {

            View v = convertView;
            if (v == null)
                v = listItemInflater.inflate(R.layout.controller_button_map_item_layout, parent, false);

            try {
                final MappingEntry item = (MappingEntry) this.getItem(position);

                TextView buttonTxtView = (TextView) v.findViewById(R.id.buttonRealTxtView);
                TextView buttonMappedTxtView = (TextView) v.findViewById(R.id.keyMappedTxtView);

                buttonTxtView.setText(item.realBtn.toString());
                buttonMappedTxtView.setText(getKeyLabel(item.mappedKeycode));
            } catch (Exception e) {
                e.printStackTrace();
                return null;
            }
            return v;
        }
    }

    /*------------ MappingEntry ----*/
    private static class MappingEntry {
        public MappingEntry(Settings.Button realBtn, int keycode) {
            this.realBtn = realBtn;
            this.mappedKeycode = keycode;
        }

        final public Settings.Button realBtn;
        final public int mappedKeycode;
    }
}
