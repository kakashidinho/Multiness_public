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
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.AsyncTask;
import android.os.Build;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.text.Html;
import android.text.Spanned;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.ref.WeakReference;
import java.math.BigInteger;
import java.net.InetAddress;
import java.nio.ByteOrder;

/**
 * Created by le on 3/6/2016.
 */
public class Utils {
    private static WifiManager.MulticastLock sMulticastLock = null;

    public static long generateId() {
        //sleep to avoid duplicated id
        try {
            Thread.sleep(5);
        } catch (Exception e) {
            e.printStackTrace();
        }
        long id = System.currentTimeMillis();

        return id;
    }

    public static String getHostIPAddress(Context context)
    {
        try {
            WifiManager wm = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
            WifiInfo info = wm.getConnectionInfo();

            if (info == null)
                return null;

            int ipAddress = info.getIpAddress();
            ipAddress = (ByteOrder.nativeOrder().equals(ByteOrder.LITTLE_ENDIAN)) ? Integer.reverseBytes(ipAddress) : ipAddress;
            byte[] addressBytes = BigInteger.valueOf(ipAddress).toByteArray();

            InetAddress inetAddress = InetAddress.getByAddress(addressBytes);
            return inetAddress.getHostAddress();
        } catch (Exception e)
        {
            e.printStackTrace();
            return null;
        }
    }

