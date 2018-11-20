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
import android.content.SharedPreferences;
import android.graphics.Rect;
import android.support.annotation.NonNull;
import android.view.KeyEvent;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/**
 * Created by le on 3/5/2016.
 */
class Settings {
    public static final boolean DEBUG = false;

    private static final String SETTINGS_FILE = "settings";
    private static final String SETTINGS_KEY_PREFIX = "com.hqgame.networknes.SETTINGS_";
    private static final String SETTINGS_VOLUME_KEY = SETTINGS_KEY_PREFIX + "VOL";
    private static final String SETTINGS_VOICE_ENABLE_KEY = SETTINGS_KEY_PREFIX + "VOICE_ENABLE";
    private static final String SETTINGS_UI_CONTROLS_ENABLE_KEY = SETTINGS_KEY_PREFIX + "UI_CONTROLS_ENABLE";
    private static final String SETTINGS_ORIENTATION_KEY = SETTINGS_KEY_PREFIX + "ORIENTATION";
    private static final String SETTINGS_VIBRATION_KEY = SETTINGS_KEY_PREFIX + "VIBRATION_KEY";
    private static final String SETTINGS_FULLSCREEN_KEY = SETTINGS_KEY_PREFIX + "FULLSCREEN";
    private static final String SETTINGS_A_IS_TURBO_KEY = SETTINGS_KEY_PREFIX + "A_IS_TURBO";
    private static final String SETTINGS_B_IS_TURBO_KEY = SETTINGS_KEY_PREFIX + "B_IS_TURBO";
    private static final String SETTINGS_UI_CONTROLS_RECTS_LANDSCAPE_KEY = SETTINGS_KEY_PREFIX + "UI_BTN_LANDSCAPE_RECTS";
    private static final String SETTINGS_UI_CONTROLS_RECTS_PORT_KEY = SETTINGS_KEY_PREFIX + "UI_BTN_PORT_RECTS";

    public static final String IMMEDIATE_PREF_FILE = "last_session";
    public static final String LAST_PLAYED_GAME_KEY = "com.hqgame.networknes.LAST_GAME";
    public static final String LEGACY_CACHED_GAME_PATHS_KEY = "com.hqgame.networknes.CACHED_GAMES";
    public static final String ORDERED_CACHED_GAME_PATHS_KEY = "com.hqgame.networknes.CACHED_GAMES_ORDERED";
    public static final String LAST_TYPED_HOST_KEY = "com.hqgame.networknes.LAST_HOST";
    public static final String LAST_TYPED_PORT_KEY = "com.hqgame.networknes.LAST_PORT";
    public static final String LAST_TYPED_ROOM_NAME_KEY = "com.hqgame.networknes.LAST_ROOM_NAME";
    public static final String LAST_SAVE_DIR_KEY = "com.hqgame.networknes.LAST_SAVE_DIR";
    public static final String LAST_MANUAL_DIR_KEY = "com.hqgame.networknes.LAST_MANUAL_DIR";
    public static final String DISABLE_NOGAME_DIALOG_KEY = "com.hqgame.networknes.DISABLE_NOGAME_DIALOG";
    public static final String SKIP_USER_RATING_KEY = "com.hqgame.networknes.SKIP_USER_RATING";
    public static final String USER_RATING_PROMPT_DATE_KEY = "com.hqgame.networknes.USER_RATING_DATE";
    public static final String USER_TOTAL_PLAY_TIME_KEY = "com.hqgame.networknes.USER_TOTAL_PLAYTIME";
    public static final String USER_TOTAL_MULTIPLAYER_TIME_KEY = "com.hqgame.networknes.USER_TOTAL_MULTIPLAYER_TIME";
    public static final String SKIP_LOBBY_FEATURE_DIALOG_KEY = "com.hqgame.networknes.SKIP_LOBBY_FEATURE_DIALOG";

