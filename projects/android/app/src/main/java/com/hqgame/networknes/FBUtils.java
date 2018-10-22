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

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.AsyncTask;
import android.os.Bundle;
import android.widget.ImageView;

import com.facebook.AccessToken;
import com.facebook.GraphRequest;
import com.facebook.GraphRequestBatch;
import com.facebook.GraphResponse;
import com.facebook.HttpMethod;

import org.json.JSONArray;
import org.json.JSONObject;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Locale;
import java.util.TreeSet;

/**
 * Created by le on 6/5/2016.
 */
public class FBUtils {
    private final static int WAIT_TIMEOUT_MS = 60000;//1 min

    private static volatile InvitationsFetchTask sInvitationsCleanupTask = null;

    public static synchronized void resumePendingInvitationsCleanupTask() {
        pausePendingInvitationsCleanupTask();

        sInvitationsCleanupTask = new InvitationsFetchTask();
        sInvitationsCleanupTask.execute();
    }

    public static synchronized void pausePendingInvitationsCleanupTask() {
        if (sInvitationsCleanupTask != null)
            sInvitationsCleanupTask.cancel(false);

        sInvitationsCleanupTask = null;
    }

    public static void deleteGraphObj(JSONObject graphObj, Runnable callback) {
        try {
            deleteGraphObjRequest(graphObj, callback).executeAsync();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void deleteGraphObj(String graphObj_id) {
        deleteGraphObjRequest(graphObj_id).executeAsync();
    }

    //Note: the result passed to resultCallback may have more than <max_invitations> items
    public static synchronized AsyncOperation fetchInvitations(int expected_max_invitations, final AsyncQuery<TreeSet<InvitationInfo> > resultCallback) {
        pausePendingInvitationsCleanupTask();

        TreeSet<InvitationInfo> re = new TreeSet<>();

        InvitationsFetchTask task = new InvitationsFetchTask(expected_max_invitations, resultCallback);
        task.execute(re);

        return task;
    }

    public static AsyncTask downloadProfilePic(Resources res, String profile_id, ImageView view) {
        ProfilePictureDownloaderTask task;
        Utils.downloadBitmapAsync(res, task = new ProfilePictureDownloaderTask(profile_id, view), view);

        return task;
    }

    private static GraphRequest fetchInvitationsRequest() {
        return fetchInvitationsRequest(null, null);
    }

    private static GraphRequest fetchInvitationsRequest(GraphRequest.Callback callback) {
        return fetchInvitationsRequest(null, callback);
    }

    private static GraphRequest fetchInvitationsRequest(Bundle params) {
        return fetchInvitationsRequest(params, null);
    }

    private static GraphRequest fetchInvitationsRequest(Bundle params, GraphRequest.Callback callback) {
        if (params == null)
            params = new Bundle();

        params.putString("fields", "id,name,data,paging,from,message,created_time");

        return new GraphRequest(
                AccessToken.getCurrentAccessToken(),
                "/me/apprequests",
                params,
                HttpMethod.GET,
                callback
        );
    }

    private static GraphRequest deleteGraphObjRequest(JSONObject graphObj) {
        String id = null;

        try {
            id = graphObj.getString("id");
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }

        return deleteGraphObjRequest(id);
    }

    private static GraphRequest deleteGraphObjRequest(JSONObject graphObj, final Runnable callback) {
        String id = null;

        try {
            id = graphObj.getString("id");
        } catch (Exception e) {
            e.printStackTrace();
            if (callback != null)
                callback.run();
            return null;
        }

        return deleteGraphObjRequest(id, new GraphRequest.Callback() {
            @Override
            public void onCompleted(GraphResponse response) {
                if (callback != null)
                    callback.run();
            }
        });
    }

    private static GraphRequest deleteGraphObjRequest(String id) {
        return new GraphRequest(
                AccessToken.getCurrentAccessToken(),
                "/" + id,
                null,
                HttpMethod.DELETE
        );
    }

    private static GraphRequest deleteGraphObjRequest(String id, GraphRequest.Callback callback) {
        return new GraphRequest(
                AccessToken.getCurrentAccessToken(),
                "/" + id,
                null,
                HttpMethod.DELETE,
                callback
        );
    }

    static interface OutOfDateInvitationHandler {
        void handleOutOfDateInvitation(JSONObject invitationObject);
    }

    private static TreeSet<InvitationInfo> parseInvitationsResponse(GraphResponse response, OutOfDateInvitationHandler invalidHandler) {
        TreeSet<InvitationInfo> re = new TreeSet<>();

        parseInvitationsResponse(re, response, invalidHandler);

        return re;
    }

    private static void parseInvitationsResponse(TreeSet<InvitationInfo> resultContainer, GraphResponse response, OutOfDateInvitationHandler invalidHandler) {
        if (response == null)
            return;

        if ((response.getError() != null))
        {
            System.out.println("parseInvitationsResponse: error from response '" + response.getError().getErrorMessage() + "'");
            return;
        }

        if (resultContainer == null)
            resultContainer = new TreeSet<>();//temporary container

        try {
            JSONArray invitations = response.getJSONObject().getJSONArray("data");

            for (int i = 0; i < invitations.length(); ++i) {
                JSONObject invitation = invitations.getJSONObject(i);

                InvitationInfo wrapper = new InvitationInfo(invitation);

                resultContainer.add(wrapper);
            }

            //remove out-of-date invitations from the same person or more than 2 days old
            Date currentTime = new Date();
            HashSet<String> invitationOwners = new HashSet<>();
            Iterator<InvitationInfo> ite = resultContainer.iterator();

            while (ite.hasNext()) {
                InvitationInfo wrapper = ite.next();

                long diffToPresentMs = currentTime.getTime() - wrapper.creationTime.getTime();
                if (diffToPresentMs > 1000 * 60 * 60 * 48)
                {
                    //more than two days old, discard
                    if (invalidHandler != null)
                        invalidHandler.handleOutOfDateInvitation(wrapper.graphObject);

                    ite.remove();
                }

                JSONObject fromInfo = wrapper.graphObject.getJSONObject("from");
                String fromId = fromInfo.getString("id");

                if (!invitationOwners.contains(fromId))
                {
                    invitationOwners.add(fromId);
                }
                else {
                    //we already have a newer invitation from the same sender, so discard this one
                    if (invalidHandler != null)
                        invalidHandler.handleOutOfDateInvitation(wrapper.graphObject);

                    ite.remove();
                }
            }//while (ite.hasNext())
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static TreeSet<InvitationInfo> parseInvitationsResponseAndCleanup(GraphResponse response, boolean asyncClean) {
        TreeSet<InvitationInfo> re = new TreeSet<>();

        parseInvitationsResponseAndCleanup(re, response, asyncClean);

        return re;
    }

    public static void parseInvitationsResponseAndCleanup(TreeSet<InvitationInfo> resultContainer, GraphResponse response, boolean asyncClean) {
        final GraphRequestBatch batchRequest = new GraphRequestBatch();

        parseInvitationsResponse(resultContainer, response, new OutOfDateInvitationHandler() {
            @Override
            public void handleOutOfDateInvitation(JSONObject invitationObject) {
                GraphRequest deleteRequest = deleteGraphObjRequest(invitationObject);
                if (deleteRequest != null)
                    batchRequest.add(deleteRequest);
            }
        });

        //perform the deletion
        if (!batchRequest.isEmpty()) {
            if (asyncClean)
                batchRequest.executeAsync();
            else {
                batchRequest.setTimeout(WAIT_TIMEOUT_MS);
                batchRequest.executeAndWait();
            }
        }
    }

    /*-------------- InvitationsFetchTask --------*/
    private static class InvitationsFetchTask extends AsyncTask<TreeSet<InvitationInfo>, Void, TreeSet<InvitationInfo>> implements AsyncOperation{
        private final AsyncQuery<TreeSet<InvitationInfo> > resultReadyCallback;
        private final int expectedMaxInvitationsToFetch;

        public InvitationsFetchTask() {
            this(0, null);
        }

        public InvitationsFetchTask(AsyncQuery<TreeSet<InvitationInfo> > callback) {
            this(0, callback);
        }

        public InvitationsFetchTask(int expectedMaxInvitationsToFetch, AsyncQuery<TreeSet<InvitationInfo> > callback) {
            this.resultReadyCallback = callback;
            this.expectedMaxInvitationsToFetch = expectedMaxInvitationsToFetch;
        }

        @Override
        protected TreeSet<InvitationInfo> doInBackground(TreeSet<InvitationInfo>... params) {
            if (this.isCancelled())
                return null;

            TreeSet<InvitationInfo> result_container = params.length > 0 ? params[0] : null;

            Bundle request_params = new Bundle();

            //fetch all pages of pending requests
            GraphRequest request = fetchInvitationsRequest(request_params);
            GraphResponse response = request.executeAndWait();
            GraphResponse nextPageResponse = null;

            while (response != null && !this.isCancelled() &&
                    (this.expectedMaxInvitationsToFetch <= 0 || result_container == null || result_container.size() < this.expectedMaxInvitationsToFetch)) {
                //fetch next page. We have to do it first before processing the current page, since the paging info may be lost if we remove some graph objects from current page
                request = response.getRequestForPagedResults(GraphResponse.PagingDirection.NEXT);
                if (request != null) {
                    nextPageResponse = request.executeAndWait();
                }
                else
                    nextPageResponse = null;

                //parse current page
                parseInvitationsResponseAndCleanup(result_container, response, false);

                //proceed to process next page
                response = nextPageResponse;
            } //while (response != null && !this.isCancelled())

            return result_container;
        }

        @Override
        protected void onPostExecute (TreeSet<InvitationInfo> result) {
            if (this.resultReadyCallback != null)
                this.resultReadyCallback.run(result);
        }

        @Override
        public void cancel() {
            this.cancel(false);
        }
    }

    /*------  ProfilePictureDownloaderTask ---*/
    static class ProfilePictureDownloaderTask extends Utils.BitmapDownloaderTask {
        //"url" will be used to store profile id instead of bitmap's URL
        public ProfilePictureDownloaderTask(String profileId, ImageView view) {
            super(profileId, view);
        }

        @Override
        // Actual download method, run in the task thread
        protected Bitmap doInBackground(Void... params) {
            if (isCancelled())
                return null;

            try {
                //query profile picture url
                Bundle fb_params = new Bundle();
                fb_params.putBoolean("redirect", false);
                fb_params.putString("type", "square");

                GraphResponse response = new GraphRequest(
                        AccessToken.getCurrentAccessToken(),
                        "/" + getUrl() + "/picture",
                        fb_params,
                        HttpMethod.GET
                ).executeAndWait();

                if (response == null)
                    return null;
                if (response.getError() != null) {
                    System.out.println("FB picture querying error: " + response.getError().getErrorMessage());
                    return null;
                }

                //now download the picture from real URL
                String picture_url = response.getJSONObject().getJSONObject("data").getString("url");

                if (isCancelled())
                    return null;
                return Utils.downloadBitmapAndWait(picture_url);
            } catch (Exception e) {
                e.printStackTrace();

                return null;
            }
        }
    }

    /*------------- InvitationInfo --------*/
    public static class InvitationInfo implements Comparable {
        final JSONObject graphObject;
        final Date creationTime;

        public InvitationInfo(JSONObject graphObject) {
            this.graphObject = graphObject;

            //parse creation date
            SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssZ", Locale.ENGLISH);

            Date l_creationTime;
            try {
                l_creationTime = dateFormat.parse(this.graphObject.getString("created_time"));
            } catch (Exception e) {
                e.printStackTrace();
                l_creationTime = new Date();
            }

            this.creationTime = l_creationTime;
        }

        @Override
        public int compareTo(Object rhs) {
            if (!(rhs instanceof InvitationInfo))
                return 0;
            //later creation time should be listed first
            return ((InvitationInfo)rhs).creationTime.compareTo(creationTime);
        }
    }
}