    public static synchronized void enableMulticast(Context context, boolean enable) {
        try {
            WifiManager wm = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);

            if (sMulticastLock == null)
                sMulticastLock = wm.createMulticastLock("multicastLock");

            if (enable) {
                sMulticastLock.acquire();

                System.out.println("Multicast enabled");
            }
            else {
                if (sMulticastLock.isHeld()) {
                    sMulticastLock.release();

                    System.out.println("Multicast disabled");
                }
                sMulticastLock = null;
            }

        } catch (Exception e)
        {
            e.printStackTrace();
        }
    }

    public static Dialog errorDialog(int messageResId) {
        return errorDialog(messageResId, null);
    }

    public static Dialog errorDialog(int messageResId, Runnable dialogClosedCallback) {
        BaseActivity activity = BaseActivity.getCurrentActivity();
        if (activity == null)
            return null;
        return errorDialog(activity, getSpannedText(activity.getString(messageResId)), dialogClosedCallback);
    }

    public static Dialog errorDialog(CharSequence message) {
        if (BaseActivity.getCurrentActivity() == null)
            return null;
        return errorDialog(BaseActivity.getCurrentActivity(), message, null);
    }

    public static Dialog errorDialog(Context context, CharSequence message) {
        return errorDialog(context, message, null);
    }

    public static Dialog errorDialog(Context context, CharSequence message, final Runnable dialogClosedCallback) {
        return alertDialog(context, context.getString(R.string.generic_err_title), message, dialogClosedCallback);
    }

    public static Dialog alertDialog(Context context, String title, CharSequence message, final Runnable dialogClosedCallback)
    {
        return alertDialog(context, title, message, null, dialogClosedCallback, null);
    }

    public static Dialog dialog(Context context, String title, CharSequence message, View addtionalView, final Runnable dialogPostitiveClickedCallback, final Runnable dialogClosedCallback)
    {
        return dialog(context, 0, title, message, addtionalView, dialogPostitiveClickedCallback, dialogClosedCallback);
    }

    public static Dialog alertDialog(Context context, String title, CharSequence message, View addtionalView, final Runnable dialogPostitiveClickedCallback, final Runnable dialogClosedCallback)
    {
        return dialog(context, R.drawable.ic_dialog_alert_holo_light, title, message, addtionalView, dialogPostitiveClickedCallback, dialogClosedCallback);
    }

    public static Dialog alertDialog(Context context, String title, CharSequence message, int dialogPositiveBtnTextRes, final Runnable dialogPostitiveClickedCallback, final Runnable dialogClosedCallback)
    {
        return dialog(context, R.drawable.ic_dialog_alert_holo_light, title, message, null, dialogPositiveBtnTextRes, dialogPostitiveClickedCallback, dialogClosedCallback);
    }

    public static Dialog alertDialog(Context context, String title, CharSequence message, View addtionalView, int dialogPositiveBtnTextRes, final Runnable dialogPostitiveClickedCallback, final Runnable dialogClosedCallback)
    {
        return dialog(context, R.drawable.ic_dialog_alert_holo_light, title, message, addtionalView, dialogPositiveBtnTextRes, dialogPostitiveClickedCallback, dialogClosedCallback);
    }

    public static Dialog dialog(Context context, int iconResId, String title, CharSequence message, View addtionalView, final Runnable dialogPostitiveClickedCallback, final Runnable dialogClosedCallback) {
        return dialog(context, iconResId, title, message, addtionalView, R.string.ok, dialogPostitiveClickedCallback, dialogClosedCallback);
    }

    public static Dialog dialog(Context context, int iconResId, String title, CharSequence message, View addtionalView, int dialogPositiveBtnTextRes, final Runnable dialogPostitiveClickedCallback, final Runnable dialogClosedCallback) {
        AlertDialog.Builder builder = new AlertDialog.Builder(context)
                .setTitle(title)
                .setMessage(message)
                .setPositiveButton(dialogPositiveBtnTextRes, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        if (dialogPostitiveClickedCallback != null)
                            dialogPostitiveClickedCallback.run();
                    }
                })
                .setIcon(iconResId);

        if (dialogClosedCallback != null) {
            builder.setOnCancelListener(new DialogInterface.OnCancelListener() {
                @Override
                public void onCancel(DialogInterface dialog) {
                    dialogClosedCallback.run();
                }
            });

            builder.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    dialogClosedCallback.run();
                }
            });
        }//if (dialogClosedCallback != null)

        if (addtionalView != null)
            builder.setView(addtionalView);

        return builder.show();
    }

    public static ProgressDialog showProgressDialog(Context context, String message, final Runnable cancelCallback) {
        ProgressDialog progressDlg = new ProgressDialog(context);
        progressDlg.setMessage(message);
        progressDlg.setIndeterminate(true);
        progressDlg.setCancelable(false);
        progressDlg.setButton(DialogInterface.BUTTON_NEGATIVE, context.getString(R.string.cancel), new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                if (cancelCallback != null)
                    cancelCallback.run();
            }
        });

        progressDlg.show();
        return progressDlg;
    }

    public static void setTexAppearance(TextView textView, Context context, int resId)
    {
        if (Build.VERSION.SDK_INT < 23)
            textView.setTextAppearance(context, resId);
        else
            textView.setTextAppearance(resId);
    }

    @SuppressWarnings("deprecation")
    public static Spanned getSpannedText(String text) {
        Spanned re = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            re = Html.fromHtml(text, Html.FROM_HTML_MODE_COMPACT);
        } else {
            re = Html.fromHtml(text);
        }

        return re;
    }

    public static Bitmap downloadBitmapAndWait(String url) {
        try {
            InputStream in = new java.net.URL(url).openStream();

            Bitmap bitmap = BitmapFactory.decodeStream(new FlushedInputStream(in));

            return bitmap;
        } catch (Exception e) {
            e.printStackTrace();

            return null;
        }
    }

    public static AsyncTask downloadBitmapAsync(Resources res, String url, ImageView imageView) {
        BitmapDownloaderTask task = new BitmapDownloaderTask(url, imageView);

        downloadBitmapAsync(res, task, imageView);

        return task;
    }

    public static void downloadBitmapAsync(Resources res, BitmapDownloaderTask downloaderTask, ImageView imageView) {
        if (cancelPotentialDownload(downloaderTask.getUrl(), imageView)) {
            DownloadedDrawable downloadedDrawable = new DownloadedDrawable(res, downloaderTask);
            imageView.setImageDrawable(downloadedDrawable);
            downloaderTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    private static boolean cancelPotentialDownload(String url, ImageView imageView) {
        BitmapDownloaderTask bitmapDownloaderTask = getBitmapDownloaderTask(imageView);

        if (bitmapDownloaderTask != null) {
            String bitmapUrl = bitmapDownloaderTask.getUrl();
            if ((bitmapUrl == null) || (!bitmapUrl.equals(url))) {
                bitmapDownloaderTask.cancel();
            } else {
                // The same URL is already being downloaded.
                return false;
            }
        }
        return true;
    }

    private static BitmapDownloaderTask getBitmapDownloaderTask(ImageView imageView) {
        if (imageView != null) {
            Drawable drawable = imageView.getDrawable();
            if (drawable instanceof DownloadedDrawable) {
                DownloadedDrawable downloadedDrawable = (DownloadedDrawable)drawable;
                return downloadedDrawable.getBitmapDownloaderTask();
            }
        }
        return null;
    }

    public static boolean hasPermission(final Activity activity, final String permission) {
        return ContextCompat.checkSelfPermission(activity, permission) == PackageManager.PERMISSION_GRANTED;
    }

    public static boolean checkPermission(final Activity activity, final String permission, String explainText, int requestCode) {
        return checkPermission(activity, permission, explainText, requestCode,null);
    }

    public static boolean checkPermission(final Activity activity, final String permission, String explainText, final int requestCode, Runnable explainDialogCancelHandler) {
        return checkPermission(activity, permission, explainText, requestCode, false, explainDialogCancelHandler);
    }

    public static boolean checkPermission(final Activity activity, final String permission, String explainText, final int requestCode, boolean forceExplain) {
        return checkPermission(activity, permission, explainText, requestCode, forceExplain, null);
    }

    public static boolean checkPermission(final Activity activity, final String permission, String explainText, final int requestCode, boolean forceExplain, Runnable explainDialogCancelHandler) {
        if (ContextCompat.checkSelfPermission(activity, permission) != PackageManager.PERMISSION_GRANTED) {
            if (forceExplain || ActivityCompat.shouldShowRequestPermissionRationale(activity, permission)) {
                if (explainDialogCancelHandler == null)
                    explainDialogCancelHandler = new Runnable() {
                        @Override
                        public void run() {
                            ActivityCompat.requestPermissions(activity, new String[] {permission}, requestCode);
                        }
                    };
                alertDialog(activity, null, explainText, explainDialogCancelHandler);
            } else {
                ActivityCompat.requestPermissions(activity, new String[] {permission}, requestCode);
            }

            return false;
        }

        return true;
    }

    /*-----------------  FlushedInputStream -------------------*/
    //See http://android-developers.blogspot.sg/2010/07/multithreading-for-performance.html
    public static class FlushedInputStream extends FilterInputStream {
        public FlushedInputStream(InputStream inputStream) {
            super(inputStream);
        }

        @Override
        public long skip(long n) throws IOException {
            long totalBytesSkipped = 0L;
            while (totalBytesSkipped < n) {
                long bytesSkipped = in.skip(n - totalBytesSkipped);
                if (bytesSkipped == 0L) {
                    int _byte = read();
                    if (_byte < 0) {
                        break;  // we reached EOF
                    } else {
                        bytesSkipped = 1; // we read one byte
                    }
                }
                totalBytesSkipped += bytesSkipped;
            }
            return totalBytesSkipped;
        }
    }

    /*------- BitmapDownloaderTask --------*/
    public static class BitmapDownloaderTask extends AsyncTask<Void, Void, Bitmap> implements AsyncOperation {
        private String url;
        private final WeakReference<ImageView> imageViewReference;

        public BitmapDownloaderTask(String url, ImageView imageView) {
            this.imageViewReference = new WeakReference<ImageView>(imageView);
            this.url = url;
        }

        public String getUrl() {
            return url;
        }

        @Override
        // Actual download method, run in the task thread
        protected Bitmap doInBackground(Void... params) {
            return downloadBitmapAndWait(url);
        }

        @Override
        // Once the image is downloaded, associates it to the imageView
        protected void onPostExecute(Bitmap bitmap) {
            if (isCancelled()) {
                bitmap = null;
            }

            if (imageViewReference != null) {
                ImageView imageView = imageViewReference.get();
                BitmapDownloaderTask bitmapDownloaderTask = getBitmapDownloaderTask(imageView);
                // Change bitmap only if this process is still associated with it
                if (this == bitmapDownloaderTask) {
                    imageView.setImageBitmap(bitmap);
                }
            }
        }

        @Override
        public void cancel() {
            super.cancel(true);
        }
    }

    /*------ DownloadedDrawable ----*/
    static class DownloadedDrawable extends BitmapDrawable {
        private final WeakReference<BitmapDownloaderTask> bitmapDownloaderTaskReference;

        public DownloadedDrawable(Resources resources, BitmapDownloaderTask bitmapDownloaderTask) {
            super(resources, BitmapFactory.decodeResource(resources, R.drawable.white));
            bitmapDownloaderTaskReference =
                    new WeakReference<BitmapDownloaderTask>(bitmapDownloaderTask);
        }

        public BitmapDownloaderTask getBitmapDownloaderTask() {
            return bitmapDownloaderTaskReference.get();
        }
    }
}
