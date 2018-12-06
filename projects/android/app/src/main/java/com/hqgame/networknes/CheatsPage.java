package com.hqgame.networknes;

import android.content.Context;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.RadioButton;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

public class CheatsPage extends BasePage implements View.OnClickListener, AdapterView.OnItemClickListener {
    public static final String GAME_NAME_KEY = "GAME_NAME";
    public static final String CHEATS_LIST_KEY = "CHEATS_LIST_KEY";

    private static final String CHEAT_NAME_KEY = "CHEAT_NAME_KEY";
    private static final String CHEAT_CODE_KEY = "CHEAT_CODE_KEY";
    private static final String CHEAT_ENABLED_KEY = "CHEAT_ENABLED_KEY";
    private static final String CHEAT_TYPE_KEY = "CHEAT_TYPE_KEY";
    private static final int MAX_CHEATS = 20;

    private ArrayList<CheatEntry> cheats = new ArrayList<>();
    private LayoutInflater listItemInflater = null;

    private CheatsListViewAdapter cheatsListViewAdapter = new CheatsListViewAdapter();
    private WeakReference<View> selectedCheatsListItemViewRef = new WeakReference<View>(null);
    private CheatEntry selectedCheat = null;

    private Button editBtn = null;
    private Button deleteBtn = null;

    private String mGameName = null;

