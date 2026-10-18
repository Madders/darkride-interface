package io.highfidelity.hifiinterface.theme;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.preference.PreferenceManager;
import android.util.Log;

import org.json.JSONObject;

import io.highfidelity.hifiinterface.HifiUtils;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * The app's theme: the Settings screen's "Theme". Themes are the app's own (assets/themes/&lt;id&gt;) or installed
 * (files/themes/&lt;id&gt;: ThemeInstaller), each a theme.json with its fonts and icons.
 */
public final class ThemeManager {
    private static final String TAG = "ThemeManager";
    public static final String PREFERENCE_KEY = "app_theme";
    public static final String DEFAULT_THEME = "darkride";
    static final String THEMES_FOLDER = "themes";
    private static final String ENGINE_THEME_KEY = "engine_theme";
    static final String ID_PATTERN = "[a-z0-9_]{1,40}";

    // A theme to choose from
    public static class Info {
        public final String id;
        public final String name;
        public final boolean builtIn;

        Info(String id, String name, boolean builtIn) {
            this.id = id;
            this.name = name;
            this.builtIn = builtIn;
        }
    }

    private static Theme sTheme;
    private static String sThemeStamp;
    // the theme last handed to the engine, and how many times one has been (InterfaceActivity compares this to know
    // when the world should take a new one up)
    private static String sPublishedTheme;
    private static int sPublishedVersion;

    public static int getPublishedVersion() {
        return sPublishedVersion;
    }

    private ThemeManager() {}

    public static synchronized Theme get(Context context) {
        String id = PreferenceManager.getDefaultSharedPreferences(context).getString(PREFERENCE_KEY, DEFAULT_THEME);
        String stamp = stamp(context, id);
        if (sTheme == null || !sTheme.id.equals(id) || !stamp.equals(sThemeStamp)) {
            sTheme = load(context, id);
            sThemeStamp = stamp(context, sTheme.id);
        }
        return sTheme;
    }

    // Uses a theme from now on
    public static void select(Context context, String id) {
        PreferenceManager.getDefaultSharedPreferences(context).edit().putString(PREFERENCE_KEY, id).apply();
    }

    // The themes there are: the app's own, then installed ones
    public static List<Info> list(Context context) {
        List<Info> themes = new ArrayList<>();
        try {
            String[] ids = context.getAssets().list(THEMES_FOLDER);
            if (ids != null) {
                Arrays.sort(ids);
                for (String id : ids) {
                    if (id.equals(DEFAULT_THEME)) {
                        themes.add(0, new Info(id, name(readAssetJson(context.getAssets(), id), id), true));
                    } else {
                        themes.add(new Info(id, name(readAssetJson(context.getAssets(), id), id), true));
                    }
                }
            }
        } catch (IOException e) {
            Log.w(TAG, "Couldn't list the app's themes", e);
        }
        File[] installed = installedFolder(context).listFiles();
        if (installed != null) {
            Arrays.sort(installed);
            for (File folder : installed) {
                File json = new File(folder, "theme.json");
                if (folder.isDirectory() && json.isFile() && folder.getName().matches(ID_PATTERN)) {
                    try {
                        themes.add(new Info(folder.getName(), name(new JSONObject(readFile(json)), folder.getName()), false));
                    } catch (Exception e) {
                        Log.w(TAG, "Couldn't read the theme " + folder.getName(), e);
                    }
                }
            }
        }
        return themes;
    }

    public static boolean isBuiltIn(Context context, String id) {
        try {
            String[] ids = context.getAssets().list(THEMES_FOLDER);
            return ids != null && Arrays.asList(ids).contains(id);
        } catch (IOException e) {
            return false;
        }
    }

    // Where installed themes go
    static File installedFolder(Context context) {
        return new File(context.getFilesDir(), THEMES_FOLDER);
    }

