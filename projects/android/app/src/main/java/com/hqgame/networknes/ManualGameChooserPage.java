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
import android.widget.ArrayAdapter;


public class ManualGameChooserPage extends BaseFileListingPage {
    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.title_activity_manual_game_search));

        return super.onCreateViewWithDefaultToolbar(inflater, container, savedInstanceState);
    }

    @Override
    protected void additionalViewConfig(View v, Bundle savedInstanceState)
    {
        //load previous directory
        if (getCurrentDirectory() == null) {
            SharedPreferences pref = getContext().getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
            String lastDirectory = pref.getString(Settings.LAST_MANUAL_DIR_KEY, null);

            goIntoDirectory(lastDirectory);
        }
    }

    @Override
    public void onPause() {
        super.onPause();

        // save current working directory
        SharedPreferences pref = getContext().getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        editor.putString(Settings.LAST_MANUAL_DIR_KEY, getCurrentDirectory());
        editor.commit();
    }

    @Override
    protected void onSelectedFile(String path)
    {
        //forward previous intent's game settings if any
        Bundle existSettings = getExtras();
        Bundle settings = existSettings != null ? existSettings : new Bundle();

        //forward the rom path to Game Page
        settings.putString(Settings.GAME_ACTIVITY_PATH_KEY, path);

        //start game activity
        BasePage intent = BasePage.create(GamePage.class);
        intent.setExtras(settings);

        goToPage(intent);
    }

    @Override
    protected void onFileIconClick(ArrayAdapter<FileListingTask.FileEntry> adapter, View view, int position) {
        FileListingTask.FileEntry fileEntry = (FileListingTask.FileEntry)adapter.getItem(position);

        if (fileEntry.isDirectory())
            return;

        onSelectedFile(fileEntry.getAbsolutePath());
    }

    @Override
    protected int getItemViewLayout() {
        return R.layout.game_file_item_layout;
    }

    protected int getFileTypeDrawable() {
        return R.drawable.play_black_drawable;
    }
}
