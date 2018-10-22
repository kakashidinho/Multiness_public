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
import android.widget.Button;

import java.io.File;

/**
 * Created by le on 4/2/2016.
 */
public class LoadSaveFilePage extends BaseFileListingPage implements View.OnClickListener {
    private Button mLoadButton = null;
    private Button mDeleteButton = null;
    private String mSelectedFile = null;

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return super.onCreateViewWithDefaultToolbar(inflater, container, savedInstanceState, FileListingTask.FileSortType.NEWEST_FIRST, R.layout.page_load_save_file);
    }

    @Override
    protected void additionalViewConfig(View v, Bundle savedInstanceState) {
        setLabel(getString(R.string.title_activity_load_save));

        mLoadButton = (Button)v.findViewById(R.id.btnLoadSave);
        mDeleteButton = (Button)v.findViewById(R.id.btnDeleteSave);

        mLoadButton.setOnClickListener(this);
        mDeleteButton.setOnClickListener(this);

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
    protected void onSelectedDirectory(String absolutePath) {
        mLoadButton.setEnabled(false);
        mDeleteButton.setEnabled(false);

        mSelectedFile = null;
    }

    @Override
    protected void onSelectedFile(String path) {
        mLoadButton.setEnabled(true);
        mDeleteButton.setEnabled(true);

        mSelectedFile = path;
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.btnLoadSave: {
                if (mSelectedFile != null) {
                    //save last browsed directory
                    SharedPreferences pref = getContext().getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
                    SharedPreferences.Editor editor = pref.edit();
                    editor.putString(Settings.LAST_SAVE_DIR_KEY, getCurrentDirectory());
                    editor.commit();

                    //forward data to game fragment
                    Bundle data = new Bundle();
                    data.putString(Settings.SAVE_PATH_KEY, mSelectedFile);

                    setResult(RESULT_OK, data);
                    finish();
                }//if (mSelectedFile != null)
            }
                break;
            case R.id.btnDeleteSave:
                Utils.dialog(
                        getContext(),
                        getString(R.string.delete),
                        getString(R.string.delete_file_msg),
                        null,
                        new Runnable() {
                            @Override
                            public void run() {
                                if (mSelectedFile != null) {
                                    try {
                                        File file = new File(mSelectedFile);
                                        file.delete();

                                        refresh();
                                    } catch (Exception e) {
                                        e.printStackTrace();
                                        Utils.alertDialog(LoadSaveFilePage.this.getContext(),
                                                getString(R.string.generic_err_title),
                                                e.getLocalizedMessage(),
                                                null);
                                    }
                                }
                            }
                        },
                        new Runnable() {
                            @Override
                            public void run() {
                                //TODO
                            }
                        });
                break;
        }
    }
}
