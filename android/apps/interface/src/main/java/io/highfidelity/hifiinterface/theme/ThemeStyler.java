package io.highfidelity.hifiinterface.theme;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextPaint;
import android.text.style.ForegroundColorSpan;
import android.text.style.MetricAffectingSpan;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.appcompat.widget.Toolbar;
import androidx.cardview.widget.CardView;

import com.google.android.material.navigation.NavigationView;

/**
 * Puts a theme on the app's views.
 */
public final class ThemeStyler {
    private static final String TITLE_SEPARATOR = " // ";
    private static final float GLOW_RADIUS_DP = 8;
    private static final float GLOW_ALPHA = 0.45f;
    private static final float CARD_SPACED_TITLE_SIZE_SP = 17;
    private static final float CARD_MAX_LETTER_SPACING = 0.1f;

    private ThemeStyler() {}

    // Headings: the display font, in capitals and spaced out if the theme says
    public static void styleDisplay(TextView view, Theme theme) {
        if (theme.displayFont != null) {
            view.setTypeface(theme.displayFont);
        }
        view.setAllCaps(theme.displayUppercase);
        view.setLetterSpacing(theme.displayLetterSpacing);
        // (only screen titles glow: a glow boxes in emoji, which place names often have)
    }

    public static void styleBody(TextView view, Theme theme) {
        if (theme.bodyFont != null) {
            view.setTypeface(theme.bodyFont);
        }
    }

    public static void styleMono(TextView view, Theme theme) {
        if (theme.monoFont != null) {
            view.setTypeface(theme.monoFont);
        }
    }

    // A screen's title: "PREFIX // TITLE" with the prefix in the primary colour and the rest in the accent, for a
    // theme with a prefix
    public static CharSequence title(Theme theme, CharSequence title) {
        String text = theme.displayText(title);
        if (theme.titlePrefix == null) {
            return fonted(text, theme.displayFont);
        }
        SpannableStringBuilder builder = new SpannableStringBuilder();
        builder.append(theme.displayText(theme.titlePrefix));
        builder.setSpan(new ForegroundColorSpan(theme.primary), 0, builder.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        int start = builder.length();
        builder.append(TITLE_SEPARATOR).append(text);
        builder.setSpan(new ForegroundColorSpan(theme.accent), start, builder.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        if (theme.displayFont != null) {
            builder.setSpan(new FontSpan(theme.displayFont), 0, builder.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        return builder;
    }

    // The toolbar's title text, once it has one: spaced and glowing like other headings
    public static void styleToolbarTitle(Toolbar toolbar, Theme theme) {
        for (int i = 0; i < toolbar.getChildCount(); i++) {
            View child = toolbar.getChildAt(i);
            if (child instanceof TextView) {
                TextView title = (TextView) child;
                title.setLetterSpacing(theme.displayLetterSpacing);
                if (theme.glow) {
                    float density = title.getResources().getDisplayMetrics().density;
                    title.setShadowLayer(GLOW_RADIUS_DP * density, 0, 0, Theme.withAlpha(theme.primary, GLOW_ALPHA));
                }
            }
        }
    }

    // The main screen: its background, toolbar, status bar and menu
    public static void styleMainScreen(Activity activity, Theme theme, View root, Toolbar toolbar,
                                       NavigationView navigation) {
        root.setBackground(new BackgroundDrawable(theme, root.getResources().getDisplayMetrics().density));
        View content = activity.findViewById(android.R.id.content);
        if (content != null) {
            content.setBackgroundColor(theme.background);
        }
        toolbar.setBackgroundColor(theme.surface);
        toolbar.setTitleTextColor(theme.text);
        activity.getWindow().setStatusBarColor(theme.surface);
        activity.getWindow().setNavigationBarColor(theme.surface);

        navigation.setBackgroundColor(theme.surface);
        if (navigation.getParent() instanceof ViewGroup && !(navigation.getParent() instanceof androidx.drawerlayout.widget.DrawerLayout)) {
            ((View) navigation.getParent()).setBackgroundColor(theme.surface);
        }
        int[][] states = { { android.R.attr.state_checked }, {} };
        navigation.setItemTextColor(new ColorStateList(states, new int[] { theme.primary, theme.text }));
        navigation.setItemIconTintList(new ColorStateList(states, new int[] { theme.primary, theme.icon }));
        Menu menu = navigation.getMenu();
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            item.setTitle(fonted(theme.displayText(item.getTitle()), theme.displayFont));
        }
    }

    // A place's card in the places list
    public static void styleCard(CardView card, Theme theme, TextView name, TextView author, View nameStrip,
                                 View people, TextView peopleCount) {
        card.setTag(io.highfidelity.hifiinterface.R.id.theme_styled, true);
        float density = card.getResources().getDisplayMetrics().density;
        boolean rounded = Theme.CORNERS_ROUNDED.equals(theme.corners);
        card.setRadius(rounded ? theme.cornerSizeDp * density : 0);
        card.setCardBackgroundColor(theme.surface);
        if (!rounded) {
            card.setForeground(new PanelFrameDrawable(theme, theme.background, density));
        }
        styleDisplay(name, theme);
        name.setTextColor(theme.text);
        if (theme.displayLetterSpacing > 0) {
            // spaced-out capitals are wide: smaller and a little tighter, so place names fit
            name.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, CARD_SPACED_TITLE_SIZE_SP);
            name.setLetterSpacing(Math.min(theme.displayLetterSpacing, CARD_MAX_LETTER_SPACING));
        }
        styleBody(author, theme);
        author.setTextColor(theme.textSecondary);
        nameStrip.setBackgroundColor(theme.panel);
        styleMono(peopleCount, theme);
        if (!rounded) {
            GradientDrawable badge = new GradientDrawable();
            badge.setColor(theme.panel);
            badge.setStroke((int) Math.max(1, density), Theme.withAlpha(theme.border, Math.max(theme.borderAlpha, 0.35f)));
            people.setBackground(badge);
        }
    }

    // A link-like menu option (log out, the privacy policy)
    public static void styleMenuOption(TextView view, Theme theme) {
        styleDisplay(view, theme);
        view.setTextColor(theme.textSecondary);
    }

    // A search box or other field
    public static void styleField(TextView field, Theme theme) {
        float density = field.getResources().getDisplayMetrics().density;
        styleBody(field, theme);
        field.setTextColor(theme.text);
        field.setHintTextColor(theme.textSecondary);
        if (!Theme.CORNERS_ROUNDED.equals(theme.corners)) {
            field.setBackground(new ColorDrawable(theme.surface));
            field.setForeground(new PanelFrameDrawable(theme, theme.background, density));
        }
    }

    private static CharSequence fonted(String text, Typeface font) {
        if (font == null) {
            return text;
        }
        SpannableString spannable = new SpannableString(text);
        spannable.setSpan(new FontSpan(font), 0, text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return spannable;
    }

    // A typeface for part of a text (TypefaceSpan only takes one from Android 9)
    private static class FontSpan extends MetricAffectingSpan {
        private final Typeface mTypeface;

        FontSpan(Typeface typeface) {
            mTypeface = typeface;
        }

        @Override
        public void updateDrawState(TextPaint paint) {
            apply(paint);
        }

        @Override
        public void updateMeasureState(TextPaint paint) {
            apply(paint);
        }

        private void apply(Paint paint) {
            paint.setTypeface(mTypeface);
        }
    }
}