    @Override
    public View onCreateViewWithDefaultToolbar(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setLabel(getString(R.string.cheats));

        View v = inflater.inflate(R.layout.page_cheats, container, false);

        Bundle args = getExtras();
        if (args == null || !args.containsKey(GAME_NAME_KEY)) {
            throw new IllegalStateException("No game name provided");
        }

        mGameName = args.getString(GAME_NAME_KEY);

        //create inflater for list view's items
        listItemInflater = (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        //register adapter for cheats list view
        ListView cheatsListView = (ListView)v.findViewById(R.id.cheats_list_view);

        cheatsListView.setAdapter(cheatsListViewAdapter);
        cheatsListView.setOnItemClickListener(this);

        //
        Button newBtn = (Button)v.findViewById(R.id.btnNewCheat);
        editBtn = (Button)v.findViewById(R.id.btnEditCheat);
        deleteBtn = (Button)v.findViewById(R.id.btnDeleteCheat);

        newBtn.setOnClickListener(this);
        editBtn.setOnClickListener(this);
        deleteBtn.setOnClickListener(this);

        loadCheats();

        return v;
    }

    @Override
    public void finish() {

        saveCheats();

        if (getRequestCode() != null) {
            // forward enabled cheats list to waiting page
            Bundle data = new Bundle();
            ArrayList<Cheat> enabledCheats = getEnabledCheats(this.cheats);

            data.putParcelableArrayList(CHEATS_LIST_KEY, enabledCheats);

            setResult(RESULT_OK, data);
        }

        // finish
        super.finish();
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btnNewCheat:
                if (cheats.size() >= MAX_CHEATS)
                {
                    Utils.errorDialog(getContext(), getString(R.string.too_many_cheats_err));
                    break;
                }
                openCheatDialog();
                break;
            case R.id.btnEditCheat:
            {
                if (selectedCheat != null) {
                    openCheatEditDialog(selectedCheat, false);
                }
            }
                break;
            case R.id.btnDeleteCheat:
            {
                if (selectedCheat != null) {
                    this.cheats.remove(selectedCheat);
                    invalidateSelectedCheatsListItemView();
                    this.cheatsListViewAdapter.notifyDataSetChanged();
                }
            }
                break;
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        invalidateSelectedCheatsListItemView();

        //highlight the selected item
        view.setBackgroundResource(android.R.color.darker_gray);
        selectedCheatsListItemViewRef = new WeakReference<View>(view);

        selectedCheat = (CheatEntry)cheatsListViewAdapter.getItem(position);

        deleteBtn.setEnabled(true);
        editBtn.setEnabled(true);
    }

    private void openCheatDialog() {
        CheatEntry newEntry = new CheatEntry();
        openCheatEditDialog(newEntry, true);
    }

    private void openCheatEditDialog(final CheatEntry entry, final boolean addToList) {
        View view = LayoutInflater.from(getContext()).inflate(R.layout.cheat_edit_dialog_layout, null);

        final EditText nameView = view.findViewById(R.id.cheat_name_edit_view);
        final EditText codeView = view.findViewById(R.id.cheat_code_edit_view);
        final RadioButton ggButton = (RadioButton)view.findViewById(R.id.game_genie_checkbox);
        final RadioButton prButton = (RadioButton)view.findViewById(R.id.pro_rocky_checkbox);

        if (entry.getName() != null)
            nameView.setText(entry.getName());
        if (entry.getCode() != null)
            codeView.setText(entry.getCode());
        switch (entry.getType()) {
            case GAME_GENIE:
                ggButton.setChecked(true);
                break;
            case PRO_ACTION_ROCKY:
                prButton.setChecked(true);
                break;
        }

        Utils.alertDialog(getContext(), null, null, view,
                new Runnable() {
                    @Override
                    public void run() {
                        String code = codeView.getText().toString();
                        String name = nameView.getText().toString();
                        Cheat.Type type = Cheat.Type.GAME_GENIE;
                        if (ggButton.isChecked())
                            type = Cheat.Type.GAME_GENIE;
                        else if (prButton.isChecked())
                            type = Cheat.Type.PRO_ACTION_ROCKY;

                        // check cheat code
                        int minLength = 0;
                        switch (type) {
                            case GAME_GENIE:
                                minLength = 6;
                                break;
                            case PRO_ACTION_ROCKY:
                                minLength = 8;
                                break;
                        }

                        if (code.length() < minLength)
                        {
                            Utils.errorDialog(R.string.invalid_cheat_code_err);
                            return;
                        }

                        // modify cheat entry
                        entry.setCode(code);
                        if (name.length() > 0)
                            entry.setName(name);
                        else
                            entry.setName(code);
                        entry.setType(type);

                        if (addToList)
                            CheatsPage.this.cheats.add(entry);

                        CheatsPage.this.cheatsListViewAdapter.notifyDataSetChanged();
                    }
                },
                null
        );
    }

    public static ArrayList<Cheat> loadCheats(Context context, String gameName) {
        ArrayList<CheatEntry> cheatEntries = new ArrayList<>();
        loadCheats(context, gameName, cheatEntries);

        return getEnabledCheats(cheatEntries);
    }

    private static ArrayList<Cheat> getEnabledCheats(ArrayList<CheatEntry> cheatEntries) {
        if (cheatEntries == null)
            return null;

        ArrayList<Cheat> enabledCheats = new ArrayList<>();
        for (CheatEntry cheatEntry: cheatEntries) {
            if (cheatEntry.isEnabled())
                enabledCheats.add(cheatEntry);
        }

        return enabledCheats;
    }

    private static void loadCheats(Context context, String gameName, ArrayList<CheatEntry> cheatEntries) {
        cheatEntries.clear();
        try {
            File folder = Utils.getAppPrivateFolder(context, "cheats");
            File gameCheats = new File(folder, gameName + ".cheats");
            FileInputStream is = new FileInputStream(gameCheats);

            String cheatsContent = Utils.readText(is);

            if (cheatsContent != null) {
                JSONArray jsonArray = new JSONArray(cheatsContent);
                for (int i = 0; i < jsonArray.length(); ++i) {
                    try {
                        JSONObject object = jsonArray.getJSONObject(i);
                        CheatEntry entry = new CheatEntry();
                        entry.setCode(object.getString(CHEAT_CODE_KEY));
                        entry.setName(object.getString(CHEAT_NAME_KEY));
                        entry.setEnabled(object.getBoolean(CHEAT_ENABLED_KEY));
                        entry.setType(Cheat.Type.valueOf(object.getString(CHEAT_TYPE_KEY)));

                        cheatEntries.add(entry);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void loadCheats() {
        loadCheats(getContext(), mGameName, this.cheats);
    }

    private void saveCheats() {
        try {
            JSONArray jsonArray = new JSONArray();
            for (CheatEntry entry: this.cheats) {
                JSONObject object = new JSONObject();
                object.put(CHEAT_NAME_KEY, entry.getName());
                object.put(CHEAT_CODE_KEY, entry.getCode());
                object.put(CHEAT_ENABLED_KEY, entry.isEnabled());
                object.put(CHEAT_TYPE_KEY, entry.getType().name());

                jsonArray.put(object);
            }


            File folder = Utils.getAppPrivateFolder(getContext(), "cheats");
            File gameCheats = new File(folder, mGameName + ".cheats");
            FileOutputStream os = new FileOutputStream(gameCheats);
            BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(os));
            bw.write(jsonArray.toString());

            bw.close();
        } catch (Exception e) {

        }
    }

    private void invalidateSelectedCheatsListItemView() {
        View selectedCheatsListItemView = this.selectedCheatsListItemViewRef.get();
        if (selectedCheatsListItemView != null) {
            selectedCheatsListItemView.setBackgroundResource(android.R.color.transparent);
        }
        selectedCheatsListItemViewRef = new WeakReference<View>(null);

        deleteBtn.setEnabled(false);
        editBtn.setEnabled(false);
    }

    public static class Cheat implements Parcelable {
        public enum Type {
            GAME_GENIE,
            PRO_ACTION_ROCKY;
        }

        public Cheat(){
            code = null;
            type = Type.GAME_GENIE;
            name = null;
        }

        private String code;
        private Type type;
        private String name;

        public String getCode() {
            return code;
        }

        public void setCode(String code) {
            this.code = code;
        }

        public Type getType() {
            return type;
        }

        public void setType(Type type) {
            this.type = type;
        }

        public String getName() {
            return name;
        }

        public void setName(String name) {
            this.name = name;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeString(name);
            dest.writeString(code);
            dest.writeString(type.name());
        }

        public Cheat(Parcel in) {
            name = in.readString();
            code = in.readString();
            type = Type.valueOf(in.readString());
        }

        public static final Parcelable.Creator<Cheat> CREATOR
                = new Parcelable.Creator<Cheat>() {
            public Cheat createFromParcel(Parcel in) {
                return new Cheat(in);
            }

            public Cheat[] newArray(int size) {
                return new Cheat[size];
            }
        };
    }

    private static class CheatEntry extends Cheat{
        private boolean enabled = true;

        public boolean isEnabled() {
            return enabled;
        }

        public void setEnabled(boolean enabled) {
            this.enabled = enabled;
        }
    }

    private class CheatsListViewAdapter extends BaseAdapter {
        @Override
        public int getCount() {
            return cheats.size();
        }

        @Override
        public Object getItem(int position) {
            return cheats.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View vi = convertView;
            if (vi == null)
                vi = listItemInflater.inflate(R.layout.cheat_item_layout, parent, false);

            TextView nameView = (TextView) vi.findViewById(R.id.cheat_name_txt_view);
            TextView codeView = (TextView) vi.findViewById(R.id.cheat_code_txt_view);
            final CheckBox checkBox = (CheckBox) vi.findViewById(R.id.cheat_enable_check_box);

            final CheatEntry info = (CheatEntry) this.getItem(position);
            if (info.equals(selectedCheat)) {
                vi.setBackgroundResource(android.R.color.darker_gray);//highlight selected item
                selectedCheatsListItemViewRef = new WeakReference<View>(vi);

                deleteBtn.setEnabled(true);
                editBtn.setEnabled(true);
            }
            else
                vi.setBackgroundResource(android.R.color.transparent);

            // name
            nameView.setText(info.getName());

            // "enabled" checkbox
            checkBox.setChecked(info.isEnabled());
            checkBox.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    info.setEnabled(checkBox.isChecked());
                }
            });

            // code
            String codePrefix = "";
            switch (info.getType()) {
                case GAME_GENIE:
                    codePrefix = "[GG] ";
                    break;
                case PRO_ACTION_ROCKY:
                    codePrefix = "[PR] ";
                    break;
            }
            codeView.setText(codePrefix + info.getCode());

            return vi;
        }
    }
}
