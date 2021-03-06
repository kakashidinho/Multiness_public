package com.hqgame.networknes;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.util.Base64;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.toolbox.JsonObjectRequest;
import com.android.volley.toolbox.Volley;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;

// even though we inherit from BaseInvitationsPage, doesn't mean this page is about invitations
// It is actually handling rooms list on lobby
public class LobbyPage extends BaseInvitationsPage<LobbyPage.Room> implements View.OnClickListener {
    private static final int EXPECTED_MAX_INVITATIONS_TO_FETCH = 25;

    private static String sCountryCode = null;

    private ArrayList<Room> mRooms = new ArrayList<>();
    private RequestQueue mQueue;
    private int mCurrentPage = 0;

    public static String getCountryCode() {
        return sCountryCode;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mQueue = Volley.newRequestQueue(getContext());
    }

    @Override
    protected View createView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.lobby_page_title));

        View v = inflater.inflate(R.layout.page_lobby, container, false);

        View nextBtn = v.findViewById(R.id.btnNextRoomsPage);
        View prevBtn = v.findViewById(R.id.btnPrevRoomsPage);
        View createPubRoom = v.findViewById(R.id.btnCreatePublicRoom);
        View createPrivRoom = v.findViewById(R.id.btnPrivateRooms);

        nextBtn.setOnClickListener(this);
        prevBtn.setOnClickListener(this);
        createPubRoom.setOnClickListener(this);
        createPrivRoom.setOnClickListener(this);

        return v;
    }

    @Override
    public void onResume() {
        super.onResume();

        // detect current country by using 3rd party website
        if (sCountryCode == null) {
            JsonObjectRequest request = new JsonObjectRequest(Request.Method.GET,
                    "http://125.212.218.120:8090/iplocation",
                    null,
                    new Response.Listener<JSONObject>() {
                        @Override
                        public void onResponse(JSONObject response) {
                            try {
                                if (response.has("countryCode")) {
                                    sCountryCode = response.getString("countryCode");

                                    System.out.println("LobbyPage: detected country = " + sCountryCode);
                                }
                            } catch (Exception e) {
                                e.printStackTrace();
                            }
                        }
                    },
                    new Response.ErrorListener() {
                        @Override
                        public void onErrorResponse(VolleyError error) {
                            // ignore
                        }
                    });

            mQueue.add(request);
        }
    }

    @Override
    protected void deleteInvitation(@NonNull Room room, final Runnable finishCallback) {
        throw new UnsupportedOperationException("");
    }

    @Override
    protected void acceptedInvitation(@NonNull Room info) {
        if (info.inviteData == null || info.inviteData.length() <= 0)
            return;

        Bundle existSettings = getExtras();
        final Bundle settings = existSettings != null ? existSettings : new Bundle();

        try {
            // indicate we are joining the host on lobby instead of from FB or Google invitation
            settings.putSerializable(Settings.GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY, Settings.RemoteControl.JOIN_INTERNET_REMOTE_CONTROL_PUBLIC);
            settings.putString(Settings.GAME_ACTIVITY_REMOTE_INVITATION_DATA_KEY, info.inviteData);
            if (sCountryCode != null)
                settings.putString(Settings.GAME_ACTIVITY_COUNTRY_CODE_KEY, sCountryCode);

            BasePage intent = BasePage.create(GamePage.class);
            intent.setExtras(settings);

            goToPage(intent);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    protected AsyncOperation doFetchInvitations(final AsyncQuery<Boolean> finishCallback) {
        //invalidate current invitations
        mRooms.clear();

        RequestListener<JSONObject> listener = new RequestListener<JSONObject>() {
            @Override
            protected void onResponseImpl(JSONObject response) {
                try {
                    JSONArray roomsListJson = response.getJSONArray("rooms");
                    mCurrentPage = response.getInt("page");

                    for (int r = 0; r < roomsListJson.length(); ++r) {
                        Room room = new Room();

                        try {
                            JSONObject roomJson = roomsListJson.getJSONObject(r);
                            String metaBase64 = roomJson.getString("meta_data");
                            String metaString = new String(Base64.decode(metaBase64, Base64.URL_SAFE), "UTF-8");

                            JSONObject metaJson = new JSONObject(metaString);
                            room.name = metaJson.getString(Settings.PUBLIC_SERVER_NAME_KEY);
                            room.gameName = metaJson.getString(Settings.PUBLIC_SERVER_ROM_NAME_KEY);
                            room.inviteData = metaJson.getString(Settings.PUBLIC_SERVER_INVITE_DATA_KEY);

                            mRooms.add(room);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                    }
                } catch (Exception e) {
                    displayErrorDialog(e.getLocalizedMessage(), new java.lang.Runnable() {
                        @Override
                        public void run() {

                        }
                    });
                }

                if (finishCallback != null)
                    finishCallback.run(true);
            }
        };

        // fetch rooms list on lobby
        String url = "http://" + GameSurfaceView.getLobbyServerAddress()
                + ":8080/rooms/"
                + GameSurfaceView.getLobbyAppId()
                + "?page=" + mCurrentPage
                + "&desc=1";
        JsonObjectRequest request = new JsonObjectRequest(Request.Method.GET, url, null,
                listener,
                listener);

        mQueue.add(request);

        return listener;
    }

    @Override
    protected Room getInvitation(int index) {
        return mRooms != null ? mRooms.get(index) : null;
    }

    @Override
    protected int getNumInvitations() {
        return mRooms != null ? mRooms.size() : 0;
    }

    @Override
    protected View createInvitationItemView(
            @NonNull Room info,
            LayoutInflater inflater,
            View convertView,
            ViewGroup parent,
            QueryCallback<Boolean> selectedItemChangedCallback) {
        View vi = convertView;
        if (vi == null)
            vi = inflater.inflate(R.layout.lobby_room_item_layout, parent, false);

        TextView nameView = (TextView) vi.findViewById(R.id.lobby_room_desc_txt_view);
        TextView descView = (TextView) vi.findViewById(R.id.lobby_room_desc2_txt_view);

        try {
            nameView.setText(info.name);
            descView.setText(info.gameName);

        } catch (Exception e) {
            e.printStackTrace();
        }
        return vi;
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btnNextRoomsPage: {
                mCurrentPage++;
                refresh();
            }
            break;
            case R.id.btnPrevRoomsPage: {
                if (mCurrentPage > 0) {
                    mCurrentPage--;
                    refresh();
                }
            }
            break;
            case R.id.btnCreatePublicRoom: {
                Bundle existSettings = getExtras();
                final Bundle settings = existSettings != null ? existSettings : new Bundle();

                try {
                    // indicate we are hosting on lobby instead of using FB or Google invitation
                    settings.putSerializable(Settings.GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY, Settings.RemoteControl.ENABLE_INTERNET_REMOTE_CONTROL_PUBLIC);
                    if (sCountryCode != null)
                        settings.putString(Settings.GAME_ACTIVITY_COUNTRY_CODE_KEY, sCountryCode);

                    BasePage intent = BasePage.create(GameChooserPage.class);
                    intent.setExtras(settings);

                    goToPage(intent);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
            break;
            case R.id.btnPrivateRooms: {
                Bundle existSettings = getExtras();
                final Bundle settings = existSettings != null ? existSettings : new Bundle();

                try {
                    BasePage intent = BasePage.create(InternetMultiplayerPage.class);
                    intent.setExtras(settings);

                    goToPage(intent);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
            break;
        }
    }

    private abstract class RequestListener<T> implements Response.Listener<T>, Response.ErrorListener, AsyncOperation {
        @Override
        final public synchronized void onErrorResponse(VolleyError error) {
            if (mIsCanceled)
                return;

            if (false) // skip error dialog for now
                displayErrorDialog(error.getLocalizedMessage(), new java.lang.Runnable() {
                    @Override
                    public void run() {

                    }
                });
            else {
                cancelInvitationsFetching();
            }
        }

        @Override
        final public void onResponse(T response) {
            if (mIsCanceled)
                return;

            onResponseImpl(response);
        }

        @Override
        final public synchronized void cancel() {
            mIsCanceled = true;
        }

        protected abstract void onResponseImpl(T response);

        private volatile boolean mIsCanceled = false;
    }

    public static class Room {
        String name;
        String gameName;
        String inviteData;
    }
}
