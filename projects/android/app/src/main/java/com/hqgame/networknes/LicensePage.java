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

import android.app.ProgressDialog;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;

public class LicensePage extends BasePage {
    public static final String LICENSE_TEXT_KEY = "LicensePage.LICENSE_RES_ID_KEY";

    AsyncTask<Void, Void, Void> mTextLoadingTask = null;
    ProgressDialog mProgressDialog = null;

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.license));

        View v = inflater.inflate(R.layout.page_license, container, false);

        Bundle args = getExtras();
        if (args == null || !args.containsKey(LICENSE_TEXT_KEY))
            return v;

        final int textResId = args.getInt(LICENSE_TEXT_KEY);
        final TextView contentView = (TextView) v.findViewById(R.id.txt_license_full);

        // load text in background
        mTextLoadingTask = new AsyncTask<Void, Void, Void>() {
            StringBuilder mTextBuilder = null;

            @Override
            protected  void onPreExecute()
            {
                mProgressDialog = Utils.showProgressDialog(getContext(), getString(R.string.loading_msg), new Runnable() {
                    @Override
                    public void run() {
                        cancel(true);
                    }
                });
            }

            @Override
            protected Void doInBackground(Void... voids) {

                try {
                    InputStream is = getResources().openRawResource(textResId);
                    BufferedReader br = new BufferedReader(new InputStreamReader(is));
                    String line;
                    StringBuilder sb = new StringBuilder();
                    while (!isCancelled() && (line = br.readLine()) != null) {
                        sb.append(line + "\n");
                    }

                    br.close();

                    mTextBuilder = sb;
                } catch (Exception e) {
                    e.printStackTrace();
                }

                return null;
            }

            @Override
            protected void onCancelled(Void result) {
                dismissProgressDialog();

                finish();
            }

            @Override
            protected void onPostExecute(Void result) {
                dismissProgressDialog();

                if (mTextBuilder == null)
                    finish();
                else {
                    if (contentView != null) {
                        contentView.setText(mTextBuilder.toString());
                    }
                }
            }
        };

        mTextLoadingTask.execute();

        return v;
    }

    private void dismissProgressDialog() {
        if (mProgressDialog != null && mProgressDialog.isShowing()) {
            mProgressDialog.dismiss();
            mProgressDialog = null;
        }
    }
}