    //the name of the game path in GameActivity's game settings bundle
    public static final String GAME_ACTIVITY_PATH_KEY = "com.hqgame.networknes.GAME";
    public static final String GAME_ACTIVITY_REMOTE_CTL_TYPE_KEY = "com.hqgame.networknes.REMOTE_TYPE";
    public static final String GAME_ACTIVITY_REMOTE_HOST_KEY = "com.hqgame.networknes.REMOTE_HOST";//this is host's address in LAN mode or name of host in Wifi Direct mode
    public static final String GAME_ACTIVITY_REMOTE_PORT_KEY = "com.hqgame.networknes.REMOTE_PORT";
    public static final String GAME_ACTIVITY_REMOTE_FB_INVITATION_ID_KEY = "com.hqgame.networknes.REMOTE_FB_INVITATION_ID";
    public static final String GAME_ACTIVITY_REMOTE_INVITATION_DATA_KEY = "com.hqgame.networknes.REMOTE_INVITATION_DATA";
    public static final String GAME_ACTIVITY_REMOTE_GOOGLE_MATCH_ID = "com.hqgame.networknes.REMOTE_GOOGLE_MATCH_ID";
    public static final String GAME_ACTIVITY_REMOTE_APPWARP_MATCH_ID = "com.hqgame.networknes.REMOTE_APPWARP_MATCH_ID";

    public static final String APPWARP_ROOM_OWNER_GOOGLE_ID_KEY = "com.hqgame.networknes.APPWARP_ROOM_OWNER_GOOGLE_ID_KEY";

    public static final String SAVE_PATH_KEY = "com.hqgame.networknes.SAVE_PATH";

    public static final int GOOGLE_MATCH_P1_MASK = 0x1;
    public static final int GOOGLE_MATCH_P2_MASK = 0x2;

    public static final byte[] GOOGLE_MATCH_DATA_MAGIC = new byte[]{'H', 'Q', 'N', 'E', 'T'};
    public static final String GOOGLE_MATCH_INVITE_DATA_MAGIC_STR = "hqinvitedata:";

    public static final int GOOGLE_MATCH_STATE_INVALID = -1;
    public static final int GOOGLE_MATCH_STATE_UNINITIALIZED = 1;
    public static final int GOOGLE_MATCH_STATE_HAS_INVITE_DATA = 2;

    public static final int GOOGLE_AUTO_MATCH_WAIT_FOR_TURN_TIMEOUT_MS = 13000;
    public static final int GOOGLE_AUTO_MATCH_WAIT_FOR_TURN_TIMEOUT_RAND_MS = 3000;
    public static final int GOOGLE_AUTO_MATCH_WAIT_TO_BE_CONNECTED_TIMEOUT_MS = 30000;
    public static final int GOOGLE_MATCH_WAIT_FOR_TURN_TIMEOUT_MS = 60000;

    public static final int MAX_QUICK_SAVES_PER_GAME = 5;
    public static final String QUICK_SAVE_PREFIX = "quicksave";

    public static final int DEFAULT_NETWORK_PORT = 23458;

    public static final List<String> FACEBOOK_PERMISSIONS = Arrays.asList("user_friends", "public_profile");

    public static final String PUBLIC_SERVER_ROM_NAME_KEY = "PUBLIC_SERVER_ROM_NAME_KEY";
    public static final String PUBLIC_SERVER_INVITE_DATA_KEY = "PUBLIC_SERVER_INVITE_DATA_KEY";
    public static final String PUBLIC_SERVER_NAME_KEY = "PUBLIC_SERVER_NAME_KEY";

    public static enum RemoteControl {
        NO_REMOTE_CONTROL,
        ENABLE_LAN_REMOTE_CONTROL,
        ENABLE_WIFI_DIRECT_REMOTE_CONTROL,
        CONNECT_LAN_REMOTE_CONTROL,
        ENABLE_INTERNET_REMOTE_CONTROL_FB,
        ENABLE_INTERNET_REMOTE_CONTROL_GOOGLE,
        ENABLE_INTERNET_REMOTE_CONTROL_PUBLIC,
        JOIN_INTERNET_REMOTE_CONTROL_FB,
        JOIN_INTERNET_REMOTE_CONTROL_GOOGLE,
        JOIN_INTERNET_REMOTE_CONTROL_PUBLIC,
        QUICKJOIN_INTERNET_REMOTE_CONTROL_GOOGLE,
    }

    public static enum Orientation {
        AUTO,
        LANDSCAPE,
        PORTRAIT;

        @Override
        public String toString() {
            return name().toLowerCase();
        }
    }

    /*
    public static enum OldButton {
        LEFT, RIGHT, UP, DOWN,
        A,
        B,
        AUTO_A,
        AUTO_B,
        START,
        SELECT,
        AB,
        QUICK_SAVE,
        QUICK_LOAD;

        @Override
        public String toString() {
            switch (this) {
                case AUTO_A:
                    return "Auto A";
                case AUTO_B:
                    return "Auto B";
                case AB:
                    return "A + B";
                case QUICK_SAVE:
                    return "Quick Save";
                case QUICK_LOAD:
                    return "Quick Load";
                default:
                    return name();
            }
        }
    }
    */

