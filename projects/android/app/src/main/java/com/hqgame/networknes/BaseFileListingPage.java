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

import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import java.io.File;
import java.lang.ref.WeakReference;

/**
 * Created by le on 4/2/2016.
 */
public abstract class BaseFileListingPage extends BasePage implements FileListingTask.Delegate, AdapterView.OnItemClickListener {
    private static final String CURRENT_DIR_KEY = "currentDirectoryKey";

    private ArrayAdapter<FileListingTask.FileEntry> fileListAdapter;

    private TextView currentDirectoryTextView;
    private FrameLayout upContainerView;
    private View fileListSeparatorView;
    private View upItemView; // this view will allow user to go back to parent directory
    private WeakReference<FileListingTask.FileEntry> selectedFileEntryRef = new WeakReference<FileListingTask.FileEntry>(null);
    private WeakReference<View> selectedListItemRef = new WeakReference<View>(null);;
    private FileListingTask.FileEntry currentDirectory = null;
    private boolean listDirectoryOnly = false;
    private WeakReference<FileListingTask> fileListingTaskRef = new WeakReference<FileListingTask>(null);
    private FileListingTask.FileSortType fileSortType = FileListingTask.FileSortType.NO_SORTING;

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
    {
        return onCreateViewWithDefaultToolbar(inflater, container, savedInstanceState, false);
    }

