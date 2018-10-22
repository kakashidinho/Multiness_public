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
import android.content.Context;
import android.content.DialogInterface;
import android.os.AsyncTask;
import android.os.Environment;

import java.io.File;
import java.lang.ref.WeakReference;
import java.security.InvalidParameterException;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;

/**
 * Created by le on 4/1/2016.
 */
public class FileListingTask extends AsyncTask<Void, Void, Void>
{
    public static interface Delegate {
        public void doOnUiThread(FileListingTask task, Runnable r);
        public void addFileToListOnUiThread(FileListingTask task, FileEntry file);
        public void clearFileListOnUiThread(FileListingTask task);
        public void onFileListFinish(FileListingTask task); // this is called when the task finishes or is canceled
    }

    private static File sSdPath;
    private static File sDataPath;

    private final File mPrivateDataPath;

    private final WeakReference<Delegate> mDelegateRef;
    private ProgressDialog mProgressDialog;
    private final FileEntry mCurrentDirectory;
    private final boolean mListDirectoryOnly;
    private final FileSortType mSortType;

    public enum FileSortType {
        NO_SORTING,
        NEWEST_FIRST,
        OLDEST_FIRST,
    }

    public FileListingTask(Context context, Delegate delegate, FileEntry currentDirectory) {
        this(context, delegate, currentDirectory, false);
    }

    public FileListingTask(Context context, Delegate delegate, FileEntry currentDirectory, boolean listDirectoryOnly) {
        this(context, delegate, currentDirectory, listDirectoryOnly, FileSortType.NO_SORTING);
    }