    public static enum Button {
        /*WARNING: this must reflect the order defined in source/mobile/Input.hpp*/
        A,
        B,
        SELECT,
        START,
        AB,

        LEFT,
        RIGHT, UP, DOWN,

        AUTO_A,
        AUTO_B,

        QUICK_SAVE,
        QUICK_LOAD;

        public static final int NORMAL_BUTTONS = 5;

        @Override
        public String toString() {
            switch (this) {
                case AUTO_A:
                    return "Auto A";
                case AUTO_B:
                    return "Auto B";
                case AB:
                    return "A + B";
                case QUICK_SAVE:
                    return "Quick Save";
                case QUICK_LOAD:
                    return "Quick Load";
                default:
                    return name();
            }
        }
    }

    private static float audioVolume = 1.f;
    private static boolean buttonsVibration = true;
    private static boolean voiceChatEnabled = true;
    private static boolean uiButtonsEnabled = true;
    private static boolean fullscreenEnabled = false;
    private static Orientation orientation = Orientation.LANDSCAPE;
    private static boolean btnATurbo = false;
    private static boolean btnBTurbo = false;
    private static boolean disableAds = false; // this setting is not persistent

    private static TreeMap<Integer, HashSet<Button>> key2BtnMap = new TreeMap<>();
    private static TreeMap<Button, Integer> btn2KeyMap = new TreeMap<>();

    private static TreeMap<Button, Integer> btn2KeyDefaultMap = new TreeMap<>();

    private static TreeMap<Button, Rect> uiButtonsRectsPortrait = new TreeMap<>();
    private static TreeMap<Button, Rect> uiButtonsRectsLandscape = new TreeMap<>();

    public static float getAudioVolume() { return audioVolume; }
    public static boolean isButtonsVibrationEnabled() { return buttonsVibration; }
    public static boolean isVoiceChatEnabled() { return voiceChatEnabled; }
    public static boolean isUIButtonsEnbled() { return uiButtonsEnabled;}
    public static Orientation getPreferedOrientation() { return orientation; }
    public static boolean isFullscreenEnabled() { return fullscreenEnabled; }
    public static boolean isBtnATurbo() { return btnATurbo; }
    public static boolean isBtnBTurbo() { return btnBTurbo; }
    public static boolean isAdsDisabled() { return disableAds; }

    //Note: one key can be mapped to multiple buttons
    public static Iterable<Button> getMappedButton(int keycode) {
        return key2BtnMap.get(keycode);
    }

    public static Iterable<Map.Entry<Button, Integer>> getMappedButtons() {
        return btn2KeyMap.entrySet();
    }

    public static Iterable<Map.Entry<Button, Rect>> getUIButtonsRects(boolean portrait) {
        if (portrait)
            return uiButtonsRectsPortrait.entrySet();
        return uiButtonsRectsLandscape.entrySet();
    }

    public static int getNumAssignedUIButtonsRects(boolean portrait) {
        if (portrait)
            return uiButtonsRectsPortrait.size();
        return uiButtonsRectsLandscape.size();
    }

    public static Rect getUIButtonRect(@NonNull Button button, boolean portrait) {
        if (portrait)
            return uiButtonsRectsPortrait.get(button);
        return uiButtonsRectsLandscape.get(button);
    }

    public static void setAudioVolume(float f) { audioVolume = f; }
    public static void enableButtonsVibration(boolean e) { buttonsVibration = e; }
    public static void enableUIButtons(boolean e) { uiButtonsEnabled = e; }
    public static void enableVoiceChat(boolean e) { voiceChatEnabled = e; }
    public static void setPreferedOrientation(Orientation o) { orientation = o; }
    public static void enableFullscreen(boolean e) { fullscreenEnabled = e; }
    public static void enableBtnATurbo(boolean e) { btnATurbo = e; }
    public static void enableBtnBTurbo(boolean e) { btnBTurbo = e; }
    public static void enableAds(boolean e) { disableAds = !e; }

    public static void setUIButtonRect(@NonNull Button button, @NonNull Rect rect, boolean portrait) {
        if (portrait)
            uiButtonsRectsPortrait.put(button, rect);
        else
            uiButtonsRectsLandscape.put(button, rect);
    }

