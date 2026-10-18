package io.highfidelity.hifiinterface.fragment;

import android.content.SharedPreferences;
import android.media.audiofx.AcousticEchoCanceler;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;
import android.preference.SwitchPreference;
import androidx.annotation.Nullable;

import io.highfidelity.hifiinterface.HifiUtils;
import io.highfidelity.hifiinterface.InterfaceActivity;
import io.highfidelity.hifiinterface.MenuOrientation;
import io.highfidelity.hifiinterface.theme.BackgroundDrawable;
import io.highfidelity.hifiinterface.theme.ThemeManager;
import io.highfidelity.hifiinterface.theme.ThemeSettings;
import io.highfidelity.hifiinterface.R;

public class SettingsFragment extends PreferenceFragment implements SharedPreferences.OnSharedPreferenceChangeListener {

    private final String HIFI_SETTINGS_ANDROID_GROUP = "Android";
    private final String HIFI_SETTINGS_AEC_KEY = "aec";
    private final String PREFERENCE_KEY_AEC = "aec";

    private final boolean DEFAULT_AEC_ENABLED = true;

    // the graphics settings, which are the engine's (AndroidHelper)
    private static final String PREFERENCE_KEY_GRAPHICS_PRESET = "graphics_preset";
    private static final String PREFERENCE_KEY_SHADOWS = "graphics_shadows";
    private static final String PREFERENCE_KEY_AMBIENT_OCCLUSION = "graphics_ambient_occlusion";
    private static final String PREFERENCE_KEY_RESOLUTION = "graphics_resolution";
    private static final String PREFERENCE_KEY_FRAME_RATE = "graphics_frame_rate";
    private static final String PREFERENCE_KEY_WORLD_DETAIL = "graphics_world_detail";
    private static final String PREFERENCE_KEY_DRAW_DISTANCE = "graphics_draw_distance";
    // a preset is put in place on the engine's thread: the switches it sets are read back after this
    private static final int PRESET_APPLY_DELAY_MSECS = 300;

    private native int nativeGetGraphicsPreset();
    private native void nativeSetGraphicsPreset(int preset);
    private native boolean nativeIsDeferredRenderingAvailable();
    private native boolean nativeGetShadowsEnabled();
    private native void nativeSetShadowsEnabled(boolean enabled);
    private native boolean nativeGetAmbientOcclusionEnabled();
    private native void nativeSetAmbientOcclusionEnabled(boolean enabled);
    private native float nativeGetResolutionScale();
    private native void nativeSetResolutionScale(float scale);
    private native int nativeGetFrameRateProfile();
    private native void nativeSetFrameRateProfile(int profile);
    private native int nativeGetWorldDetail();
    private native void nativeSetWorldDetail(int quality);
    private native float nativeGetDrawDistance();
    private native void nativeSetDrawDistance(float meters);

    private boolean mLoadingGraphicsSettings;
    // choosing, installing and customising themes (not over the world)
    private ThemeSettings mThemeSettings;

    @Override
    public void onActivityResult(int requestCode, int resultCode, android.content.Intent data) {
        if (mThemeSettings == null || !mThemeSettings.onActivityResult(requestCode, resultCode, data)) {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.settings);
        boolean aecAvailable = AcousticEchoCanceler.isAvailable();
        PreferenceManager.setDefaultValues(getContext(), R.xml.settings, false);

        if (!aecAvailable) {
            findPreference(PREFERENCE_KEY_AEC).setEnabled(false);
            HifiUtils.getInstance().updateHifiSetting(HIFI_SETTINGS_ANDROID_GROUP, HIFI_SETTINGS_AEC_KEY, false);
        }

        getPreferenceScreen().getSharedPreferences().edit().putBoolean(PREFERENCE_KEY_AEC,
                aecAvailable && HifiUtils.getInstance().getHifiSettingBoolean(HIFI_SETTINGS_ANDROID_GROUP, HIFI_SETTINGS_AEC_KEY, DEFAULT_AEC_ENABLED)).commit();

        if (!nativeIsDeferredRenderingAvailable()) {
            for (String key : new String[] { PREFERENCE_KEY_SHADOWS, PREFERENCE_KEY_AMBIENT_OCCLUSION }) {
                findPreference(key).setEnabled(false);
                findPreference(key).setSummary(R.string.graphics_needs_gles32);
            }
        }
        loadGraphicsSettings();

        // the menu's orientation is nothing to do with the world
        if (getActivity() instanceof InterfaceActivity) {
            getPreferenceScreen().removePreference(findPreference("pref_key_app"));
        } else {
            mThemeSettings = new ThemeSettings(this);
        }
    }

