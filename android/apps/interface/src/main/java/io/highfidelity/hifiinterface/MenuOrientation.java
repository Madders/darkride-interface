package io.highfidelity.hifiinterface;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.preference.PreferenceManager;

/**
 * Which way up the menu screens (places, people, login...) are: the Settings screen's "Menu orientation". The world
 * itself is always landscape (InterfaceActivity, in the manifest).
 */
public final class MenuOrientation {
    public static final String PREFERENCE_KEY = "menu_orientation";
    public static final String LANDSCAPE = "landscape";
    public static final String PORTRAIT = "portrait";
    public static final String FOLLOW_PHONE = "auto";

    private MenuOrientation() {}

    public static void apply(Activity activity) {
        String value = PreferenceManager.getDefaultSharedPreferences(activity).getString(PREFERENCE_KEY, LANDSCAPE);
        int orientation;
        switch (value) {
            case PORTRAIT:
                orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
                break;
            case FOLLOW_PHONE:
                orientation = ActivityInfo.SCREEN_ORIENTATION_FULL_USER;
                break;
            default:
                orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
                break;
        }
        if (activity.getRequestedOrientation() != orientation) {
            activity.setRequestedOrientation(orientation);
        }
    }
}
