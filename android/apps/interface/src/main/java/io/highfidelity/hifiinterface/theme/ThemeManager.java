package io.highfidelity.hifiinterface.theme;

import android.content.Context;
import android.content.res.AssetManager;
import android.preference.PreferenceManager;
import android.util.Log;

import io.highfidelity.hifiinterface.HifiUtils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

/**
 * The app's theme: the Settings screen's "Theme", one of the themes in assets/themes.
 */
public final class ThemeManager {
    private static final String TAG = "ThemeManager";
    public static final String PREFERENCE_KEY = "app_theme";
    public static final String DEFAULT_THEME = "darkride";
    private static final String THEMES_FOLDER = "themes";

    private static Theme sTheme;

    private ThemeManager() {}

    public static synchronized Theme get(Context context) {
        String id = PreferenceManager.getDefaultSharedPreferences(context).getString(PREFERENCE_KEY, DEFAULT_THEME);
        if (sTheme == null || !sTheme.id.equals(id)) {
            sTheme = load(context.getAssets(), id);
        }
        return sTheme;
    }

    private static Theme load(AssetManager assets, String id) {
        // (theme ids are folder names: nothing that leaves the themes folder)
        if (id == null || !id.matches("[a-z0-9_]+")) {
            id = DEFAULT_THEME;
        }
        try {
            return Theme.load(assets, THEMES_FOLDER + "/" + id);
        } catch (Exception e) {
            Log.w(TAG, "Couldn't load the theme " + id + ", so using " + DEFAULT_THEME, e);
            try {
                return Theme.load(assets, THEMES_FOLDER + "/" + DEFAULT_THEME);
            } catch (Exception e2) {
                throw new IllegalStateException("The default theme is missing", e2);
            }
        }
    }

    // The theme's colours for the world's buttons and virtual pad (the engine's settings, which its scripts and the
    // display plugin read when the world starts): none for a theme that leaves them white
    public static void publishToEngine(Context context) {
        Theme theme = get(context);
        HifiUtils utils = HifiUtils.getInstance();
        utils.updateHifiSettingString("Android", "themeIconColor", theme.tintIcons ? hex(theme.icon) : "");
        utils.updateHifiSettingString("Android", "themeAlertColor", theme.tintIcons ? hex(theme.alert) : "");
        // its own images for the world's buttons and virtual pad, by name (icons/<name>.png)
        StringBuilder icons = new StringBuilder();
        try {
            String[] files = context.getAssets().list(THEMES_FOLDER + "/" + theme.id + "/icons");
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
        } catch (IOException e) {
            Log.w(TAG, "Couldn't list the theme's icons", e);
        }
        utils.updateHifiSettingString("Android", "themeIconFolder", "assets:/" + THEMES_FOLDER + "/" + theme.id + "/icons");
        utils.updateHifiSettingString("Android", "themeIcons", icons.toString());
    }

    private static String hex(int color) {
        return String.format("#%06X", color & 0xFFFFFF);
    }

    static String readAsset(AssetManager assets, String path) throws IOException {
        try (InputStream in = assets.open(path)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            int count;
            while ((count = in.read(buffer)) > 0) {
                out.write(buffer, 0, count);
            }
            return new String(out.toByteArray(), StandardCharsets.UTF_8);
        }
    }
}