    // The engine's graphics settings into the screen's preferences
    private void loadGraphicsSettings() {
        mLoadingGraphicsSettings = true;
        int preset = Math.max(2, Math.min(4, nativeGetGraphicsPreset()));
        float scale = nativeGetResolutionScale();
        String resolution = scale <= 0.5f ? "0.5" : (scale <= 0.75f ? "0.75" : "1.0");
        getPreferenceScreen().getSharedPreferences().edit()
                .putString(PREFERENCE_KEY_GRAPHICS_PRESET, String.valueOf(preset))
                .putBoolean(PREFERENCE_KEY_SHADOWS, nativeGetShadowsEnabled())
                .putBoolean(PREFERENCE_KEY_AMBIENT_OCCLUSION, nativeGetAmbientOcclusionEnabled())
                .putString(PREFERENCE_KEY_RESOLUTION, resolution)
                .commit();
        ((ListPreference) findPreference(PREFERENCE_KEY_GRAPHICS_PRESET)).setValue(String.valueOf(preset));
        ((SwitchPreference) findPreference(PREFERENCE_KEY_SHADOWS)).setChecked(nativeGetShadowsEnabled());
        ((SwitchPreference) findPreference(PREFERENCE_KEY_AMBIENT_OCCLUSION)).setChecked(nativeGetAmbientOcclusionEnabled());
        ((ListPreference) findPreference(PREFERENCE_KEY_RESOLUTION)).setValue(resolution);
        // (30 fps and below show as 30; the screen's rate as the fastest)
        int frameRate = Math.max(1, Math.min(3, nativeGetFrameRateProfile()));
        ((ListPreference) findPreference(PREFERENCE_KEY_FRAME_RATE)).setValue(String.valueOf(frameRate));
        int worldDetail = Math.max(0, Math.min(2, nativeGetWorldDetail()));
        ((ListPreference) findPreference(PREFERENCE_KEY_WORLD_DETAIL)).setValue(String.valueOf(worldDetail));
        float distance = nativeGetDrawDistance();
        String drawDistance = distance <= 100.0f ? "100" : (distance <= 400.0f ? "400" : "16384");
        ((ListPreference) findPreference(PREFERENCE_KEY_DRAW_DISTANCE)).setValue(drawDistance);
        mLoadingGraphicsSettings = false;
    }

    @Override
    public void onViewCreated(android.view.View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        // over the world the panel is the background; on the menu screens, the theme's
        if (!(getActivity() instanceof InterfaceActivity)) {
            view.setBackground(new BackgroundDrawable(ThemeManager.get(getActivity()), getResources().getDisplayMetrics().density));
        }
    }

    public static SettingsFragment newInstance() {
        SettingsFragment fragment = new SettingsFragment();
        return fragment;
    }

    @Override
    public void onResume() {
        super.onResume();
        getPreferenceScreen().getSharedPreferences().registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onPause() {
        super.onPause();
        getPreferenceScreen().getSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        switch (key) {
            case "aec":
                HifiUtils.getInstance().updateHifiSetting(HIFI_SETTINGS_ANDROID_GROUP, HIFI_SETTINGS_AEC_KEY, sharedPreferences.getBoolean(key, false));
                break;
            case PREFERENCE_KEY_GRAPHICS_PRESET:
                if (!mLoadingGraphicsSettings) {
                    nativeSetGraphicsPreset(Integer.parseInt(sharedPreferences.getString(key, "2")));
                    if (getView() != null) {
                        getView().postDelayed(this::loadGraphicsSettings, PRESET_APPLY_DELAY_MSECS);
                    }
                }
                break;
            case PREFERENCE_KEY_SHADOWS:
                if (!mLoadingGraphicsSettings) {
                    nativeSetShadowsEnabled(sharedPreferences.getBoolean(key, false));
                }
                break;
            case PREFERENCE_KEY_AMBIENT_OCCLUSION:
                if (!mLoadingGraphicsSettings) {
                    nativeSetAmbientOcclusionEnabled(sharedPreferences.getBoolean(key, false));
                }
                break;
            case PREFERENCE_KEY_FRAME_RATE:
                if (!mLoadingGraphicsSettings) {
                    nativeSetFrameRateProfile(Integer.parseInt(sharedPreferences.getString(key, "2")));
                }
                break;
            case PREFERENCE_KEY_WORLD_DETAIL:
                if (!mLoadingGraphicsSettings) {
                    nativeSetWorldDetail(Integer.parseInt(sharedPreferences.getString(key, "1")));
                }
                break;
            case PREFERENCE_KEY_DRAW_DISTANCE:
                if (!mLoadingGraphicsSettings) {
                    nativeSetDrawDistance(Float.parseFloat(sharedPreferences.getString(key, "16384")));
                }
                break;
            case ThemeManager.PREFERENCE_KEY:
                // the menu screens again, in the new theme
                if (getActivity() != null && !(getActivity() instanceof InterfaceActivity)) {
                    ThemeManager.publishToEngine(getActivity());
                    getActivity().recreate();
                }
                break;
            case MenuOrientation.PREFERENCE_KEY:
                if (getActivity() != null && !(getActivity() instanceof InterfaceActivity)) {
                    MenuOrientation.apply(getActivity());
                }
                break;
            case PREFERENCE_KEY_RESOLUTION:
                if (!mLoadingGraphicsSettings) {
                    nativeSetResolutionScale(Float.parseFloat(sharedPreferences.getString(key, "1.0")));
                }
                break;
            default:
                break;
        }
    }
}
