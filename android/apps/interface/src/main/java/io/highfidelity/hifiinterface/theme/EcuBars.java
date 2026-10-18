package io.highfidelity.hifiinterface.theme;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import io.highfidelity.hifiinterface.R;

/**
 * The main screen's bars in the "ECU" layout (layout/activity_main_ecu.xml): the top bar with the section, status,
 * profile, date and clock, and the sections as tabs along the bottom, the current one in the primary colour with the
 * accent under it. The Home Assistant boiler ECU display's look.
 */
public class EcuBars {
    public interface Listener {
        // a section's tab (the navigation menu's item)
        void onSectionSelected(MenuItem item);
        void onProfileClicked(View anchor);
    }

    private static final long CLOCK_TICK_MSECS = 15 * 1000;
    private static final float RULE_ALPHA = 0.35f;
    private static final int UNDERLINE_DP = 3;

    private final Activity mActivity;
    private final Theme mTheme;
    private final Listener mListener;
    private final TextView mTitle;
    private final TextView mStatus;
    private final TextView mDate;
    private final TextView mClock;
    private final ImageView mProfilePicture;
    private final TextView mProfileName;
    private final LinearLayout mTabs;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private CharSequence mCurrentTitle = "";

    private final Runnable mTick = new Runnable() {
        @Override
        public void run() {
            Date now = new Date();
            mDate.setText(new SimpleDateFormat("EEE d MMM", Locale.getDefault()).format(now).toUpperCase(Locale.getDefault()));
            mClock.setText(new SimpleDateFormat("HH:mm", Locale.getDefault()).format(now));
            mHandler.postDelayed(this, CLOCK_TICK_MSECS);
        }
    };

    public EcuBars(Activity activity, Theme theme, Listener listener) {
        mActivity = activity;
        mTheme = theme;
        mListener = listener;
        mTitle = activity.findViewById(R.id.ecu_title);
        mStatus = activity.findViewById(R.id.ecu_status);
        mDate = activity.findViewById(R.id.ecu_date);
        mClock = activity.findViewById(R.id.ecu_clock);
        mProfilePicture = activity.findViewById(R.id.ecu_profile_picture);
        mProfileName = activity.findViewById(R.id.ecu_profile_name);
        mTabs = activity.findViewById(R.id.ecu_tabs);

        int rule = Theme.withAlpha(theme.primary, RULE_ALPHA);
        activity.findViewById(R.id.ecu_top_bar).setBackgroundColor(Theme.withAlpha(theme.surface, 0.88f));
        activity.findViewById(R.id.ecu_top_rule).setBackgroundColor(rule);
        activity.findViewById(R.id.ecu_bottom_rule).setBackgroundColor(rule);
        mTabs.setBackgroundColor(Theme.withAlpha(theme.surface, 0.88f));

        ThemeStyler.styleDisplay(mProfileName, theme);
        mProfileName.setTextColor(theme.text);
        ThemeStyler.styleMono(mDate, theme);
        mDate.setTextColor(theme.textSecondary);
        ThemeStyler.styleMono(mClock, theme);
        mClock.setTextColor(theme.text);
        ThemeStyler.styleMono(mStatus, theme);
        activity.findViewById(R.id.ecu_profile).setOnClickListener(listener::onProfileClicked);
    }

    public void start() {
        mHandler.removeCallbacks(mTick);
        mTick.run();
    }

    public void stop() {
        mHandler.removeCallbacks(mTick);
    }

    // The section shown: the title, and its tab
    public void setTitle(CharSequence title) {
        mCurrentTitle = title == null ? "" : title;
        mTitle.setText(ThemeStyler.title(mTheme, mCurrentTitle));
        mTitle.setLetterSpacing(mTheme.displayLetterSpacing);
        if (mTheme.glow) {
            float density = mActivity.getResources().getDisplayMetrics().density;
            mTitle.setShadowLayer(8 * density, 0, 0, Theme.withAlpha(mTheme.primary, 0.45f));
        }
        highlightTab();
    }

    // The sections (the navigation menu's visible items) as tabs
    public void setSections(Menu menu) {
        mTabs.removeAllViews();
        float density = mActivity.getResources().getDisplayMetrics().density;
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (!item.isVisible()) {
                continue;
            }
            LinearLayout tab = new LinearLayout(mActivity);
            tab.setOrientation(LinearLayout.VERTICAL);
            tab.setGravity(Gravity.CENTER);
            tab.setTag(item.getTitle().toString());
            TypedValue ripple = new TypedValue();
            mActivity.getTheme().resolveAttribute(android.R.attr.selectableItemBackground, ripple, true);
            tab.setBackgroundResource(ripple.resourceId);
            TextView label = new TextView(mActivity);
            label.setText(item.getTitle().toString());
            label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
            label.setGravity(Gravity.CENTER);
            ThemeStyler.styleDisplay(label, mTheme);
            tab.addView(label, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT));
            View underline = new View(mActivity);
            LinearLayout.LayoutParams underlineParams = new LinearLayout.LayoutParams((int) (64 * density),
                    (int) (UNDERLINE_DP * density));
            underlineParams.topMargin = (int) (6 * density);
            tab.addView(underline, underlineParams);
            tab.setOnClickListener(view -> mListener.onSectionSelected(item));
            mTabs.addView(tab, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1));
        }
        highlightTab();
    }

    // Logged in or not: the status chip and the profile
    public void setUser(String username, boolean loggedIn) {
        String status = loggedIn ? "● ONLINE" : "○ GUEST";
        int color = loggedIn ? mTheme.ok : mTheme.textSecondary;
        mStatus.setText(status);
        mStatus.setTextColor(color);
        GradientDrawable chip = new GradientDrawable();
        chip.setColor(Theme.withAlpha(color, 0.10f));
        chip.setStroke((int) Math.max(1, mActivity.getResources().getDisplayMetrics().density), color);
        mStatus.setBackground(chip);
        mProfileName.setText(loggedIn && username != null && !username.isEmpty() ? username
                : mActivity.getString(R.string.login));
    }

    public void setProfilePicture(Drawable picture) {
        mProfilePicture.setImageDrawable(picture);
    }

    private void highlightTab() {
        String current = mCurrentTitle.toString();
        for (int i = 0; i < mTabs.getChildCount(); i++) {
            LinearLayout tab = (LinearLayout) mTabs.getChildAt(i);
            boolean selected = current.equalsIgnoreCase(String.valueOf(tab.getTag()));
            ((TextView) tab.getChildAt(0)).setTextColor(selected ? mTheme.primary : mTheme.textSecondary);
            tab.getChildAt(1).setBackgroundColor(selected ? mTheme.accent : 0);
        }
    }
}
