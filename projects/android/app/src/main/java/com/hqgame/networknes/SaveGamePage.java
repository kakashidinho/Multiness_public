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
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;

import java.io.File;

/**
 * Created by le on 4/2/2016.
 */
public class SaveGamePage extends BaseFileListingPage implements View.OnClickListener {
    private Button mSaveButton = null;
    private EditText mSaveNameText = null;

    @Override
    public void onNavigatingTo(Activity activity) {
        super.onNavigatingTo(activity);

        Utils.checkPermission(activity,
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                activity.getString(R.string.write_permission_msg),
                0);
    }

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.title_activity_save_game));

        return super.onCreateViewWithDefaultToolbar(inflater, container, savedInstanceState, FileListingTask.FileSortType.NEWEST_FIRST, R.layout.page_save_game);
    }

    @Override
    protected void additionalViewConfig(View v, Bundle savedInstanceState) {
        mSaveButton = (Button)v.findViewById(R.id.btnSaveGame);
        mSaveNameText = (EditText)v.findViewById(R.id.txtSaveName);

        long currentTime = System.currentTimeMillis();
        mSaveNameText.setText("save" + currentTime + ".ns");

        mSaveButton.setOnClickListener(this);

        if (getCurrentDirectory() == null) {
            //load previous directory
            SharedPreferences pref = getContext().getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
            String lastDirectory = pref.getString(Settings.LAST_SAVE_DIR_KEY, null);

            if (lastDirectory == null)
                lastDirectory = getContext().getFilesDir().getAbsolutePath();
            goIntoDirectory(lastDirectory);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
    }

    @Override
    protected void onSelectedFile(String path) {
        if (mSaveNameText != null)
        {
            try {
                mSaveNameText.setText((new File(path)).getName());
            } catch (Exception e) {
                e.printStackTrace();
            }
        }//if (mSaveNameText != null)
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.btnSaveGame:
            {
                try {
                    String fileName = mSaveNameText.getText().toString();
                    String currentDir = getCurrentDirectory();
                    File fileToSave = null;
                    if (currentDir != null)
                        fileToSave = new File(currentDir, fileName);
                    else
                        fileToSave = new File("/", fileName);

                    if (fileToSave.exists())//existing file
                    {
                        final String fullPathToSave = fileToSave.getAbsolutePath();

                        Utils.dialog(
                                getContext(),
                                getString(R.string.file_exist_title),
                                getString(R.string.file_exist_msg),
                                null,
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        performSaveGame(fullPathToSave);
                                    }
                                },
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        //TODO
                                    }
                                });
                    }//if (fileToSave.exists())
                    else
                        performSaveGame(fileToSave.getAbsolutePath());
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
                break;
        }//switch (view.getId())
    }

    private void performSaveGame(String path) {
        //check if the path is writable
        try {
            File file = new File(path);
            if (!file.getParentFile().canWrite())
            {
                Utils.alertDialog(getContext(), "Error", getString(R.string.file_write_permission_err_msg), null);
                return;
            }
        } catch (Exception e) {
            Utils.alertDialog(getContext(), "Error", e.getLocalizedMessage(), null);
            return;
        }

        //save last browsed directory
        SharedPreferences pref = getContext().getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        editor.putString(Settings.LAST_SAVE_DIR_KEY, getCurrentDirectory());
        editor.commit();

        //forward data to the waiting fragment
        Bundle data = new Bundle();
        data.putString(Settings.SAVE_PATH_KEY, path);
        setResult(RESULT_OK, data);
        finish();
    }
}