    private static Theme load(Context context, String id) {
        // (theme ids are folder names: nothing that leaves the themes folder)
        if (id == null || !id.matches(ID_PATTERN)) {
            id = DEFAULT_THEME;
        }
        try {
            File installed = new File(installedFolder(context), id);
            if (!isBuiltIn(context, id) && new File(installed, "theme.json").isFile()) {
                return Theme.load(installed);
            }
            return Theme.load(context.getAssets(), THEMES_FOLDER + "/" + id);
        } catch (Exception e) {
            Log.w(TAG, "Couldn't load the theme " + id + ", so using " + DEFAULT_THEME, e);
            try {
                return Theme.load(context.getAssets(), THEMES_FOLDER + "/" + DEFAULT_THEME);
            } catch (Exception e2) {
                throw new IllegalStateException("The default theme is missing", e2);
            }
        }
    }

    // Which version of a theme this is: an installed one changes when it's installed again or customised
    private static String stamp(Context context, String id) {
        File json = new File(new File(installedFolder(context), id), "theme.json");
        return id + "@" + (json.isFile() ? json.lastModified() : 0);
    }

    // The theme's colours and images for the world's buttons and virtual pad (the engine's settings, which its scripts
    // and the display plugin read): no colours for a theme that leaves them white
    public static void publishToEngine(Context context) {
        Theme theme = get(context);
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        String current = stamp(context, theme.id);
        if (sPublishedTheme == null) {
            // what the engine was given last time (its settings keep it)
            sPublishedTheme = preferences.getString(ENGINE_THEME_KEY, "");
        }
        if (current.equals(sPublishedTheme)) {
            return;
        }
        sPublishedTheme = current;
        sPublishedVersion++;
        preferences.edit().putString(ENGINE_THEME_KEY, current).apply();
        HifiUtils utils = HifiUtils.getInstance();
        utils.updateHifiSettingString("Android", "themeIconColor", theme.tintIcons ? hex(theme.icon) : "");
        utils.updateHifiSettingString("Android", "themeAlertColor", theme.tintIcons ? hex(theme.alert) : "");

        // its own images for the world's buttons and virtual pad, by name (icons/<name>.png)
        String[] files = null;
        String folder;
        File installed = new File(new File(installedFolder(context), theme.id), "icons");
        if (!isBuiltIn(context, theme.id)) {
            files = installed.list();
            folder = installed.getAbsolutePath();
        } else {
            try {
                files = context.getAssets().list(THEMES_FOLDER + "/" + theme.id + "/icons");
            } catch (IOException e) {
                Log.w(TAG, "Couldn't list the theme's icons", e);
            }
            folder = "assets:/" + THEMES_FOLDER + "/" + theme.id + "/icons";
        }
        StringBuilder icons = new StringBuilder();
        if (files != null) {
            for (String file : files) {
                if (file.endsWith(".png")) {
                    if (icons.length() > 0) {
                        icons.append(',');
                    }
                    icons.append(file.substring(0, file.length() - ".png".length()));
                }
            }
        }
        utils.updateHifiSettingString("Android", "themeIconFolder", folder);
        utils.updateHifiSettingString("Android", "themeIcons", icons.toString());
    }

    private static String hex(int color) {
        return String.format("#%06X", color & 0xFFFFFF);
    }

    private static String name(JSONObject json, String fallback) {
        return json == null ? fallback : json.optString("name", fallback);
    }

    private static JSONObject readAssetJson(AssetManager assets, String id) {
        try {
            return new JSONObject(readAsset(assets, THEMES_FOLDER + "/" + id + "/theme.json"));
        } catch (Exception e) {
            return null;
        }
    }

    static String readAsset(AssetManager assets, String path) throws IOException {
        try (InputStream in = assets.open(path)) {
            return read(in);
        }
    }

    static String readFile(File file) throws IOException {
        try (InputStream in = new FileInputStream(file)) {
            return read(in);
        }
    }

    private static String read(InputStream in) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int count;
        while ((count = in.read(buffer)) > 0) {
            out.write(buffer, 0, count);
        }
        return new String(out.toByteArray(), StandardCharsets.UTF_8);
    }
}