    public FileListingTask(Context context, Delegate delegate, FileEntry currentDirectory, boolean listDirectoryOnly, FileSortType sortType)
    {
        this.mDelegateRef = new WeakReference<Delegate>(delegate);
        this.mCurrentDirectory = currentDirectory;
        this.mPrivateDataPath = context.getFilesDir();

        this.mProgressDialog = new ProgressDialog(context);
        this.mProgressDialog.setMessage(context.getString(R.string.loading_msg));
        this.mProgressDialog.setIndeterminate(true);
        this.mProgressDialog.setCancelable(false);
        this.mProgressDialog.setButton(DialogInterface.BUTTON_NEGATIVE, context.getString(R.string.cancel), new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                FileListingTask.this.cancel(false);
            }
        });

        this.mListDirectoryOnly = listDirectoryOnly;
        this.mSortType = sortType;
    }

    @Override
    protected  void onPreExecute()
    {
        this.mProgressDialog.show();
    }

    @Override
    protected Void doInBackground(Void ... params) {

        clearFileList();

        if (mCurrentDirectory == null)//root
        {
            try {
                addFileToList(new FileEntry(new File("/"), "file system"));
            }
            catch (Exception e) {
                e.printStackTrace();
            }

            try {
                //sdcard
                File sdPath = sSdPath;
                addFileToList(new FileEntry(sdPath, "sdcard"));
            }
            catch (Exception e)
            {
                e.printStackTrace();
            }

            try {
                //user data directory
                File dataPath = sDataPath;
                addFileToList(new FileEntry(dataPath));
            }
            catch (Exception e)
            {
                e.printStackTrace();
            }
        }//if (mCurrentDirectory == null)
        else
        {
            try {
                addFileToList(new FileEntry(".."));
                File[] files = listFiles();
                for (File file : files) {
                    if (isCancelled())
                        break;
                    if (!file.getName().equals("..") && !file.getName().equals(".") && (file.isDirectory() || !mListDirectoryOnly))
                        addFileToList(new FileEntry(file));
                }
            }
            catch (Exception e)
            {
                e.printStackTrace();
            }
        }//if (mCurrentDirectory == null)

        return null;
    }

    @Override
    protected void onPostExecute(Void result) {
        dismissProgressDialog();

        Delegate delegate = mDelegateRef.get();
        if (delegate != null)
            delegate.onFileListFinish(this);
    }

    @Override
    protected void onCancelled(Void result) {
        dismissProgressDialog();

        Delegate delegate = mDelegateRef.get();
        if (delegate != null)
            delegate.onFileListFinish(this);
    }

    private void dismissProgressDialog() {
        if (mProgressDialog != null) {
            mProgressDialog.dismiss();
            mProgressDialog = null;
        }
    }

    public static File getSdPath() {
        return sSdPath;
    }

    public static File getDataPath() {
        return sDataPath;
    }

    private void clearFileList()
    {
        Delegate delegate = mDelegateRef.get();
        if (delegate == null)
            return;

        delegate.clearFileListOnUiThread(this);
    }

    private File[] listFiles(){
        File[] files = mCurrentDirectory.getFile().listFiles();
        //check if this directory contains the private data directory directly or indirectly
        File privateDataParent = null;
        try {
            File p = mPrivateDataPath;
            while (privateDataParent == null && p != null) {
                File parent = p.getParentFile();
                if (parent.equals(mCurrentDirectory.getFile()))
                {
                    privateDataParent = p;
                }
                else
                    p = parent;
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        if (privateDataParent != null) {
            HashSet<File> fileSet;
            if (files != null)
                fileSet = new HashSet<>(Arrays.asList(files));
            else
                fileSet = new HashSet<>();//empty list

            fileSet.add(privateDataParent);

            files = fileSet.toArray(new File[0]);
        }//if (privateDataParent != null)

        //sort file list
        if (files != null) {
            Arrays.sort(files, new FileEntryComparator());
        }//if (files != null)

        return files;
    }

    private void addFileToList(final FileEntry file)
    {
        if (isCancelled() || file == null)
            return;

        Delegate delegate = mDelegateRef.get();
        if (delegate == null)
            return;

        delegate.addFileToListOnUiThread(this, file);
    }

    private class FileEntryComparator implements Comparator<File> {
        @Override
        public int compare(File lhs, File rhs) {
            //directory should be listed before file
            if (lhs.isDirectory() != rhs.isDirectory())
                return lhs.isDirectory() ? -1 : 1;

            switch (mSortType)
            {
                case NEWEST_FIRST:
                {
                    if (lhs.lastModified() < rhs.lastModified())
                        return 1;
                    if (lhs.lastModified() > rhs.lastModified())
                        return -1;
                }
                break;
                case OLDEST_FIRST:
                {
                    if (lhs.lastModified() > rhs.lastModified())
                        return 1;
                    if (lhs.lastModified() < rhs.lastModified())
                        return -1;
                }
                    break;
                default:
            }

            return 0;//don't care
        }
    }

    /*------- FileEntry -----*/
    public static class FileEntry {
        public FileEntry(File file)
        {
            this(file, null);
        }

        public FileEntry(String path)
        {
            this(new File(path), null);
        }

        public FileEntry(File file, String name)
        {
            this.file = file;
            this.name = name;

            if (file == null)
                throw new InvalidParameterException("File must not be null");
        }

        FileEntry getParent() {
            if (file.equals("/"))
                return null;
            if (file.equals(sSdPath))
                return null;
            if (file.equals(sDataPath))
                return null;

            return new FileEntry(file.getParentFile());
        }

        String getAbsolutePath() {
            return file.getAbsolutePath();
        }

        boolean isDirectory() {
            return file.isDirectory();
        }

        @Override
        public String toString()
        {
            return name != null ? name: file.getName();
        }

        private File getFile() {
            return file;
        }

        private final File file;
        private final String name;
    }


    static {
        try {
            //sdcard
            sSdPath = Environment.getExternalStorageDirectory();
        } catch (Exception e) {
            e.printStackTrace();

            sSdPath = null;
        }

        try {
            //user data directory
            sDataPath = Environment.getDataDirectory();
        }
        catch (Exception e)
        {
            e.printStackTrace();
            sDataPath = null;
        }
    }
}