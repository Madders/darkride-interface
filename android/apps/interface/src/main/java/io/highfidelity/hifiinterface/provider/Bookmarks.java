package io.highfidelity.hifiinterface.provider;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * Worlds saved by address with a name, on this phone only: for a world being tested, or a private one, that the
 * metaverse's directory doesn't list. Newest first.
 */
public class Bookmarks {

    public static class Bookmark {
        public final String name;
        public final String address;

        public Bookmark(String name, String address) {
            this.name = name;
            this.address = address;
        }

        public boolean matches(String filterText) {
            String filter = filterText == null ? "" : filterText.trim().toLowerCase();
            return filter.isEmpty() || name.toLowerCase().contains(filter) || address.toLowerCase().contains(filter);
        }
    }

    private static final String PREFERENCES = "bookmarks";
    private static final String KEY = "bookmarks";

    private final SharedPreferences mPreferences;

    public Bookmarks(Context context) {
        mPreferences = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
    }

    public List<Bookmark> getAll() {
        List<Bookmark> bookmarks = new ArrayList<>();
        try {
            JSONArray array = new JSONArray(mPreferences.getString(KEY, "[]"));
            for (int i = 0; i < array.length(); i++) {
                JSONObject entry = array.getJSONObject(i);
                bookmarks.add(new Bookmark(entry.optString("name"), entry.optString("address")));
            }
        } catch (JSONException e) {
            // unreadable: as good as none
        }
        return bookmarks;
    }

    // Saves a bookmark, first in the list; one with the same address is replaced (renamed)
    public void save(String name, String address) {
        List<Bookmark> bookmarks = getAll();
        remove(bookmarks, address);
        bookmarks.add(0, new Bookmark(name, address));
        write(bookmarks);
    }

    public void delete(String address) {
        List<Bookmark> bookmarks = getAll();
        remove(bookmarks, address);
        write(bookmarks);
    }

    private static void remove(List<Bookmark> bookmarks, String address) {
        for (int i = bookmarks.size() - 1; i >= 0; i--) {
            if (bookmarks.get(i).address.equalsIgnoreCase(address)) {
                bookmarks.remove(i);
            }
        }
    }

    private void write(List<Bookmark> bookmarks) {
        JSONArray array = new JSONArray();
        try {
            for (Bookmark bookmark : bookmarks) {
                array.put(new JSONObject().put("name", bookmark.name).put("address", bookmark.address));
            }
        } catch (JSONException e) {
            return;
        }
        mPreferences.edit().putString(KEY, array.toString()).apply();
    }
}
