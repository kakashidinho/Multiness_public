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

import android.content.Intent;
import android.net.Uri;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.style.UnderlineSpan;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

public class CreditsPage extends BasePage implements View.OnClickListener {

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.credits));

        View v = inflater.inflate(R.layout.page_credits, container, false);

        // version text
        TextView versionView = (TextView)v.findViewById(R.id.app_version_tv);
        versionView.setText(String.format(getString(R.string.app_version_text), BuildConfig.VERSION_NAME, BuildConfig.VERSION_CODE));

        // copyright license
        View licenseLinkView = makeSpanText(v, R.id.program_license_link);
        View nestopiaLicenseLinkView = makeSpanText(v, R.id.nestopia_license_link);
        View raknetLicenseLinkView = makeSpanText(v, R.id.raknet_license_link);
        View miniupnpLicenseLinkView = makeSpanText(v, R.id.miniupnp_license_link);
        licenseLinkView.setOnClickListener(this);
        nestopiaLicenseLinkView.setOnClickListener(this);
        raknetLicenseLinkView.setOnClickListener(this);
        miniupnpLicenseLinkView.setOnClickListener(this);

        // privacy policy
        View privacyUrlTextView = makeSpanText(v, R.id.privacy_policy_url_text);
        if (privacyUrlTextView != null) {
            privacyUrlTextView.setOnClickListener(this);
        }

        View urlArea = v.findViewById(R.id.privacy_policy_area);
        if (urlArea != null) {
            urlArea.setOnClickListener(this);
        }

        return v;
    }

    private void openPrivacyPolicy() {
        Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(getString(R.string.privacy_policy_msg)));
        startActivity(browserIntent);
    }

    private TextView makeSpanText(View parent, int resId) {
        TextView tv = (TextView)parent.findViewById(resId);
        if (tv != null) {
            SpannableString content = new SpannableString(tv.getText());
            content.setSpan(new UnderlineSpan(), 0, content.length(), 0);
            tv.setText(content);
        }

        return tv;
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.program_license_link: case R.id.nestopia_license_link:
            case R.id.raknet_license_link: case R.id.miniupnp_license_link:
            {
                try {
                    Bundle settings = new Bundle();
                    switch (v.getId()) {
                        case R.id.program_license_link:
                            settings.putInt(LicensePage.LICENSE_TEXT_KEY, R.raw.gpl_v3_license);
                            break;
                        case R.id.nestopia_license_link:
                            settings.putInt(LicensePage.LICENSE_TEXT_KEY, R.raw.nestopia_license);
                            break;
                        case R.id.raknet_license_link:
                            settings.putInt(LicensePage.LICENSE_TEXT_KEY, R.raw.raknet_license);
                            break;
                        case R.id.miniupnp_license_link:
                            settings.putInt(LicensePage.LICENSE_TEXT_KEY, R.raw.miniupnp_license);
                            break;
                    }

                    BasePage licenseViewPage = BasePage.create(LicensePage.class);
                    licenseViewPage.setExtras(settings);

                    goToPage(licenseViewPage);
                } catch (Exception e) {

                }
            }
                break;
            case R.id.privacy_policy_area: case R.id.privacy_policy_url_text:
                openPrivacyPolicy();
                break;
        }
    }
}