    protected View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState, boolean listDirectoryOnly)
    {
        return onCreateViewWithDefaultToolbar(inflater, container, savedInstanceState, listDirectoryOnly, FileListingTask.FileSortType.NO_SORTING, R.layout.page_file_listing_base);
    }

    protected View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState, FileListingTask.FileSortType sortType)
    {
        return onCreateViewWithDefaultToolbar(inflater, container, savedInstanceState, sortType, R.layout.page_file_listing_base);
    }

    protected View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState, FileListingTask.FileSortType sortType, int contentViewLayoutId)
    {
        return onCreateViewWithDefaultToolbar(inflater, container, savedInstanceState, false, sortType, contentViewLayoutId);
    }

    protected View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState, boolean listDirectoryOnly, FileListingTask.FileSortType sortType, int contentViewLayoutId) {
        View v = inflater.inflate(contentViewLayoutId, container, false);

        this.listDirectoryOnly = listDirectoryOnly;
        this.fileSortType = sortType;

        currentDirectoryTextView = (TextView)v.findViewById(R.id.ui_current_directory_name);
        upContainerView = (FrameLayout)v.findViewById(R.id.ui_up_container);
        fileListSeparatorView = v.findViewById(R.id.ui_file_list_separator);
        upItemView = inflater.inflate(getItemViewLayout(), upContainerView, false);
        TextView upTextView = (TextView) upItemView.findViewById(R.id.file_name_txt_view);
        upTextView.setText("..");
        ImageView upImageView = (ImageView) upItemView.findViewById(R.id.file_type_img_view);
        if (upImageView != null)
            upImageView.setImageResource(getUpFolderDrawable());

        final ListView fileListView = (ListView)v.findViewById(R.id.ui_file_list);

        //create files list adapter
        fileListAdapter = new FileListViewAdapter();

        fileListView.setAdapter(fileListAdapter);

        //register list view's on click listener
        fileListView.setOnItemClickListener(this);

        // register up item view's event listeners
        upItemView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                goUp();
            }
        });
        upItemView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        v.setBackgroundResource(android.R.color.darker_gray);// highlight it
                        break;
                    case MotionEvent.ACTION_CANCEL:
                    case MotionEvent.ACTION_UP:
                        v.setBackgroundResource(android.R.color.transparent);
                        break;
                }
                return false;
            }
        });

        additionalViewConfig(v, savedInstanceState);

        if (currentDirectory == null && savedInstanceState != null && savedInstanceState.containsKey(CURRENT_DIR_KEY))
        {
            String currentDirectioryPath = savedInstanceState.getString(CURRENT_DIR_KEY, getContext().getFilesDir().getAbsolutePath());
            currentDirectory = new FileListingTask.FileEntry(currentDirectioryPath);
        }
        goIntoDirectory(currentDirectory);

        return v;
    }

    @Override
    public void onSaveInstanceState (Bundle outState) {
        if (currentDirectory != null) {
            outState.putString(CURRENT_DIR_KEY, currentDirectory.getAbsolutePath());
        }
        super.onSaveInstanceState(outState);
    }

    @Override
    public void onResume() {
        super.onResume();
    }

    // if user enter the page by accepting the invitation on notification bar, then
    // cancel existing listing task
    @Override
    protected void onJoiningGoogleMatchBegin(String matchId) {
        cancelFileListingTask();
        super.onJoiningGoogleMatchBegin(matchId);
    }

    // FileListingTask.Delegate
    @Override
    public void doOnUiThread(FileListingTask task, Runnable r)
    {
        getActivity().runOnUiThread(r);
    }

    @Override
    public void addFileToListOnUiThread(final FileListingTask task, final FileListingTask.FileEntry file) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                FileListingTask currentTask = fileListingTaskRef.get();
                if (currentTask != task) {
                    System.out.println("Cancelled task trying to update the UI. Ignored");
                    return;
                }
                if (file.toString().equals(".."))
                    return;
                fileListAdapter.add(file);
            }
        });
    }
    @Override
    public void clearFileListOnUiThread(final FileListingTask task) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                FileListingTask currentTask = fileListingTaskRef.get();
                if (currentTask != task) {
                    System.out.println("Cancelled task trying to update the UI. Ignored");
                    return;
                }
                fileListAdapter.clear();
            }
        });
    }

    @Override
    public void onFileListFinish(final FileListingTask task) {
        // no need to add since add() already calls notifyDataSetChanged()
        // fileListAdapter.notifyDataSetChanged();
    }
    //-------------

    public String getCurrentDirectory() {
        return currentDirectory != null ? currentDirectory.getAbsolutePath() : null;
    }

    private void select(FileListingTask.FileEntry file)
    {
        selectedFileEntryRef = new WeakReference<FileListingTask.FileEntry>(file);

        if (file.toString().equals(".."))//parent
        {
            FileListingTask.FileEntry parent = null;
            try {
                parent = currentDirectory.getParent();
            }
            catch (Exception e) {
                e.printStackTrace();
                parent = null;
            }

            goIntoDirectory(parent);
        }
        else if (file.isDirectory())//directory
        {
            goIntoDirectory(file);
        }
        else// file
        {
            onSelectedFile(file.getAbsolutePath());
        }
    }

    protected void goUp() {
        if (currentDirectory == null) // root
            finish();
        else {
            FileListingTask.FileEntry parent = null;
            try {
                parent = currentDirectory.getParent();
            }
            catch (Exception e) {
                e.printStackTrace();
                parent = null;
            }

            goIntoDirectory(parent);
        }
    }

    protected void goIntoDirectory(File path) {
         if (path.exists())
            goIntoDirectory(new FileListingTask.FileEntry(path));
    }

    protected void goIntoDirectory(String path) {
        if (path == null)
            return;
        File file = new File(path);
        goIntoDirectory(file);
    }

    private void goIntoDirectory(FileListingTask.FileEntry directory)
    {
        currentDirectory = directory;

        if (currentDirectory != null)
        {
            currentDirectoryTextView.setText(currentDirectory.getAbsolutePath());

            // show separator between "up" and file list
            fileListSeparatorView.setVisibility(View.VISIBLE);

            // add up item view
            upContainerView.removeAllViews();
            upContainerView.addView(upItemView);
        }
        else {
            //this is root
            currentDirectoryTextView.setText("root");

            // hide separator
            fileListSeparatorView.setVisibility(View.INVISIBLE);
            // remove up item view
            upContainerView.removeAllViews();
        }

        // we have entered into a new directory,
        // should de-highlight the selected item
        View selectedListItem = selectedListItemRef.get();
        if (selectedListItem != null) {
            selectedListItem.setBackgroundResource(android.R.color.transparent);
        }
        //invalidate selected listview's item
        selectedListItemRef = new WeakReference<View>(null);

        //cancel current task
        cancelFileListingTask();

        FileListingTask fileListingTask = new FileListingTask(getContext(), this, currentDirectory, listDirectoryOnly, this.fileSortType);
        fileListingTaskRef = new WeakReference<FileListingTask>(fileListingTask);
        fileListingTask.execute();

        onSelectedDirectory(getCurrentDirectory());
    }

    protected void refresh() {
        goIntoDirectory(currentDirectory);
    }

    private void cancelFileListingTask() {
        FileListingTask currentTask = fileListingTaskRef.get();
        if (currentTask != null)
        {
            currentTask.cancel(false);
        }
    }

    /*---------- AdapterView.OnItemClickListener implementation --------------*/
    @Override
    public void onItemClick(AdapterView<?> adapterView, View view, int position, long id) {
        final FileListingTask.FileEntry item = (FileListingTask.FileEntry) adapterView.getItemAtPosition(position);

        View selectedListItem = selectedListItemRef.get();
        if (selectedListItem != null) {
            selectedListItem.setBackgroundResource(android.R.color.transparent);
        }

        //highligh selected item
        view.setBackgroundResource(android.R.color.darker_gray);
        selectedListItemRef = new WeakReference<View>(view);

        select(item);
    }

    private class FileListViewAdapter extends ArrayAdapter<FileListingTask.FileEntry> {

        public FileListViewAdapter() {
            super(BaseFileListingPage.this.getContext(), getItemViewLayout(), R.id.file_name_txt_view);
        }

        @Override
        public View getView(final int position, View convertView, ViewGroup parent) {
            View v = super.getView(position, convertView, parent);

            final FileListingTask.FileEntry item = (FileListingTask.FileEntry) this.getItem(position);
            if (item != selectedFileEntryRef.get())
                v.setBackgroundResource(android.R.color.transparent);
            else {
                v.setBackgroundResource(android.R.color.darker_gray);//highlight selected file
                selectedListItemRef = new WeakReference<View>(v);
            }

            //make text bold if this is directory
            TextView textView = (TextView) v.findViewById(R.id.file_name_txt_view);
            if (textView != null) {
                if (item != null && item.isDirectory()) {
                    textView.setTypeface(null, Typeface.BOLD);
                    textView.setTextColor(Color.rgb(0,0,128));
                }
                else {
                    textView.setTypeface(null, Typeface.NORMAL);
                    textView.setTextColor(Color.BLACK);
                }
            } else {
                throw new RuntimeException("Item view must include 'file_name_txt_view' id for text view");
            }

            // set file icon
            ImageView iconView = (ImageView) v.findViewById(R.id.file_type_img_view);
            if (iconView != null) {
                if (item != null) {
                    if (item.toString().equals("..")) {
                        iconView.setImageResource(getUpFolderDrawable());
                        iconView.setOnClickListener(null);
                    }
                    else if (item.isDirectory()) {
                        iconView.setImageResource(getFolderTypeDrawable());
                        iconView.setOnClickListener(null);
                    } else {
                        iconView.setImageResource(getFileTypeDrawable());

                        iconView.setOnClickListener(new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                onFileIconClick(FileListViewAdapter.this, v, position);
                            }
                        });
                    }
                }
            }

            return v;
        }
    }

    // ------------ implementation dependent -------------------
    protected void additionalViewConfig(View contentView, Bundle savedInstanceState) {
        //this is called at the end of this class's onCreateView
    }

    protected void onSelectedDirectory(String absolutePath) {
    }

    protected void onFileIconClick(ArrayAdapter<FileListingTask.FileEntry> adapter, View view, int position) {
    }

    protected int getItemViewLayout() {
        return R.layout.file_list_item_layout;
    }

    protected int getFileTypeDrawable() {
        return R.drawable.text_file_drawable;
    }

    protected int getFolderTypeDrawable() {
        return R.drawable.folder_black_drawable;
    }

    protected int getUpFolderDrawable() {
        return R.drawable.up_black_drawable;
    }

    protected abstract void onSelectedFile(String absolutePath);
}
