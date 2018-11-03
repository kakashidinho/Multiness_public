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
import android.app.ProgressDialog;
import android.app.SearchManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.ListView;

import org.json.JSONArray;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.ref.WeakReference;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Locale;
import java.util.Set;
import java.util.TreeSet;


public class GameChooserPage extends BaseMenuPage {
    private static final int READ_STORAGE_REQUEST_CODE = 1;

    private static final int MAX_CACHED_FILES = 100;

    private TreeSet<GameEntry> uniqueGamePathList = new TreeSet<>();
    private TreeSet<File> cachedGamePathList = new TreeSet<>(new AlphabetFileNameComparator());
    private ArrayAdapter<GameEntry> gameListAdapter;
    private GameAutoSearchTask gameSearchTask = null;


    public GameChooserPage() {
        // Required empty public constructor
    }

    @Override
    public void onNavigatingTo(Activity activity) {
        super.onNavigatingTo(activity);

        Utils.checkPermission(activity,
                Manifest.permission.READ_EXTERNAL_STORAGE, activity.getString(R.string.read_permission_msg), READ_STORAGE_REQUEST_CODE);
    }

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.title_activity_game_chooser));

        View v = inflater.inflate(R.layout.page_game_chooser, container, false);
        
        Button manualBtn = (Button)v.findViewById(R.id.btnManualSearch);
        Button autoBtn = (Button)v.findViewById(R.id.btnAutoSearch);
        ListView gameListView = (ListView)v.findViewById(R.id.game_list);

        //forward previous intent's game settings if any
        Bundle existSettings = getExtras();
        final Bundle settings = existSettings != null ? existSettings : new Bundle();

        //register button's handlers
        manualBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                //start searching for game manually
                BasePage intent = BasePage.create(ManualGameChooserPage.class);
                intent.setExtras(settings);

                goToPage(intent);
            }
        });

        autoBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                autoSearch();//start auto search
            }
        });

        //register list view's on click listener
        gameListView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> adapterView, View view, int position, long id) {
                final GameEntry item = (GameEntry) adapterView.getItemAtPosition(position);

                //start Game Activity
                settings.putString(Settings.GAME_ACTIVITY_PATH_KEY, item.getFile().getAbsolutePath());
                BasePage intent = BasePage.create(GamePage.class);
                intent.setExtras(settings);

                goToPage(intent);
            }
        });

        //create game's list adapter
        gameListAdapter = new ArrayAdapter<GameEntry>(getContext(), R.layout.game_file_item_layout, R.id.file_name_txt_view);

        gameListView.setAdapter(gameListAdapter);

        return v;
    }

    @Override
    public void onResume() {
        System.out.println("GameChooserPage.onResume()");

        super.onResume();

        try {
            SharedPreferences pref = getContext().getSharedPreferences(getClass().getSimpleName(), Context.MODE_PRIVATE);

            //load cached file list
            String cachedPathsStr = pref.getString(Settings.ORDERED_CACHED_GAME_PATHS_KEY, "[]");
            JSONArray cachedPaths = new JSONArray(cachedPathsStr);
            for (int i = 0; i < cachedPaths.length(); ++i) {
                String path = cachedPaths.getString(i);
                if (path != null) {
                    cachedGamePathList.add(new File(path));
                }
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }

        // check for storage permission
        if (Utils.hasPermission(getActivity(), Manifest.permission.READ_EXTERNAL_STORAGE)) {

            //auto search existing games and populate the list
            autoSearch();
        }
    }

    @Override
    public void onPause() {
        System.out.println("GameChooserPage.onPause()");

        super.onPause();

        cancelGameAutoSearch();

        //cache the file list for faster search in future
        try {
            SharedPreferences pref = getContext().getSharedPreferences(getClass().getSimpleName(), Context.MODE_PRIVATE);

            SharedPreferences.Editor edit = pref.edit();
            edit.putString(Settings.ORDERED_CACHED_GAME_PATHS_KEY, new JSONArray(cachedGamePathList).toString());

            if (pref.contains(Settings.LEGACY_CACHED_GAME_PATHS_KEY))
                edit.remove(Settings.LEGACY_CACHED_GAME_PATHS_KEY);

            edit.apply();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String permissions[], int[] grantResults) {
        switch (requestCode) {
            case READ_STORAGE_REQUEST_CODE: {
                autoSearch();
            }
            break;
        }
    }

    // if user enter the page by accepting the invitation on notification bar, then
    // cancel existing auto search task
    @Override
    protected void onJoiningGoogleMatchBegin(String matchId) {
        cancelGameAutoSearch();
        super.onJoiningGoogleMatchBegin(matchId);
    }

    private void autoSearch()
    {
        System.out.println("GameChooserPage.autoSearch()");

        cancelGameAutoSearch();

        gameListAdapter.clear();
        uniqueGamePathList.clear();

        // TODO: move last played game to first of the list

        //use cached file list
        addCachedGamePathsToList();

        //search for games
        ProgressDialog gameSearchProgressDlg = new ProgressDialog(getContext());
        gameSearchProgressDlg.setMessage(getString(R.string.auto_search_loading));
        gameSearchProgressDlg.setIndeterminate(true);
        gameSearchProgressDlg.setCancelable(true);
        gameSearchProgressDlg.setButton(DialogInterface.BUTTON_NEGATIVE, getString(R.string.cancel), new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                cancelGameAutoSearch();
            }
        });
        gameSearchProgressDlg.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                cancelGameAutoSearch();
            }
        });

        gameSearchTask = new GameAutoSearchTask(this, gameSearchProgressDlg);
        gameSearchTask.execute();
    }

    private void cancelGameAutoSearch()
    {
        System.out.println("GameChooserPage.cancelGameAutoSearch()");
        if (gameSearchTask != null)
            gameSearchTask.cancel(false);

        gameSearchTask = null;
    }

    private void onAutoSearchFinished() {
        try {
            if (gameListAdapter.getCount() == 0) {
                if (Utils.hasPermission(getActivity(), Manifest.permission.READ_EXTERNAL_STORAGE))
                    displayNoGameDialog();
                else
                    Utils.alertDialog(getContext(), null, getString(R.string.read_permission_msg), new Runnable() {
                        @Override
                        public void run() {
                            try {
                                // open app's settings page
                                Intent intent = new Intent();
                                intent.setAction(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                                Uri uri = Uri.fromParts("package", getContext().getPackageName(), null);
                                intent.setData(uri);
                                getActivity().startActivity(intent);
                            } catch (Exception e) {
                                e.printStackTrace();
                            }
                        }
                    });
            } else {
                sortGameListView();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void onAutoSearchCancelled() {
        sortGameListView();
    }

    private void addGameToList(String path, boolean fromCachedList) {
        addGameToList(new File(path), fromCachedList);
    }

    private void addGameToList(File oriFile, boolean fromCachedList) {
        if (!oriFile.exists())
            return;

        try {
            // convert to canonical path
            File file = oriFile.getCanonicalFile();
            GameEntry newGameEntry = new GameEntry(file);
            if (uniqueGamePathList.contains(newGameEntry))
                return;

            uniqueGamePathList.add(newGameEntry);
            gameListAdapter.add(newGameEntry);

            if (!fromCachedList && cachedGamePathList.size() < MAX_CACHED_FILES)
                cachedGamePathList.add(file);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void sortGameListView() {
        // since the game list is already sorted, but array adapter is not,
        // we only need to clear and re-add the list to the adapter
        gameListAdapter.clear();
        gameListAdapter.addAll(uniqueGamePathList);
        gameListAdapter.notifyDataSetChanged();
    }

    private void addCachedGamePathsToList() {
        Iterator<File> ite = cachedGamePathList.iterator();

        while (ite.hasNext()) {
            File file = ite.next();
            try {
                if (file.exists() && file.canRead())
                    addGameToList(file, true);
                else
                    ite.remove();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private void displayNoGameDialog() {
        final SharedPreferences pref = getContext().getSharedPreferences(Settings.IMMEDIATE_PREF_FILE, Context.MODE_PRIVATE);
        boolean disabledDialog = pref.getBoolean(Settings.DISABLE_NOGAME_DIALOG_KEY, false);
        if (!disabledDialog)
        {
            forceDisplayNoGameDialog(pref);
        }//if (!disabledDialog)
    }

    private void forceDisplayNoGameDialog(final SharedPreferences pref) {
        try {
            View dialogContent = getActivity().getLayoutInflater().inflate(R.layout.no_game_dialog_content_layout, null);
            final EditText nameTextView = (EditText)dialogContent.findViewById(R.id.game_name_to_search_on_inet_view);
            final CheckBox disableDialogCheckbox = (CheckBox)dialogContent.findViewById(R.id.dontShowAgainCheckBox);

            disableDialogCheckbox.setChecked(pref.getBoolean(Settings.DISABLE_NOGAME_DIALOG_KEY, false));

            //save user preference of displaying this dialog again in future or not
            disableDialogCheckbox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                    SharedPreferences.Editor editor = pref.edit();
                    editor.putBoolean(Settings.DISABLE_NOGAME_DIALOG_KEY, isChecked);
                    editor.commit();
                }
            });

            Utils.dialog(
                    getContext(),
                    getString(R.string.no_game_title),
                    getString(R.string.no_game_msg),
                    dialogContent,
                    new Runnable() {
                        @Override
                        public void run() {
                            String name = nameTextView.getText().toString();
                            if (name == null || name.length() == 0)
                            {
                                Utils.alertDialog(getContext(), "Error", getString(R.string.err_invalid_name), new Runnable() {
                                    @Override
                                    public void run() {
                                        forceDisplayNoGameDialog(pref);//display the popup again
                                    }
                                });
                                return;
                            }

                            //open webpage to search
                            internetSearchFor(name);
                        }
                    },
                    new Runnable() {
                        @Override
                        public void run() {
                            //TODO
                        }
                    });

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    static class GameEntry implements Comparable<GameEntry> {
        public GameEntry(File file)
        {
            this.file = file;
        }

        public GameEntry(String path)
        {
            this.file = new File(path);
        }

        File getFile() {
            return file;
        }

        @Override
        public String toString()
        {
            return file.getName();
        }

        @Override
        public int hashCode() {
            return file.hashCode();
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == null || (obj instanceof GameEntry) == false)
                return false;
            GameEntry rhs = (GameEntry)obj;

            return file.equals(rhs.getFile());
        }

        @Override
        public int compareTo(@NonNull GameEntry rhs) {
            return AlphabetFileNameComparator.compareName(file, rhs.getFile());
        }

        private File file;
    }

    private void internetSearchFor(String gameName) {
        String searchTerm = gameName + " NES rom";

        try {
            Intent search = new Intent(Intent.ACTION_WEB_SEARCH);
            search.putExtra(SearchManager.QUERY, searchTerm);

            startActivity(search);
        } catch (Exception e) {
            e.printStackTrace();

            //fallback
            Uri uri = Uri.parse("http://www.google.com/#q=" + searchTerm.replace(" ", "%20"));
            Intent intent = new Intent(Intent.ACTION_VIEW, uri);
            startActivity(intent);
        }
    }

    static class GameAutoSearchTask extends AsyncTask<Void, Void, Void>
    {
        private ProgressDialog progressDialog;
        private final WeakReference<GameChooserPage> parentPageRef;
        private final File sdcardAndroidDir;
        private final File sdcardDir;
        private final File secondarySdcardDir;

        private HashSet<File> excludedDirs = new HashSet<>();

        public GameAutoSearchTask(GameChooserPage parent, ProgressDialog progressDialog)
        {
            this.progressDialog = progressDialog;
            this.parentPageRef = new WeakReference<>(parent);

            // cache the path to /sdcard/ and any detected secondary sdcard
            // also cache the path to /sdcard/Android folder, this folder may contains a lot of files, so we will process it last
            File _sdcardAndroidDir = null;
            File _sdcardDir = null;
            String _secondarySdcardDirPath = null;

            try {
                _sdcardDir = Environment.getExternalStorageDirectory();
            } catch (Exception e) {
                e.printStackTrace();
            }

            try {
                _sdcardAndroidDir = new File(Environment.getExternalStorageDirectory(), "Android");
            } catch (Exception e) {
                e.printStackTrace();
            }

            try {
                // secondary sdcard
                _secondarySdcardDirPath = System.getenv("SECONDARY_STORAGE");
                if ((null == _secondarySdcardDirPath) || (_secondarySdcardDirPath.length() == 0)) {
                    _secondarySdcardDirPath = System.getenv("EXTERNAL_SDCARD_STORAGE");
                }
            }
            catch (Exception e)
            {
                e.printStackTrace();
            }

            this.sdcardDir = _sdcardDir;
            this.sdcardAndroidDir = _sdcardAndroidDir;
            this.secondarySdcardDir = (_secondarySdcardDirPath != null && _secondarySdcardDirPath.length() > 0) ? new File(_secondarySdcardDirPath) : null;
        }

        @Override
        protected  void onPreExecute()
        {
            this.progressDialog.show();
        }

        @Override
        protected Void doInBackground(Void ... params) {
            // ---------- sdcard --------------*/
            try {
                //search in sdcard
                searchFile(this.sdcardDir);

                //search in /sdcard/Android
                if (this.sdcardAndroidDir != null && this.sdcardAndroidDir.exists())
                    searchFile(this.sdcardAndroidDir);
            }
            catch (Exception e)
            {
                e.printStackTrace();
            }

            excludeDirectory(this.sdcardDir);

            // --------------- search in user data directory ----------------
            try {
                searchFile(Environment.getDataDirectory());
            }
            catch (Exception e)
            {
                e.printStackTrace();
            }

            try {
                excludeDirectory(Environment.getDataDirectory());
            }
            catch (Exception e)
            {
                e.printStackTrace();
            }

            // ------------- search in secondary sdcard ------------------------
            try {
                searchFile(this.secondarySdcardDir);
            }
            catch (Exception e)
            {
                e.printStackTrace();
            }

            excludeDirectory(this.secondarySdcardDir);

            // -------------- search all mounted storages --------------
            try {
                searchMountedStorages();
            } catch (Exception e) {
                e.printStackTrace();
            }

            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            dismissProgressDialog();

            GameChooserPage parentActivity = this.parentPageRef.get();
            if (parentActivity != null) {
                parentActivity.onAutoSearchFinished();
            }
        }

        @Override
        protected void onCancelled(Void result) {
            dismissProgressDialog();

            GameChooserPage parentActivity = this.parentPageRef.get();
            if (parentActivity != null) {
                parentActivity.onAutoSearchCancelled();
            }
        }

        private void dismissProgressDialog() {
            if (this.progressDialog != null) {
                this.progressDialog.dismiss();
                this.progressDialog = null;
            }
        }

        private File getCanonicalFile(File file) {
            File canonicalFile;
            try {
                canonicalFile = file.getCanonicalFile();
            } catch (Exception e) {
                e.printStackTrace();
                canonicalFile = file;
            }

            return canonicalFile;
        }

        private void excludeDirectory(File dir) {
            if (dir == null)
                return;

            if (dir != null) {
                this.excludedDirs.add(getCanonicalFile(dir));
            }
        }

        private boolean isDirectoryExcluded(File dir) {
            if (dir == null)
                return true;

            if (this.excludedDirs.size() <= 0)
                return false;

            return this.excludedDirs.contains(getCanonicalFile(dir));
        }

        private void searchMountedStorages() throws Exception {
            Set<String> mountedStorages = getMountedStorages();
            for (String mounted: mountedStorages) {
                if (mounted != null) {
                    try {
                        File file = new File(mounted);
                        if (file.exists())
                            searchFile(file);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            }
        }

        private Set<String> getMountedStorages() throws Exception {
            final HashSet<String> out = new HashSet<String>();
            if (true) {
                out.add("/storage/");
                out.add("/mnt/");
            } else {
                // solution from https://stackoverflow.com/questions/11281010/how-can-i-get-external-sd-card-path-for-android-4-0
                String reg = "(?i).*vold.*(vfat|ntfs|exfat|fat32|ext3|ext4).*rw.*";
                final Process process = new ProcessBuilder().command("mount")
                        .redirectErrorStream(true).start();
                process.waitFor();
                final InputStream is = process.getInputStream();
                BufferedReader br = new BufferedReader(new InputStreamReader(is));
                String line = null;
                while ((line = br.readLine()) != null) {
                    // parse output
                    if (!line.toLowerCase(Locale.US).contains("asec")) {
                        if (line.matches(reg)) {
                            String[] parts = line.split(" ");
                            for (String part : parts) {
                                if (part.startsWith("/"))
                                    if (!part.toLowerCase(Locale.US).contains("vold"))
                                        out.add(part);
                            }
                        }
                    }
                }
                br.close();
            }

            return out;
        }

        private void searchFile(final File file)
        {
            if (isCancelled() || file == null)
                return;

            if (file.isDirectory())
            {
                if (isDirectoryExcluded(file))
                    return;

                File[] files = file.listFiles();

                if (files != null) {
                    for (File subfile : files) {
                        if (isCancelled())
                            return;

                        boolean isSdcardAndroidDir = subfile.equals(this.sdcardAndroidDir);
                        if (!isSdcardAndroidDir)//ignore the folder "/sdcard/Android", this folder may contains a lot of files, so we won't process it here
                            searchFile(subfile);
                    }
                }
            }//if (file.isDirectory())
            else {
                //real file
                String name = file.getName();
                final GameChooserPage parentPage;
                final Activity activity;

                if ((name.endsWith(".zip") || name.endsWith(".ZIP")
                        || name.endsWith(".nes"))
                        && (parentPage = parentPageRef.get()) != null
                        && (activity = parentPage.getActivity()) != null)
                {
                    if (BaseActivity.verifyGame(activity, file.getAbsolutePath()))
                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                if (parentPage.gameSearchTask == GameAutoSearchTask.this)
                                    parentPage.addGameToList(file, false);
                                else {
                                    System.out.println("Cancelled Auto search task trying to update the UI. Ignored");
                                }
                            }
                        });
                }
            }//if (file.isDirectory())
        }
    }

    /* ------------ comparator that compares file name instead of full path ----------*/
    private static class AlphabetFileNameComparator implements Comparator<File> {

        @Override
        public int compare(File file1, File file2) {
            return compareName(file1, file2);
        }

        public static int compareName(File file1, File file2) {
            if (file1 == file2)
                return 0;
            if (file1 == null)
                return -1;
            if (file2 == null)
                return 1;

            // compare ignore case first
            int nameCompare = file1.getName().compareToIgnoreCase(file2.getName());

            // if equal, compare case sensitive
            if (nameCompare == 0)
                nameCompare = file1.getName().compareTo(file2.getName());

            // if still equal, compare full path
            if (nameCompare == 0)
                return file1.compareTo(file2);

            return nameCompare;
        }
    }

}
