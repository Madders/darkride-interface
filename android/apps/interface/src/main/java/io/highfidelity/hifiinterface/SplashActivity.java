package io.highfidelity.hifiinterface;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.FrameLayout;

import io.highfidelity.hifiinterface.theme.PocketWatchView;
import io.highfidelity.hifiinterface.theme.Theme;
import io.highfidelity.hifiinterface.theme.ThemeManager;

public class SplashActivity extends Activity {

    public static final String EXTRA_START_IN_DOMAIN = "start-in-domain";
    private boolean mStartInDomain;

    private native void registerLoadCompleteListener();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        MenuOrientation.apply(this);
        setContentView(R.layout.activity_splash);
        Theme theme = ThemeManager.get(this);
        findViewById(R.id.root_activity_splash).setBackgroundColor(theme.background);
        if (theme.whimsy) {
            // the White Rabbit's pocket watch instead of the spinner
            findViewById(R.id.splash_progress).setVisibility(View.GONE);
            ((FrameLayout) findViewById(R.id.splash_progress_container)).addView(new PocketWatchView(this, theme),
                    new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        }
        registerLoadCompleteListener();
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    protected void onResume() {
        super.onResume();
        View decorView = getWindow().getDecorView();
        // Hide the status bar.
        int uiOptions = View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
        decorView.setSystemUiVisibility(uiOptions);
    }

    @Override
    protected void onStop() {
        super.onStop();
    }

    public void onAppLoadedComplete() {
        if (!mStartInDomain) {
            if (HifiUtils.getInstance().isUserLoggedIn()) {
                startActivity(new Intent(this, MainActivity.class));
            } else {
                Intent menuIntent = new Intent(this, LoginMenuActivity.class);
                menuIntent.putExtra(LoginMenuActivity.EXTRA_FINISH_ON_BACK, true);
                startActivity(menuIntent);
            }
        }
        SplashActivity.this.finish();
    }
   
    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(EXTRA_START_IN_DOMAIN, mStartInDomain);
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        mStartInDomain = savedInstanceState.getBoolean(EXTRA_START_IN_DOMAIN, false);
    }
}