    public static void resetUIButtonsRects(boolean portrait) {
        if (portrait)
            uiButtonsRectsPortrait.clear();
        else
            uiButtonsRectsLandscape.clear();
    }

    public static void mapKeyToButton(int keycode, Button button) {
        //old mapped key
        Integer oldKey = btn2KeyMap.get(button);

        //remove button from old key's mapped buttons list
        if (oldKey != null) {
            if (oldKey.equals(keycode))//no change
                return;

            HashSet<Button> oldMappedButtons = key2BtnMap.get(oldKey);

            if (oldMappedButtons != null) {
                oldMappedButtons.remove(button);
                if (oldMappedButtons.size() == 0)
                    key2BtnMap.remove(oldKey);
            }
        }//if (oldKey != null)

        //update button 2 key map
        btn2KeyMap.put(button, keycode);

        //update key 2 buttons map
        HashSet<Button> mappedButtons = key2BtnMap.get(keycode);
        if (mappedButtons == null) {
            mappedButtons = new HashSet<>();
            key2BtnMap.put(keycode, mappedButtons);
        }

        mappedButtons.add(button);
    }

    public static void resetMappedButtonSetting() {
        for (Map.Entry<Button, Integer> entry : btn2KeyDefaultMap.entrySet()) {
            mapKeyToButton(entry.getValue(), entry.getKey());
        }
    }

    private static String buttonSettingKey(Button button) {
        return SETTINGS_KEY_PREFIX + "BUTTON_" + button.name();
    }

    private static void loadMappedButtonSetting(SharedPreferences pref, Button button, int defaultKey) {
        mapKeyToButton(pref.getInt(buttonSettingKey(button), defaultKey), button);
    }

