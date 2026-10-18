package io.highfidelity.hifiinterface.theme;

import android.app.Fragment;
import android.app.FragmentManager;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.util.SparseIntArray;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import io.highfidelity.hifiinterface.R;

/**
 * Puts a theme on whole screens: walks their views, swapping the app's own colours for the theme's (light text for
 * its text, grey for its secondary text, the blues and violet for its primary, the dark blues for its background,
 * surface and panels) and setting its fonts, and for a theme with its own shapes, its fields and buttons. Lists are
 * themed as their rows appear. With the Darkride theme, whose colours are the app's own, nothing changes.
 */
public final class ThemeWalker {
    private static final int TEXT = 1;
    private static final int TEXT_SECONDARY = 2;
    private static final int PRIMARY = 3;
    private static final int OK = 4;
    private static final int ALERT = 5;
    private static final int BACKGROUND = 6;
    private static final int SURFACE = 7;
    private static final int PANEL = 8;
    private static final float HEADING_MIN_SIZE_SP = 22;

    private ThemeWalker() {}

    // Themes every fragment's screen in an activity as it's made
    public static void themeFragments(Context context, FragmentManager fragmentManager) {
        fragmentManager.registerFragmentLifecycleCallbacks(new FragmentManager.FragmentLifecycleCallbacks() {
            @Override
            public void onFragmentViewCreated(FragmentManager fm, Fragment fragment, View view, Bundle savedInstanceState) {
                apply(view, ThemeManager.get(context));
            }
        }, true);
    }

    public static void apply(View view, Theme theme) {
        apply(view, theme, roles(view.getResources()));
    }

    private static void apply(View view, Theme theme, SparseIntArray roles) {
        if (view.getTag(R.id.theme_styled) != null) {
            // styled already, by ThemeStyler
            return;
        }
        boolean ownShapes = !Theme.CORNERS_ROUNDED.equals(theme.corners);
        Drawable background = view.getBackground();
        if (background instanceof ColorDrawable) {
            int role = roles.get(((ColorDrawable) background).getColor(), 0);
            if (role == BACKGROUND && ownShapes) {
                view.setBackground(new BackgroundDrawable(theme, view.getResources().getDisplayMetrics().density));
            } else if (role != 0) {
                view.setBackgroundColor(color(theme, role));
            }
        }

        if (view instanceof EditText) {
            if (ownShapes) {
                ThemeStyler.styleField((TextView) view, theme);
            }
        } else if (view instanceof Button && !(view instanceof CompoundButton)) {
            if (ownShapes) {
                styleButton((Button) view, theme);
            }
        } else if (view instanceof TextView) {
            TextView text = (TextView) view;
            // headings: big text (the display font is wide, and long names in it wrap badly)
            float sizeSp = text.getTextSize() / text.getResources().getDisplayMetrics().scaledDensity;
            boolean heading = sizeSp >= HEADING_MIN_SIZE_SP;
            if (heading) {
                ThemeStyler.styleDisplay(text, theme);
            } else {
                ThemeStyler.styleBody(text, theme);
            }
            int role = roles.get(text.getCurrentTextColor(), 0);
            if (role != 0 && role < BACKGROUND) {
                text.setTextColor(color(theme, role));
            }
        }

        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                apply(group.getChildAt(i), theme, roles);
            }
            if (group instanceof RecyclerView || group instanceof AbsListView) {
                // rows appear later, and come and go
                group.setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() {
                    @Override
                    public void onChildViewAdded(View parent, View child) {
                        apply(child, theme, roles);
                    }

                    @Override
                    public void onChildViewRemoved(View parent, View child) {}
                });
            }
        }
    }

    // A button: outlined in the primary colour on a panel, its text in the display font
    private static void styleButton(Button button, Theme theme) {
        float density = button.getResources().getDisplayMetrics().density;
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(theme.panel);
        shape.setStroke((int) Math.max(1, density), theme.primary);
        button.setBackground(shape);
        button.setForeground(new PanelFrameDrawable(theme, theme.background, density));
        ThemeStyler.styleDisplay(button, theme);
        button.setTextColor(theme.primary);
    }

    private static int color(Theme theme, int role) {
        switch (role) {
            case TEXT: return theme.text;
            case TEXT_SECONDARY: return theme.textSecondary;
            case PRIMARY: return theme.primary;
            case OK: return theme.ok;
            case ALERT: return theme.alert;
            case BACKGROUND: return theme.background;
            case SURFACE: return theme.surface;
            case PANEL: return theme.panel;
            default: return 0;
        }
    }

    // The app's own colours (res/values/colors.xml), by what they're for
    private static SparseIntArray roles(Resources resources) {
        SparseIntArray roles = new SparseIntArray();
        int[][] table = {
            { TEXT, R.color.menuOption, R.color.white_opaque, R.color.clearText },
            { TEXT_SECONDARY, R.color.colorButton3, R.color.editTextHint, R.color.starUnselectedTint },
            { PRIMARY, R.color.hifiAquamarine, R.color.darkrideViolet, R.color.colorButton1, R.color.colorAccent,
                R.color.tabs, R.color.hifiLogoColor },
            { OK, R.color.darkrideGreen },
            { ALERT, R.color.colorLoginError },
            { BACKGROUND, R.color.backgroundLight },
            { SURFACE, R.color.backgroundDark, R.color.backgroundSearch, R.color.colorButton4 },
            { PANEL, R.color.darkrideOverlay, R.color.black_060 },
        };
        for (int[] row : table) {
            for (int i = 1; i < row.length; i++) {
                roles.put(resources.getColor(row[i], null), row[0]);
            }
        }
        roles.put(0xFFFFFFFF, TEXT);
        return roles;
    }
}
