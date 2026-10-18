package io.highfidelity.hifiinterface.theme;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Fragment;
import android.content.Intent;
import android.net.Uri;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.List;

import io.highfidelity.hifiinterface.R;

/**
 * The Settings screen's themes: choosing one, installing one from a zip, customising one as "My theme", exporting one
 * to share, and removing an installed one.
 */
public class ThemeSettings {
    public static final int REQUEST_INSTALL = 4101;
    public static final int REQUEST_EXPORT = 4102;

    private static final String KEY_INSTALL = "theme_install";
    private static final String KEY_CUSTOMISE = "theme_customise";
    private static final String KEY_EXPORT = "theme_export";
    private static final String KEY_REMOVE = "theme_remove";

    private final PreferenceFragment mFragment;

    public ThemeSettings(PreferenceFragment fragment) {
        mFragment = fragment;
        Preference install = fragment.findPreference(KEY_INSTALL);
        if (install == null) {
            return;
        }
        install.setOnPreferenceClickListener(preference -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] { "application/zip", "application/x-zip-compressed",
                    "application/octet-stream" });
            fragment.startActivityForResult(intent, REQUEST_INSTALL);
            return true;
        });
        fragment.findPreference(KEY_EXPORT).setOnPreferenceClickListener(preference -> {
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("application/zip");
            intent.putExtra(Intent.EXTRA_TITLE, ThemeManager.get(activity()).id + ".zip");
            fragment.startActivityForResult(intent, REQUEST_EXPORT);
            return true;
        });
        fragment.findPreference(KEY_REMOVE).setOnPreferenceClickListener(preference -> {
            Theme theme = ThemeManager.get(activity());
            if (ThemeManager.isBuiltIn(activity(), theme.id)) {
                return true;
            }
            ThemeInstaller.remove(activity(), theme.id);
            ThemeManager.select(activity(), ThemeManager.DEFAULT_THEME);
            themeChanged();
            return true;
        });
        fragment.findPreference(KEY_CUSTOMISE).setOnPreferenceClickListener(preference -> {
            customise();
            return true;
        });
        refresh();
    }

    // The theme list and the remove option, for the themes there are now
    public void refresh() {
        ListPreference list = (ListPreference) mFragment.findPreference(ThemeManager.PREFERENCE_KEY);
        if (list == null) {
            return;
        }
        List<ThemeManager.Info> themes = ThemeManager.list(activity());
        CharSequence[] names = new CharSequence[themes.size()];
        CharSequence[] ids = new CharSequence[themes.size()];
        for (int i = 0; i < themes.size(); i++) {
            names[i] = themes.get(i).name;
            ids[i] = themes.get(i).id;
        }
        list.setEntries(names);
        list.setEntryValues(ids);
        Theme theme = ThemeManager.get(activity());
        list.setValue(theme.id);
        Preference remove = mFragment.findPreference(KEY_REMOVE);
        boolean builtIn = ThemeManager.isBuiltIn(activity(), theme.id);
        remove.setEnabled(!builtIn);
        remove.setSummary(builtIn ? activity().getString(R.string.theme_remove_builtin)
                : activity().getString(R.string.theme_remove_summary, theme.name));
    }

    // From SettingsFragment.onActivityResult
    public boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != REQUEST_INSTALL && requestCode != REQUEST_EXPORT) {
            return false;
        }
        if (resultCode != Activity.RESULT_OK || data == null || data.getData() == null) {
            return true;
        }
        Uri uri = data.getData();
        if (requestCode == REQUEST_INSTALL) {
            try {
                String id = ThemeInstaller.install(activity(), uri);
                ThemeManager.select(activity(), id);
                Toast.makeText(activity(), activity().getString(R.string.theme_installed, id), Toast.LENGTH_SHORT).show();
                themeChanged();
            } catch (ThemeInstaller.ThemeException e) {
                new AlertDialog.Builder(activity())
                        .setMessage(activity().getString(R.string.theme_install_failed, e.getMessage()))
                        .setPositiveButton(android.R.string.ok, null)
                        .show();
            }
        } else {
            try {
                ThemeInstaller.export(activity(), ThemeManager.get(activity()).id, uri);
                Toast.makeText(activity(), R.string.theme_exported, Toast.LENGTH_SHORT).show();
            } catch (Exception e) {
                Toast.makeText(activity(), e.getMessage(), Toast.LENGTH_LONG).show();
            }
        }
        return true;
    }

    // "My theme": the current theme with other colours, corners, layout, glow and grid
    private void customise() {
        Activity activity = activity();
        Theme theme = ThemeManager.get(activity);
        float density = activity.getResources().getDisplayMetrics().density;
        int padding = (int) (20 * density);
        LinearLayout form = new LinearLayout(activity);
        form.setOrientation(LinearLayout.VERTICAL);
        form.setPadding(padding, padding / 2, padding, 0);

        String[] colourNames = activity.getResources().getStringArray(R.array.theme_custom_colour_names);
        String[] colourValues = activity.getResources().getStringArray(R.array.theme_custom_colour_values);
        Spinner primary = spinner(form, R.string.theme_custom_primary, colourNames, 0);
        Spinner accent = spinner(form, R.string.theme_custom_accent, colourNames, 0);
        String[] corners = { Theme.CORNERS_ROUNDED, Theme.CORNERS_CUT, Theme.CORNERS_SQUARE };
        Spinner cornerChoice = spinner(form, R.string.theme_custom_corners,
                activity.getResources().getStringArray(R.array.theme_custom_corner_names), indexOf(corners, theme.corners));
        String[] layouts = { Theme.LAYOUT_STANDARD, Theme.LAYOUT_CONSOLE };
        Spinner layoutChoice = spinner(form, R.string.theme_custom_layout,
                activity.getResources().getStringArray(R.array.theme_custom_layout_names), indexOf(layouts, theme.layout));
        Switch glow = toggle(form, R.string.theme_custom_glow, theme.glow);
        Switch grid = toggle(form, R.string.theme_custom_grid, Theme.PATTERN_GRID.equals(theme.backgroundPattern));

        new AlertDialog.Builder(activity)
                .setTitle(R.string.theme_customise)
                .setView(form)
                .setNegativeButton(android.R.string.cancel, null)
                .setPositiveButton(R.string.theme_custom_save, (dialog, which) -> {
                    try {
                        JSONObject changes = new JSONObject();
                        JSONObject colors = new JSONObject();
                        String primaryValue = colourValues[primary.getSelectedItemPosition()];
                        String accentValue = colourValues[accent.getSelectedItemPosition()];
                        if (!primaryValue.isEmpty()) {
                            colors.put("primary", primaryValue);
                        }
                        if (!accentValue.isEmpty()) {
                            colors.put("accent", accentValue);
                        }
                        changes.put("colors", colors);
                        JSONObject shape = new JSONObject();
                        shape.put("corners", corners[cornerChoice.getSelectedItemPosition()]);
                        shape.put("glow", glow.isChecked());
                        shape.put("backgroundPattern", grid.isChecked() ? Theme.PATTERN_GRID : Theme.PATTERN_NONE);
                        if (!Theme.CORNERS_ROUNDED.equals(corners[cornerChoice.getSelectedItemPosition()])) {
                            shape.put("brackets", true);
                            shape.put("border", "primary");
                        }
                        changes.put("shape", shape);
                        changes.put("layout", layouts[layoutChoice.getSelectedItemPosition()]);
                        ThemeInstaller.saveCustom(activity, theme.id, changes);
                        ThemeManager.select(activity, ThemeInstaller.MY_THEME);
                        themeChanged();
                    } catch (JSONException | java.io.IOException e) {
                        Toast.makeText(activity, e.getMessage(), Toast.LENGTH_LONG).show();
                    }
                })
                .show();
    }

    private Spinner spinner(LinearLayout form, int label, String[] items, int selected) {
        TextView title = new TextView(activity());
        title.setText(label);
        title.setPadding(0, (int) (12 * activity().getResources().getDisplayMetrics().density), 0, 0);
        form.addView(title);
        Spinner spinner = new Spinner(activity());
        ArrayAdapter<String> adapter = new ArrayAdapter<>(activity(), android.R.layout.simple_spinner_item, items);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);
        spinner.setSelection(Math.max(0, selected));
        form.addView(spinner, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
        return spinner;
    }

    private Switch toggle(LinearLayout form, int label, boolean checked) {
        Switch toggle = new Switch(activity());
        toggle.setText(label);
        toggle.setChecked(checked);
        toggle.setPadding(0, (int) (12 * activity().getResources().getDisplayMetrics().density), 0, 0);
        form.addView(toggle, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
        return toggle;
    }

    private static int indexOf(String[] values, String value) {
        for (int i = 0; i < values.length; i++) {
            if (values[i].equals(value)) {
                return i;
            }
        }
        return 0;
    }

    // The menus again in the theme (and the world takes it up when it's next shown)
    private void themeChanged() {
        Activity activity = activity();
        ThemeManager.publishToEngine(activity);
        activity.recreate();
    }

    private Activity activity() {
        return ((Fragment) mFragment).getActivity();
    }
}