    private static void loadUIButtonsLayout(SharedPreferences pref, String prefKey, TreeMap<Button, Rect> layoutPerOrientation) {
        // ui buttons' layout, load from json string
        String uiBtnRectsJsonAsString = pref.getString(prefKey, "{}");
        try {
            JSONObject uiBtnRectsJson = new JSONObject(uiBtnRectsJsonAsString);
            Iterator<String> ite = uiBtnRectsJson.keys();
            while (ite != null && ite.hasNext()) {
                String btnName = ite.next();
                Button btn = Button.valueOf(btnName);

                JSONArray rectAsJsonArray = uiBtnRectsJson.getJSONArray(btnName);
                Rect rect = new Rect();
                rect.left = rectAsJsonArray.getInt(0);
                rect.right = rectAsJsonArray.getInt(1);
                rect.top = rectAsJsonArray.getInt(2);
                rect.bottom = rectAsJsonArray.getInt(3);

                layoutPerOrientation.put(btn, rect);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void saveUIButtonsLayout(SharedPreferences.Editor editor, String prefKey, TreeMap<Button, Rect> layoutPerOrientation) {
        // ui buttons layout, save as json string
        JSONObject uiBtnRectsJson = new JSONObject();
        Iterable<Map.Entry<Button, Rect>> uiBtnRectsEntries = layoutPerOrientation.entrySet();
        if (uiBtnRectsEntries != null) {
            for (Map.Entry<Button, Rect> entry: uiBtnRectsEntries) {
                JSONArray rectAsArray = new JSONArray();

                Button button = entry.getKey();
                Rect rect = entry.getValue();

                rectAsArray.put(rect.left).put(rect.right).put(rect.top).put(rect.bottom);

                try {
                    uiBtnRectsJson.put(button.name(), rectAsArray);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }

        editor.putString(prefKey, uiBtnRectsJson.toString());
    }

    public static void loadGlobalSettings(Context context) {
        SharedPreferences pref = context.getSharedPreferences(SETTINGS_FILE, Context.MODE_PRIVATE);

        audioVolume = pref.getFloat(SETTINGS_VOLUME_KEY, 1.f);
        buttonsVibration = pref.getBoolean(SETTINGS_VIBRATION_KEY, true);
        voiceChatEnabled = pref.getBoolean(SETTINGS_VOICE_ENABLE_KEY, true);
        uiButtonsEnabled = pref.getBoolean(SETTINGS_UI_CONTROLS_ENABLE_KEY, true);
        fullscreenEnabled = pref.getBoolean(SETTINGS_FULLSCREEN_KEY, false);
        btnATurbo = pref.getBoolean(SETTINGS_A_IS_TURBO_KEY, false);
        btnBTurbo = pref.getBoolean(SETTINGS_B_IS_TURBO_KEY, false);

        try {
            String orientationName = pref.getString(SETTINGS_ORIENTATION_KEY, Orientation.LANDSCAPE.name());
            orientation = Orientation.valueOf(Orientation.class, orientationName);
        } catch (Exception e) {
            e.printStackTrace();
            orientation = Orientation.AUTO;
        }

        /*
        buttons mapping
         */
        for (Map.Entry<Button, Integer> entry : btn2KeyDefaultMap.entrySet()) {
            loadMappedButtonSetting(pref, entry.getKey(), entry.getValue());
        }

        // ui buttons layout
        loadUIButtonsLayout(pref, SETTINGS_UI_CONTROLS_RECTS_PORT_KEY, uiButtonsRectsPortrait);
        loadUIButtonsLayout(pref, SETTINGS_UI_CONTROLS_RECTS_LANDSCAPE_KEY, uiButtonsRectsLandscape);
    }

    public static void saveGlobalSettings(Context context) {
        SharedPreferences pref = context.getSharedPreferences(SETTINGS_FILE, Context.MODE_PRIVATE);

        SharedPreferences.Editor editor = pref.edit();
        editor.putFloat(SETTINGS_VOLUME_KEY, audioVolume);
        editor.putBoolean(SETTINGS_VIBRATION_KEY, buttonsVibration);
        editor.putBoolean(SETTINGS_VOICE_ENABLE_KEY, voiceChatEnabled);
        editor.putBoolean(SETTINGS_UI_CONTROLS_ENABLE_KEY, uiButtonsEnabled);
        editor.putString(SETTINGS_ORIENTATION_KEY, orientation.name());
        editor.putBoolean(SETTINGS_FULLSCREEN_KEY, fullscreenEnabled);
        editor.putBoolean(SETTINGS_A_IS_TURBO_KEY, btnATurbo);
        editor.putBoolean(SETTINGS_B_IS_TURBO_KEY, btnBTurbo);

        /*
        buttons mapping
         */
        Iterable<Map.Entry<Button, Integer>> mappedEntries = getMappedButtons();
        if (mappedEntries != null) {
            for (Map.Entry<Button, Integer> entry : mappedEntries) {
                editor.putInt(buttonSettingKey(entry.getKey()), entry.getValue());
            }
        }//if (mappedEntries != null)

        // ui buttons layout
        saveUIButtonsLayout(editor, SETTINGS_UI_CONTROLS_RECTS_PORT_KEY, uiButtonsRectsPortrait);
        saveUIButtonsLayout(editor, SETTINGS_UI_CONTROLS_RECTS_LANDSCAPE_KEY, uiButtonsRectsLandscape);

        editor.apply();
    }

    static {
        //initialize default button mapping table
        btn2KeyDefaultMap.put(Button.LEFT, KeyEvent.KEYCODE_DPAD_LEFT);
        btn2KeyDefaultMap.put(Button.RIGHT, KeyEvent.KEYCODE_DPAD_RIGHT);
        btn2KeyDefaultMap.put(Button.UP, KeyEvent.KEYCODE_DPAD_UP);
        btn2KeyDefaultMap.put(Button.DOWN, KeyEvent.KEYCODE_DPAD_DOWN);
        btn2KeyDefaultMap.put(Button.A, KeyEvent.KEYCODE_BUTTON_A);
        btn2KeyDefaultMap.put(Button.B, KeyEvent.KEYCODE_BUTTON_X);
        btn2KeyDefaultMap.put(Button.SELECT, KeyEvent.KEYCODE_BUTTON_SELECT);
        btn2KeyDefaultMap.put(Button.START, KeyEvent.KEYCODE_BUTTON_START);
        btn2KeyDefaultMap.put(Button.AUTO_A, KeyEvent.KEYCODE_UNKNOWN);
        btn2KeyDefaultMap.put(Button.AUTO_B, KeyEvent.KEYCODE_UNKNOWN);
        btn2KeyDefaultMap.put(Button.AB, KeyEvent.KEYCODE_UNKNOWN);
        btn2KeyDefaultMap.put(Button.QUICK_SAVE, KeyEvent.KEYCODE_UNKNOWN);
        btn2KeyDefaultMap.put(Button.QUICK_LOAD, KeyEvent.KEYCODE_UNKNOWN);
    }
}
