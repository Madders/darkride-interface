package io.highfidelity.hifiinterface.theme;

import android.content.res.AssetManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.util.Log;

import org.json.JSONObject;

/**
 * A look for the app: colours, fonts and shapes, read from a theme's theme.json (assets/themes/&lt;id&gt;/). Themes are
 * data only: nothing in one runs.
 */
public class Theme {
    private static final String TAG = "Theme";

    public static final String CORNERS_ROUNDED = "rounded";
    public static final String CORNERS_CUT = "cut";
    public static final String CORNERS_SQUARE = "square";
    public static final String PATTERN_NONE = "none";
    public static final String PATTERN_GRID = "grid";
    public static final String LAYOUT_STANDARD = "standard";
    public static final String LAYOUT_ECU = "ecu";

    public String id;
    public String name;

    public int background;
    public int surface;
    public int panel;
    public int primary;
    public int accent;
    public int alert;
    public int ok;
    public int text;
    public int textSecondary;
    public int disabled;
    public int icon;

    // null for the system's font
    public Typeface displayFont;
    public Typeface bodyFont;
    public Typeface monoFont;
    public boolean displayUppercase;
    public float displayLetterSpacing;

    public String corners = CORNERS_ROUNDED;
    public float cornerSizeDp = 8;
    public boolean brackets;
    // the border colour, and how strong it is (0 for none)
    public int border;
    public float borderAlpha;
    public boolean glow;
    // behind the screens: "none" or "grid"
    public String backgroundPattern = PATTERN_NONE;

    // Wonderland touches (Darkride): the pocket watch while loading, the rabbit hole into worlds, the Cheshire grin when
    // nothing's found
    public boolean whimsy;

    // the main screen's layout: "standard" (a menu rail or drawer) or "ecu" (bars top and bottom: EcuBars)
    public String layout = LAYOUT_STANDARD;

    // whether the world's buttons and virtual pad take the icon colour (otherwise they're white)
    public boolean tintIcons;

    // shown before screen titles, as "PREFIX // TITLE"; null for the title alone
    public String titlePrefix;

    static Theme load(AssetManager assets, String folder) throws Exception {
        JSONObject json = new JSONObject(ThemeManager.readAsset(assets, folder + "/theme.json"));
        Theme theme = new Theme();
        theme.id = json.getString("id");
        theme.name = json.optString("name", theme.id);

        JSONObject colors = json.getJSONObject("colors");
        theme.background = color(colors, "background", 0xFF1C2136);
        theme.surface = color(colors, "surface", theme.background);
        theme.panel = color(colors, "panel", theme.surface);
        theme.primary = color(colors, "primary", 0xFFA548FF);
        theme.accent = color(colors, "accent", theme.primary);
        theme.alert = color(colors, "alert", 0xFFFF5555);
        theme.ok = color(colors, "ok", 0xFF50FA7B);
        theme.text = color(colors, "text", Color.WHITE);
        theme.textSecondary = color(colors, "textSecondary", theme.text);
        theme.disabled = color(colors, "disabled", theme.textSecondary);
        theme.icon = color(colors, "icon", theme.text);

        JSONObject fonts = json.optJSONObject("fonts");
        if (fonts != null) {
            theme.displayFont = font(assets, folder, fonts, "display");
            theme.bodyFont = font(assets, folder, fonts, "body");
            theme.monoFont = font(assets, folder, fonts, "mono");
            theme.displayUppercase = fonts.optBoolean("displayUppercase", false);
            theme.displayLetterSpacing = (float) fonts.optDouble("displayLetterSpacing", 0);
        }

        JSONObject shape = json.optJSONObject("shape");
        if (shape != null) {
            theme.corners = shape.optString("corners", CORNERS_ROUNDED);
            theme.cornerSizeDp = (float) shape.optDouble("cornerSize", 8);
            theme.brackets = shape.optBoolean("brackets", false);
            String border = shape.isNull("border") ? null : shape.optString("border", null);
            theme.border = "accent".equals(border) ? theme.accent : theme.primary;
            theme.borderAlpha = border == null ? 0 : (float) shape.optDouble("borderAlpha", 0.35);
            theme.glow = shape.optBoolean("glow", false);
            theme.backgroundPattern = shape.optString("backgroundPattern", PATTERN_NONE);
        }

        theme.layout = json.optString("layout", LAYOUT_STANDARD);
        theme.whimsy = json.optBoolean("whimsy", false);

        JSONObject icons = json.optJSONObject("icons");
        if (icons != null) {
            theme.tintIcons = icons.optBoolean("tint", false);
        }

        JSONObject title = json.optJSONObject("title");
        if (title != null && !title.isNull("prefix")) {
            theme.titlePrefix = title.optString("prefix", null);
        }
        return theme;
    }

    // A screen's title in this theme's style
    public String displayText(CharSequence text) {
        String value = text == null ? "" : text.toString();
        return displayUppercase ? value.toUpperCase() : value;
    }

    public static int withAlpha(int color, float alpha) {
        return (Math.round(Math.max(0, Math.min(1, alpha)) * 255) << 24) | (color & 0x00FFFFFF);
    }

    private static int color(JSONObject colors, String key, int fallback) {
        String value = colors.optString(key, null);
        if (value == null || value.isEmpty()) {
            return fallback;
        }
        try {
            return Color.parseColor(value);
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "Bad colour " + key + ": " + value);
            return fallback;
        }
    }

    private static Typeface font(AssetManager assets, String folder, JSONObject fonts, String key) {
        if (fonts.isNull(key)) {
            return null;
        }
        String path = fonts.optString(key, null);
        // only the theme's own font files
        if (path == null || path.contains("..") || !(path.endsWith(".ttf") || path.endsWith(".otf"))) {
            return null;
        }
        try {
            return Typeface.createFromAsset(assets, folder + "/" + path);
        } catch (RuntimeException e) {
            Log.w(TAG, "Couldn't load the font " + path, e);
            return null;
        }
    }
}
